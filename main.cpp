#include <iostream>
#include <memory>
#include <algorithm>

#include <cstdint>
#include <cmath>

#include "murmurhash/MurmurHash3.h"

namespace lvldb
{

class bloom_filter_t
{
	bloom_filter_t():
		// XXX
		num_hashes_(7),
		num_buckets_(ceil(100 / 8.0) * 8),
		indexes_(new uint8_t[num_hashes_]),
		buckets_(new uint8_t[num_buckets_ / 8])
		// XXX
	{ }

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

	size_t num_hashes_, num_buckets_;
	std::unique_ptr<uint8_t[]> indexes_, buckets_;
	const uint32_t seed_ = M_E * 1000000000;

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
