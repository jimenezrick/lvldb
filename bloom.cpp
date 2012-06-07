#include <iostream>
#include <algorithm>

#include "bloom.hpp"
#include "murmurhash/MurmurHash3.h"

using namespace lvldb;

bloom_filter_t::bloom_filter_t(size_t num_keys, double error_rate)
{
	auto filter = calculate_filter(num_keys, error_rate);

	num_hashes_  = filter.first;
	num_buckets_ = filter.second;

	indexes_ = std::unique_ptr<size_t[]>(new size_t[num_hashes_]);
	buckets_ = std::unique_ptr<uint8_t[]>(new uint8_t[num_buckets_ / 8]);
}

void bloom_filter_t::insert(const void *key, size_t len)
{
	generate_indexes(key, len);
	for (size_t i = 0; i < num_hashes_; ++i)
		set_bit(indexes_[i]);
}

bool bloom_filter_t::member(const void *key, size_t len) const
{
	generate_indexes(key, len);
	for (size_t i = 0; i < num_hashes_; ++i) {
		if (get_bit(indexes_[i]) == 0)
			return false;
	}

	return true;
}

void bloom_filter_t::clear()
{
	std::fill_n(buckets_.get(), num_buckets_ / 8, 0);
}

// See: Brian Kernighan's population count
size_t bloom_filter_t::count() const
{
	size_t count = 0;

	for (size_t i = 0; i < num_buckets_ / 8; ++i) {
		for (uint8_t byte = buckets_[i]; byte; ++count)
			byte &= byte - 1;
	}

	return count;
}

#ifndef NDEBUG
void bloom_filter_t::print_debug_info() const
{
	std::cout << "=== bloom_filter_t ===\n";
	std::cout << "num_hashes_  = " << num_hashes_  << "\n";
	std::cout << "num_buckets_ = " << num_buckets_ << "\n";
	std::cout << "seed_        = " << seed_        << "\n";
	std::cout << "count()      = " << count()      << "\n";
}
#endif

// See: Bloom filter, Wikipedia
//      http://en.wikipedia.org/wiki/Bloom_filter#Probability_of_false_positives
std::pair<size_t, size_t> bloom_filter_t::calculate_filter(size_t num_keys, double error_rate)
{
	double log2       = log(2);
	size_t total_bits = -ceil(num_keys * log(error_rate) / (log2 * log2));

	// Round to 8 for the byte array's size
	total_bits = ceil(total_bits / 8.0) * 8;

	size_t optimal_num_hashes = ceil(log2 * total_bits / num_keys);

	return {optimal_num_hashes, total_bits};
}

// See: Adam Kirsch and Michael Mitzenmacher
//      Less Hashing, Same Performance: Building a Better Bloom Filter
void bloom_filter_t::generate_indexes(const void *key, size_t len) const
{
	uint64_t hash[2];

	MurmurHash3_x64_128(key, len, seed_, hash);
	for (size_t i = 0; i < num_hashes_; ++i)
		indexes_[i] = (hash[0] + i * hash[1]) % num_buckets_;
}

inline void bloom_filter_t::set_bit(size_t n)
{
	buckets_[n / 8] |= (1 << n % 8);
}

inline int bloom_filter_t::get_bit(size_t n) const
{
	return buckets_[n / 8] & (1 << n % 8);
}
