#ifndef BLOOM_HPP
#define BLOOM_HPP

#include <memory>
#include <cstdint>
#include <cmath>

namespace lvldb
{

class bloom_filter_t
{
	public:

	bloom_filter_t(size_t num_keys, double error_rate);
	void insert(const void *key, size_t len);
	bool member(const void *key, size_t len) const;
	void clear();
	size_t count() const;
#ifndef NDEBUG
	void print_debug_info() const;
#endif

	private:

	size_t                     num_hashes_, num_buckets_;
	std::unique_ptr<size_t[]>  indexes_;
	std::unique_ptr<uint8_t[]> buckets_;
	const uint32_t             seed_ = M_E * 1000000000;

	static std::pair<size_t, size_t> calculate_filter(size_t num_keys, double error_rate);
	void generate_indexes(const void *key, size_t len) const;
	void set_bit(size_t n);
	int get_bit(size_t n) const;
};

}

#endif
