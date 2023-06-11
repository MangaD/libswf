/**
 * libswf - Tag class
 */

#ifndef TAG_HPP
#define TAG_HPP

#include <array>
#include <vector> // vector
#include <cstdint> // uint8_t
#include <bitset> // bitset
#include <map> // map

namespace swf {

	class Tag {
	public:
		Tag() : i(0), id(0), type(), longTag(false), data(), symbolName() {}
		virtual ~Tag() {};
		size_t i;
		size_t id;
		short type;
		bool longTag;
		size_t parseTagHeader(uint8_t *buffer, size_t &pos);
		std::array<uint8_t, 2> makeTagHeader(size_t length);
		std::vector<uint8_t> data;
		virtual std::vector<uint8_t> toBytes();

		/// The symbol name is not saved directly in the tag, it is saved
		/// in the SymbolClass tag, which contains a dictionary of symbols.
		/// We thus full this parameter in every Tag object merely for the convenience
		/// of not having to search for it later, especially since there can be
		/// tags with.same ID, making this process more difficult.
		std::string symbolName;
	};

	class Tag_DefineBinaryData : public Tag {
	public:
		Tag_DefineBinaryData() : Tag(), reserved(0) {}
		virtual ~Tag_DefineBinaryData() {};
		// tagId - 2 bytes
		uint32_t reserved; // must be 0
		std::vector<uint8_t> toBytes() override;
	};

	class Tag_DefineSound : public Tag {
	public:
		Tag_DefineSound() : Tag(), soundFormat(), soundRate(), soundSize(),
			soundType(), soundSampleCount() {}
		virtual ~Tag_DefineSound() {};
		// soundId - 2 bytes
		std::bitset<4> soundFormat;
		std::bitset<2> soundRate;
		std::bitset<1> soundSize;
		std::bitset<1> soundType;
		uint32_t soundSampleCount;
		static std::map<int, std::string> codingFormats;
		static std::map<int, std::string> soundRatesNames;
		static std::map<int, int> soundRates;
		inline static std::string formatName(int f) { return (codingFormats.find(f) == codingFormats.end()) ? "Unknown" : codingFormats[f]; }
		inline static std::string soundRateName(int f) { return (soundRatesNames.find(f) == soundRatesNames.end()) ? "Unknown" : soundRatesNames[f]; }
		std::vector<uint8_t> toBytes() override;
	};

	/**
	 * DefineBitsLossless
	 * DefineBitsLossless2
	 */
	class Tag_DefineBitsLossless : public Tag {
	public:
		Tag_DefineBitsLossless() : Tag(), version2(), bitmapFormat(), bitmapWidth(),
		bitmapHeight(), bitmapColorTableSize() {}
		bool version2;
		uint8_t bitmapFormat;
		uint16_t bitmapWidth;
		uint16_t bitmapHeight;
		uint8_t bitmapColorTableSize; //if bitmapFormat = 3, otherwise absent
		std::vector<uint8_t> toBytes() override;
	};

	class Tag_SymbolClass : public Tag {
	public:
		Tag_SymbolClass() : Tag(), numSymbols(0), symbolClass() {}
		virtual ~Tag_SymbolClass() {};
		uint16_t numSymbols;
		std::vector< std::pair<size_t, std::string> > symbolClass;
		std::vector<uint8_t> toBytes() override;
	};

}


#endif
