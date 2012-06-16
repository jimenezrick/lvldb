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
		lvldb::seq_t seq0 = this->seq();

		if (slot != 0)
			initialized_ = true;

		if (initialized_)
			assert(slot % 4 == 3);
		else
			assert(slot == 0);

		slot = seq0 * 4;

#ifndef NO_OUTPUT
		std::cout << "task 1 seq = " << seq0;
		std::cout << " slot = "      << slot << std::endl;
#endif
	}

	void stop()
	{
		lvldb::task_t<Fence>::stop();
		std::cerr << "finished task 1 seq = " << this->seq() << std::endl;
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
		lvldb::seq_t seq0 = this->seq();

		if (seq0 > 0)
			assert(slot > 0);

		assert(static_cast<size_t>(slot) == seq0 * 4);

		slot = seq0 * 4 + 1;

#ifndef NO_OUTPUT
		std::cout << "task 2 seq = " << seq0;
		std::cout << " slot = "      << slot << std::endl;
#endif
	}

	void stop()
	{
		lvldb::task_t<Fence>::stop();
		std::cerr << "finished task 2 seq = " << this->seq() << std::endl;
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
		lvldb::seq_t seq0 = this->seq();

		if (seq0 > 0)
			assert(slot > 0);

		assert(static_cast<size_t>(slot) == seq0 * 4);

		slot = seq0 * 4 + 2;

#ifndef NO_OUTPUT
		std::cout << "task 3 seq = " << seq0;
		std::cout << " slot = "      << slot << std::endl;
#endif
	}

	void stop()
	{
		lvldb::task_t<Fence>::stop();
		std::cerr << "finished task 3 seq = " << this->seq() << std::endl;
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
		lvldb::seq_t seq0 = this->seq();

		if (seq0 > 0)
			assert(slot > 0);

		assert(static_cast<size_t>(slot) == seq0 * 4 + 1 ||
		       static_cast<size_t>(slot) == seq0 * 4 + 2);

		slot = seq0 * 4 + 3;

#ifndef NO_OUTPUT
		std::cout << "task 4 seq = " << seq0;
		std::cout << " slot = "      << slot << std::endl;
#endif
	}

	void stop()
	{
		lvldb::task_t<Fence>::stop();
		std::cerr << "finished task 4 seq = " << this->seq() << std::endl;
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

	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <disruptor size>\n";

		return EXIT_FAILURE;
	}

	disruptor_t d(std::stoi(argv[1]));
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

	f1.add_task(t1);
	f2.add_task(t2);
	f2.add_task(t3);
	f3.add_task(t4);

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
