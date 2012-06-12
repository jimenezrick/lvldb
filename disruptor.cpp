#if 0
#define NDEBUG
#endif

#include <memory>
#include <atomic>
#include <cassert>

#include <unistd.h>






#include <iostream>
#include <stdio.h>






typedef unsigned long      seq_t;
typedef std::atomic<seq_t> atomic_seq_t;

template<typename T>
class disruptor_t
{
	public:

	typedef T slot_t;

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
};

class fence_t
{
	public:

	enum type_t {producer, consumer};

	fence_t(type_t type):
		type_(type),
		seq_(0),
		next_(0),
		next_fence_(nullptr)
	{
		assert(sizeof(fence_t) >= sysconf(_SC_LEVEL1_DCACHE_LINESIZE));
	}

	void set_next_fence(const fence_t *next_fence)
	{
		next_fence_ = next_fence;
	}

	inline void acquire_slot(seq_t &task_seq)
	{
		if (type_ == producer && seq_ == 0 && next_ == 0)
			task_seq = next_++;
		else {
			check_consistency();

			// XXX: Copia local de las variables atomicas
			// XXX: Aritmetica modular del anillo
			while (next_ == next_fence_->seq_)
				pause_thread();
			task_seq = next_++;

			check_consistency();
		}
	}

	inline void release_slot(seq_t task_seq)
	{
		seq_t expected = task_seq;

		check_consistency();

		// XXX: Copia local de las variables atomicas
		// XXX: Aritmetica modular del anillo
		while (seq_ + 1 == next_fence_->seq_)
			pause_thread();

		while (!seq_.compare_exchange_strong(expected, task_seq + 1)) {
			expected = task_seq;
			pause_thread();
		}

		check_consistency();
	}

	private:

	char           padding_[64];
	type_t         type_;
	atomic_seq_t   seq_, next_;
	const fence_t *next_fence_;

	inline void pause_thread()
	{
		// XXX: Pausa con espera exponencial tras varias iteraciones
		__asm__ __volatile__("pause");
	}

	inline void check_consistency()
	{
		assert(seq_ <= next_);
		assert(next_fence_ != nullptr);

		if (seq_ != 0)
			assert(seq_ != next_fence_->seq_);

		if (type_ == producer)
			assert(next_fence_->type_ == consumer);
	}
};






#include <thread>

void fun1(fence_t *f)
{
	seq_t s;

	while (true) {
		f->acquire_slot(s);
		sleep(1);
		f->release_slot(s);

		flockfile(stdout);
		std::cout << "t1 s = " << s << std::endl;
		funlockfile(stdout);
	}
}

void fun2(fence_t *f)
{
	seq_t s;

	while (true) {
		f->acquire_slot(s);
		f->release_slot(s);

		flockfile(stdout);
		std::cout << "t2 s = " << s << std::endl;
		funlockfile(stdout);
	}
}

void fun3(fence_t *f)
{
	seq_t s;

	while (true) {
		f->acquire_slot(s);
		f->release_slot(s);

		flockfile(stdout);
		std::cout << "t3 s = " << s << std::endl;
		funlockfile(stdout);
	}
}

int main()
{
	disruptor_t<char> d(1024);
	fence_t f1(fence_t::producer), f2(fence_t::consumer), f3(fence_t::consumer);

	f3.set_next_fence(&f2);
	f2.set_next_fence(&f1);
	f1.set_next_fence(&f3);

	std::thread t3(&fun3, &f3);
	std::thread t2(&fun2, &f2);
	std::thread t1(&fun1, &f1);

	t1.join();
	t2.join();
	t3.join();

	return 0;
}
