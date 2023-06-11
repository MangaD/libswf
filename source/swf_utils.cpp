/**
 * libswf - Utility functions
 */

#include <string>     // string
#include "swf_utils.hpp"

using namespace std;

bool isPEfile (const vector<uint8_t> &exe) {
	if (exe.size() < 0x3C + 4) return false;
	string exeSig = { static_cast<char>(exe[0]), static_cast<char>(exe[1]) };
	if (exeSig == "MZ") { // Mark Zbikowski
		uint32_t pointerPE = bytestodec_le<uint32_t>( exe.data() + 0x3C );
		if (exe.size() < pointerPE + 4) return false;
		exeSig = { static_cast<char>(exe[pointerPE]), static_cast<char>(exe[pointerPE+1]) };
		if (exeSig == "PE") {
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
}

bool isELFfile (const vector<uint8_t> &exe) {
	if (exe.size() < 4) return false;
	string exeSig = { static_cast<char>(exe[1]), static_cast<char>(exe[2]), static_cast<char>(exe[3]) };
	if (exe[0] == 0x7F && exeSig == "ELF") {
		return true;
	} else {
		return false;
	}
}

/**
 * Image files
 * https://en.wikipedia.org/wiki/Portable_Network_Graphics
 */
bool isPNGfile (const vector<uint8_t> &png) {
	const uint8_t magicNumber[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	if (strncmp ((const char *) png.data(), (const char *) magicNumber, 8) == 0) {
		return true;
	}
	return false;
}
bool isJPEGfile (const vector<uint8_t> &jpeg) {
	const uint8_t header[2] = { 0xFF, 0xD8 };
	const uint8_t footer[2] = { 0xFF, 0xD9 };
	if (strncmp ((const char *) jpeg.data(), (const char *) header, 2) == 0) {
		if (strncmp ((const char *) jpeg.data() + jpeg.size() - 2, (const char *) footer, 2) == 0) {
			return true;
		}
	}
	return false;
}
bool isGIFfile (const vector<uint8_t> &gif) {
	const uint8_t magicNumber[4] = { 0x47, 0x49, 0x46, 0x38 };
	if (strncmp ((const char *) gif.data(), (const char *) magicNumber, 4) == 0) {
		return true;
	}
	return false;
}

/**
 * Byte operations
 */

void bytesToBitset(swf_utils::dynamic_bitset &bs, const vector<uint8_t> &bytes) {
	for (size_t i = 0; i < bytes.size(); ++i) {
		bs <<= 8;
		swf_utils::dynamic_bitset dbsTmp(8, bytes[i]);
		bs |= dbsTmp;
	}
}

void subBitset(const swf_utils::dynamic_bitset &bs, swf_utils::dynamic_bitset &sub, int start_pos) {
	for (int i = static_cast<int>(bs.size()) - 1 - static_cast<int>(start_pos),
	     j = static_cast<int>(sub.size()) - 1;
	     i >= static_cast<int>(bs.size()) - static_cast<int>(sub.size()) - start_pos && j >= 0;
	     i--, j--)
	{
		sub[j] = bs[i];
	}
}
