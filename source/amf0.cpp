/**
 * libswf - AMF class
 */

#include <string>
#include <vector>
#include <cstdint>  // uint8_t
#include <sstream>  // stringstream
#include <iostream> // std::hex
#include <iomanip>  // setw, setfill
#include <cmath>    // isfinite

#include <json.hpp>
#include <fifo_map.hpp> // https://github.com/nlohmann/fifo_map/blob/master/src/fifo_map.hpp
#include "amf0.hpp"
#include "swf.hpp" // swf_exception
#include "swf_utils.hpp" // debug

/**
 * For ordering the JSON elements in a FIFO manner, this is useful because:
 * 1. Easier to debug, just compare original decompressed file with new one.
 * 2. For ordering elements in ECMA arrays, as reading them out of order may not work out well.
 *
 * https://github.com/nlohmann/json/issues/485#issuecomment-333652309
 * A workaround to give to use fifo_map as map, we are just ignoring the 'less' compare
 */
template<class K, class V, class dummy_compare, class A>
using my_workaround_fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;
using json = nlohmann::basic_json<my_workaround_fifo_map>;

// For normal use, without ordering
//using json = nlohmann::json;

using namespace std;
using namespace swf;

/**
 * JSON: https://github.com/nlohmann/json
 */
AMF0::AMF0(const uint8_t *buffer, const size_t buffer_size) : jsonObj() {

	if (buffer_size == 0) {
		throw swf_exception("AMF0: No data given to parse.");
	}

	size_t pos = 0;

	if (buffer[pos] == AMF0::TYPED_OBJECT_MARKER) {
		++pos;

		// Get class name
		uint16_t len = bytestodec_be<uint16_t>(buffer+pos);
		pos += 2;

		string cn{buffer + pos, buffer + pos + len};
		pos += len;

		// Creates a key with the class name and an object as value
		json j;
		jsonObj.emplace(cn, j);

		this->parseAMF0Object(buffer, buffer_size, pos, jsonObj[cn]);
	} else {
		throw swf_exception("Sorry, only serialization of objects implemented.");
	}
}

void AMF0::parseAMF0Object(const uint8_t *buffer, const size_t buffer_size, size_t &pos, json &obj) {

	string varName;
	bool readVarName = false;

	while (pos < buffer_size) {

		if (!readVarName) {
			// Get variable name
			uint16_t len = bytestodec_be<uint16_t>(buffer + pos);
			pos += 2;

			varName = {buffer + pos, buffer + pos + len};
			pos += len;

			readVarName = true;
		}
		/// See section 2.2 of amf0-file-format-specification.pdf
		else if (buffer[pos] == AMF0::NUMBER_MARKER) {
			++pos;

			double d = AMF0::readDouble(buffer, pos);

			/**
			 * NaN can have many different representations, and
			 * infinite can have 2 representations (positive and negative),
			 * so we store an array of bytes for them to keep their
			 * exact representation.
			 */
			if(isfinite(d)) {
				obj[varName] = d;
			} else {
				obj[varName];
				for (size_t i = 0; i < 8; ++i) {
					obj[varName].emplace_back(buffer[pos-8+i]);
				}
			}

			readVarName = false;
		}
		/// See section 2.3 of amf0-file-format-specification.pdf
		else if (buffer[pos] == AMF0::BOOLEAN_MARKER) {
			++pos;

			bool b = buffer[pos];
			++pos;

			obj[varName] = b;

			readVarName = false;
		}
		/// See section 2.4 of amf0-file-format-specification.pdf
		else if (buffer[pos] == AMF0::STRING_MARKER) {
			++pos;

			uint16_t len = bytestodec_be<uint16_t>(buffer + pos);;
			pos += 2;

			string s{buffer + pos, buffer + pos + len};
			pos += len;

			obj[varName] = s;

			readVarName = false;
		}
		/// See section 2.5 of amf0-file-format-specification.pdf
		else if (buffer[pos] == AMF0::OBJECT_MARKER) {
			++pos;

			// Creates a key with the class name and an object as value
			json j;
			obj.emplace(varName, j);

			this->parseAMF0Object(buffer, buffer_size, pos, obj[varName]);

			readVarName = false;
		}
		/// See section 2.7 of amf0-file-format-specification.pdf
		else if (buffer[pos] == AMF0::NULL_MARKER) {
			++pos;

			obj[varName] = nullptr;

			readVarName = false;
		}
		/// See section 2.8 of amf0-file-format-specification.pdf
		else if (buffer[pos] == AMF0::UNDEFINED_MARKER) {
			++pos;

			obj[varName] = "HFW_undefinedXXX"; // XXX Don't know a better way to distinguish from null

			readVarName = false;
		}
		/// See section 2.9 of amf0-file-format-specification.pdf
		else if (buffer[pos] == AMF0::REFERENCE_MARKER) {
			++pos;

			uint16_t index = bytestodec_be<uint16_t>(buffer + pos);
			pos += 2;

			json j;
			j["HFW_referenceXXX"] = index; // XXX way I found to have a reference
			obj.emplace(varName, j);

			readVarName = false;
		}
		/// See section 2.10 of amf0-file-format-specification.pdf
		else if (buffer[pos] == AMF0::ECMA_ARRAY_MARKER) {
			++pos;

			uint32_t aLen = bytestodec_be<uint32_t>(buffer + pos);
			pos += 4;

			/**
			 * ECMA arrays work like objects except they are prefixed with a length.
			 * But sometimes this length is zero and the array contains objects,
			 * I don't know why.
			 */
			obj[varName];
			obj[varName]["HFW_ArrayLenXXX"] = aLen;

			this->parseAMF0Object(buffer, buffer_size, pos, obj[varName]);

			readVarName = false;
		}
		/// at the end of typed object, anonymous object and array
		/// See section 2.11 of amf0-file-format-specification.pdf
		else if (buffer[pos] == AMF0::OBJECT_END_MARKER) {
			++pos;
			return;
		}
		/// See section 2.18 of amf0-file-format-specification.pdf
		else if (buffer[pos] == AMF0::TYPED_OBJECT_MARKER) {
			++pos;

			// Get class name
			uint16_t len = bytestodec_be<uint16_t>(buffer + pos);
			pos += 2;

			string cn{buffer + pos, buffer + pos + len};
			pos += len;

			// Creates a key with the class name and an object as value
			json j;
			j["HFW_classNameXXX"] = cn; // XXX way I found to have a named object
			obj.emplace(varName, j);

			this->parseAMF0Object(buffer, buffer_size, pos, obj[varName]);

			readVarName = false;
		} else {
			std::stringstream stream;
			stream << std::hex << setw(2) << setfill('0') << static_cast<int>(buffer[pos]);
			throw swf_exception("Position: " + to_string(pos) + ". Marker '0x" + stream.str() + "' not valid or not implemented.");
		}
	}
}

vector<uint8_t> AMF0::fromJSON(const string &js) {

	vector<uint8_t> bytes;
	json obj = json::parse(js);

	bytes.emplace_back(AMF0::TYPED_OBJECT_MARKER);

	// class name's length
	const string &key = obj.items().begin().key();
	AMF0::writeStringWithLenPrefixU16(bytes, key);

	for (auto &el : obj[key].items()) {
		AMF0::writeStringWithLenPrefixU16(bytes, el.key());
		parseJSONElement(bytes, el.value());
	}

	//end of object
	array<uint8_t, 3> endObj = {0x00, 0x00, AMF0::OBJECT_END_MARKER};
	concatVectorWithContainer(bytes, endObj);

	return bytes;
}

void AMF0::parseJSONElement(vector<uint8_t> &buffer, const json &el) {

	if (el.is_null()) {
		buffer.emplace_back(AMF0::NULL_MARKER);
	} else if (el.is_boolean()) {
		buffer.emplace_back(AMF0::BOOLEAN_MARKER);
		if (el.get<bool>() == true) {
			buffer.emplace_back(0x01);
		} else {
			buffer.emplace_back(0x00);
		}
	} else if (el.is_number()) {
		buffer.emplace_back(AMF0::NUMBER_MARKER);
		double d = el.get<double>();
		array<uint8_t, 8> num_b = AMF0::writeDouble(d);
		concatVectorWithContainer(buffer, num_b);
	} else if (el.is_object()) {

		if (el.find("HFW_ArrayLenXXX") != el.end() &&
		    el["HFW_ArrayLenXXX"].is_number()) {
			buffer.emplace_back(AMF0::ECMA_ARRAY_MARKER);
			array<uint8_t, 4> len = dectobytes_be<uint32_t>(el["HFW_ArrayLenXXX"]);
			concatVectorWithContainer(buffer, len);
		} else if (el.find("HFW_classNameXXX") != el.end() &&
		           el["HFW_classNameXXX"].is_string()) {
			buffer.emplace_back(AMF0::TYPED_OBJECT_MARKER);
			string className = el["HFW_classNameXXX"].get<string>();
			AMF0::writeStringWithLenPrefixU16(buffer, className);
		} else if (el.find("HFW_referenceXXX") != el.end()) {
			buffer.emplace_back(AMF0::REFERENCE_MARKER);
			array<uint8_t, 2> index = dectobytes_be<uint16_t>(el["HFW_referenceXXX"]);
			concatVectorWithContainer(buffer, index);
			return;
		} else {
			buffer.emplace_back(AMF0::OBJECT_MARKER);
		}

		for (auto &el2 : el.items()) {
			if (el2.key() == "HFW_classNameXXX" || el2.key() == "HFW_ArrayLenXXX") {
				continue;
			}
			AMF0::writeStringWithLenPrefixU16(buffer, el2.key());
			parseJSONElement(buffer, el2.value());
		}

		array<uint8_t, 3> endObj = {0x00, 0x00, AMF0::OBJECT_END_MARKER};
		concatVectorWithContainer(buffer, endObj);

	} else if (el.is_string()) {
		string s = el.get<string>();
		if (s == "HFW_undefinedXXX") {
			buffer.emplace_back(AMF0::UNDEFINED_MARKER);
		} else {
			buffer.emplace_back(AMF0::STRING_MARKER);
			AMF0::writeStringWithLenPrefixU16(buffer, s);
		}
	} else if (el.is_array()) {
		if (el.size() == 8) { // Non-finite number
			buffer.emplace_back(AMF0::NUMBER_MARKER);
			concatVectorWithContainer(buffer, el);
		} else {
			throw swf_exception("JSON Arrays only in use for non-finite numbers.");
		}
	} else {
		throw swf_exception("Unrecognized JSON type.");
	}
}

/**
 * First tried this solution:
 * https://stackoverflow.com/questions/30118308/convert-8-bytes-to-double-value?lq=1
 * But it is undefined behavior in C++:
 * https://stackoverflow.com/questions/11373203/accessing-inactive-union-member-and-undefined-behavior
 * and little-endian only.
 *
 * Solution by Som1Lse:
 */
double AMF0::readDouble(const uint8_t* buffer, size_t &pos) {
	uint64_t u;

	u = static_cast<uint64_t>(buffer[pos]) << 56 |
	    static_cast<uint64_t>(buffer[pos + 1]) << 48 |
	    static_cast<uint64_t>(buffer[pos + 2]) << 40 |
	    static_cast<uint64_t>(buffer[pos + 3]) << 32 |
	    static_cast<uint64_t>(buffer[pos + 4]) << 24 |
	    static_cast<uint64_t>(buffer[pos + 5]) << 16 |
	    static_cast<uint64_t>(buffer[pos + 6]) << 8 |
	    static_cast<uint64_t>(buffer[pos + 7]);

	pos += 8;

	return bit_cast<double>(u);
}

array<uint8_t, 8> AMF0::writeDouble(double d) {
	return dectobytes_be<uint64_t>(bit_cast<uint64_t>(d));
}

void AMF0::writeStringWithLenPrefixU16(std::vector<uint8_t> &data, const std::string &s) {
	std::array<uint8_t, 2> len = dectobytes_be<uint16_t>(static_cast<uint16_t>(s.size()));
	concatVectorWithContainer(data, len);
	concatVectorWithContainer(data, s);
}
