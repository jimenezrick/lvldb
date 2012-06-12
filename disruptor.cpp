#include <memory>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cassert>

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
		ring_(std::unique_ptr<T[]>(new T[size]))
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

class task_t
{
	// XXX XXX XXX
	// XXX XXX XXX
	// XXX XXX XXX
};

template<typename T>
class fence_t
{
	public:

	enum type_t {producer, consumer};

	fence_t(disruptor_t<T> &disruptor, type_t type):
		seq_(0),
		next_(0),
		disruptor_(disruptor),
		type_(type),
		next_fence_(nullptr)
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

	inline T acquire_slot(seq_t &task_seq)
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
			seq_t next = next_;

			while (disruptor_.get_index(next) == disruptor_.get_index(next_fence_->seq_))
				pause_thread();

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
		seq_t expected;

		check_consistency();

		while (disruptor_.get_index(task_seq + 1) == disruptor_.get_index(next_fence_->seq_))
			pause_thread();

		expected = task_seq;
		while (!seq_.compare_exchange_strong(expected, task_seq + 1)) {
			expected = task_seq;
			pause_thread();
		}

		check_consistency();
	}

	private:

	char            padding_[64];
	atomic_seq_t    seq_, next_;
	disruptor_t<T> &disruptor_;
	const type_t    type_;
	const fence_t  *next_fence_;

	inline void pause_thread()
	{
		//
		// XXX: Pausa con espera exponencial tras varias iteraciones
		//      Crear 2 atributos: atomic_retries_, atomic_sleep_
		//
		__asm__ __volatile__("pause");
	}

	inline void check_consistency()
	{
		assert(seq_ <= next_);
		assert(next_fence_ != nullptr);
		assert(seq_ != 0 ? seq_ != next_fence_->seq_ : true);
		assert(type_ == producer ? next_fence_->type_ == consumer : true);
	}
};

}

#include <iostream>
#include <thread>

void fun1(lvldb::fence_t<long> *f)
{
	lvldb::seq_t s;

	while (true) {
		f->acquire_slot(s);
		sleep(1);
		f->release_slot(s);

		flockfile(stdout);
		std::cout << "t1 s = " << s << std::endl;
		funlockfile(stdout);
	}
}

void fun2(lvldb::fence_t<long> *f)
{
	lvldb::seq_t s;

	while (true) {
		f->acquire_slot(s);
		f->release_slot(s);

		flockfile(stdout);
		std::cout << "t2 s = " << s << std::endl;
		funlockfile(stdout);
	}
}

void fun3(lvldb::fence_t<long> *f)
{
	lvldb::seq_t s;

	while (true) {
		f->acquire_slot(s);
		f->release_slot(s);

		flockfile(stdout);
		std::cout << "t3 s = " << s << std::endl;
		funlockfile(stdout);
	}
}

void fun4(lvldb::fence_t<long> *f)
{
	lvldb::seq_t s;

	while (true) {
		f->acquire_slot(s);
		f->release_slot(s);

		flockfile(stdout);
		std::cout << "t4 s = " << s << std::endl;
		funlockfile(stdout);
	}
}

int main(int argc, char *argv[])
{
	lvldb::disruptor_t<long> d(8);
	lvldb::fence_t<long>     f1(d, lvldb::fence_t<long>::producer), f2(d, lvldb::fence_t<long>::consumer);
	lvldb::fence_t<long>     f3(d, lvldb::fence_t<long>::consumer);

	f3.set_next_fence(&f2);
	f2.set_next_fence(&f1);
	f1.set_next_fence(&f3);

	std::thread t4(&fun4, &f3);
	std::thread t3(&fun3, &f2);
	std::thread t2(&fun2, &f2);
	std::thread t1(&fun1, &f1);

	t1.join();
	t2.join();
	t3.join();
	t4.join();

	return 0;
}
