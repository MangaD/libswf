/**
 * libswf - Tag class
 */

#include <vector> // vector
#include <cstdint> // uint8_t
#include "tag.hpp"
#include "swf_utils.hpp"

using namespace std;
using namespace swf;

/**
 * Tag
 */

size_t Tag::parseTagHeader(uint8_t *buffer, size_t &pos) {
	uint16_t tagCodeAndLength = bytestodec_le<uint16_t>(buffer + pos);
	pos += 2;

	this->type = (tagCodeAndLength >> 6);
	uint32_t length = tagCodeAndLength & 0x3F;

	if (length == 0b111111) {
		// Long tag
		length = bytestodec_le<uint32_t>(buffer + pos);
		pos += 4;
		this->longTag = true;
	}

	return length;
}

array<uint8_t, 2> Tag::makeTagHeader(size_t length) {
	uint16_t tagCodeAndLength = static_cast<uint16_t>(this->type << 6);
	// Convert short tag to longer tag in case it isn't and it got bigger.
	if (length >= 63) {
		this->longTag = true;
	}
	if (this->longTag) {
		tagCodeAndLength = tagCodeAndLength | 0x3f;
	} else {
		// Long tags can be shorter than 63 bytes, we don't convert them!
		tagCodeAndLength = tagCodeAndLength | static_cast<uint16_t>(length);
	}
	return dectobytes_le<uint16_t>(tagCodeAndLength);
}

vector<uint8_t> Tag::toBytes() {
	vector<uint8_t> buffer;
	auto header = this->makeTagHeader(this->data.size());
	buffer.insert(buffer.end(), header.begin(), header.end());

	if (this->longTag) {
		// Long tag
		array<uint8_t, 4> len = dectobytes_le<uint32_t>(static_cast<uint32_t>(this->data.size()));
		buffer.insert(buffer.end(), len.begin(), len.end());
	}

	buffer.insert(buffer.end(), this->data.begin(), this->data.end());

	return buffer;
}

/**
 * Tag_DefineSound
 */

map<int, string> Tag_DefineSound::codingFormats = {
	{0, "Uncompressed, native-endian"}, {1, "ADPCM"}, {2, "MP3"},
	{3, "Uncompressed, little-endian"}, {4, "Nellymoser 16 kHz"},
	{5, "Nellymoser 8 kHz"}, {6, "Nellymoser"}, {7, "Speex"}
};

map<int, int> Tag_DefineSound::soundRates = {
	{0, 5512}, {1, 11025}, {2, 22050},
	{3, 44100}
};

map<int, string> Tag_DefineSound::soundRatesNames = {
	{0, "5512 Hz"}, {1, "11025 Hz"}, {2, "22050 Hz"},
	{3, "44100 Hz"}
};


vector<uint8_t> Tag_DefineSound::toBytes() {
	vector<uint8_t> buffer;
	auto length = this->data.size() + 2 + 1 + 4; // sound id + sound info + sample count
	auto header = this->makeTagHeader(length);
	buffer.insert(buffer.end(), header.begin(), header.end());

	if (this->longTag) {
		// Long tag
		array<uint8_t, 4> len = dectobytes_le<uint32_t>(static_cast<uint32_t>(length));
		buffer.insert(buffer.end(), len.begin(), len.end());
	}

	array<uint8_t, 2> sid = dectobytes_le<uint16_t>(static_cast<uint16_t>(this->id));
	buffer.insert(buffer.end(), sid.begin(), sid.end());

	auto soundInfo = static_cast<std::uint8_t>(
	    (this->soundFormat.to_ulong() << 4)|
	    (this->soundRate.to_ulong() << 2)|
	    (this->soundSize.to_ulong() << 1)|
	    (this->soundType.to_ulong() << 0)
	);

	buffer.emplace_back(soundInfo);

	array<uint8_t, 4> sc = dectobytes_le<uint32_t>(this->soundSampleCount);
	buffer.insert(buffer.end(), sc.begin(), sc.end());

	buffer.insert(buffer.end(), this->data.begin(), this->data.end());

	return buffer;
}



/**
 * Tag_SymbolClass
 */

vector<uint8_t> Tag_SymbolClass::toBytes() {

	vector<uint8_t> content;

	array<uint8_t, 2> numSym = dectobytes_le<uint16_t>(static_cast<uint16_t>(this->symbolClass.size()));
	content.insert(content.end(), numSym.begin(), numSym.end());

	for (auto p : this->symbolClass) {
		array<uint8_t, 2> tid = dectobytes_le<uint16_t>( static_cast<uint16_t>(p.first) );
		content.insert(content.end(), tid.begin(), tid.end());
		content.insert(content.end(), p.second.begin(), p.second.end());
		content.emplace_back(0);
	}

	vector<uint8_t> buffer;
	auto header = this->makeTagHeader(content.size());
	buffer.insert(buffer.end(), header.begin(), header.end());

	if (this->longTag) {
		array<uint8_t, 4> len = dectobytes_le<uint32_t>(static_cast<uint32_t>(content.size()));
		buffer.insert(buffer.end(), len.begin(), len.end());
	}

	buffer.insert(buffer.end(), content.begin(), content.end());

	return buffer;
}



/**
 * Tag_DefineBitsLossless
 */

vector<uint8_t> Tag_DefineBitsLossless::toBytes() {
	vector<uint8_t> buffer;
	// character id (2) + bitmapFormat (1) + bitmapWidth (2) + bitmapHeight (2) + bitmapColorTableSize (1)
	auto length = this->data.size() + 2 + 1 + 2 + 2 + (this->bitmapFormat == 3 ? 1 : 0);
	auto header = this->makeTagHeader(length);
	buffer.insert(buffer.end(), header.begin(), header.end());

	if (this->longTag) {
		// Long tag
		array<uint8_t, 4> len = dectobytes_le<uint32_t>(static_cast<uint32_t>(length));
		buffer.insert(buffer.end(), len.begin(), len.end());
	}

	array<uint8_t, 2> cid = dectobytes_le<uint16_t>(static_cast<uint16_t>(this->id));
	buffer.insert(buffer.end(), cid.begin(), cid.end());

	buffer.emplace_back(this->bitmapFormat);

	array<uint8_t, 2> bw = dectobytes_le<uint16_t>(static_cast<uint16_t>(this->bitmapWidth));
	buffer.insert(buffer.end(), bw.begin(), bw.end());

	array<uint8_t, 2> bh = dectobytes_le<uint16_t>(static_cast<uint16_t>(this->bitmapHeight));
	buffer.insert(buffer.end(), bh.begin(), bh.end());

	if (this->bitmapFormat == 3) {
		buffer.emplace_back(this->bitmapColorTableSize);
	}

	buffer.insert(buffer.end(), this->data.begin(), this->data.end());

	return buffer;
}


/**
 * Tag_DefineBinaryData
 */

vector<uint8_t> Tag_DefineBinaryData::toBytes() {
	vector<uint8_t> buffer;
	auto length = this->data.size() + 2 + 4;
	auto header = this->makeTagHeader(length);
	buffer.insert(buffer.end(), header.begin(), header.end());

	if (this->longTag) {
		// Long tag
		// character id (2) + reserved (4)
		array<uint8_t, 4> len = dectobytes_le<uint32_t>(static_cast<uint32_t>(length));
		buffer.insert(buffer.end(), len.begin(), len.end());
	}

	array<uint8_t, 2> cid = dectobytes_le<uint16_t>(static_cast<uint16_t>(this->id));
	buffer.insert(buffer.end(), cid.begin(), cid.end());

	array<uint8_t, 4> res = dectobytes_le<uint32_t>(this->reserved);
	buffer.insert(buffer.end(), res.begin(), res.end());

	buffer.insert(buffer.end(), this->data.begin(), this->data.end());

	return buffer;
}
