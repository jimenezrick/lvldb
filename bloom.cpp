#include <iostream>
#include <set>

#include <memory>
#include <algorithm>
#include <cstdint>
#include <cmath>

#include "murmurhash/MurmurHash3.h"

namespace lvldb
{

class bloom_filter_t
{
	public:

	bloom_filter_t(size_t num_keys, double error_rate)
	{
		auto filter = calculate_filter(num_keys, error_rate);

		num_hashes_  = filter.first;
		num_buckets_ = filter.second;

		indexes_ = std::unique_ptr<size_t[]>(new size_t[num_hashes_]);
		buckets_ = std::unique_ptr<uint8_t[]>(new uint8_t[num_buckets_ / 8]);
	}

	void insert(const void *key, size_t len)
	{
		generate_indexes(key, len);
		for (size_t i = 0; i < num_hashes_; ++i)
			set_bit(indexes_[i]);
	}

	bool member(const void *key, size_t len)
	{
		generate_indexes(key, len);
		for (size_t i = 0; i < num_hashes_; ++i) {
			if (get_bit(indexes_[i]) == 0)
				return false;
		}

		return true;
	}

	void clear()
	{
		std::fill_n(buckets_.get(), num_buckets_ / 8, 0);
	}

	private:

	size_t                     num_hashes_, num_buckets_;
	std::unique_ptr<size_t[]>  indexes_;
	std::unique_ptr<uint8_t[]> buckets_;
	const uint32_t             seed_ = M_E * 1000000000;

	// Ref.: Bloom filter, Wikipedia
	//       http://en.wikipedia.org/wiki/Bloom_filter#Probability_of_false_positives
	static std::pair<size_t, size_t> calculate_filter(size_t num_keys, double error_rate)
	{
		double log2       = log(2);
		size_t total_bits = -ceil(num_keys * log(error_rate) / (log2 * log2));

		// Round to 8 for the byte array's size
		total_bits = ceil(total_bits / 8.0) * 8;

		size_t optimal_num_hashes = ceil(log2 * total_bits / num_keys);

		return {optimal_num_hashes, total_bits};
	}

	// Ref.: Adam Kirsch and Michael Mitzenmacher
	//       Less Hashing, Same Performance: Building a Better Bloom Filter
	void generate_indexes(const void *key, size_t len)
	{
		uint64_t hash[2];

		MurmurHash3_x64_128(key, len, seed_, hash);
		for (size_t i = 0; i < num_hashes_; ++i)
			indexes_[i] = (hash[0] + i * hash[1]) % num_buckets_;
	}

	inline void set_bit(size_t n)
	{
		buckets_[n / 8] |= (1 << n % 8);
	}

	inline int get_bit(size_t n)
	{
		return buckets_[n / 8] & (1 << n % 8);
	}
};

}

int main(int argc, char *argv[])
{
	argv[0][0] = argc;
	return 0;
}
