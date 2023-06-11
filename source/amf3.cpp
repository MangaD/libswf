/**
 * libswf - AMF class
 *
 * Resources: http://datalker.com/ext-docs-5.1/5.1.0-apidocs/source/Packet.html
 */

#include "amf3.hpp"
#include "amf0.hpp" // readDouble, writeDouble
#include "swf.hpp" // swf_exception
#include "swf_utils.hpp" // debug
#include <string>
#include <vector>
#include <cstdint> // uint8_t
#include <sstream> // stringstream
#include <iostream> // hex
#include <iomanip> // setw, setfill
#include <bitset>
#include <stdexcept> // std::out_of_range

// TODO
// AMF3_TYPE to_json and from_json
// https://nlohmann.github.io/json/features/arbitrary_types/

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

AMF3::AMF3(const uint8_t* buffer) : object(), stringRefs(),
			objTraitsRefs(), objRefs() {

	size_t pos = 0;
	this->object = parseAmf3(buffer, pos);
}


std::shared_ptr<AMF3_TYPE> AMF3::parseAmf3(const uint8_t* buffer, size_t &pos) {

	/**
	 * Section 3.2, 3.3, 3.4, 3.5 of amf3-file-format-spec.pdf
	 */
	if (buffer[pos] == AMF3::UNDEFINED_MARKER ||
	    buffer[pos] == AMF3::NULL_MARKER ||
	    buffer[pos] == AMF3::FALSE_MARKER ||
	    buffer[pos] == AMF3::TRUE_MARKER) {

		auto obj = std::make_shared<AMF3_TYPE>();
		obj->type = buffer[pos];

		++pos;
		return obj;
	}
	/**
	 * Section 3.6 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::INTEGER_MARKER) {

		auto obj = std::make_shared<AMF3_INTEGER>();
		obj->type = buffer[pos];

		++pos;

		obj->i = static_cast<int32_t>(AMF3::decodeU29(buffer, pos));

		return obj;
	}
	/**
	 * Section 3.7 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::DOUBLE_MARKER) {

		auto obj = std::make_shared<AMF3_DOUBLE>();
		obj->type = buffer[pos];

		++pos;

		obj->d = AMF0::readDouble(buffer, pos);

		return obj;
	}
	/**
	 * Section 3.8 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::STRING_MARKER) {

		auto obj = std::make_shared<AMF3_STRING>();
		obj->type = buffer[pos];

		++pos;

		string str = AMF3::readString(buffer, pos, obj->isReference, obj->u29);
		if (!obj->isReference) {
			obj->s = str;
		}

		return obj;
	}
	/**
	 * Section 3.11 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::ARRAY_MARKER) {

		auto obj = std::make_shared<AMF3_ARRAY>();
		obj->type = buffer[pos];

		++pos;

		obj->u29 = decodeU29(buffer, pos);
		obj->isReference = !(obj->u29 & 0x1);
		obj->u29 >>= 1;

		if (!obj->isReference) {
			obj->denseValues.reserve(obj->u29); // count of dense portion

			/**
			 * AMF considers Arrays in two parts, the dense portion and the associative portion. The
			 * binary representation of the associative portion consists of name/value pairs (potentially
			 * none) terminated by an empty string. The binary representation of the dense portion is the
			 * size of the dense portion (potentially zero) followed by an ordered list of values
			 * (potentially none). The order these are written in AMF is first the size of the dense
			 * portion, an empty string terminated list of name/value pairs, followed by size values.
			 */

			// If the first key is not an empty string - this is the associative
			// portion.  Read keys and values from the array until
			// we get to an empty string key
			while (true) {
				auto keyObj = std::make_unique<AMF3_STRING>();
				keyObj->type = AMF3::STRING_MARKER;
				string key = AMF3::readString(buffer, pos, keyObj->isReference, keyObj->u29);

				if (!key.empty()) {
					auto value = this->parseAmf3(buffer, pos);
					obj->associativeNameValues.emplace_back(std::move(keyObj), std::move(value));
				} else {
					break;
				}
			}

			// This is the dense portion
			for (uint32_t i = 0; i < obj->u29; i++) {
				obj->denseValues.emplace_back(this->parseAmf3(buffer, pos));
			}

			// Add to reference table
			objRefs.emplace_back(obj);
		}

		return obj;
	}
	/**
	 * Section 3.12 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::OBJECT_MARKER) {

		auto obj = std::make_shared<AMF3_OBJECT>();
		obj->type = buffer[pos];

		++pos;

		obj->u29 = decodeU29(buffer, pos);

		obj->isReference = !(obj->u29 & 0x1);
		obj->u29 >>= 1;

		if (obj->isReference) {
			return obj;
		}

		obj->isTraitRef = !(obj->u29 & 0x1);
		obj->u29 >>= 1;

		if (obj->isTraitRef) {
			obj->trait = objTraitsRefs[obj->u29];
		} else {
			obj->isTraitExt = obj->u29 & 0x1;
			obj->u29 >>= 1;

			if (obj->isTraitExt) {
				throw swf_exception("AMF3 Object traits ext not implemented because it is program dependent.");
			}

			auto trait = std::make_shared<AMF3_TRAIT>();

			trait->isDynamic = obj->u29 & 0x1;
			obj->u29 >>= 1; // number of sealed traits member names that follow after the class name

			trait->className = AMF3::readObjectKey(buffer, pos);

			for(uint32_t i = 0; i < obj->u29; i++) {
				trait->memberNames.emplace_back(AMF3::readObjectKey(buffer, pos));
			}

			obj->trait = trait;
			objTraitsRefs.emplace_back(trait);
		}


		// Read the sealed member values
		for(uint32_t i = 0; i < obj->trait->memberNames.size(); i++) {
			obj->sealedValues.emplace_back(this->parseAmf3(buffer, pos));
		}

		if (obj->trait->isDynamic) {
			// If the dynamic flag is set, dynamic members may follow
			// the sealed members. Read key/value pairs until we
			// encounter an empty string key signifying the end of the
			// dynamic members.
			while (true) {
				auto keyObj = std::make_unique<AMF3_STRING>();
				keyObj->type = AMF3::STRING_MARKER;
				string key = AMF3::readString(buffer, pos, keyObj->isReference, keyObj->u29);

				if (!key.empty()) {
					auto value = this->parseAmf3(buffer, pos);
					obj->dynamicNameValues.emplace_back(std::move(keyObj), std::move(value));
				} else {
					break;
				}
			}
		}

		objRefs.emplace_back(obj);

		return obj;

	}
	/**
	 * Section 3.14 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::BYTE_ARRAY_MARKER) {

		auto obj = std::make_shared<AMF3_BYTEARRAY>();
		obj->type = buffer[pos];

		++pos;

		obj->u29_length = pos;

		obj->u29 = decodeU29(buffer, pos);
		obj->isReference = !(obj->u29 & 0x1);
		obj->u29 >>= 1;

		obj->u29_length = pos - obj->u29_length;

		if (!obj->isReference) {
			obj->binaryData = { buffer + pos, buffer + pos + obj->u29 };

			// Add to reference table
			objRefs.emplace_back(obj);
		}

		return obj;
	}
	else {
		stringstream stream;
		stream << hex << setw(2) << setfill('0') << static_cast<int>(buffer[pos]);
		throw swf_exception("Position: " + to_string(pos) + ". Marker '0x" +
		                    stream.str() + "' not valid or not implemented.");
	}
}

std::string AMF3::readObjectKey(const uint8_t* buffer, size_t &pos) {
	bool isStrRef;
	uint32_t index;
	string key = AMF3::readString(buffer, pos, isStrRef, index);
	if (isStrRef) {
		throw swf_exception("String reference as object key not implemented.");
	}
	return key;
}

std::string AMF3::readString(const uint8_t* buffer, size_t &pos, bool &isStrRef, uint32_t &strLen) {
	uint32_t header = decodeU29(buffer, pos);
	strLen = header >> 1;
	string s;
	if (header & 0x1) {
		isStrRef = false;

		if (strLen == 0) {
			s = "";
		} else {
			s = {buffer + pos, buffer + pos + strLen};
			stringRefs.emplace_back(s);
		}

		pos += strLen;
	} else {
		isStrRef = true;
		s = stringRefs[strLen];
	}
	SWF_DEBUG(s);
	return s;
}

/**
 * Takes 7 bits from each byte.
 * Returns before reading 4 bytes if the byte it is
 * reading does not start with bit 1
 *
 * Example:
 *     Hex: 98 A7 4F
 *     Binary: 10011000 10100111 01001111
 *
 *     The first 2 bytes start with bit 1 so we keep
 *     reading until the 3rd byte that starts with bit
 *     0. We remove the first bit of each byte, so it becomes:
 *     Binary: 0011000 0100111 1001111
 *     Decimal: 398287
 *
 * See section 1.3.1 of amf3-file-format-spec.pdf
 */
uint32_t AMF3::decodeU29(const uint8_t* buffer, size_t &pos) {
	uint32_t num = 0;
	for (size_t i = pos, max = pos+3; i < max; ++i) {
		if (buffer[i] < 0x80) {
			num |= buffer[i];
			++pos;
			break;
		} else {
			num = (num | (buffer[i] & 0x7f)) << 7;
			if (i == max-1) {
				num <<= 1;
				num |= buffer[i+1];
				++pos;
			}
			++pos;
		}
	}
	return num;
}

/**
 * By Som1Lse
 *
 * See section 1.3.1 of amf3-file-format-spec.pdf
 */
uint8_t* AMF3::encodeU29(uint8_t* r, uint32_t n) {
	if (n >= 0x20000000) {
		throw out_of_range("The largest unsigned integer value that can be represented is 2^29 - 1.");
	}

	if (n >= 0x200000) { // 4 bytes
		*r++ = static_cast<uint8_t>(n >> 22) | 0x80;
	} else { // less than 4 bytes
		//put a 0 bit at the beginning of the last byte
		n = (n & 0x7F) | ((n & 0xFFFFFF80) << 1);
	}
	if (n >= 0x8000) {
		*r++ = static_cast<uint8_t>(n >> 15) | 0x80;
	}
	if (n >= 0x100) {
		*r++ = static_cast<uint8_t>(n >> 8) | 0x80;
	}

	*r++ = static_cast<uint8_t>(n);

	return r;
}

/* Another solution
uint8_t* AMF3::encodeU29(uint8_t* r, uint32_t n) {
	if (n >= 0x20000000) {
		throw out_of_range("The largest unsigned integer value that can be represented is 2^29 - 1.");
	}

	if (n <= 0x7f) {
		*r++ = static_cast<uint8_t>(n);
	} else if (n <= 0x3fff) {
		*r++ = static_cast<uint8_t>((n >> 7) | 0x80);
		*r++ = static_cast<uint8_t>(n & 0x7f);
	} else if (n <= 0x1fffff) {
		*r++ = static_cast<uint8_t>((n >> 14) | 0x80);
		*r++ = static_cast<uint8_t>((n >> 7) | 0x80);
		*r++ = static_cast<uint8_t>(n & 0x7f);
	} else {
		*r++ = static_cast<uint8_t>((n >> 22) | 0x80);
		*r++ = static_cast<uint8_t>((n >> 15) | 0x80);
		*r++ = static_cast<uint8_t>((n >> 8) | 0x80);
		*r++ = static_cast<uint8_t>(n);
	}

	return r;
}*/

// ByteArray has last bit set to 1 in case it is a value.
uint8_t* AMF3::encodeBALength(uint8_t* r, uint32_t n) {
	return AMF3::encodeU29(r, (n << 1) | 1);
}


/*
void AMF3::amf3TypeToJSON(json &j, const AMF3_TYPE &type) {

	if (type.type == AMF3::UNDEFINED_MARKER) {
		j = "AMF3_UNDEFINED";
	}
	else if (type.type == AMF3::NULL_MARKER) {
		j = nullptr;
	}
	else if (type.type == AMF3::FALSE_MARKER) {
		j = false;
	}
	else if (type.type == AMF3::TRUE_MARKER) {
		j = true;
	}
	else if (type.type == AMF3::INTEGER_MARKER) {
		j = type.u29;
	}
	else if (type.type == AMF3::DOUBLE_MARKER) {
		// NaN can have many different representations, and
		// infinite can have 2 representations (positive and negative),
		// so we store an array of bytes for them to keep their
		// exact representation.
		if(isfinite(type.d)) {
			j = type.d;
		} else {
			json j2;
			j2.emplace_back("AMF3_DOUBLE_NAN");
			auto arr = AMF0::writeDouble(type.d);
			for (size_t i = 0; i < 8; ++i) {
				j2.emplace_back(arr[i]);
			}
			j = j2;
		}
	}
	else if (type.type == AMF3::STRING_MARKER) {
		if (type.isReference) {
			json j2;
			j2.emplace("AMF3_STRING_REF", type.u29);
			j = j2;
		} else {
			j = type.s;
		}
	}
	else if (type.type == AMF3::ARRAY_MARKER) {
		if (type.isReference) {
			json j2;
			j2.emplace("AMF3_ARRAY_REFERENCE", type.u29);
			j = j2;
		} else {
			if (!type.jsonObj.empty()) {
				json j2;
				j2.emplace("AMF3_ASSOCIATIVE_ARRAY", type.jsonObj);
				j = j2;
			}
			if(!type.amf3Array.empty()) {
				json j2;
				j2.emplace("AMF3_DENSE_ARRAY", type.u29);
				for (size_t i = 0; i < type.amf3Array.size(); ++i) {
					this->amf3TypeToJSON(j2[to_string(i)], type.amf3Array[i]);
				}
				j = j2;
			}
		}
	}
	else if (type.type == AMF3::OBJECT_MARKER) {

		if (type.isReference) {
			json j2;
			j2.emplace("AMF3_OBJECT_REFERENCE", type.u29);
			j = j2;
		} else if (type.isTraitRef) {
			json j2;
			j2.emplace("AMF3_TRAITS_REF", type.u29);
			j = j2;
		} else if (type.isTraitExt) {
			throw swf_exception("AMF3 Object traits ext not implemented because it is program dependent.");
		} else {
			j = type.jsonObj;
			if (type.isDynamic) {
				j["AMF3_OBJECT_DYNAMIC"] = "";
			}
		}
	}
	else {
		std::stringstream stream;
		stream << std::hex << setw(2) << setfill('0') << static_cast<int>(type.type);
		throw swf_exception("Type '0x" + stream.str() + "' not valid or not implemented in JSON.");
	}
}
*/
