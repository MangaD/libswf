/**
 * libswf - SWF class
 */

#ifndef LIBSWF_HPP
#define LIBSWF_HPP

// https://stackoverflow.com/questions/14251038/debug-macros-in-c/14256296
#ifdef SWF_DEBUG_BUILD
	#include <iostream>
	#define SWF_DEBUG(x) do { std::cout << x << std::endl; } while (0)
	#define SWF_DEBUG_NNL(x) do { std::cout << x; } while (0)
#else
	#define SWF_DEBUG(x) do {} while (0)
	#define SWF_DEBUG_NNL(x) do {} while (0)
#endif

#include <exception> // exception
#include <string>
#include <vector>
#include <map>
#include <cstdint> //uint8_t, uint32_t
#include <memory> // unique_ptr
#include <algorithm> // find_if
#include "tag.hpp"

namespace swf {

	class swf_exception : public std::exception {
		public:
			explicit swf_exception(const std::string &message = "swf_exception")
				: std::exception(), error_message(message) {}
			const char *what() const noexcept
			{
				return error_message.c_str();
			}
		private:
			std::string error_message;
	};

	class Projector {
	public:
		Projector() : windows(false), buffer() {};
		bool windows;
		std::vector<uint8_t> buffer;
		const std::array<uint8_t, 4> footer = { 0x56, 0x34, 0x12, 0xFA };
	};

	class SWF {
	public:
		explicit SWF(const std::vector<uint8_t> &buffer);
		std::vector <Tag *> getTagsOfType(int type);
		Tag * getTagWithId(size_t id); // Every definition tag must specify a unique ID. Duplicate IDs are not allowed.
		inline static std::string tagName(int id) { return (tagTypeNames.find(id) == tagTypeNames.end()) ? "Unknown" : tagTypeNames[id]; }
		inline static int tagId(const std::string &name) {
			auto it = std::find_if(tagTypeNames.begin(), tagTypeNames.end(),
			             [name](const std::pair<int, std::string> &type) -> bool { return type.second == name; });
			if (it != tagTypeNames.end()) return it->first; else return -2;
		}
		std::vector<uint8_t> toBytes() const;
		std::vector<uint8_t> zlibCompress(const std::vector<uint8_t> &swf);
		std::vector<uint8_t> zlibDecompress(const std::vector<uint8_t> &swf);
		std::vector<uint8_t> lzmaCompress(const std::vector<uint8_t> &swf);
		std::vector<uint8_t> lzmaDecompress(const std::vector<uint8_t> &swf);
		std::vector<uint8_t> exe2swf(const std::vector<uint8_t> &exe);
		std::vector<uint8_t> exportImage(size_t imageId);
		std::vector<uint8_t> exportMp3(size_t soundId);
		const std::vector<uint8_t>& exportBinary(size_t tagId);
		std::vector<uint8_t> exportExe(const std::vector<uint8_t> &proj, int compression);
		std::vector<uint8_t> exportSwf(int compression);
		void replaceImg(const std::vector<uint8_t> &imgBuf, size_t imageId);
		void replaceMp3(const std::vector<uint8_t> &mp3Buf, size_t soundId);
		void replaceBinary(const std::vector<uint8_t> &binBuf, size_t tagId);
		inline bool hasProjector() { return ! projector.buffer.empty(); }
		inline bool isProjectorWindows()  { return projector.windows; }
		std::vector< std::pair<size_t, std::string> > getAllSymbols() const;

		/// Apparently there can be symbols with same ID (e.g. HF v0.3.0 has story04
		/// and story05 both with ID=216), although this is likely a bug, as there is
		/// no tag for story05 and tags can't have the same ID. Regardless, we return
		/// all symbols with the given ID, in case there is more than one.
		std::vector<std::string> getSymbolName(size_t id) const;
		inline uint8_t getVersion() const { return this->version; };
		inline void setVersion(const uint8_t v) { this->version = v; };
	private:
		void parseSwf(const std::vector<uint8_t> &buffer);
		void fillTagsSymbolName();
		static std::map<int, std::string> tagTypeNames;
		std::vector <std::unique_ptr<Tag>> tags;
		uint8_t version; // 1 byte, after signature, followed by 4 bytes representing the SWF file length
		std::vector<uint8_t> frameSize; // 9 bytes on HF (it is a dynamic size)
		std::array<uint8_t, 2> frameRate;
		std::array<uint8_t, 2> frameCount;
		size_t parseSwfHeader(std::vector<uint8_t> &buffer);
		void debugFrameSize(const std::vector<uint8_t>&bytes, size_t nbits);
		Projector projector;
	};

} // swf

#endif // LIBSWF_HPP
