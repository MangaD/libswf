/**
 * libswf - Utility functions
 */

#include "dynamic_bitset.hpp"

#include <algorithm> // std::rotate, std::min, std::copy

namespace swf_utils {

	dynamic_bitset::dynamic_bitset(const char * bits_s) : bits() {
		strToBits(bits_s, bits);
	}

	dynamic_bitset::dynamic_bitset(const char * bits_s, size_t length) : bits() {
		bits.reserve(length);
		strToBits(bits_s, bits);
	}

	void strToBits(const char * bits_s, std::vector<bool> &bits) {
		for (int i = static_cast<int>(strlen(bits_s)-1); i >= 0; --i) {
			if (bits_s[i] == '1') {
				bits.emplace_back(true);
			} else if (bits_s[i] == '0') {
				bits.emplace_back(false);
			} else {
				throw std::invalid_argument("String must contain only 0's and 1's.");
			}
		}
	}

	unsigned long dynamic_bitset::to_ulong() const {
		unsigned long bin = 0;
		for (size_t i = bits.size() - 1; static_cast<long int>(i) >= 0; --i) {
			if (bin * 2 < bin) {
				throw std::overflow_error("dynamic_bitset::to_ulong: bitset size is too big to be represented by the return type.");
			}
			bin *= 2;
			bin = bin + bits[i];
		}
		return bin;
	}

	dynamic_bitset& dynamic_bitset::operator&=(const dynamic_bitset& rhs) {
		for (size_t i = 0, end = std::min(bits.size(),rhs.size()); i < end; ++i) {
			bits[i] = bits[i] & rhs[i];
		}
		return *this;
	}

	dynamic_bitset& dynamic_bitset::operator|=(const dynamic_bitset& rhs) {
		for (size_t i = 0, end = std::min(bits.size(),rhs.size()); i < end; ++i) {
			bits[i] = bits[i] | rhs[i];
		}
		return *this;
	}

	dynamic_bitset& dynamic_bitset::operator^=(const dynamic_bitset& rhs) {
		for (size_t i = 0, end = std::min(bits.size(),rhs.size()); i < end; ++i) {
			bits[i] = bits[i] ^ rhs[i];
		}
		return *this;
	}


	dynamic_bitset dynamic_bitset::operator~() const {
		dynamic_bitset ds{*this};
		for (size_t i = 0; i < bits.size(); ++i) {
			ds[i] = !ds[i];
		}
		return ds;
	}

	dynamic_bitset& dynamic_bitset::operator<<=(size_t n) {

		std::vector<bool> tmp(bits.size(), 0);
		std::copy ( bits.begin(), bits.begin() + (bits.size()-n), tmp.begin() + n );
		bits = tmp;
	/*
		// also, it is worth noting that std::rotate technically requires
		// forward iterators, while std::vector<bool> technically doesn't provide that
		std::rotate(bits.begin(),bits.begin()+(bits.size()-n),bits.end());
		for (size_t i = 0; i < n; ++i) {
			bits[i] = 0;
		}
	*/
		return *this;
	}

	dynamic_bitset& dynamic_bitset::operator>>=(size_t n) {

		std::vector<bool> tmp(bits.size(), 0);
		std::copy ( bits.begin() + n, bits.begin() + (bits.size()-n), tmp.begin() );
		bits = tmp;
	/*
		// also, it is worth noting that std::rotate technically requires
		// forward iterators, while std::vector<bool> technically doesn't provide that
		std::rotate(bits.begin(),bits.begin()+n,bits.end());
		for (size_t i = bits.size() - 1; i > bits.size() - 1 - n && static_cast<long int>(i) >= 0; --i) {
			bits[i] = 0;
		}
	*/
		return *this;
	}

}
