/**
 * libswf - SWF class
 */

#include "swf.hpp"

#include <algorithm> // search, iter_swap
#include <bitset>    // bitset
#include <cmath>     // ceil
#include <map>       // map
//#include "xz_lzma_wrapper.hpp"
#include <lodepng/lodepng.h> // export/import png
#include "zlib_wrapper.hpp"
#include "lzmasdk_wrapper.hpp"
#include "minimp3_ex.hpp" // get_mp3_info
#include "swf_utils.hpp"
#include "dynamic_bitset.hpp"

using namespace std;
using namespace swf;

map<int, string> SWF::tagTypeNames = {
    {-1, "File Header"}, {0, "End"}, {1, "ShowFrame"}, {2, "DefineShape"}, {3, "FreeCharacter"}, {4, "PlaceObject"},
    {5, "RemoveObject"}, {6, "DefineBitsJPEG"}, {7, "DefineButton"}, {8, "JPEGTables"}, {9, "SetBackgroundColor"},
    {10, "DefineFont"}, {11, "DefineText"}, {12, "DoAction"}, {13, "DefineFontInfo"}, {14, "DefineSound"}, {15, "StartSound"},
    {16, "StopSound"}, {17, "DefineButtonSound"}, {18, "SoundStreamHead"}, {19, "SoundStreamBlock"}, {20, "DefineBitsLossless"},
    {21, "DefineBitsJPEG2"}, {22, "DefineShape2"}, {23, "DefineButtonCxform"}, {24, "Protect"}, {25, "PathsArePostscript"},
    {26, "PlaceObject2"}, {28, "RemoveObject2"}, {29, "SyncFrame"}, {31, "FreeAll"}, {32, "DefineShape3"}, {33, "DefineText2"},
    {34, "DefineButton2"}, {35, "DefineBitsJPEG3"}, {36, "DefineBitsLossless2"}, {37, "DefineEditText"}, {38, "DefineVideo"},
    {39, "DefineSprite"}, {40, "NameCharacter"}, {41, "ProductInfo"}, {42, "DefineTextFormat"}, {43, "FrameLabel"},
    {45, "SoundStreamHead2"}, {46, "DefineMorphShape"}, {47, "GenerateFrame"}, {48, "DefineFont2"}, {49, "GeneratorCommand"},
    {50, "DefineCommandObject"}, {51, "CharacterSet"}, {52, "ExternalFont"}, {56, "Export"}, {57, "Import"},
    {58, "EnableDebugger"}, {59, "DoInitAction"}, {60, "DefineVideoStream"}, {61, "VideoFrame"}, {62, "DefineFontInfo2"},
    {63, "DebugID"}, {64, "EnableDebugger2"}, {65, "ScriptLimits"}, {66, "SetTabIndex"}, {69, "FileAttributes"},
    {70, "PlaceObject3"}, {71, "Import2"}, {72, "DoABCDefine"}, {73, "DefineFontAlignZones"}, {74, "CSMTextSettings"},
    {75, "DefineFont3"}, {76, "SymbolClass"}, {77, "Metadata"}, {78, "DefineScalingGrid"}, {82, "DoABC"}, {83, "DefineShape4"},
    {84, "DefineMorphShape2"}, {86, "DefineSceneAndFrameData"}, {87, "DefineBinaryData"}, {88, "DefineFontName"},
    {89, "StartSound2"}, {90, "DefineBitsJPEG4"}, {91, "DefineFont4"}, {93, "EnableTelemetry"}, {94, "PlaceObject4"}
    };

SWF::SWF(const vector<uint8_t> &buffer) : tags(), version(),
			frameSize(), frameRate(), frameCount(), projector() {
	this->parseSwf(buffer);
}


/**
 * Export SWF as EXE. The binary file is as follows:
 * Windows:
 * 1. Projector binary
 * 2. SWF binary
 * 3. Footer 0xFA123456 (little endian)
 * 4. SWF binary length
 * Linux:
 * 1. Projector binary
 * 2. SWF binary length
 * 3. Footer 0xFA123456 (little endian)
 * 4. SWF binary
 */
vector<uint8_t> SWF::exportExe(const vector<uint8_t> &proj, CompressionChoice compression) {

	vector<uint8_t> bytes = this->toBytes();

	if (compression == CompressionChoice::zlib) {
		bytes = zlibCompress(bytes);
	} else if (compression == CompressionChoice::lzma) {
		bytes = lzmaCompress(bytes);
	} else if (compression != CompressionChoice::uncompressed) {
		throw swf_exception("Invalid compression option.");
	}

	SWF_DEBUG("SWF binary size: " << bytes.size() << " bytes (" << bytesToMiB(bytes.size()) << " MiB).");

	// Compressed length to save alongside footer
	// so that we can calculate later the start position of the swf file
	array<uint8_t, 4> length = dectobytes_le<uint32_t>(static_cast<uint32_t>(bytes.size()));

	if (!proj.empty()) {
		this->projector.buffer = proj;
		if (isPEfile(this->projector.buffer)) {
			this->projector.windows = true;
		} else if (isELFfile(this->projector.buffer)) {
			this->projector.windows = false;
		} else {
			throw swf_exception("Invalid projector file.");
		}
	} else if (!this->hasProjector()) {
		throw swf_exception("No projector file given.");
	}

	if (this->projector.windows) {
		// 1st comes projector
		concatVectorWithContainer(bytes, this->projector.buffer, false);
		// 2nd comes swf (aka original 'bytes')
		// 3rd comes projector footer
		concatVectorWithContainer(bytes, this->projector.footer);
		// 4th comes swf length
		concatVectorWithContainer(bytes, length);
	} else {
		// 1st comes projector
		// 2nd comes swf length
		// 3rd comes projector footer
		// 4th comes swf (aka original 'bytes')
		concatVectorWithContainer(bytes, this->projector.footer, false);
		concatVectorWithContainer(bytes, length, false);
		concatVectorWithContainer(bytes, this->projector.buffer, false);
	}

	return bytes;
}

/**
 * Export SWF
 */
vector<uint8_t> SWF::exportSwf(CompressionChoice compression) {

	vector<uint8_t> bytes = this->toBytes();

	if (compression == CompressionChoice::zlib) {
		bytes = zlibCompress(bytes);
	} else if (compression == CompressionChoice::lzma) {
		bytes = lzmaCompress(bytes);
	} else if (compression != CompressionChoice::uncompressed) {
		throw swf_exception("Invalid compression option.");
	}

	SWF_DEBUG("SWF binary size: " << bytes.size() << " bytes (" << bytesToMiB(bytes.size()) << " MiB).");

	return bytes;
}

void SWF::parseSwf(const vector<uint8_t> &buffer) {

	vector<uint8_t> swfBuf;

	swfBuf = exe2swf(buffer);

	if (swfBuf.size() > 4) {
		SWF_DEBUG("Read " << swfBuf.size() << " bytes (" << bytesToMiB(swfBuf.size()) << " MiB).");
	} else {
		throw swf_exception("Invalid SWF file. File too small.");
	}

	size_t cur = parseSwfHeader(swfBuf);

	//find all tags
	size_t tagStart = cur;

	for (int id = 1; tagStart < swfBuf.size(); ++id) {

		auto t = make_unique<Tag>();
		auto len = t->parseTagHeader(swfBuf.data(), cur);
		t->i = id;

		if (tagName(t->type) == "DefineBinaryData") {
			auto dbd = make_unique<Tag_DefineBinaryData>();
			dbd->i = t->i;
			dbd->type = t->type;
			dbd->longTag = t->longTag;
			dbd->id = bytestodec_le<uint16_t>(swfBuf.data() + cur);
			cur += 2;
			dbd->reserved = bytestodec_le<uint32_t>(swfBuf.data() + cur);
			cur += 4;
			len -= 6;
			// Copy tag data
			for (size_t i = 0; i < len; ++i) {
				dbd->data.emplace_back(swfBuf[cur++]);
			}
			// Add tag to vector of tags
			tags.emplace_back(move(dbd));
		} else if (tagName(t->type) == "DefineSound") {
			auto ds = make_unique<Tag_DefineSound>();
			ds->i = t->i;
			ds->type = t->type;
			ds->longTag = t->longTag;
			ds->id = bytestodec_le<uint16_t>(swfBuf.data() + cur);
			cur += 2;
			bitset<8> soundInfo{};
			bytesToBitset(soundInfo, array<uint8_t, 1>{swfBuf[cur++]});
			subBitset(soundInfo, ds->soundFormat, 0);
			subBitset(soundInfo, ds->soundRate, 4);
			subBitset(soundInfo, ds->soundSize, 4 + 2);
			subBitset(soundInfo, ds->soundType, 4 + 2 + 1);
			ds->soundSampleCount = bytestodec_le<uint32_t>(swfBuf.data() + cur);
			cur += 4;
			len -= 7;
			// Copy tag data
			for (size_t i = 0; i < len; ++i) {
				ds->data.emplace_back(swfBuf[cur++]);
			}
			// Add tag to vector of tags
			tags.emplace_back(move(ds));
		} else if (tagName(t->type) == "DefineBitsLossless" || tagName(t->type) == "DefineBitsLossless2") {
			auto dbl = make_unique<Tag_DefineBitsLossless>();
			dbl->i = t->i;
			dbl->type = t->type;
			dbl->longTag = t->longTag;
			dbl->id = bytestodec_le<uint16_t>(swfBuf.data() + cur);
			cur += 2;
			if (tagName(t->type) == "DefineBitsLossless2") {
				dbl->version2 = true;
			}
			dbl->bitmapFormat = bytestodec_le<uint8_t>(swfBuf.data() + cur);
			++cur;
			dbl->bitmapWidth = bytestodec_le<uint16_t>(swfBuf.data() + cur);
			cur += 2;
			dbl->bitmapHeight = bytestodec_le<uint16_t>(swfBuf.data() + cur);
			cur += 2;
			if (dbl->bitmapFormat == 3) {
				dbl->bitmapColorTableSize = bytestodec_le<uint8_t>(swfBuf.data() + cur);
				++cur;
				len--;
			}
			len -= 7;

			for (size_t i = 0; i < len; ++i) {
				dbl->data.emplace_back(swfBuf[cur++]);
			}
			// Add tag to vector of tags
			tags.emplace_back(move(dbl));

		} else if (tagName(t->type) == "SymbolClass") {
			auto sc = make_unique<Tag_SymbolClass>();

			sc->i = t->i;
			sc->type = t->type;
			sc->longTag = t->longTag;

			sc->numSymbols = bytestodec_le<uint16_t>(swfBuf.data() + cur);
			cur += 2;
			for (int i = 0; i < sc->numSymbols; ++i) {
				uint16_t tid = bytestodec_le<uint16_t>(swfBuf.data() + cur);
				cur += 2;
				string name;
				while (true) {
					if (swfBuf[cur] == 0) {
						break;
					}
					name += static_cast<char>(swfBuf[cur]);
					++cur;
				}
				++cur;
				sc->symbolClass.emplace_back(pair<size_t, string>(tid, name));
				//SWF_DEBUG(tid << " - " << name);
			}
			// Add tag to vector of tags
			tags.emplace_back(move(sc));
		} else {
			// Copy tag data
			for (size_t i = 0; i < len; ++i) {
				t->data.emplace_back(swfBuf[cur++]);
			}
			// Add tag to vector of tags
			tags.emplace_back(move(t));
		}

		/*SWF_DEBUG("Tag type: " << t.type << " (" << tagName(t.type) << ")\nTag length: " <<
		      to_string(t.length) << " bytes (" << bytesToKiB(t.length) + " KiB).");*/

		tagStart = cur;
	}

	fillTagsSymbolName();
}

void SWF::fillTagsSymbolName() {
	/// Apparently there can be symbols with same ID (e.g. HF v0.3.0 has story04
	/// and story05 both with ID=216), although this is likely a bug, as there is
	/// no tag for story05 and tags can't have the same ID. But considering this,
	/// we fill the tags' symbolName as if there could be tags with same ID.
	std::map<size_t, size_t> duplicates;
	for (auto &t : this->tags) {
		auto names = this->getSymbolName(t->id);
		if (names.empty()) {
			continue;
		} else if (names.size() == 1) {
			t->symbolName = names[0];
		} else if (names.size() > 1) {
			if (duplicates.find(t->id) == duplicates.end()) {
				duplicates[t->id] = 0;
			} else {
				++duplicates[t->id];
			}
			t->symbolName = names[duplicates[t->id]];
		}
	}
}

size_t SWF::parseSwfHeader(vector<uint8_t> &buffer) {
	size_t cur = 0;

	//Check if file is SWF and what compression is used
	SWF_DEBUG_NNL("Compression: ");
	// bytes 0, 1, 2
	string signature{static_cast<char>(buffer[cur]), static_cast<char>(buffer[++cur]), static_cast<char>(buffer[++cur])};

	if (signature == "FWS") { // FWS is SWF in little-endian
		SWF_DEBUG("Uncompressed");
	} else if (signature == "CWS") {
		SWF_DEBUG("zlib");
		buffer = zlibDecompress(buffer);
	} else if (signature == "ZWS") {
		SWF_DEBUG("LZMA");
		buffer = lzmaDecompress(buffer);
	} else {
		throw swf_exception("Invalid SWF file. Unrecognized header.");
	}

	// Check version
	this->version = buffer[++cur]; // byte 3
	SWF_DEBUG("SWF version: " << to_string(this->version));

	// Check file length
	// bytes 4, 5, 6, 7
	uint32_t length = bytestodec_le<uint32_t>(buffer.data() + cur + 1);
	cur += 4;
	SWF_DEBUG("File length: " << length << " bytes (" << bytesToMiB(length) << " MiB).");

	if (buffer.size() != length) {
		throw swf_exception("Bytes read and SWF size don't match.");
	}

	SWF_DEBUG("Frame size:");
	// Nbits - number of bits used for each field of the frame size (there are 4 fields)
	bitset<5> nbits_bitset(buffer[++cur] >> 3); // byte 8
	int nbits = static_cast<int>(nbits_bitset.to_ulong());
	SWF_DEBUG("\tNbits: " << nbits);
	size_t frameSizeBytes = static_cast<int>(ceil(((static_cast<float>(nbits) * 4.0f) + 5.0f) / 8.0f));

	// If frame size is 9 bytes long, cur should be 17 after this
	for (size_t i = 0; i < frameSizeBytes; ++i) {
		this->frameSize.emplace_back(static_cast<char>(buffer[cur++]));
	}

#ifdef SWF_DEBUG_BUILD
	debugFrameSize(this->frameSize, nbits);
#endif

	this->frameRate = {buffer[cur], buffer[++cur]}; // bytes 17, 18

	// Looks like 1st byte isn't used, otherwise this should be little-endian
	SWF_DEBUG("Frame rate: " << to_string(this->frameRate[1]));

	this->frameCount = {buffer[++cur], buffer[++cur]}; // bytes 19, 20

	SWF_DEBUG("Frame count: " << bytestodec_le<uint16_t>(this->frameCount.data()));

	return ++cur;
}

void SWF::debugFrameSize(const vector<uint8_t> &fs, size_t nbits) {
	size_t frameSizeBits = nbits * 4 + 5;

	// frameSizeBits must be a multiple of 8
	// because when converting the bytes to a bitset,
	// the last bits and not the first, are garbage
	while (frameSizeBits % 8 != 0) {
		++frameSizeBits;
	}

	swf_utils::dynamic_bitset framesize_bitset(frameSizeBits);
	swf_utils::dynamic_bitset xMin_bitset(nbits);
	swf_utils::dynamic_bitset xMax_bitset(nbits);
	swf_utils::dynamic_bitset yMin_bitset(nbits);
	swf_utils::dynamic_bitset yMax_bitset(nbits);

	bytesToBitset(framesize_bitset, fs);

	subBitset(framesize_bitset, xMin_bitset, 5);
	subBitset(framesize_bitset, xMax_bitset, 5 + 16);
	subBitset(framesize_bitset, yMin_bitset, 5 + 16 * 2);
	subBitset(framesize_bitset, yMax_bitset, 5 + 16 * 3);

#ifdef SWF_DEBUG_BUILD
	int xMin = static_cast<int>(xMin_bitset.to_ulong());
	int xMax = static_cast<int>(xMax_bitset.to_ulong());
	int yMin = static_cast<int>(yMin_bitset.to_ulong());
	int yMax = static_cast<int>(yMax_bitset.to_ulong());

	SWF_DEBUG("\tXmin: " << xMin << " twips (" << twipsToPx(xMin) << " px)");
	SWF_DEBUG("\tXmax: " << xMax << " twips (" << twipsToPx(xMax) << " px)");
	SWF_DEBUG("\tYmin: " << yMin << " twips (" << twipsToPx(yMin) << " px)");
	SWF_DEBUG("\tYmax: " << yMax << " twips (" << twipsToPx(yMax) << " px)");
#endif
}

vector<Tag *> SWF::getTagsOfType(int type) {
	vector<Tag *> tmp;
	for (auto &t : this->tags) {
		if (t->type == type) {
			tmp.emplace_back(t.get());
		}
	}
	return tmp;
}

Tag * SWF::getTagWithId(size_t id) {
	for (auto &t : this->tags) {
		if (t->id == id) {
			return t.get();
		}
	}
	return nullptr;
}

vector< pair<size_t, string> > SWF::getAllSymbols() const {
	vector< pair<size_t, string> > symbols;
	for (auto const &t : this->tags) {
		if (t->type == tagId("SymbolClass")) {
			auto *sc = static_cast<Tag_SymbolClass *>(t.get());
			concatVectorWithContainer(symbols, sc->symbolClass);
		}
	}
	return symbols;
}

vector<string> SWF::getSymbolName(size_t id) const {
	vector<string> names;
	for (auto const &t : this->tags) {
		if (t->type == tagId("SymbolClass")) {
			auto *sc = static_cast<Tag_SymbolClass *>(t.get());
			auto it = find_if(sc->symbolClass.begin(), sc->symbolClass.end(),
				  [id](const pair<size_t, string> &p) -> bool { return p.first == id; });
			if (it != sc->symbolClass.end()) {
				names.emplace_back(it->second);
			}
		}
	}
	return names;
}

vector<uint8_t> SWF::toBytes() const {
	vector<uint8_t> buffer{'F', 'W', 'S', this->version};

	vector<uint8_t> buf;
	for (const auto &t : tags) {
		vector<uint8_t> bytes = t->toBytes();
		buf.insert(buf.end(), bytes.begin(), bytes.end());
	}

	uint32_t length = 12 + static_cast<uint32_t>(this->frameSize.size()) + static_cast<uint32_t>(buf.size());

	array<uint8_t, 4> len = dectobytes_le<uint32_t>(length);
	buffer.insert(buffer.end(), len.begin(), len.end());
	buffer.insert(buffer.end(), this->frameSize.begin(), this->frameSize.end());
	buffer.insert(buffer.end(), this->frameRate.begin(), this->frameRate.end());
	buffer.insert(buffer.end(), this->frameCount.begin(), this->frameCount.end());

	buffer.insert(buffer.end(), buf.begin(), buf.end());

	return buffer;
}

vector<uint8_t> SWF::zlibCompress(const vector<uint8_t> &swf) {

	vector<uint8_t> buffer{'C', 'W', 'S', (this->version >= 6 ? this->version : static_cast<uint8_t>(6))};
	buffer.reserve(buffer.size() + swf.size()); // more efficient
	buffer.insert(buffer.end(), swf.begin() + 4, swf.begin() + 8); // Length
	vector<uint8_t> compressed = zlib::zlib_compress(swf.data()+8, swf.size()-8, Z_BEST_COMPRESSION);
	buffer.insert(buffer.end(), compressed.begin(), compressed.end());
	return buffer;
}

vector<uint8_t> SWF::zlibDecompress(const vector<uint8_t> &swf) {

	vector<uint8_t> buffer(swf.begin(), swf.begin() + 8);
	buffer[0] = 'F';

	vector<uint8_t> tmp(swf.begin() + 8, swf.end());
	vector<uint8_t> decompressed = zlib::zlib_decompress(tmp);

	buffer.insert(buffer.end(), decompressed.begin(), decompressed.end());

	return buffer;
}

vector<uint8_t> SWF::lzmaCompress(const vector<uint8_t> &swf) {

	vector<uint8_t> buffer{'Z', 'W', 'S', (this->version >= 13 ? this->version : static_cast<uint8_t>(13))};
	buffer.reserve(buffer.size() + swf.size()); // more efficient
	buffer.insert(buffer.end(), swf.begin() + 4, swf.begin() + 8); // Length

	vector<uint8_t> tmp(swf.begin() + 8, swf.end());
	//vector<uint8_t> compressed = xz_lzma_compress(tmp); // Using XZ Utils
	vector<uint8_t> compressed = lzmasdk::lzmasdk_compress(tmp); // Using LZMA SDK

	// -5 because lzma properties are not included in the size
	array<uint8_t, 4> compressedSize = dectobytes_le<uint32_t>(static_cast<uint32_t>(compressed.size()) - 5);
	buffer.insert(buffer.end(), compressedSize.begin(), compressedSize.end());

	buffer.insert(buffer.end(), compressed.begin(), compressed.end());

	return buffer;
}

vector<uint8_t> SWF::lzmaDecompress(const vector<uint8_t> &swf) {

	vector<uint8_t> buffer(swf.begin(), swf.begin() + 8);
	buffer[0] = 'F';

	vector<uint8_t> tmp(swf.begin() + 12, swf.end());
	//vector<uint8_t> decompressed = xz_lzma_decompress(tmp); // Using XZ Utils
	vector<uint8_t> decompressed = lzmasdk::lzmasdk_decompress(tmp); // Using LZMA SDK

	buffer.insert(buffer.end(), decompressed.begin(), decompressed.end());

	return buffer;
}

vector<uint8_t> SWF::exe2swf(const vector<uint8_t> &exe) {

	vector<string> sigs{"FWS", "CWS", "ZWS"};
	size_t swfStart = 0;
	size_t swfEnd = 0;
	uint32_t swfLength = 0;

	if (isPEfile(exe)) {
		vector<uint8_t>::const_iterator it = exe.begin();
		while ((it = search(it + 4, exe.end(), this->projector.footer.begin(), this->projector.footer.end())) != exe.end()) {
			size_t pos = static_cast<size_t>(it - exe.begin());
			if (exe.size() < pos + 8)
				throw swf_exception("SWF not found inside EXE file.");
			swfLength = bytestodec_le<uint32_t>(exe.data() + pos + 4);
			if (exe.size() - pos == 8)
				break;
		}
		if (it == exe.end())
			throw swf_exception("SWF not found inside EXE file.");

		swfStart = exe.size() - swfLength - 8;
		swfEnd = swfStart + swfLength;

		string swfSig{static_cast<char>(exe[swfStart]),
			      static_cast<char>(exe[swfStart + 1]), static_cast<char>(exe[swfStart + 2])};

		if (find(sigs.begin(), sigs.end(), swfSig) == sigs.end()) {
			throw swf_exception("SWF not found inside EXE file.");
		}

		this->projector.buffer = {exe.begin(), exe.begin() + swfStart};
		this->projector.windows = true;
	} else if (isELFfile(exe)) {
		vector<uint8_t>::const_iterator it = exe.begin();
		size_t pos = 0;
		while ((it = search(it + 4, exe.end(), this->projector.footer.begin(), this->projector.footer.end())) != exe.end()) {
			pos = (it - exe.begin());

			if (exe.size() < pos + 12) {
				throw swf_exception("SWF not found inside ELF file.");
			}

			swfLength = bytestodec_le<uint32_t>(exe.data() + pos - 4);

			swfStart = pos + 4;
			swfEnd = swfStart + swfLength;

			string swfSig{static_cast<char>(exe[swfStart]),
				      static_cast<char>(exe[swfStart + 1]), static_cast<char>(exe[swfStart + 2])};

			if (find(sigs.begin(), sigs.end(), swfSig) != sigs.end())
				break;
		}
		if (it == exe.end() || exe.size() < swfEnd)
			throw swf_exception("SWF not found inside ELF file.");

		this->projector.buffer = {exe.begin(), exe.begin() + pos - 4};
		this->projector.windows = false;
	} else {
		return exe;
	}

	vector<uint8_t> swf{exe.begin() + swfStart, exe.begin() + swfEnd};

	return swf;
}

/**
 * Missing implementation of:
 * - DefineBits
 * - DefineBitsJPEG2
 * - DefineBitsJPEG3
 * - DefineBitsJPEG4
 * - DefineBitsLossless format 4 (format 3 is untested)
 *
 * Using LodePNG: https://lodev.org/lodepng/
 */
vector<uint8_t> SWF::exportImage(size_t imageId) {

	/*vector<Tag *> db_v = this->getTagsOfType(SWF::tagId("DefineBits"));
	vector<Tag *> dbj2_v = this->getTagsOfType(SWF::tagId("DefineBitsJPEG2"));
	vector<Tag *> dbj3_v = this->getTagsOfType(SWF::tagId("DefineBitsJPEG3"));
	vector<Tag *> dbj4_v = this->getTagsOfType(SWF::tagId("DefineBitsJPEG4"));*/

	vector<Tag *> dbl_v = this->getTagsOfType(SWF::tagId("DefineBitsLossless"));
	vector<Tag *> dbl2_v = this->getTagsOfType(SWF::tagId("DefineBitsLossless2"));
	dbl_v.insert(dbl_v.end(), dbl2_v.begin(), dbl2_v.end());
	for (auto &t : dbl_v) {
		auto dbl = static_cast<Tag_DefineBitsLossless *>(t);
		if (imageId == dbl->id) {

			vector<uint8_t> png;
			unsigned error;

			/**
			 * DefineBitsLossless:
			 *     COLORMAPDATA for format 3
			 *     BITMAPDATA for formats 4 and 5
			 *
			 * DefineBitsLossless2:
			 *     ColorTableRGB and ColormapPixelData for format 3
			 *     ARGB[image data size] for formats 4 and 5
			 */
			vector<uint8_t> decompressedImgData = zlib::zlib_decompress(dbl->data);

			if (!dbl->version2) {

				if (dbl->bitmapFormat == 3) {
					// XXX Untested

					vector<uint8_t> img;
					img.reserve(dbl->bitmapWidth * dbl->bitmapHeight);

					size_t pixelDataStart = (dbl->bitmapColorTableSize + 1) * 3;

					lodepng::State state;

					//generate palette
					for (size_t i = 0; i < pixelDataStart; i += 3) {
						//palette must be added both to input and output color mode, because in this
						//sample both the raw image and the expected PNG image use that palette.
						lodepng_palette_add(&state.info_png.color,
								    decompressedImgData[i], // r
								    decompressedImgData[i + 1], // g
								    decompressedImgData[i + 2], // b
								    0xFF); // a
						lodepng_palette_add(&state.info_raw,
								    decompressedImgData[i],
								    decompressedImgData[i + 1],
								    decompressedImgData[i + 2],
								    0xFF);
					}

					//both the raw image and the encoded image must get colorType 3 (palette)
					state.info_png.color.colortype = LCT_PALETTE; //if you comment this line, and create the above palette in info_raw instead, then you get the same image in a RGBA PNG.
					state.info_png.color.bitdepth = 8;
					state.info_raw.colortype = LCT_PALETTE;
					state.info_raw.bitdepth = 8;
					state.encoder.auto_convert = 0; //we specify ourselves exactly what output PNG color mode we want

					int pad = 0;
					for (size_t i = pixelDataStart; i < decompressedImgData.size(); ++i, ++pad) {
						/**
						 * In the DefineBitsLossless COLORMAPDATA structure, every row of pixels must
						 * have a multiple of 4 number of pixels.
						 */
						if (pad == dbl->bitmapWidth) {
							i += (dbl->bitmapWidth % 4) -1;
							pad = -1;
							continue;
						} else {
							img.emplace_back(decompressedImgData[i]);
						}
					}

					error = lodepng::encode(png, img, dbl->bitmapWidth, dbl->bitmapHeight, state);
				} else if (dbl->bitmapFormat == 4) {
					throw swf_exception("Exporting image for 'DefineBitsLossless' format 4 is not implemented.");
				} else {
					// Convert (R)RGB to RGBA - first R means Reserved and is always 0
					for (size_t i = 0; i < decompressedImgData.size(); i += 4) {
						iter_swap(decompressedImgData.begin() + i, decompressedImgData.begin() + i + 1);
						iter_swap(decompressedImgData.begin() + i + 1, decompressedImgData.begin() + i + 2);
						iter_swap(decompressedImgData.begin() + i + 2, decompressedImgData.begin() + i + 3);
						decompressedImgData[i+3] = 0xFF;
					}
					error = lodepng::encode(png, decompressedImgData, dbl->bitmapWidth, dbl->bitmapHeight);
				}

			} else {

				if (dbl->bitmapFormat == 3) {
					/**
					 * Field				Type							Comment
					 *
					 * ColorTableRGB 		RGBA[color table size]			Defines the mapping from color indices to RGBA values.
					 * 														Number of RGBA values is BitmapColorTableSize + 1.
					 *
					 * ColormapPixelData	UI8[image data size]			Array of color indices. Number of entries is BitmapWidth
					 * 														BitmapHeight, subject to padding (see note preceding
					 *														this table).
					 *
					 * In the ColorTable we have an array of RGBA possible values.
					 * The length of ColorTable in bytes is (BitmapColorTableSize + 1) * 4.
					 *
					 * The ColormapPixelData is an array of indices to ColorTableRGB whose
					 * concatenation form the image.
					 */
					vector<uint8_t> img;
					img.reserve(dbl->bitmapWidth * dbl->bitmapHeight);

					size_t pixelDataStart = (dbl->bitmapColorTableSize + 1) * 4;

					lodepng::State state;

					//generate palette
					for (size_t i = 0; i < pixelDataStart; i += 4) {
						//palette must be added both to input and output color mode, because in this
						//sample both the raw image and the expected PNG image use that palette.
						lodepng_palette_add(&state.info_png.color,
								    decompressedImgData[i], // r
								    decompressedImgData[i + 1], // g
								    decompressedImgData[i + 2], // b
								    decompressedImgData[i + 3]); // a
						lodepng_palette_add(&state.info_raw,
								    decompressedImgData[i],
								    decompressedImgData[i + 1],
								    decompressedImgData[i + 2],
								    decompressedImgData[i + 3]);
					}

					//both the raw image and the encoded image must get colorType 3 (palette)
					state.info_png.color.colortype = LCT_PALETTE; //if you comment this line, and create the above palette in info_raw instead, then you get the same image in a RGBA PNG.
					state.info_png.color.bitdepth = 8;
					state.info_raw.colortype = LCT_PALETTE;
					state.info_raw.bitdepth = 8;
					state.encoder.auto_convert = 0; //we specify ourselves exactly what output PNG color mode we want

					int pad = 0;
					for (size_t i = pixelDataStart; i < decompressedImgData.size(); ++i, ++pad) {
						/**
						 * In the DefineBitsLossless2 ALPHACOLORMAPDATA structure, every row of pixels must
						 * have a multiple of 4 number of pixels.
						 */
						if (pad == dbl->bitmapWidth) {
							i += (dbl->bitmapWidth % 4) -1;
							pad = -1;
							continue;
						} else {
							img.emplace_back(decompressedImgData[i]);
						}
					}

					error = lodepng::encode(png, img, dbl->bitmapWidth, dbl->bitmapHeight, state);

				} else {
					// Convert ARGB to RGBA
					for (size_t i = 0; i < decompressedImgData.size(); i += 4) {
						iter_swap(decompressedImgData.begin() + i, decompressedImgData.begin() + i + 1); // RAGB
						iter_swap(decompressedImgData.begin() + i + 1, decompressedImgData.begin() + i + 2); // RGAB
						iter_swap(decompressedImgData.begin() + i + 2, decompressedImgData.begin() + i + 3); // RGBA

						// The RGB data is multiplied by the alpha channel value.
						float alpha = decompressedImgData[i+3] / 255.0f;
					DIAGNOSTIC_PUSH()
					DIAGNOSTIC_IGNORE("-Wfloat-equal")
						if (alpha != 0) {
					DIAGNOSTIC_POP()
							decompressedImgData[i] = static_cast<uint8_t>(decompressedImgData[i] / alpha);
							decompressedImgData[i+1] = static_cast<uint8_t>(decompressedImgData[i+1] / alpha);
							decompressedImgData[i+2] = static_cast<uint8_t>(decompressedImgData[i+2] / alpha);
						}
					}
					error = lodepng::encode(png, decompressedImgData, dbl->bitmapWidth, dbl->bitmapHeight);
				}
			}

			if (error) {
				throw swf_exception("PNG encoder error (" + to_string(error) + "): " + lodepng_error_text(error));
			}
			return png;
		}
	}
	throw swf_exception("No such Image ID: " + to_string(imageId));
}


const vector<uint8_t>& SWF::exportBinary(size_t tagId) {

	vector<Tag *> tv = this->getTagsOfType(SWF::tagId("DefineBinaryData"));
	for (auto &t : tv) {
		auto ds = static_cast<Tag_DefineBinaryData *>(t);
		if (tagId == ds->id) {
			return ds->data;
		}
	}
	throw swf_exception("No such Tag ID: " + to_string(tagId));
}


void SWF::replaceBinary(const vector<uint8_t> &binBuf, size_t tagId) {

	vector<Tag *> tv = this->getTagsOfType(SWF::tagId("DefineBinaryData"));
	for (auto &t : tv) {
		auto ds = static_cast<Tag_DefineBinaryData *>(t);
		if (tagId == ds->id) {
			ds->data = binBuf;
			return;
		}
	}
	throw swf_exception("No such Tag ID: " + to_string(tagId));
}


vector<uint8_t> SWF::exportMp3(size_t soundId) {

	vector<Tag *> tv = this->getTagsOfType(SWF::tagId("DefineSound"));
	for (auto &t : tv) {
		auto ds = static_cast<Tag_DefineSound *>(t);
		if (soundId == ds->id) {
			/**
			 * In the SWF, MP3 data starts with a SeekSamples fields that
			 * represents the number of samples to skip. It is usually 0x00 0x00
			 * and we remove this because it is not part of the MP3 data.
			 *
			 * swf-file-format-spec.pdf - page 188
			 */
			return {ds->data.begin() + 2, ds->data.end()};
		}
	}
	throw swf_exception("No such Sound ID: " + to_string(soundId));
}

/**
 * Missing implementation of:
 * - DefineBits
 * - DefineBitsJPEG2
 * - DefineBitsJPEG3
 * - DefineBitsJPEG4
 * - DefineBitsLossless
 *
 * To-do: detect if it's PNG, JPEG, GIF or something else.
 */
void SWF::replaceImg(const vector<uint8_t> &imgBuf, size_t imageId) {

	/*vector<Tag *> db_v = this->getTagsOfType(SWF::tagId("DefineBits"));
	vector<Tag *> dbj2_v = this->getTagsOfType(SWF::tagId("DefineBitsJPEG2"));
	vector<Tag *> dbj3_v = this->getTagsOfType(SWF::tagId("DefineBitsJPEG3"));
	vector<Tag *> dbj4_v = this->getTagsOfType(SWF::tagId("DefineBitsJPEG4"));*/

	vector<Tag *> dbl_v = this->getTagsOfType(SWF::tagId("DefineBitsLossless"));
	vector<Tag *> dbl2_v = this->getTagsOfType(SWF::tagId("DefineBitsLossless2"));
	dbl_v.insert(dbl_v.end(), dbl2_v.begin(), dbl2_v.end());
	for (auto &t : dbl_v) {
		auto dbl = static_cast<Tag_DefineBitsLossless *>(t);
		if (imageId == dbl->id) {

			vector<uint8_t> argb;
			unsigned width, height;

			if (isPNGfile(imgBuf)) {
				unsigned error = lodepng::decode(argb, width, height, imgBuf);
				if (error) {
					throw swf_exception("PNG decoder error (" + to_string(error) + "): " + lodepng_error_text(error));
				}
				// Convert RGBA to ARGB
				for (size_t i = 0; i < argb.size(); i += 4) {
					iter_swap(argb.begin() + i, argb.begin() + i + 3);// AGBR
					iter_swap(argb.begin() + i + 1, argb.begin() + i + 2); // ABGR
					iter_swap(argb.begin() + i + 1, argb.begin() + i + 3); // ARGB

					// The RGB data must already be multiplied by the alpha channel value.
					argb[i+1] = static_cast<uint8_t>((argb[i+1] * argb[i]) / 255u);
					argb[i+2] = static_cast<uint8_t>((argb[i+2] * argb[i]) / 255u);
					argb[i+3] = static_cast<uint8_t>((argb[i+3] * argb[i]) / 255u);

					if (!dbl->version2) {
						// Instead of alpha we have reserved which is always 0
						argb[i] = 0;
					}
				}
			} else {
				throw swf_exception("Only PNG format is implemented.");
			}/*else if (isJPEGfile(imgBuf)) {
				throw swf_exception("It is a JPEG file!");
			} else if (isGIFfile(imgBuf)) {
				throw swf_exception("It is a GIF file!");
			} else {
				throw swf_exception("Only PNG, JPEG and GIF formats are supported.");
			}*/

			vector<uint8_t> compressed = zlib::zlib_compress(argb, Z_BEST_COMPRESSION);

			dbl->bitmapWidth = static_cast<uint16_t>(width);
			dbl->bitmapHeight = static_cast<uint16_t>(height);
			dbl->bitmapFormat = 5;
			dbl->data = compressed;

			break;
		}
	}
}

void SWF::replaceMp3(const vector<uint8_t> &mp3Buf, size_t soundId) {

	vector<Tag *> tv = this->getTagsOfType(SWF::tagId("DefineSound"));
	for (auto &t : tv) {
		auto ds = static_cast<Tag_DefineSound *>(t);
		if (soundId == ds->id) {

			mp3::mp3_info info;

			try {
				info = mp3::get_mp3_info(mp3Buf);
			} catch (const mp3::mp3_exception &me) {
				throw swf_exception(me.what());
			}

			SWF_DEBUG("MP3 info:");
			SWF_DEBUG("\tStereo: " << (info.stereo ? "yes" : "no"));
			SWF_DEBUG("\tSample rate: " << info.hz << " Hz");
			SWF_DEBUG("\tLayer: " << info.layer);
			SWF_DEBUG("\tAvg. bitrate: " << info.avg_bitrate_kbps << " kbps");
			SWF_DEBUG("\tSample count: " << info.total_samples);

			if (info.hz != 5512 && info.hz != 11025
			    && info.hz != 22050 && info.hz != 44100) {
				throw swf_exception("MP3 sample rate must be one of the following: "
					"5512 Hz, 11025 Hz, 22050 Hz, 44100 Hz");
			}

			for (auto sr : Tag_DefineSound::soundRates) {
				if (sr.second == info.hz) {
					ds->soundRate = sr.first;
					SWF_DEBUG("\tSample rate (swf format): " << ds->soundRate.to_ulong());
					break;
				}
			}

			ds->soundFormat = 2;
			ds->soundSize = 1;
			ds->soundType = (info.stereo ? 1 : 0);
			ds->soundSampleCount = static_cast<uint32_t>(info.total_samples);

			/**
			 * Get rid of ID3v2 tag at the beginning as it is an MP3 frame and
			 * get rid of ID3v1 tag at the end as it is not an MP3 frame.
			 *
			 * http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
			 *
			 * swf-file-format-spec.pdf - page 188
			 */
			//ds->data = { mp3Buf.begin() + info.id3v2size, mp3Buf.begin() + info.id3v1position - info.id3v2size };

			ds->data = { mp3Buf.begin() + info.id3v2size, mp3Buf.end() - info.id3v1size };

			/**
			 * In the SWF, MP3 data starts with a SeekSamples fields that
			 * represents the number of samples to skip. It is usually 0x00 0x00.
			 *
			 * swf-file-format-spec.pdf - page 188
			 */
			ds->data.insert(ds->data.begin(), 0x00);
			ds->data.insert(ds->data.begin(), 0x00);

			return;
		}
	}
	throw swf_exception("No such Sound ID: " + to_string(soundId));
}
