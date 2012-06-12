#if 0
#define NDEBUG
#endif

#include <memory>
#include <atomic>
#include <cassert>

#include <unistd.h>






#include <iostream>
#include <stdio.h>






typedef unsigned long slot_t;
typedef unsigned long seq_t;
typedef std::atomic<seq_t> atomic_seq_t;

// Slot of type T:
// template<typename T>
class disruptor_t
{
	public:
	std::unique_ptr<long[]>  ring_;

	const size_t size_ = 1024;

	disruptor_t()
	{
		ring_ = std::unique_ptr<long[]>(new long[size_]);
	}






	// Ref: Fast algorithm to find the modulo a power of two
	inline size_t get_index(size_t n)
	{
		return n & (size_ - 1);
	}
};



class task_t
{
};




class fence_t
{
	public:

	enum fence_type_t {producer, consumer};

	fence_t(fence_type_t fence_type):
		seq_(0),
		next_(0),
		next_fence_(nullptr),
		fence_type_(fence_type)
	{
		assert(sizeof(fence_t) >= sysconf(_SC_LEVEL1_DCACHE_LINESIZE));
	}

	void set_next_fence(const fence_t *next_fence)
	{
		next_fence_ = next_fence;
	}

	inline void acquire_slot(seq_t &task_seq)
	{
		if (fence_type_ == producer && seq_ == 0 && next_ == 0)
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
	atomic_seq_t   seq_, next_;
	const fence_t *next_fence_;
	fence_type_t   fence_type_;

	inline void pause_thread()
	{
		// XXX: Pausa con espera exponencial tras varias iteraciones
		__asm__ __volatile__("pause");
	}

	inline void check_consistency()
	{
		assert(next_fence_ != nullptr);
		assert(seq_ <= next_);

		if (seq_ != 0)
			assert(seq_ != next_fence_->seq_);
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
		//sleep(1);
		f->release_slot(s);

		flockfile(stdout);
		std::cout << "t2 s = " << s << std::endl;
		funlockfile(stdout);
	}
}

int main()
{
	fence_t f1(fence_t::producer), f2(fence_t::consumer);

	f1.set_next_fence(&f2);
	f2.set_next_fence(&f1);

	std::thread t1(&fun1, &f1);
	std::thread t2(&fun2, &f2);

	t1.join();
	t2.join();

	return 0;
}
