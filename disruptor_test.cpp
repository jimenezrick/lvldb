#include <iostream>

#include "disruptor.hpp"

template<typename Fence>
class task1_t: public lvldb::task_t<Fence>
{
	public:

	task1_t(Fence &fence):
		lvldb::task_t<Fence>(fence),
		initialized_(false)
	{ }

	void process_slot(typename Fence::slot_t &slot)
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

template<typename Fence>
class task2_t: public lvldb::task_t<Fence>
{
	public:

	task2_t(Fence &fence):
		lvldb::task_t<Fence>(fence)
	{ }

	void process_slot(typename Fence::slot_t &slot)
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

template<typename Fence>
class task3_t: public lvldb::task_t<Fence>
{
	public:

	task3_t(Fence &fence):
		lvldb::task_t<Fence>(fence)
	{ }

	void process_slot(typename Fence::slot_t &slot)
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

template<typename Fence>
class task4_t: public lvldb::task_t<Fence>
{
	public:

	task4_t(Fence &fence):
		lvldb::task_t<Fence>(fence)
	{ }

	void process_slot(typename Fence::slot_t &slot)
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

int main(int argc, char *argv[])
{
	typedef lvldb::disruptor_t<long>           disruptor_t;
	typedef lvldb::atomic_fence_t<disruptor_t> fence_t;

	typedef task1_t<fence_t> task1_t;
	typedef task2_t<fence_t> task2_t;
	typedef task3_t<fence_t> task3_t;
	typedef task4_t<fence_t> task4_t;

	disruptor_t d(8);
	fence_t     f1(d, fence_t::producer);
	fence_t     f2(d, fence_t::consumer);
	fence_t     f3(d, fence_t::consumer);

	task1_t t1(f1);
	task2_t t2(f2);
	task3_t t3(f2);
	task4_t t4(f3);

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
