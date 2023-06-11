/**
 * C++ Wrapper to LZMA SDK library
 *
 * Tutorials:
 * - https://github.com/fancycode/pylzma/blob/master/src/pylzma/pylzma_compress.c
 * - http://www.asawicki.info/news_1368_lzma_sdk_-_how_to_use.html
 */

#include <algorithm> // min
#include <cstring> // memcpy
#include <lzma/C/LzmaEnc.h> // LZMA encode functions
#include <lzma/C/LzmaDec.h> // LZMA decode functions
#include <lzma/C/Lzma2Dec.h> // LZMA2 decode functions
#include "lzmasdk_wrapper.hpp"

using namespace std;

namespace lzmasdk {

	typedef struct {
		ISeqInStream SeqInStream;
		const std::vector<unsigned char> *Buf;
		size_t BufPos;
	} VectorInStream;

	SRes VectorInStream_Read(const ISeqInStream *p, void *buf, size_t *size) {
		VectorInStream *ctx = (VectorInStream*)p;
		*size = min(*size, ctx->Buf->size() - ctx->BufPos);
		if (*size) memcpy(buf, &(*ctx->Buf)[ctx->BufPos], *size);
		ctx->BufPos += *size;
		return SZ_OK;
	}

	typedef struct {
		ISeqOutStream SeqOutStream;
		std::vector<unsigned char> *Buf;
	} VectorOutStream;

	size_t VectorOutStream_Write(const ISeqOutStream *p, const void *buf, size_t size) {
		VectorOutStream *ctx = (VectorOutStream*)p;
		if (size) {
			size_t oldSize = ctx->Buf->size();
			ctx->Buf->resize(oldSize + size);
			memcpy(&(*ctx->Buf)[oldSize], buf, size);
		}
		return size;
	}

	static void *Alloc(ISzAllocPtr p, size_t size) { (void)p; return malloc(size); }
	static void Free(ISzAllocPtr p, void *address) { (void)p; free(address); }

	vector<uint8_t> lzmasdk_compress(const vector<uint8_t> &in_data)
	{
		vector<uint8_t> out_data;

		static ISzAlloc allocator = { Alloc, Free };

		CLzmaEncProps props;
		CLzmaEncHandle encoder = nullptr;

		Byte header[LZMA_PROPS_SIZE];
		size_t headerSize = LZMA_PROPS_SIZE;
		int res;

		int dictionary = 23;         // [0,27], default 23 (8MB)
		int fastBytes = 128;         // [5,273], default 128
		int literalContextBits = 3;  // [0,8], default 3
		int literalPosBits = 0;      // [0,4], default 0
		int posBits = 2;             // [0,4], default 2
		int eos = 1;                 // write "end of stream" marker?
		int multithreading = 1;      // use multithreading if available?
		int algorithm = 2;

		encoder = LzmaEnc_Create(&allocator);
		if (encoder == nullptr) {
			throw lzmasdk_exception("lzma sdk: no memory.");
		}

		LzmaEncProps_Init(&props);

		props.dictSize = 1 << dictionary;
		props.lc = literalContextBits;
		props.lp = literalPosBits;
		props.pb = posBits;
		props.algo = algorithm;
		props.fb = fastBytes;
		// props.btMode = 1;
		// props.numHashBytes = 4;
		// props.mc = 32;
		props.writeEndMark = eos ? 1 : 0;
		props.numThreads = multithreading ? 2 : 1;
		LzmaEncProps_Normalize(&props);
		res = LzmaEnc_SetProps(encoder, &props);
		if (res != SZ_OK) {
			if (encoder != nullptr) {
				LzmaEnc_Destroy(encoder, &allocator, &allocator);
			}
			throw lzmasdk_exception("lzma sdk: Could not set encoder properties: " + to_string(res));
		}

		LzmaEnc_WriteProperties(encoder, header, &headerSize);

		out_data.insert(out_data.end(), header, header + headerSize);

		VectorInStream inStream = { {&VectorInStream_Read}, &in_data, 0 };
		VectorOutStream outStream = { {&VectorOutStream_Write}, &out_data };

		res = LzmaEnc_Encode(encoder, (ISeqOutStream*)&outStream, (ISeqInStream*)&inStream, nullptr, &allocator, &allocator);

		if (res != SZ_OK) {
			if (encoder != nullptr) {
				LzmaEnc_Destroy(encoder, &allocator, &allocator);
			}
			throw lzmasdk_exception("lzma sdk: Error during compressing: " + to_string(res));
		}

		if (encoder != nullptr) {
			LzmaEnc_Destroy(encoder, &allocator, &allocator);
		}

		return out_data;
	}


	vector<uint8_t> lzmasdk_decompress(const vector<uint8_t> &in_data, const int lzma2)
	{
		vector<uint8_t> out_data;

		static ISzAlloc allocator = { Alloc, Free };

		const int BLOCK_SIZE (128*1024);

		size_t avail;
		union {
			CLzmaDec lzma;
			CLzma2Dec lzma2;
		} state;
		ELzmaStatus status;
		size_t srcLen, destLen;
		int res;
		int propertiesLength;

		propertiesLength = lzma2 ? 1 : LZMA_PROPS_SIZE;

		vector<Byte> tmp (BLOCK_SIZE);

		if (lzma2) {
			Lzma2Dec_Construct(&state.lzma2);
			res = Lzma2Dec_Allocate(&state.lzma2, in_data[0], &allocator);
			if (res != SZ_OK) {
				Lzma2Dec_Free(&state.lzma2, &allocator);
				throw lzmasdk_exception("lzma sdk: Incorrect stream properties: " + to_string(res));
			}
		} else {
			LzmaDec_Construct(&state.lzma);
			res = LzmaDec_Allocate(&state.lzma, in_data.data(), propertiesLength, &allocator);
			if (res != SZ_OK) {
				LzmaDec_Free(&state.lzma, &allocator);
				throw lzmasdk_exception("lzma sdk: Incorrect stream properties: " + to_string(res));
			}
		}

		const uint8_t *data = in_data.data() + propertiesLength;
		avail = in_data.size() - propertiesLength;

		if (lzma2) {
			Lzma2Dec_Init(&state.lzma2);
		} else {
			LzmaDec_Init(&state.lzma);
		}
		while (true) {
			srcLen = avail;
			destLen = BLOCK_SIZE;

			if (lzma2) {
				res = Lzma2Dec_DecodeToBuf(&state.lzma2, tmp.data(), &destLen, data, &srcLen, LZMA_FINISH_ANY, &status);
			} else {
				res = LzmaDec_DecodeToBuf(&state.lzma, tmp.data(), &destLen, data, &srcLen, LZMA_FINISH_ANY, &status);
			}
			data += srcLen;
			avail -= srcLen;
			if (res == SZ_OK && destLen > 0) {
				out_data.insert(out_data.end(), tmp.begin(), tmp.begin() + destLen);
			}
			if (res != SZ_OK || status == LZMA_STATUS_FINISHED_WITH_MARK || status == LZMA_STATUS_NEEDS_MORE_INPUT) {
				break;
			}
		}


		if (status == LZMA_STATUS_NEEDS_MORE_INPUT) {
			throw lzmasdk_exception("lzma sdk: Data error during decompression.");
		} else if (res != SZ_OK) {
			throw lzmasdk_exception("lzma sdk: Error while decompressing: " + to_string(res));
		}

		if (lzma2) {
			Lzma2Dec_Free(&state.lzma2, &allocator);
		} else {
			LzmaDec_Free(&state.lzma, &allocator);
		}

		return out_data;
	}

} // lzmasdk
