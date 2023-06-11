/**
 * libswf - Utility functions
 */

#include <iostream>  // std::ostream
#include <vector>    // std::vector
#include <stdexcept> // std::out_of_range, std::invalid_argument
#include <cstring>   // strlen

#ifndef SWF_DYNAMIC_BITSET_HPP
#define SWF_DYNAMIC_BITSET_HPP

/**
 * References:
 *    http://www.cplusplus.com/reference/bitset/
 *    https://www.boost.org/doc/libs/1_36_0/libs/dynamic_bitset/dynamic_bitset.html
 *    https://boostorg.github.io/compute/boost/compute/dynamic_bitset.html
 */

namespace swf_utils {

	class dynamic_bitset {

	public:
		inline explicit dynamic_bitset(size_t size, bool fill=0) : bits(size, fill) {}
		explicit dynamic_bitset(const char * bits_s);
		// More efficient
		dynamic_bitset(const char * bits_s, size_t length);

		template<class T> dynamic_bitset(size_t size, T value)
				: bits() {
			bits.resize(size, 0);
			T value_bak = value;
			for(int i = 0; value > 0; ++i) {
				if (i >= static_cast<int>(bits.size())) {
					throw std::out_of_range("Value '" + std::to_string(value_bak) +
					    "' doesn't fit in " + std::to_string(size) + " bits.");
				}
				bits[i] = value % 2;
				value = value / 2;
			}
		}

		inline std::vector<bool>::reference operator[](size_t pos) { return bits[pos]; }
		inline bool operator[](size_t pos) const { return bits[pos]; }

		unsigned long to_ulong() const;

		inline size_t size() const { return bits.size(); }

		dynamic_bitset& operator&=(const dynamic_bitset& rhs);
		dynamic_bitset& operator|=(const dynamic_bitset& rhs);
		dynamic_bitset& operator^=(const dynamic_bitset& rhs);
		dynamic_bitset& operator<<=(size_t n);
		dynamic_bitset& operator>>=(size_t n);

		dynamic_bitset operator~() const;

	private:
		std::vector<bool> bits;

		inline friend std::ostream & operator << (std::ostream &out, const dynamic_bitset &b) {
			for (auto i = b.bits.end()-1; i != b.bits.begin()-1; --i) {
				out << (*i ? "1" : "0");
			}
			return out;
		}

		inline friend bool operator==(const dynamic_bitset& lhs, const dynamic_bitset& rhs) {
			return lhs.bits == rhs.bits;
		}

		inline friend bool operator!=(const dynamic_bitset& lhs, const dynamic_bitset& rhs) {
			return lhs.bits != rhs.bits;
		}

	};

	inline namespace literals {
		inline dynamic_bitset operator"" _bits (const char * bits_s, size_t length) {
			return dynamic_bitset(bits_s, length);
		}
	}

	void strToBits(const char * bits_s, std::vector<bool> &bits);

}

#endif
