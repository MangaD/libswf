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

#include <lzma.h>
#include "xz_lzma_wrapper.hpp"

using namespace std;

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

struct lzma_helper {
	lzma_stream stream;
	DIAGNOSTIC_PUSH()
	DIAGNOSTIC_IGNORE("-Wzero-as-null-pointer-constant")
	lzma_helper() : stream( LZMA_STREAM_INIT ) {}
	DIAGNOSTIC_POP()
	~lzma_helper() {
		lzma_end( &stream );
	}
};

/**
 * https://hackage.haskell.org/package/lzma-clib-5.2.2/src/src/liblzma/api/lzma/base.h
 */
void lerr(int ret, const string &func) {
	switch (ret) {
	case LZMA_MEM_ERROR:
		throw xz_lzma_exception("lzma: " + func + " Memory allocation failed.");
		break;
	case LZMA_OPTIONS_ERROR:
		throw xz_lzma_exception("lzma: " + func + " Specified preset is not supported.");
		break;
	case LZMA_UNSUPPORTED_CHECK:
		throw xz_lzma_exception("lzma: " + func + " Specified integrity check is not supported.");
		break;
	case LZMA_FORMAT_ERROR:
		throw xz_lzma_exception("lzma: " + func + " Format error.");
		break;
	case LZMA_DATA_ERROR:
		throw xz_lzma_exception("lzma: " + func + " Decoders return this error if the input data is corrupt." +
		 " This can mean, for example, invalid CRC32 in headers " +
		 " or invalid check of uncompressed data.");
		break;
	default:
		throw xz_lzma_exception("lzma: " + func + " Unknown error, possibly a bug. Error code: " + to_string(ret));
		break;
	}
}

/**
 * lzma C++ tutorial 1: http://ptspts.blogspot.com/2011/11/how-to-simply-compress-c-string-with.html
 * lzma C++ tutorial 2: https://github.com/elsamuko/xz_test/blob/master/src/xz/encode.cpp
 * lzma C tutorial: https://lists.samba.org/archive/linux/2013-November/032622.html
 *
 * Relevant tutorial: https://github.com/alperakcan/smashfs/blob/master/src/mkfs/compressor-lzma.c
 */
vector<uint8_t> xz_lzma_compress(const vector<uint8_t> &in_data, const uint32_t preset, const bool xz)
{
	vector<uint8_t> out_data;

	lzma_helper helper;
	lzma_stream* strm = &helper.stream;

	lzma_options_lzma opt;
	lzma_lzma_preset(&opt, preset);

	lzma_ret ret;

	if (!xz) {
		// .lzma format
		ret = lzma_alone_encoder(strm, &opt);
		if (ret != LZMA_OK) {
			lerr(ret, "lzma_alone_encoder()");
		}
	} else {
		// .xz format
		ret = lzma_easy_encoder(strm, preset, LZMA_CHECK_CRC64);
		if (ret != LZMA_OK) {
			lerr(ret, "lzma_easy_encoder()");
		}
	}

	uint8_t temp_buffer[BUFSIZ];

	strm->next_in = in_data.data();
	strm->avail_in = in_data.size();
	strm->next_out = temp_buffer;
	strm->avail_out = BUFSIZ;

	while (strm->avail_in != 0) {
		ret = lzma_code(strm, LZMA_RUN);
		if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
			lerr(ret, "lzma_code()");
		}

		if(strm->avail_out == 0) {
			out_data.insert(out_data.end(), temp_buffer, temp_buffer + BUFSIZ);
            strm->next_out = temp_buffer;
            strm->avail_out = BUFSIZ;
        }

	}

	ret = LZMA_OK;
	while (ret == LZMA_OK) {
		if (strm->avail_out == 0) {
			out_data.insert(out_data.end(), temp_buffer, temp_buffer + BUFSIZ);
			strm->next_out = temp_buffer;
			strm->avail_out = BUFSIZ;
		}
		ret = lzma_code(strm, LZMA_FINISH);
	}

	if (ret != LZMA_STREAM_END){
		throw xz_lzma_exception("lzma: Stream is not complete.");
	}

	out_data.insert(out_data.end(), temp_buffer, temp_buffer + BUFSIZ - strm->avail_out);

	return out_data;
}


/**
 * lzma C++ tutorial 1: http://ptspts.blogspot.com/2011/11/how-to-simply-compress-c-string-with.html
 * lzma C++ tutorial 2: https://github.com/elsamuko/xz_test/blob/master/src/xz/decode.cpp
 *
 * Relevant tutorial: https://github.com/alperakcan/smashfs/blob/master/src/mkfs/compressor-lzma.c
 */
vector<uint8_t> xz_lzma_decompress(const vector<uint8_t> &in_data)
{
		vector<uint8_t> out_data;

		lzma_helper helper;
		lzma_stream* strm = &helper.stream;
		//lzma_ret ret = lzma_alone_decoder(strm, UINT64_MAX); // For .lzma format
		lzma_ret ret = lzma_auto_decoder(strm, UINT64_MAX, LZMA_CONCATENATED);

		if (ret != LZMA_OK) {
			lerr(ret, "lzma_auto_decoder()");
		}

		uint8_t temp_buffer[BUFSIZ];

		strm->next_in = in_data.data();
		strm->avail_in = in_data.size();
		strm->next_out = temp_buffer;
		strm->avail_out = BUFSIZ;

		while (strm->avail_in != 0) {
			ret = lzma_code(strm, LZMA_RUN);
			if (ret != LZMA_OK && ret != LZMA_STREAM_END && ret != LZMA_NO_CHECK) {
				lerr(ret, "lzma_code()");
			}

			if(strm->avail_out == 0) {
				out_data.insert(out_data.end(), temp_buffer, temp_buffer + BUFSIZ);
	            strm->next_out = temp_buffer;
	            strm->avail_out = BUFSIZ;
	        }

		}

		ret = LZMA_OK;
		while (ret == LZMA_OK) {
			if (strm->avail_out == 0) {
				out_data.insert(out_data.end(), temp_buffer, temp_buffer + BUFSIZ);
				strm->next_out = temp_buffer;
				strm->avail_out = BUFSIZ;
			}
			ret = lzma_code(strm, LZMA_FINISH);
		}

		if (ret != LZMA_STREAM_END){
			throw xz_lzma_exception("lzma: Stream is not complete.");
		}

		out_data.insert(out_data.end(), temp_buffer, temp_buffer + BUFSIZ - strm->avail_out);

		return out_data;
}
