/**
 * C++ Wrapper to LZMA SDK library
 *
 * Tutorials:
 * - https://github.com/fancycode/pylzma/blob/master/src/pylzma/pylzma_compress.c
 * - http://www.asawicki.info/news_1368_lzma_sdk_-_how_to_use.html
 */

#ifndef LZMASDK_HPP
#define LZMASDK_HPP

#include <vector> // vector
#include <cstdint> // uint8_t
#include <string>
#include <exception> // exception
#include <lzma/C/LzmaEnc.h> // LZMA encode functions
#include <lzma/C/LzmaDec.h> // LZMA decode functions
#include <lzma/C/Lzma2Dec.h> // LZMA2 decode functions

namespace lzmasdk {

	std::vector<uint8_t> lzmasdk_compress(const std::vector<uint8_t> &in_data);
	std::vector<uint8_t> lzmasdk_decompress(const std::vector<uint8_t> &in_data, const int lzma2 = 0);

	class lzmasdk_exception : public std::exception {
		public:
			explicit lzmasdk_exception(const std::string &message = "lzmasdk_exception")
				: std::exception(), error_message(message) {}
			const char *what() const noexcept
			{
				return error_message.c_str();
			}
		private:
			std::string error_message;
	};

	SRes VectorInStream_Read(const ISeqInStream *p, void *buf, size_t *size);
	size_t VectorOutStream_Write(const ISeqOutStream *p, const void *buf, size_t size);

} // lzmasdk

#endif // LZMASDK_HPP
