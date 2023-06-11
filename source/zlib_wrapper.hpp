/**
 * C++ Wrapper to zlib library
 */


#ifndef ZLIB_WRAPPER_HPP
#define ZLIB_WRAPPER_HPP

#include <vector> // vector
#include <cstdint> // uint8_t
#include <string>
#include <exception> // exception
#include <zlib.h> // Z_BEST_COMPRESSION

namespace zlib {

	std::vector<uint8_t> zlib_compress(const uint8_t* in_data, const size_t in_data_size, const int level);
	inline std::vector<uint8_t> zlib_compress(const std::vector<uint8_t> &in_data, const int level) {
		return zlib_compress(in_data.data(), in_data.size(), level);
	}
	std::vector<uint8_t> zlib_decompress(const std::vector<uint8_t> &in_data);

	class zlib_exception : public std::exception {
		public:
			explicit zlib_exception(const std::string &message = "zlib_exception")
				: std::exception(), error_message(message) {}
			const char *what() const noexcept
			{
				return error_message.c_str();
			}
		private:
			std::string error_message;
	};

	void zerr(int ret, const std::string &func);

} // zlib

#endif // ZLIB_WRAPPER_HPP
