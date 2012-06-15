#include <iostream>

#include "disruptor.hpp"

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
