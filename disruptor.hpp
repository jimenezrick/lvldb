#include <vector>
#include <memory>
#include <atomic>
#include <limits>
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

const seq_t max_seq = std::numeric_limits<seq_t>::max();

void check_cache_overlap(size_t type_size)
{
	long line_size;

	if ((line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE)) == -1) {
		perror("sysconf");
		exit(EXIT_FAILURE);
	}

	assert(type_size >= static_cast<size_t>(line_size));
}

template<typename Slot>
class disruptor_t
{
	public:

	typedef Slot slot_t;

	disruptor_t(size_t size):
		size_(size),
		ring_(std::unique_ptr<slot_t[]>(new slot_t[size]()))
	{
		// Test if size is a power of two
		assert((size & (size - 1)) == 0);
	}

	inline size_t get_index(seq_t seq)
	{
		// Fast algorithm to find the modulo of a power of two
		return seq & (size_ - 1);
	}

	inline slot_t operator[](seq_t seq) const
	{
		return ring_[get_index(seq)];
	}

	inline slot_t &operator[](seq_t seq)
	{
		return ring_[get_index(seq)];
	}

	private:

	const size_t              size_;
	std::unique_ptr<slot_t[]> ring_;
};

template<typename Disr>
class fence_t
{
	public:

	typedef typename Disr::slot_t slot_t;
	enum type_t {producer, consumer};

	fence_t(Disr &disruptor, type_t type):
		disruptor_(disruptor),
		type_(type)
	{ }

	void set_next_fence(const fence_t *next_fence)
	{
		next_fence_ = next_fence;
	}

	virtual slot_t &acquire_slot(atomic_seq_t &task_seq) = 0;
	virtual void release_slot(atomic_seq_t &task_seq) = 0;

	protected:

	Disr          &disruptor_;
	const type_t   type_;
	const fence_t *next_fence_;
};

template<typename Fence>
class task_t;

template<typename Disr>
class atomic_fence_t: public fence_t<Disr>
{
	public:

	typedef typename Disr::slot_t slot_t;

	atomic_fence_t(Disr &disruptor, typename fence_t<Disr>::type_t type,
		       int atomic_retries = 32, int atomic_sleep = 25):
		fence_t<Disr>(disruptor, type),
		next_(0),
		atomic_retries_(atomic_retries),
		atomic_sleep_(atomic_sleep)
	{
		check_cache_overlap(sizeof(atomic_fence_t));
	}

	void add_task(const task_t<atomic_fence_t<Disr>> &task)
	{
		seqs_.push_back(task.seq());
	}

	slot_t &acquire_slot(atomic_seq_t &task_seq)
	{
		check_consistency();

		while (true) {
			int   pauses = 0;
			seq_t next   = next_;

			if (this->type_ == fence_t<Disr>::producer) {
				while (true) {
					seq_t min_seq = next_fence()->min_seq();

					if (this->disruptor_.get_index(next) ==
					    this->disruptor_.get_index(min_seq)) {
						if (task_seq != max_seq)
							task_seq.store(next_.load());

						pause_thread(pauses);
					} else
						break;
				}
			} else {
				while (true) {
					seq_t min_seq = next_fence()->min_seq();

					if (next == min_seq || min_seq != max_seq) {
						if (task_seq != max_seq)
							task_seq.store(next_.load());

						pause_thread(pauses);
					} else
						break;
				}
			}

			if (!next_.compare_exchange_strong(next, next + 1))
				continue;

			task_seq = next;
			break;
		}

		check_consistency();

		return this->disruptor_[task_seq];
	}

	inline void release_slot(atomic_seq_t &task_seq)
	{
		seq_t next   = next_;
		int   pauses = 0;

		check_consistency();

		while (this->disruptor_.get_index(next) ==
		       this->disruptor_.get_index(next_fence()->min_seq()))
			pause_thread(pauses);

		task_seq = next;

		check_consistency();
	}

	private:

	char                              padding_[64];
	atomic_seq_t                      next_;
	const int                         atomic_retries_;
	const int                         atomic_sleep_; // Milliseconds
	std::vector<const atomic_seq_t *> seqs_;

	inline const atomic_fence_t *next_fence()
	{
		const atomic_fence_t *fence =
			static_cast<const atomic_fence_t *>(this->next_fence_);

		assert(dynamic_cast<const atomic_fence_t *>(this->next_fence_) != nullptr);

		return fence;
	}

	inline void pause_thread(int &pauses)
	{
		if (pauses < atomic_retries_)
			__asm__ __volatile__("pause" ::: "memory");
		else {
			struct timespec req = {0, atomic_sleep_ * 1000 * 1000};

			pthread_testcancel();

			if (nanosleep(&req, nullptr) == -1) {
				perror("nanosleep");
				exit(EXIT_FAILURE);
			}
		}

		pauses++;
	}

	seq_t min_seq() const
	{
		seq_t min = max_seq;

		assert(seqs_.size() > 0);

		for (const atomic_seq_t *pseq : seqs_) {
			seq_t seq = std::atomic_load(pseq);

			min = seq < min ? seq : min;
		}

		return min;
	}

	inline void check_consistency()
	{
		assert(this->type_ == fence_t<Disr>::producer ?
		       next_fence()->type_ == fence_t<Disr>::consumer : true);
		assert(this->next_fence_ != nullptr);
#ifndef NDEBUG
		for (const atomic_seq_t *pseq : seqs_)
			assert(std::atomic_load(pseq) <= next_ ||
			       std::atomic_load(pseq) == max_seq);
#endif
	}
};

// XXX XXX XXX
//template<typename Disr>
//class mutex_fence_t: public fence_t<Disr>
//{
//        private:

//        char            padding_[64];
//        seq_t           seq_;
//        pthread_mutex_t seq_mutex_ = PTHREAD_MUTEX_INITIALIZER;
//        pthread_cond_t  seq_cond_  = PTHREAD_COND_INITIALIZER;
//};
// XXX XXX XXX

template<typename Fence>
class task_t
{
	public:

	typedef typename Fence::slot_t slot_t;

	task_t(Fence &fence, int test_cancel_iters = 256):
		seq_(max_seq),
		fence_(fence),
		test_cancel_iters_(test_cancel_iters)
	{
		check_cache_overlap(sizeof(task_t));
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
		if (pthread_cancel(thread_) != 0) {
			perror("pthread_cancel");
			exit(EXIT_FAILURE);
		}

		if (pthread_join(thread_, nullptr) != 0) {
			perror("pthread_join");
			exit(EXIT_FAILURE);
		}
	}

	const atomic_seq_t *seq() const
	{
		return &seq_;
	}

	protected:

	seq_t seq()
	{
		return seq_;
	}

	private:

	char         padding_[64];
	atomic_seq_t seq_;
	Fence       &fence_;
	pthread_t    thread_;
	const int    test_cancel_iters_;

	static void *start_thread(void *arg)
	{
		static_cast<task_t *>(arg)->run();

		return nullptr;
	}

	void run()
	{
		while (true) {
			for (int i = 0; i < test_cancel_iters_; i++) {
				slot_t &slot = fence_.acquire_slot(seq_);

				process_slot(slot);
				fence_.release_slot(seq_);
			}

			pthread_testcancel();
		}
	}

	virtual void process_slot(slot_t &slot) = 0;
};

}
