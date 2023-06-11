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
#include <algorithm> // find_if

using namespace std;
using namespace swf;

AMF3::AMF3(const uint8_t* buffer, size_t &pos) : object(), stringRefs(),
			objTraitsRefs(), objRefs() {

	this->object = deserialize(buffer, pos);
}


amf3type_sptr AMF3::deserialize(const uint8_t* buffer, size_t &pos) {

	/**
	 * Section 3.2, 3.3, 3.4, 3.5 of amf3-file-format-spec.pdf
	 */
	if (buffer[pos] == AMF3::UNDEFINED_MARKER ||
	    buffer[pos] == AMF3::NULL_MARKER ||
	    buffer[pos] == AMF3::FALSE_MARKER ||
	    buffer[pos] == AMF3::TRUE_MARKER) {

		auto obj = make_shared<AMF3_TYPE>(buffer[pos]);
		++pos;
		return obj;
	}
	/**
	 * Section 3.6 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::INTEGER_MARKER) {

		auto obj = make_shared<AMF3_INTEGER>();

		++pos;

		obj->i = u32Toi29(AMF3::decodeU29(buffer, pos));

		return obj;
	}
	/**
	 * Section 3.7 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::DOUBLE_MARKER) {

		auto obj = make_shared<AMF3_DOUBLE>();

		++pos;

		obj->d = AMF0::readDouble(buffer, pos);

		return obj;
	}
	/**
	 * Section 3.8 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::STRING_MARKER) {

		auto obj = make_shared<AMF3_STRING>();

		++pos;

		obj->s = AMF3::decodeString(buffer, pos);

		return obj;
	}
	/**
	 * Section 3.11 of amf3-file-format-spec.pdf
	 */
	else if (buffer[pos] == AMF3::ARRAY_MARKER) {

		auto obj = make_shared<AMF3_ARRAY>();

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
			auto key = AMF3::decodeString(buffer, pos);

			if (!key->empty()) {
				obj->associativeNameValues.emplace_back(key, this->deserialize(buffer, pos));
			} else {
				break;
			}
		}

		// This is the dense portion
		for (uint32_t i = 0; i < u29; i++) {
			obj->denseValues.emplace_back(this->deserialize(buffer, pos));
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

			trait->className = AMF3::decodeString(buffer, pos);

			for(uint32_t i = 0; i < u29; i++) {
				trait->memberNames.emplace_back(AMF3::decodeString(buffer, pos));
			}

			obj->trait = trait;
			objTraitsRefs.emplace_back(trait);
		}


		// Read the sealed member values
		for(uint32_t i = 0; i < obj->trait->memberNames.size(); i++) {
			obj->sealedValues.emplace_back(this->deserialize(buffer, pos));
		}

		if (obj->trait->isDynamic) {
			// If the dynamic flag is set, dynamic members may follow
			// the sealed members. Read key/value pairs until we
			// encounter an empty string key signifying the end of the
			// dynamic members.
			while (true) {
				auto key = AMF3::decodeString(buffer, pos);

				if (!key->empty()) {
					obj->dynamicNameValues.emplace_back(key, this->deserialize(buffer, pos));
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
		throw swf_exception("Deserialize: Position: " + to_string(pos) + ". Marker '0x" +
		                    stream.str() + "' not valid or not implemented.");
	}
}

/*
 * NOTE TO SELF: When serializing, must keep reference tables in mind!
 */
std::vector<uint8_t> AMF3::serialize(const amf3type_sptr & value) {

	std::vector<uint8_t> vec { value->type };

	if (value->type == AMF3::UNDEFINED_MARKER ||
	    value->type == AMF3::NULL_MARKER ||
	    value->type == AMF3::FALSE_MARKER ||
	    value->type == AMF3::TRUE_MARKER) {

		return vec;

	} else if (value->type == AMF3::INTEGER_MARKER) {

		auto valI = static_cast<AMF3_INTEGER *>(value.get());
		uint8_t a[4];
		vec.insert(vec.end(), std::begin(a), AMF3::encodeU29(a, static_cast<uint32_t>(i32Tou29(valI->i))));

	} else if (value->type == AMF3::DOUBLE_MARKER) {

		auto valD = static_cast<AMF3_DOUBLE *>(value.get());
		concatVectorWithContainer(vec, AMF0::writeDouble(valD->d));

	} else if (value->type == AMF3::STRING_MARKER) {

		auto valS = static_cast<AMF3_STRING *>(value.get());
		concatVectorWithContainer(vec, this->encodeString(*(valS->s)));

	} else if (value->type == AMF3::ARRAY_MARKER) {

		auto valA = static_cast<AMF3_ARRAY *>(value.get());
		uint8_t a[4];

		auto it = find(objRefs.begin(), objRefs.end(), value);

		// if array has been used, make a reference for it
		if (it != objRefs.end()) {
			size_t index = it - objRefs.begin();
			vec.insert(vec.end(), std::begin(a), AMF3::encodeU29(a, static_cast<uint32_t>(index << 1)));
		} else {
			size_t u29 = (valA->denseValues.size() << 1) | 1;
			vec.insert(vec.end(), std::begin(a), AMF3::encodeU29(a, static_cast<uint32_t>(u29)));

			for (auto &p : valA->associativeNameValues) {
				concatVectorWithContainer(vec, this->encodeString(*(p.first)));
				concatVectorWithContainer(vec, this->serialize(p.second));
			}
			vec.emplace_back(0x01);

			for (auto &amf3 : valA->denseValues) {
				concatVectorWithContainer(vec, this->serialize(amf3));
			}
			// Empty arrays are not sent by reference. I don't know why but while debugging
			// Hero Fighter Spt files (namely the 809 - Drew one) this was the case.
			if (!(valA->denseValues.empty() && valA->associativeNameValues.empty())) {
				objRefs.emplace_back(value);
			}
		}

	} else if (value->type == AMF3::OBJECT_MARKER) {

		auto valO = static_cast<AMF3_OBJECT *>(value.get());
		uint8_t a[4];

		auto it = find(objRefs.begin(), objRefs.end(), value);

		// if object has been used, make a reference for it
		if (it != objRefs.end()) {
			size_t index = it - objRefs.begin();
			vec.insert(vec.end(), std::begin(a), AMF3::encodeU29(a, static_cast<uint32_t>(index << 1)));

		} else {

			auto it2 = find(objTraitsRefs.begin(), objTraitsRefs.end(), valO->trait);

			if (it2 != objTraitsRefs.end()) {

				size_t u29 = ((it2 - objTraitsRefs.begin()) << 2) | 1;
				vec.insert(vec.end(), std::begin(a), AMF3::encodeU29(a, static_cast<uint32_t>(u29)));

			} else {

				size_t u29 = (valO->trait->memberNames.size() << 4) | (valO->trait->isDynamic ? 0xB : 0x3);
				vec.insert(vec.end(), std::begin(a), AMF3::encodeU29(a, static_cast<uint32_t>(u29)));
				concatVectorWithContainer(vec, this->encodeString(*valO->trait->className));
				for (auto &s : valO->trait->memberNames) {
					concatVectorWithContainer(vec, this->encodeString(*s));
				}
				objTraitsRefs.emplace_back(valO->trait);
			}

			for (auto &amf3 : valO->sealedValues) {
				concatVectorWithContainer(vec, this->serialize(amf3));
			}

			if (valO->trait->isDynamic) {
				for (auto &p : valO->dynamicNameValues) {
					concatVectorWithContainer(vec, this->encodeString(*(p.first)));
					concatVectorWithContainer(vec, this->serialize(p.second));
				}
				vec.emplace_back(0x01);
			}

			objRefs.emplace_back(value);
		}

	} else if (value->type == AMF3::BYTE_ARRAY_MARKER) {

		auto valBA = static_cast<AMF3_BYTEARRAY *>(value.get());
		uint8_t a[4];

		auto it = find(objRefs.begin(), objRefs.end(), value);
		// if byte array has been used, make a reference for it
		if (it != objRefs.end()) {
			size_t index = it - objRefs.begin();
			vec.insert(vec.end(), std::begin(a), AMF3::encodeU29(a, static_cast<uint32_t>(index << 1)));
		} else {
			size_t u29 = (valBA->binaryData.size() << 1) | 1;
			vec.insert(vec.end(), std::begin(a), AMF3::encodeU29(a, static_cast<uint32_t>(u29)));
			concatVectorWithContainer(vec, valBA->binaryData);
			objRefs.emplace_back(value);
		}
	} else {
		stringstream stream;
		stream << hex << setw(2) << setfill('0') << static_cast<int>(value->type);
		throw swf_exception("Serialize: Marker '0x" + stream.str() + "' not valid or not implemented.");
	}

	return vec;
}

string_sptr AMF3::decodeString(const uint8_t* buffer, size_t &pos) {
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

std::vector<uint8_t> AMF3::encodeString(const string& s) {

	if (s.empty()) {
		return { 0x01 };
	}

	std::vector<uint8_t> vec;
	uint8_t a[4];
	auto it = find_if(stringRefs.begin(), stringRefs.end(), [s](const string_sptr& s_ptr) {
		return *s_ptr == s;
	});
	// if string has been used, make a reference for it
	if (it != stringRefs.end()) {
		size_t index = it - stringRefs.begin();
		vec.insert(vec.end(), std::begin(a), AMF3::encodeU29(a, static_cast<uint32_t>(index << 1)));
	} else {
		size_t u29 = (s.size() << 1) | 1;
		vec.insert(vec.end(), std::begin(a), AMF3::encodeU29(a, static_cast<uint32_t>(u29)));
		vec.insert(vec.end(), s.begin(), s.end());
		stringRefs.emplace_back(make_shared<string>(s));
	}
	return vec;
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
		throw out_of_range("The largest unsigned integer value that can be represented is 2^29 - 1. Your number is: " + to_string(n));
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
		j = "__AMF3_UNDEFINED__";
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
			j2.emplace_back("__AMF3_DOUBLE_NAN__");
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

		j = json::array(); // important for empty arrays

		if (!arr->associativeNameValues.empty()) {
			json j1;
			j1["__AMF3_ARRAY_ASSOCIATIVE__"] = nullptr;
			for (auto& a : arr->associativeNameValues) {
				j1.emplace(*(a.first), AMF3::to_json(a.second));
			}
			j.emplace_back(j1);
		}
		if(!arr->denseValues.empty()) {
			for (auto& a : arr->denseValues) {
				j.emplace_back(AMF3::to_json(a));
			}
		}
	}
	else if (type->type == AMF3::OBJECT_MARKER) {
		auto obj = static_cast<AMF3_OBJECT *>(type.get());

		if(!obj->sealedValues.empty()) {

			for (size_t i = 0; i < obj->sealedValues.size(); ++i) {
				json obj2 = AMF3::to_json(obj->sealedValues[i]);
				j.emplace(*(obj->trait->memberNames[i]), obj2);
			}
		}
		if (!obj->dynamicNameValues.empty()) {
			json j1;
			for (auto& a : obj->dynamicNameValues) {
				j1.emplace(*(a.first), AMF3::to_json(a.second));
			}
			j["__AMF3_OBJECT_DYNAMIC__"] = j1;
		}

		j["__AMF3_OBJECT_CLASSNAME__"] = *(obj->trait->className);
		j["__AMF3_OBJECT_IS_DYNAMIC__"] = obj->trait->isDynamic;

	}
	else {
		stringstream stream;
		stream << hex << setw(2) << setfill('0') << static_cast<int>(type->type);
		throw swf_exception("Type '0x" + stream.str() + "' not implemented in JSON.");
	}

	return j;
}

amf3type_sptr AMF3::from_json(const json& j) {

	if (j.is_null()) {
		return make_shared<AMF3_TYPE>(AMF3::NULL_MARKER);
	} else if (j.is_boolean()) {
		return make_shared<AMF3_TYPE>(j.get<bool>() == true ? AMF3::TRUE_MARKER : AMF3::FALSE_MARKER);
	} else if (j.is_number_integer()) {
		return make_shared<AMF3_INTEGER>(j.get<int>());
	} else if (j.is_number_float()) {
		return make_shared<AMF3_DOUBLE>(j.get<double>());
	} else if (j.is_string()) {
		string s = j.get<string>();
		if (s == "__AMF3_UNDEFINED__") {
			return make_shared<AMF3_TYPE>(AMF3::UNDEFINED_MARKER);
		} else {
			return make_shared<AMF3_STRING>(s);
		}
	} else if (j.is_array()) {
		if (j.size() == 9 && j[0] == "__AMF3_DOUBLE_NAN__") {
			array<uint8_t, 8> arr;
			for (size_t i = 0; i < 8; ++i) {
				if (j[i+1].is_number_integer()) {
					arr[i] = j[i+1];
				} else {
					throw swf_exception("Error reading non-finite double. Byte is not an integer.");
				}
			}
			double d = AMF0::readDouble(arr.data(), 0);
			return make_shared<AMF3_DOUBLE>(d);
		}

		auto arr = make_shared<AMF3_ARRAY>();

		for (auto el : j) {
			if (el.is_object() && el.contains("__AMF3_ARRAY_ASSOCIATIVE__")) {
				for (auto& [key, value] : el.items()) {
					if (key != "__AMF3_ARRAY_ASSOCIATIVE__") {
						arr->associativeNameValues.emplace_back(make_shared<string>(key),
						                                        AMF3::from_json(value));
					}
				}
			} else {
				arr->denseValues.emplace_back(AMF3::from_json(el));
			}
		}

		return arr;
	} else if (j.is_object()) {

		auto trait = make_shared<AMF3_TRAIT>();
		auto obj = make_shared<AMF3_OBJECT>();
		obj->trait = trait;

		for (auto& [key, value] : j.items()) {
			if (key == "__AMF3_OBJECT_CLASSNAME__") {
				trait->className = make_shared<string>(value.get<string>());
			} else if (key == "__AMF3_OBJECT_IS_DYNAMIC__") {
				trait->isDynamic = value.get<bool>();
			} else if (key == "__AMF3_OBJECT_DYNAMIC__" && value.is_object()) {
				for (auto& [k, v] : value.items()) {
					obj->dynamicNameValues.emplace_back(make_shared<string>(k),
					                                    AMF3::from_json(v));
				}
			} else {
				trait->memberNames.emplace_back(make_shared<string>(key));
				obj->sealedValues.emplace_back(AMF3::from_json(value));
			}
		}

		return obj;
	} else {
		throw swf_exception("Unrecognized JSON type.");
	}

}

bool AMF3_ARRAY::operator==(const AMF3_ARRAY& rhs) const {
	bool assocEq = std::equal(this->associativeNameValues.begin(), this->associativeNameValues.end(),
	                          rhs.associativeNameValues.begin(), rhs.associativeNameValues.end(),
	                          [](auto& lhs, auto& rhs2) {
		if (*lhs.first != *rhs2.first) { return false; }
		if (lhs.second != rhs2.second) { return false; }
		return true;
	});
	bool denseEq = std::equal(this->denseValues.begin(), this->denseValues.end(),
	                          rhs.denseValues.begin(), rhs.denseValues.end(),
	                          [](auto& lhs, auto& rhs2) {
		if (lhs != rhs2) { return false; }
		return true;
	});
	return assocEq && denseEq;
}

bool AMF3_TRAIT::operator==(const AMF3_TRAIT& rhs) const {
	if (*this->className != *rhs.className) { return false; }
	if (this->isDynamic != rhs.isDynamic) { return false; }
	return std::equal(this->memberNames.begin(), this->memberNames.end(),
	                  rhs.memberNames.begin(), rhs.memberNames.end(),
	                  [](auto& lhs, auto& rhs2){ return *lhs == *rhs2; });
}

bool AMF3_OBJECT::operator==(const AMF3_OBJECT& rhs) const {
	if (this->trait != rhs.trait) { return false; }
	bool dynamEq = std::equal(this->dynamicNameValues.begin(), this->dynamicNameValues.end(),
	                          rhs.dynamicNameValues.begin(), rhs.dynamicNameValues.end(),
	                          [](auto& lhs, auto& rhs2) {
		if (*lhs.first != *rhs2.first) { return false; }
		if (lhs.second != rhs2.second) { return false; }
		return true;
	});
	bool sealedEq = std::equal(this->sealedValues.begin(), this->sealedValues.end(),
	                          rhs.sealedValues.begin(), rhs.sealedValues.end(),
	                          [](auto& lhs, auto& rhs2) {
		if (lhs != rhs2) { return false; }
		return true;
	});
	return dynamEq && sealedEq;
}

namespace swf {

	bool operator==(const amf3type_sptr& lhs, const amf3type_sptr& rhs) {
		if (lhs->type != rhs->type) { return false; }
		if (lhs->type == AMF3::INTEGER_MARKER) {
			auto valLhs = static_cast<AMF3_INTEGER *>(lhs.get());
			auto valRhs = static_cast<AMF3_INTEGER *>(rhs.get());
			return *valLhs == *valRhs;
		} else if (lhs->type == AMF3::DOUBLE_MARKER) {
			auto valLhs = static_cast<AMF3_DOUBLE *>(lhs.get());
			auto valRhs = static_cast<AMF3_DOUBLE *>(rhs.get());
			return *valLhs == *valRhs;
		} else if (lhs->type == AMF3::STRING_MARKER) {
			auto valLhs = static_cast<AMF3_STRING *>(lhs.get());
			auto valRhs = static_cast<AMF3_STRING *>(rhs.get());
			return *valLhs == *valRhs;
		} else if (lhs->type == AMF3::ARRAY_MARKER) {
			auto valLhs = static_cast<AMF3_ARRAY *>(lhs.get());
			auto valRhs = static_cast<AMF3_ARRAY *>(rhs.get());
			return *valLhs == *valRhs;
		} else if (lhs->type == AMF3::OBJECT_MARKER) {
			auto valLhs = static_cast<AMF3_OBJECT *>(lhs.get());
			auto valRhs = static_cast<AMF3_OBJECT *>(rhs.get());
			return *valLhs == *valRhs;
		} else if (lhs->type == AMF3::BYTE_ARRAY_MARKER) {
			auto valLhs = static_cast<AMF3_BYTEARRAY *>(lhs.get());
			auto valRhs = static_cast<AMF3_BYTEARRAY *>(rhs.get());
			return *valLhs == *valRhs;
		} else {
			std::stringstream stream;
			stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(lhs->type);
			throw swf_exception("Equal: Marker '0x" + stream.str() + "' not valid or not implemented.");
		}
	}

	bool operator==(const amf3trait_sptr& lhs, const amf3trait_sptr& rhs) {
		return *lhs == *rhs;
	}

	int32_t u32Toi29(uint32_t u) {
		assert(u < 0x20000000); // this should never happen, unless the program is tampered with
		return static_cast<std::int32_t>(u << 3) >> 3;
	}

	uint32_t i32Tou29(int32_t i) {
		return static_cast<std::uint32_t>(i << 3) >> 3;
	}
}
