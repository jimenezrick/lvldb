#include <iostream>
#include <set>
#include <cassert>

#include "bloom.hpp"

#define TEST_SIZE       100000
#define TEST_ERROR_RATE 0.01

int main(int argc, char *argv[])
{
	lvldb::bloom_filter_t bloom(TEST_SIZE, TEST_ERROR_RATE);
	std::set<int>         set;
	int                   false_positives = 0;
	double                false_positives_rate;

	bloom.insert("foo", 3);
	bloom.insert("bar", 3);
	bloom.insert("fur", 3);

	assert(bloom.member("foo", 3));
	assert(bloom.member("bar", 3));
	assert(bloom.member("fur", 3));
	assert(bloom.member("x", 1) == false);

	bloom.print_debug_info();
	std::cout << std::endl;
	bloom.clear();

	for (int i = 0; i < TEST_SIZE; ++i) {
		if (bloom.member(&i, sizeof(i)))
			++false_positives;

		set.insert(i);
		bloom.insert(&i, sizeof(i));
		assert(bloom.member(&i, sizeof(i)));
	}

	false_positives_rate = (double) false_positives / (double) TEST_SIZE;
	assert(fabs(false_positives_rate - TEST_ERROR_RATE) < TEST_ERROR_RATE);

	std::cout << "test_size            = " << TEST_SIZE            << "\n";
	std::cout << "test_error_rate      = " << TEST_ERROR_RATE      << "\n";
	std::cout << "false_positives      = " << false_positives      << "\n";
	std::cout << "false_positives_rate = " << false_positives_rate << "\n";
	std::cout << std::endl;

	set.clear();
	bloom.clear();
	false_positives = 0;

	for (int i = 0; i < TEST_SIZE; ++i) {
		if (i % 3 != 0 && i % 7 != 0) {
			set.insert(i);
			bloom.insert(&i, sizeof(i));
			assert(bloom.member(&i, sizeof(i)));
		}
	}

	for (int i = 0; i < TEST_SIZE; ++i) {
		if (set.find(i) != set.end())
			assert(bloom.member(&i, sizeof(i)));
		else if (bloom.member(&i, sizeof(i)))
			++false_positives;
	}

	false_positives_rate = (double) false_positives / (double) TEST_SIZE;
	assert(fabs(false_positives_rate - TEST_ERROR_RATE) < TEST_ERROR_RATE);

	std::cout << "test_size            = " << TEST_SIZE            << "\n";
	std::cout << "test_error_rate      = " << TEST_ERROR_RATE      << "\n";
	std::cout << "false_positives      = " << false_positives      << "\n";
	std::cout << "false_positives_rate = " << false_positives_rate << "\n";

	return 0;
}
