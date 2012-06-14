#include <memory>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cassert>

#include <pthread.h>
#include <unistd.h>

namespace lvldb
{

typedef unsigned long      seq_t;
typedef std::atomic<seq_t> atomic_seq_t;

template<typename T>
class disruptor_t
{
	public:

	disruptor_t(size_t size):
		size_(size),
		ring_(std::unique_ptr<T[]>(new T[size]()))
	{
		// Test if size is a power of two
		assert((size & (size - 1)) == 0);
	}

	inline size_t get_index(seq_t seq)
	{
		// Fast algorithm to find the modulo of a power of two
		return seq & (size_ - 1);
	}

	inline T operator[](seq_t seq) const
	{
		return ring_[get_index(seq)];
	}

	inline T &operator[](seq_t seq)
	{
		return ring_[get_index(seq)];
	}

	private:

	const size_t         size_;
	std::unique_ptr<T[]> ring_;
};

template<typename T>
class fence_t
{
	public:

	enum type_t {producer, consumer};

	fence_t(disruptor_t<T> &disruptor, type_t type,
		int atomic_retries = 4, int atomic_sleep = 100):
		seq_(0),
		next_(0),
		disruptor_(disruptor),
		type_(type),
		next_fence_(nullptr),
		atomic_retries_(atomic_retries),
		atomic_sleep_(atomic_sleep),
		stop_tasks_(false)
	{
		long line_size;

		if ((line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE)) == -1) {
			perror("sysconf");
			exit(EXIT_FAILURE);
		}

		assert(sizeof(fence_t) >= static_cast<size_t>(line_size));
	}

	void set_next_fence(const fence_t *next_fence)
	{
		next_fence_ = next_fence;
	}

	inline T &acquire_slot(seq_t &task_seq)
	{
		if (type_ == producer && next_ == 0) {
			seq_t expected = 0;

			if (next_.compare_exchange_strong(expected, 1)) {
				task_seq = 0;

				return disruptor_[task_seq];
			}
		}

		check_consistency();

		while (true) {
			int pauses = 0;
			seq_t next = next_;

			while (disruptor_.get_index(next) ==
			       disruptor_.get_index(next_fence_->seq_))
				pause_thread(pauses);

			if (!next_.compare_exchange_strong(next, next + 1))
				continue;

			task_seq = next;
			break;
		}

		check_consistency();

		return disruptor_[task_seq];
	}

	inline void release_slot(seq_t task_seq)
	{
		int pauses;
		seq_t expected;

		check_consistency();

		pauses = 0;
		while (disruptor_.get_index(task_seq + 1) ==
		       disruptor_.get_index(next_fence_->seq_))
			pause_thread(pauses);

		pauses = 0;
		expected = task_seq;
		while (!seq_.compare_exchange_strong(expected, task_seq + 1)) {
			expected = task_seq;
			pause_thread(pauses);
		}

		check_consistency();
	}

	void stop_tasks()
	{
		stop_tasks_ = true;
	}

	private:

	char              padding_[64];
	atomic_seq_t      seq_, next_;
	disruptor_t<T>   &disruptor_;
	const type_t      type_;
	const fence_t    *next_fence_;
	const int         atomic_retries_;
	const int         atomic_sleep_; // Milliseconds
	std::atomic<bool> stop_tasks_;

	inline void pause_thread(int &pauses)
	{
		if (pauses < atomic_retries_)
			__asm__ __volatile__("pause" ::: "memory");
		else {
			struct timespec req = {0, atomic_sleep_ * 1000 * 1000};

			if (stop_tasks_)
				pthread_exit(nullptr);

			if (nanosleep(&req, nullptr) == -1) {
				perror("nanosleep");
				exit(EXIT_FAILURE);
			}
		}

		pauses++;
	}

	inline void check_consistency()
	{
		assert(seq_ <= next_);
		assert(next_fence_ != nullptr);
		assert(seq_ != 0 ? seq_ != next_fence_->seq_ : true);
		assert(type_ == producer ? next_fence_->type_ == consumer : true);
	}
};

template<typename T>
class task_t
{
	public:

	task_t(fence_t<T> &fence):
		fence_(fence)
	{ }

	static void *start_thread(void *arg)
	{
		static_cast<task_t *>(arg)->run();

		return nullptr;
	}

	void start()
	{
		if (pthread_create(&thread_, nullptr, start_thread, this) != 0) {
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	void stop()
	{
		fence_.stop_tasks();

		if (pthread_join(thread_, nullptr) != 0) {
			perror("pthread_join");
			exit(EXIT_FAILURE);
		}
	}

	inline void run()
	{
		while (true) {
			T &slot = fence_.acquire_slot(seq_);

			process_slot(slot);
			fence_.release_slot(seq_);
		}
	}

	protected:

	seq_t       seq_;
	fence_t<T> &fence_;
	pthread_t   thread_;

	virtual void process_slot(T &slot) = 0;
};

}

#include <iostream>

template<typename T>
class task1_t: public lvldb::task_t<T>
{
	public:

	task1_t(lvldb::fence_t<T> &fence):
		lvldb::task_t<T>(fence),
		initialized_(false)
	{ }

	void process_slot(T &slot)
	{
		if (slot != 0)
			initialized_ = true;

		if (initialized_)
			assert(slot % 4 == 3);
		else
			assert(slot == 0);

		slot = this->seq_ * 4;

		flockfile(stdout);
		std::cout << "task 1 seq = " << this->seq_;
		std::cout << " slot = "      << slot << std::endl;
		funlockfile(stdout);
	}

	private:

	bool initialized_;
};

template<typename T>
class task2_t: public lvldb::task_t<T>
{
	public:

	task2_t(lvldb::fence_t<T> &fence):
		lvldb::task_t<T>(fence)
	{ }

	void process_slot(T &slot)
	{
		if (this->seq_ > 0)
			assert(slot > 0);

		assert(static_cast<size_t>(slot) == this->seq_ * 4);

		slot = this->seq_ * 4 + 1;

		flockfile(stdout);
		std::cout << "task 2 seq = " << this->seq_;
		std::cout << " slot = "      << slot << std::endl;
		funlockfile(stdout);
	}
};

template<typename T>
class task3_t: public lvldb::task_t<T>
{
	public:

	task3_t(lvldb::fence_t<T> &fence):
		lvldb::task_t<T>(fence)
	{ }

	void process_slot(T &slot)
	{
		if (this->seq_ > 0)
			assert(slot > 0);

		assert(static_cast<size_t>(slot) == this->seq_ * 4);

		slot = this->seq_ * 4 + 2;

		flockfile(stdout);
		std::cout << "task 3 seq = " << this->seq_;
		std::cout << " slot = "      << slot << std::endl;
		funlockfile(stdout);
	}
};

template<typename T>
class task4_t: public lvldb::task_t<T>
{
	public:

	task4_t(lvldb::fence_t<T> &fence):
		lvldb::task_t<T>(fence)
	{ }

	void process_slot(T &slot)
	{
		if (this->seq_ > 0)
			assert(slot > 0);

		assert(static_cast<size_t>(slot) == this->seq_ * 4 + 1 ||
		       static_cast<size_t>(slot) == this->seq_ * 4 + 2);

		slot = this->seq_ * 4 + 3;

		flockfile(stdout);
		std::cout << "task 4 seq = " << this->seq_;
		std::cout << " slot = "      << slot << std::endl;
		funlockfile(stdout);
	}
};

// XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX
//
// Implementar mutex_fence_t y renombrar el otro a atomic_fence_t
//
// Hacer un benchmark con mutex_fence_t y en cada slot que se calcule
// varias iteraciones de MurmurHash para que tarde. El productor coge
// datos de algun sitio?
//
// XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX

int main(int argc, char *argv[])
{
	lvldb::disruptor_t<long> d(8);
	lvldb::fence_t<long>     f1(d, lvldb::fence_t<long>::producer);
	lvldb::fence_t<long>     f2(d, lvldb::fence_t<long>::consumer);
	lvldb::fence_t<long>     f3(d, lvldb::fence_t<long>::consumer);

	task1_t<long> t1(f1);
	task2_t<long> t2(f2);
	task3_t<long> t3(f2);
	task4_t<long> t4(f3);

	f3.set_next_fence(&f2);
	f2.set_next_fence(&f1);
	f1.set_next_fence(&f3);

	t1.start();
	t2.start();
	t3.start();
	t4.start();

	sleep(10);

	t1.stop();
	t2.stop();
	t3.stop();
	t4.stop();

	return 0;
}
