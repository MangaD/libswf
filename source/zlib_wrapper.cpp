/**
 * C++ Wrapper to zlib library
 */

// Define before including zlib.h
#define ZLIB_CONST

#include "zlib_wrapper.hpp"
#include <zlib.h>

using namespace std;

namespace zlib {

	struct zlib_helper_deflate {
		z_stream stream;
		zlib_helper_deflate() : stream() {}
		~zlib_helper_deflate() {
			(void)deflateEnd(&stream);
		}
	};

	struct zlib_helper_inflate {
		z_stream stream;
		zlib_helper_inflate() : stream() {}
		~zlib_helper_inflate() {
			(void)inflateEnd(&stream);
		}
	};

	void zerr(int ret, const string &func) {
	    switch (ret) {
	    case Z_ERRNO:
	    	throw zlib_exception("zlib: " + func + " I/O Error.");
	        break;
	    case Z_STREAM_ERROR:
	    	throw zlib_exception("zlib: " + func + " Invalid compression level.");
	        break;
	    case Z_DATA_ERROR:
	    	throw zlib_exception("zlib: " + func + " Invalid or incomplete deflate data.");
	        break;
	    case Z_MEM_ERROR:
	    	throw zlib_exception("zlib: " + func + " Out of memory.");
	        break;
	    case Z_VERSION_ERROR:
	    	throw zlib_exception("zlib: " + func + " zlib version mismatch.");
	    	break;
		default:
			throw zlib_exception("zlib: " + func + " Unknown error, possibly a bug. Error code: " + to_string(ret));
			break;
		}
	}

	/**
	 * zlib C tutorial: http://zlib.net/zlib_how.html
	 * StackOverflow C++ tutorial: https://stackoverflow.com/questions/4538586/how-to-compress-a-buffer-with-zlib
	 */
	vector<uint8_t> zlib_compress(const vector<uint8_t> &in_data, const int level)
	{
		vector<uint8_t> out_data;

		const size_t BUFSIZE = 128 * 1024;
		uint8_t temp_buffer[BUFSIZE];

		zlib_helper_deflate helper;
		z_stream *strm = &helper.stream;
		strm->zalloc = nullptr;
		strm->zfree = nullptr;
		strm->opaque = nullptr;
		strm->next_in = in_data.data(); // Can be const if #define ZLIB_CONST
		strm->avail_in = static_cast<uInt>(in_data.size());
		strm->next_out = temp_buffer;
		strm->avail_out = BUFSIZE;

		int ret = deflateInit(strm, level);
		if (ret != Z_OK) {
			throw zlib_exception("zlib: deflateInit() failed with code: " + to_string(ret));
		}

		while (strm->avail_in != 0) {
			ret = deflate(strm, Z_NO_FLUSH);
			if (ret != Z_OK && ret != Z_STREAM_END) {
				zerr(ret, "deflate()");
			}
			if (strm->avail_out == 0) {
				out_data.insert(out_data.end(), temp_buffer, temp_buffer + BUFSIZE);
				strm->next_out = temp_buffer;
				strm->avail_out = BUFSIZE;
			}
		}

		ret = Z_OK;
		while (ret == Z_OK) {
			if (strm->avail_out == 0) {
				out_data.insert(out_data.end(), temp_buffer, temp_buffer + BUFSIZE);
				strm->next_out = temp_buffer;
				strm->avail_out = BUFSIZE;
			}
			ret = deflate(strm, Z_FINISH);
		}

		if (ret != Z_STREAM_END){
			throw zlib_exception("zlib: Stream is not complete.");
		}

		out_data.insert(out_data.end(), temp_buffer, temp_buffer + BUFSIZE - strm->avail_out);

		return out_data;
	}


	/**
	 * zlib C tutorial: http://zlib.net/zlib_how.html
	 */
	vector<uint8_t> zlib_decompress(const vector<uint8_t> &in_data)
	{
		vector<uint8_t> out_data;

		const size_t BUFSIZE = 128 * 1024;
		uint8_t temp_buffer[BUFSIZE];

		zlib_helper_inflate helper;
		z_stream *strm = &helper.stream;
		strm->zalloc = nullptr;
		strm->zfree = nullptr;
		strm->opaque = nullptr;
		strm->next_in = in_data.data(); // Can be const if #define ZLIB_CONST
		strm->avail_in = static_cast<uInt>(in_data.size());
		strm->next_out = temp_buffer;
		strm->avail_out = BUFSIZE;

		int ret = inflateInit(strm);
		if (ret != Z_OK) {
			throw zlib_exception("zlib: inflateInit() failed with code: " + to_string(ret));
		}

		while (strm->avail_in != 0) {
			ret = inflate(strm, Z_NO_FLUSH);
			if (ret != Z_OK && ret != Z_STREAM_END) {
				zerr(ret, "inflate()");
			}
			if (strm->avail_out == 0) {
				out_data.insert(out_data.end(), temp_buffer, temp_buffer + BUFSIZE);
				strm->next_out = temp_buffer;
				strm->avail_out = BUFSIZE;
			}
		}

		ret = Z_OK;
		while (ret == Z_OK) {
			if (strm->avail_out == 0) {
				out_data.insert(out_data.end(), temp_buffer, temp_buffer + BUFSIZE);
				strm->next_out = temp_buffer;
				strm->avail_out = BUFSIZE;
			}
			ret = inflate(strm, Z_FINISH);
		}

		if (ret != Z_STREAM_END){
			throw zlib_exception("zlib: Stream is not complete.");
		}

		out_data.insert(out_data.end(), temp_buffer, temp_buffer + BUFSIZE - strm->avail_out);

		return out_data;
	}

} // zlib
