/**
 * C++ Wrapper to xz library
 *
 * This library was meant to compress and decompress in LZMA, but for some reason
 * the Adobe Flash Player doesn't seem to like either the .xz and .lzma formats,
 * so I switched to using the LZMA SDK.
 *
 * API: https://github.com/bminor/xz/blob/master/src/liblzma/api/lzma/container.h
 * Example: https://github.com/Arkq/swfpack/blob/master/src/swfpack.c
 */

#ifndef XZLZMA_HPP
#define XZLZMA_HPP

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

#include <vector> // vector
#include <cstdint> // uint8_t
#include <string>
#include <exception> // exception

std::vector<uint8_t> xz_lzma_compress(const std::vector<uint8_t> &in_data, const uint32_t preset = 6, const bool xz = true);
std::vector<uint8_t> xz_lzma_decompress(const std::vector<uint8_t> &in_data);

class xz_lzma_exception : public std::exception {
	public:
		explicit xz_lzma_exception(const std::string &message = "xz_lzma_exception")
			: std::exception(), error_message(message) {}
		const char *what() const noexcept
		{
			return error_message.c_str();
		}
	private:
		std::string error_message;
};

void lerr(int ret, const std::string &func);

#endif
