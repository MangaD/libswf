/**
 * libswf - Utility functions
 */

#ifndef SWF_UTILS_HPP
#define SWF_UTILS_HPP

#ifdef __GNUC__
	#define QUOTE(s) #s
	#define DIAGNOSTIC_PUSH() _Pragma("GCC diagnostic push")
	#define DIAGNOSTIC_IGNORE(warning) _Pragma(QUOTE(GCC diagnostic ignored warning))
	#define DIAGNOSTIC_POP() _Pragma("GCC diagnostic pop")
#else
	#define DIAGNOSTIC_PUSH()
	#define DIAGNOSTIC_IGNORE(warning)
	#define DIAGNOSTIC_POP()
#endif

#include <array>      // array
#include <vector>     // vector
#include <cstdint>    // uint8_t
#include <bitset>     // bitset

#include "dynamic_bitset.hpp"

#ifdef __cpp_lib_bit_cast
	#include <bit> // c++20, bit_cast
#else
	// https://en.cppreference.com/w/cpp/numeric/bit_cast
	namespace std {
		template <class To, class From>
		typename std::enable_if<
			(sizeof(To) == sizeof(From)) &&
			std::is_trivially_copyable<From>::value &&
			std::is_trivial<To>::value,
			// this implementation requires that To is trivially default constructible
			To>::type
		// constexpr support needs compiler magic
		bit_cast(const From &src) noexcept {
			To dst;
			std::memcpy(&dst, &src, sizeof(To));
			return dst;
		}
	}
#endif


/**
 * Unit conversions
 */
inline std::size_t bytesToMiB (std::size_t bytes) { return bytes/1024/1024; }
inline std::size_t bytesToKiB (std::size_t bytes) { return bytes/1024; }
inline int twipsToPx (int twips) { return twips/20; }

/**
 * Executable files
 */
bool isPEfile (const std::vector<uint8_t> &exe);
bool isELFfile (const std::vector<uint8_t> &exe);

/**
 * Image files
 * https://en.wikipedia.org/wiki/Portable_Network_Graphics
 */
bool isPNGfile (const std::vector<uint8_t> &png);
bool isJPEGfile (const std::vector<uint8_t> &jpeg);
bool isGIFfile (const std::vector<uint8_t> &gif);


/**
 * Byte operations
 */
template<std::size_t decimal>
void bytesToBitset(std::bitset<decimal> &bs, const std::array<uint8_t, decimal/8> &bytes) {
	for (std::size_t i = 0; i < bytes.size(); ++i) {
		bs <<= 8;
		std::bitset<decimal> bsTmp = bytes[i];
		bs |= bsTmp;
	}
}
template<std::size_t decimal>
void bytesToBitset(std::bitset<decimal> &bs, const std::vector<uint8_t> &bytes) {
	for (std::size_t i = 0; i < bytes.size(); ++i) {
		bs <<= 8;
		std::bitset<decimal> bsTmp = bytes[i];
		bs |= bsTmp;
	}
}
void bytesToBitset(swf_utils::dynamic_bitset &bs, const std::vector<uint8_t> &bytes);

template<std::size_t bigBs, std::size_t smallBs>
void subBitset(const std::bitset<bigBs> &bs, std::bitset<smallBs> &sub, int start_pos) {
	for (int i = static_cast<int>(bigBs) - 1 - static_cast<int>(start_pos),
	     j = static_cast<int>(smallBs) - 1;
	     i >= static_cast<int>(bigBs) - static_cast<int>(smallBs) - start_pos && j >= 0;
	     i--, j--)
	{
		sub[j] = bs[i];
	}
}
void subBitset(const swf_utils::dynamic_bitset &bs, swf_utils::dynamic_bitset &sub, int start_pos);

/**
 * Bytes to decimal in little-endian
 * https://stackoverflow.com/questions/12240299/convert-bytes-to-int-uint-in-c
 * https://stackoverflow.com/questions/11644362/are-the-results-of-bitwise-operations-on-signed-integers-defined
 */
template<class T>
T bytestodec_le(const std::uint8_t* bytes) {
	T d = 0;
	for (std::size_t i = 0; i < sizeof(d); ++i) {
		d = static_cast<T>(d|(bytes[i] << (i * 8)));
	}
	return d;
}

/// Bytes to decimal in big-endian
template<class T>
T bytestodec_be(const std::uint8_t* bytes) {
	T d = 0;
	for (std::size_t i = 0; i < sizeof(d); ++i) {
		d = static_cast<T>(d|(bytes[i] << ((sizeof(d)-i-1) * 8)));
	}
	return d;
}

template<class T>
std::array<uint8_t, sizeof(T)> dectobytes_le(T d) {

	std::array<uint8_t, sizeof(T)> bytes;

	for (std::size_t i = 0; i < sizeof(d); ++i) {
		uint8_t tmp = 0;
		tmp |= static_cast<uint8_t>(d);
		bytes[i] = tmp;
		d = static_cast<T>(d >> 8);
	}

	return bytes;
}

template<class T>
std::array<uint8_t, sizeof(T)> dectobytes_be(T d) {

	std::array<uint8_t, sizeof(T)> bytes;

	for (int i = sizeof(d)-1; i >= 0; i--) {
		uint8_t tmp = 0;
		tmp |= static_cast<uint8_t>(d);
		bytes[i] = tmp;
		d = static_cast<T>(d >> 8);
	}

	return bytes;
}

/**
 * Inserts second vector at the end of the first.
 */
template<class T, class container>
void concatVectorWithContainer(std::vector<T> &first, const container &second, bool end=true) {
	first.reserve(first.size() + second.size());
	auto pos = end ? first.end() : first.begin();
	first.insert(pos, second.begin(), second.end());
}

#endif // UTILS_HPP
