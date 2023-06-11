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
#include <stdexcept> // out_of_range
#include <cassert> // assert

using namespace std;
using namespace swf;

AMF3::AMF3(const uint8_t* buffer, size_t &pos) : object(), stringRefs(),
			objTraitsRefs(), objRefs() {

	this->object = parseAmf3(buffer, pos);
}


amf3type_sptr AMF3::parseAmf3(const uint8_t* buffer, size_t &pos) {

	/**
	 * Section 3.2, 3.3, 3.4, 3.5 of amf3-file-format-spec.pdf
	 */
	if (buffer[pos] == AMF3::UNDEFINED_MARKER ||
	    buffer[pos] == AMF3::NULL_MARKER ||
	    buffer[pos] == AMF3::FALSE_MARKER ||
	    buffer[pos] == AMF3::TRUE_MARKER) {

		auto obj = make_shared<AMF3_TYPE>();
		obj->type = buffer[pos];

		++pos;
		return obj;
	}
	/**
	 * Section 3.6 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::INTEGER_MARKER) {

		auto obj = make_shared<AMF3_INTEGER>();
		obj->type = buffer[pos];

		++pos;

		obj->i = u32Toi29(AMF3::decodeU29(buffer, pos));

		return obj;
	}
	/**
	 * Section 3.7 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::DOUBLE_MARKER) {

		auto obj = make_shared<AMF3_DOUBLE>();
		obj->type = buffer[pos];

		++pos;

		obj->d = AMF0::readDouble(buffer, pos);

		return obj;
	}
	/**
	 * Section 3.8 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::STRING_MARKER) {

		auto obj = make_shared<AMF3_STRING>();
		obj->type = buffer[pos];

		++pos;

		obj->s = AMF3::readString(buffer, pos);

		return obj;
	}
	/**
	 * Section 3.11 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::ARRAY_MARKER) {

		auto obj = make_shared<AMF3_ARRAY>();
		obj->type = buffer[pos];

		++pos;

		uint32_t u29 = decodeU29(buffer, pos); // count of the dense portion or reference index
		bool isRef = !(u29 & 0x1);
		u29 >>= 1;

		if (isRef) {
			return objRefs[u29];
		}

		obj->denseValues.reserve(u29); // count of dense portion

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
			auto key = AMF3::readString(buffer, pos);

			if (!key->empty()) {
				auto value = this->parseAmf3(buffer, pos);
				obj->associativeNameValues.emplace_back(key, move(value));
			} else {
				break;
			}
		}

		// This is the dense portion
		for (uint32_t i = 0; i < u29; i++) {
			obj->denseValues.emplace_back(this->parseAmf3(buffer, pos));
		}

		// Add to reference table
		objRefs.emplace_back(obj);

		return obj;
	}
	/**
	 * Section 3.12 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::OBJECT_MARKER) {

		auto obj = make_shared<AMF3_OBJECT>();
		obj->type = buffer[pos];

		++pos;

		// object reference, trait reference or number of sealed
		// traits member names that follow after the class name (an integer)
		uint32_t u29 = decodeU29(buffer, pos);

		bool isRef = !(u29 & 0x1);
		u29 >>= 1;

		if (isRef) {
			return objRefs[u29];
		}

		bool isTraitRef = !(u29 & 0x1);
		u29 >>= 1;

		if (isTraitRef) {
			obj->trait = objTraitsRefs[u29];
		} else {
			bool isTraitExt = u29 & 0x1;
			u29 >>= 1;

			if (isTraitExt) {
				throw swf_exception("AMF3 Object traits ext not implemented because it is program dependent.");
			}

			auto trait = make_shared<AMF3_TRAIT>();

			trait->isDynamic = u29 & 0x1;
			u29 >>= 1; // number of sealed traits member names that follow after the class name

			trait->className = AMF3::readString(buffer, pos);

			for(uint32_t i = 0; i < u29; i++) {
				trait->memberNames.emplace_back(AMF3::readString(buffer, pos));
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
				auto key = AMF3::readString(buffer, pos);

				if (!key->empty()) {
					auto value = this->parseAmf3(buffer, pos);
					obj->dynamicNameValues.emplace_back(key, move(value));
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

		auto obj = make_shared<AMF3_BYTEARRAY>();
		obj->type = buffer[pos];

		++pos;

		uint32_t u29 = decodeU29(buffer, pos); // reference index or byte length
		bool isRef = !(u29 & 0x1);
		u29 >>= 1;

		if (isRef) {
			return objRefs[u29];
		}

		obj->binaryData = { buffer + pos, buffer + pos + u29 };
		pos += u29;

		// Add to reference table
		objRefs.emplace_back(obj);

		return obj;
	}
	else {
		stringstream stream;
		stream << hex << setw(2) << setfill('0') << static_cast<int>(buffer[pos]);
		throw swf_exception("Position: " + to_string(pos) + ". Marker '0x" +
		                    stream.str() + "' not valid or not implemented.");
	}
}

string_sptr AMF3::readString(const uint8_t* buffer, size_t &pos) {
	uint32_t u29 = decodeU29(buffer, pos); // reference index or string length
	bool isRef = !(u29 & 0x1);
	u29 >>= 1;

	if (isRef) {
		return stringRefs[u29];
	}

	if (u29 == 0) {
		return make_shared<string>(""); // empty string is never sent by reference
	} else {
		auto s = make_shared<string>(buffer + pos, buffer + pos + u29);
		pos += u29;
		stringRefs.emplace_back(s);
		return s;
	}

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

json AMF3::to_json(amf3type_sptr & type) {

	json j;

	if (type->type == AMF3::UNDEFINED_MARKER) {
		j = "AMF3_UNDEFINED";
	}
	else if (type->type == AMF3::NULL_MARKER) {
		j = nullptr;
	}
	else if (type->type == AMF3::FALSE_MARKER) {
		j = false;
	}
	else if (type->type == AMF3::TRUE_MARKER) {
		j = true;
	}
	else if (type->type == AMF3::INTEGER_MARKER) {
		auto i = static_cast<AMF3_INTEGER *>(type.get());
		j = i->i;
	}
	else if (type->type == AMF3::DOUBLE_MARKER) {
		auto d = static_cast<AMF3_DOUBLE *>(type.get());
		// NaN can have many different representations, and
		// infinite can have 2 representations (positive and negative),
		// so we store an array of bytes for them to keep their
		// exact representation.
		if(isfinite(d->d)) {
			j = d->d;
		} else {
			json j2;
			j2.emplace_back("AMF3_DOUBLE_NAN");
			auto arr = AMF0::writeDouble(d->d);
			for (size_t i = 0; i < 8; ++i) {
				j2.emplace_back(arr[i]);
			}
			j = j2;
		}
	}
	else if (type->type == AMF3::STRING_MARKER) {
		auto s = static_cast<AMF3_STRING *>(type.get());
		j = *(s->s);
	}
	else if (type->type == AMF3::ARRAY_MARKER) {
		auto arr = static_cast<AMF3_ARRAY *>(type.get());

		if (!arr->associativeNameValues.empty()) {
			json j1;
			for (auto& a : arr->associativeNameValues) {
				json obj = this->to_json(a.second);
				j1.emplace(*(a.first), obj);
			}
			j["AMF3_ARRAY_ASSOCIATIVE"] = j1;
		}
		if(!arr->denseValues.empty()) {
			for (auto& a : arr->denseValues) {
				json obj = this->to_json(a);
				j.emplace_back(obj);
			}
		}
	}
	else if (type->type == AMF3::OBJECT_MARKER) {
		auto obj = static_cast<AMF3_OBJECT *>(type.get());

		json j1;

		if(!obj->sealedValues.empty()) {

			for (size_t i = 0; i < obj->sealedValues.size(); ++i) {
				json obj2 = this->to_json(obj->sealedValues[i]);
				j1.emplace(*(obj->trait->memberNames[i]), obj2);
			}
		}
		if (!obj->dynamicNameValues.empty()) {
			json j2;
			for (auto& a : obj->dynamicNameValues) {
				json obj2 = this->to_json(a.second);
				j2.emplace(*(a.first), obj2);
			}
			j1["AMF3_OBJECT_DYNAMIC"] = j2;
		}

		j[*(obj->trait->className)] = j1;
	}
	else {
		stringstream stream;
		stream << hex << setw(2) << setfill('0') << static_cast<int>(type->type);
		throw swf_exception("Type '0x" + stream.str() + "' not implemented in JSON.");
	}

	return j;
}
/*
 * NOTE TO SELF: When serializing, must keep reference tables in mind!
amf3type_sptr AMF3::from_json(const json& j) {
	j.at("name").get_to(p.name);
	j.at("address").get_to(p.address);
	j.at("age").get_to(p.age);
}
*/

namespace swf {

	int32_t u32Toi29(uint32_t u) {
		assert(u < 0x20000000); // this should never happen, unless the program is tampered with
		return static_cast<std::int32_t>(u << 3) >> 3;
	}
}
