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

using namespace std;
using namespace swf;

AMF0::AMF0(const uint8_t* buffer, size_t &pos) : object() {
	this->object = deserialize(buffer, pos);
}

amf0type_sptr AMF0::deserialize(const uint8_t* buffer, size_t &pos) {

	/**
	 * Section 2.7, 2.8 of amf0-file-format-specification.pdf
	 */
	if (buffer[pos] == AMF0::UNDEFINED_MARKER ||
	    buffer[pos] == AMF0::NULL_MARKER ||
	    buffer[pos] == AMF0::OBJECT_END_MARKER) {

		auto obj = make_shared<AMF0_TYPE>(buffer[pos]);
		++pos;
		return obj;
	}
	/**
	 * See section 2.2 of amf0-file-format-specification.pdf
	 */
	else if (buffer[pos] == AMF0::NUMBER_MARKER) {

		auto obj = make_shared<AMF0_NUMBER>();
		++pos;
		obj->d = AMF0::readDouble(buffer, pos);
		return obj;
	}
	/**
	 * See section 2.3 of amf0-file-format-specification.pdf
	 */
	else if (buffer[pos] == AMF0::BOOLEAN_MARKER) {

		auto obj = make_shared<AMF0_BOOLEAN>();
		++pos;
		obj->b = buffer[pos++];
		return obj;
	}
	/**
	 * See section 2.4 of amf0-file-format-specification.pdf
	 */
	else if (buffer[pos] == AMF0::STRING_MARKER) {

		auto obj = make_shared<AMF0_STRING>();
		++pos;
		obj->s = decodeString(buffer, pos);
		return obj;
	}
	/**
	 * See section 2.5 of amf0-file-format-specification.pdf
	 */
	else if (buffer[pos] == AMF0::OBJECT_MARKER) {

		auto obj = make_shared<AMF0_OBJECT>();
		++pos;

		string key = decodeString(buffer, pos);
		amf0type_sptr value = this->deserialize(buffer, pos);

		while (!(key.empty() && value->type == AMF0::OBJECT_END_MARKER)) {
			obj->keyValuePairs.emplace_back(key, value);
			key = decodeString(buffer, pos);
			value = this->deserialize(buffer, pos);
		}

		return obj;
	}
	/**
	 * See section 2.9 of amf0-file-format-specification.pdf
	 */
	else if (buffer[pos] == AMF0::REFERENCE_MARKER) {

		auto obj = make_shared<AMF0_REFERENCE>();
		++pos;
		obj->index = readU16(buffer, pos);
		return obj;
	}
	/**
	 * See section 2.10 of amf0-file-format-specification.pdf
	 */
	else if (buffer[pos] == AMF0::ECMA_ARRAY_MARKER) {

		auto obj = make_shared<AMF0_ECMA_ARRAY>();
		++pos;

		obj->associativeCount = readU32(buffer, pos);

		string key = decodeString(buffer, pos);
		amf0type_sptr value = this->deserialize(buffer, pos);

		while (!(key.empty() && value->type == AMF0::OBJECT_END_MARKER)) {
			obj->keyValuePairs.emplace_back(key, value);
			key = decodeString(buffer, pos);
			value = this->deserialize(buffer, pos);
		}

		return obj;
	}
	/**
	 * See section 2.12 of amf0-file-format-specification.pdf
	 */
	else if (buffer[pos] == AMF0::STRICT_ARRAY_MARKER) {

		auto obj = make_shared<AMF0_STRICT_ARRAY>();
		++pos;

		obj->arrayCount = readU32(buffer, pos);
		for (uint32_t i = 0; i < obj->arrayCount; ++i) {
			obj->values.emplace_back(this->deserialize(buffer, pos));
		}

		return obj;
	}
	/**
	 * See section 2.18 of amf0-file-format-specification.pdf
	 */
	else if (buffer[pos] == AMF0::TYPED_OBJECT_MARKER) {

		auto obj = make_shared<AMF0_TYPED_OBJECT>();
		++pos;

		obj->className = decodeString(buffer, pos);

		string key = decodeString(buffer, pos);
		amf0type_sptr value = this->deserialize(buffer, pos);

		while (!(key.empty() && value->type == AMF0::OBJECT_END_MARKER)) {
			obj->keyValuePairs.emplace_back(key, value);
			key = decodeString(buffer, pos);
			value = this->deserialize(buffer, pos);
		}

		return obj;
	}
	else {
		stringstream stream;
		stream << hex << setw(2) << setfill('0') << static_cast<int>(buffer[pos]);
		throw swf_exception("Deserialize: Position: " + to_string(pos) + ". Marker '0x" +
		                    stream.str() + "' not valid or not implemented.");
	}

}

std::vector<uint8_t> AMF0::serialize(const amf0type_sptr & value) {

	std::vector<uint8_t> vec { value->type };

	if (value->type == AMF0::UNDEFINED_MARKER ||
	    value->type == AMF0::NULL_MARKER ||
	    value->type == AMF0::OBJECT_END_MARKER) {

		return vec;

	} else if (value->type == AMF0::NUMBER_MARKER) {

		auto valD = static_cast<AMF0_NUMBER *>(value.get());
		concatVectorWithContainer(vec, AMF0::writeDouble(valD->d));

	} else if (value->type == AMF0::BOOLEAN_MARKER) {

		auto valB = static_cast<AMF0_BOOLEAN *>(value.get());
		vec.emplace_back(static_cast<uint8_t>(valB->b));

	} else if (value->type == AMF0::STRING_MARKER) {

		auto valS = static_cast<AMF0_STRING *>(value.get());
		concatVectorWithContainer(vec, AMF0::encodeString(valS->s));

	} else if (value->type == AMF0::OBJECT_MARKER) {

		auto valO = static_cast<AMF0_OBJECT *>(value.get());

		for (auto &p : valO->keyValuePairs) {
			concatVectorWithContainer(vec, AMF0::encodeString(p.first));
			concatVectorWithContainer(vec, this->serialize(p.second));
		}

		concatVectorWithContainer(vec, AMF0::encodeString(""));
		vec.emplace_back(AMF0::OBJECT_END_MARKER);

	} else if (value->type == AMF0::REFERENCE_MARKER) {

		auto valR = static_cast<AMF0_REFERENCE *>(value.get());
		concatVectorWithContainer(vec, this->writeU16(valR->index));

	} else if (value->type == AMF0::ECMA_ARRAY_MARKER) {

		auto valO = static_cast<AMF0_ECMA_ARRAY *>(value.get());

		concatVectorWithContainer(vec, this->writeU32(valO->associativeCount));

		for (auto &p : valO->keyValuePairs) {
			concatVectorWithContainer(vec, AMF0::encodeString(p.first));
			concatVectorWithContainer(vec, this->serialize(p.second));
		}

		concatVectorWithContainer(vec, AMF0::encodeString(""));
		vec.emplace_back(AMF0::OBJECT_END_MARKER);

	} else if (value->type == AMF0::STRICT_ARRAY_MARKER) {

		auto valA = static_cast<AMF0_STRICT_ARRAY *>(value.get());

		concatVectorWithContainer(vec, this->writeU32(valA->arrayCount));

		for (auto &p : valA->values) {
			concatVectorWithContainer(vec, this->serialize(p));
		}

	} else if (value->type == AMF0::TYPED_OBJECT_MARKER) {

		auto valO = static_cast<AMF0_TYPED_OBJECT *>(value.get());

		concatVectorWithContainer(vec, AMF0::encodeString(valO->className));

		for (auto &p : valO->keyValuePairs) {
			concatVectorWithContainer(vec, AMF0::encodeString(p.first));
			concatVectorWithContainer(vec, this->serialize(p.second));
		}

		concatVectorWithContainer(vec, AMF0::encodeString(""));
		vec.emplace_back(AMF0::OBJECT_END_MARKER);

	} else {
		stringstream stream;
		stream << hex << setw(2) << setfill('0') << static_cast<int>(value->type);
		throw swf_exception("Serialize: Marker '0x" + stream.str() + "' not valid or not implemented.");
	}

	return vec;
}

json AMF0::to_json(const amf0type_sptr & type) {

	json j;

	if (type->type == AMF0::UNDEFINED_MARKER) {
		j = "__AMF0_UNDEFINED__";
	}
	else if (type->type == AMF0::NULL_MARKER) {
		j = nullptr;
	}
	else if (type->type == AMF0::BOOLEAN_MARKER) {
		auto b = static_cast<AMF0_BOOLEAN *>(type.get());
		j = b->b;
	}
	else if (type->type == AMF0::NUMBER_MARKER) {
		auto d = static_cast<AMF0_NUMBER *>(type.get());
		// NaN can have many different representations, and
		// infinite can have 2 representations (positive and negative),
		// so we store an array of bytes for them to keep their
		// exact representation.
		if(isfinite(d->d)) {
			j = d->d;
		} else {
			json j2;
			j2.emplace_back("__AMF0_DOUBLE_NAN__");
			auto arr = AMF0::writeDouble(d->d);
			for (size_t i = 0; i < 8; ++i) {
				j2.emplace_back(arr[i]);
			}
			j = j2;
		}
	}
	else if (type->type == AMF0::STRING_MARKER) {
		auto s = static_cast<AMF0_STRING *>(type.get());
		j = s->s;
	}
	else if (type->type == AMF0::ECMA_ARRAY_MARKER) {
		auto arr = static_cast<AMF0_ECMA_ARRAY *>(type.get());
		/**
		 * ECMA arrays work like objects except they are prefixed with an "associative-count".
		 * But sometimes this count is zero and the array contains objects,
		 * I suspect, based on observation, that the count reflects the count of
		 * ordinal indices.
		 */
		j["__AMF0_ARRAY_ASSOCIATIVE_COUNT__"] = arr->associativeCount;
		for (auto& a : arr->keyValuePairs) {
			j.emplace(a.first, AMF0::to_json(a.second));
		}
	}
	else if (type->type == AMF0::STRICT_ARRAY_MARKER) {
		auto arr = static_cast<AMF0_STRICT_ARRAY *>(type.get());

		j = json::array(); // important for empty arrays

		/**
		 * I am assuming there is no need to store arr->arrayCount
		 * because it should be equal to the number of values in the array.
		 */
		if(!arr->values.empty()) {
			for (auto& a : arr->values) {
				j.emplace_back(AMF0::to_json(a));
			}
		}
	}
	else if (type->type == AMF0::REFERENCE_MARKER) {
		auto ref = static_cast<AMF0_REFERENCE *>(type.get());
		j.emplace("__AMF0_REFERENCE__", ref->index);
	}
	else if (type->type == AMF0::OBJECT_MARKER) {
		auto obj = static_cast<AMF0_OBJECT *>(type.get());
		for (auto& a : obj->keyValuePairs) {
			j.emplace(a.first, AMF0::to_json(a.second));
		}
	}
	else if (type->type == AMF0::TYPED_OBJECT_MARKER) {

		auto obj = static_cast<AMF0_TYPED_OBJECT *>(type.get());
		j.emplace("__AMF0_OBJECT_CLASSNAME__", obj->className);
		for (auto& a : obj->keyValuePairs) {
			j.emplace(a.first, AMF0::to_json(a.second));
		}
	}
	else {
		stringstream stream;
		stream << hex << setw(2) << setfill('0') << static_cast<int>(type->type);
		throw swf_exception("Type '0x" + stream.str() + "' not implemented in JSON.");
	}

	return j;
}

amf0type_sptr AMF0::from_json(const json& j) {

	if (j.is_null()) {
		return make_shared<AMF0_TYPE>(AMF0::NULL_MARKER);
	} else if (j.is_boolean()) {
		return make_shared<AMF0_BOOLEAN>(j.get<bool>());
	} else if (j.is_number()) {
		return make_shared<AMF0_NUMBER>(j.get<double>());
	} else if (j.is_string()) {
		string s = j.get<string>();
		if (s == "__AMF0_UNDEFINED__") {
			return make_shared<AMF0_TYPE>(AMF0::UNDEFINED_MARKER);
		} else {
			return make_shared<AMF0_STRING>(s);
		}
	} else if (j.is_array()) {
		if (j.size() == 9 && j[0] == "__AMF0_DOUBLE_NAN__") {
			array<uint8_t, 8> arr;
			for (size_t i = 0; i < 8; ++i) {
				if (j[i+1].is_number_integer()) {
					arr[i] = j[i+1];
				} else {
					throw swf_exception("Error reading non-finite double. Byte is not an integer.");
				}
			}
			double d = AMF0::readDouble(arr.data(), 0);
			return make_shared<AMF0_NUMBER>(d);
		}

		auto arr = make_shared<AMF0_STRICT_ARRAY>();
		for (auto el : j) {
			arr->values.emplace_back(AMF0::from_json(el));
		}
		return arr;

	} else if (j.is_object()) {

		if (j.contains("__AMF0_OBJECT_CLASSNAME__")) {
			auto obj = make_shared<AMF0_TYPED_OBJECT>();
			for (auto& [key, value] : j.items()) {
				if (key == "__AMF0_OBJECT_CLASSNAME__") {
					obj->className = value.get<string>();
				} else {
					obj->keyValuePairs.emplace_back(key, AMF0::from_json(value));
				}
			}
			return obj;
		} else if (j.contains("__AMF0_ARRAY_ASSOCIATIVE_COUNT__")) {
			auto obj = make_shared<AMF0_ECMA_ARRAY>();
			for (auto& [key, value] : j.items()) {
				if (key == "__AMF0_ARRAY_ASSOCIATIVE_COUNT__") {
					obj->associativeCount = value.get<int>();
				} else {
					obj->keyValuePairs.emplace_back(key, AMF0::from_json(value));
				}
			}
			return obj;
		} else if (j.contains("__AMF0_REFERENCE__")) {
			auto obj = make_shared<AMF0_REFERENCE>();
			obj->index = static_cast<uint16_t>(j.at("__AMF0_REFERENCE__").get<int>());
			return obj;
		} else {
			auto obj = make_shared<AMF0_OBJECT>();
			for (auto& [key, value] : j.items()) {
				obj->keyValuePairs.emplace_back(key, AMF0::from_json(value));
			}
			return obj;
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

uint16_t AMF0::readU16(const std::uint8_t* buffer, size_t &pos) {
	uint16_t u = bytestodec_be<uint16_t>(buffer + pos);
	pos += 2;
	return u;
}

array<uint8_t, 2> AMF0::writeU16(uint16_t u) {
	return dectobytes_be<uint16_t>(u);
}

uint32_t AMF0::readU32(const std::uint8_t* buffer, size_t &pos) {
	uint32_t u = bytestodec_be<uint32_t>(buffer + pos);
	pos += 4;
	return u;
}

array<uint8_t, 4> AMF0::writeU32(uint32_t u) {
	return dectobytes_be<uint32_t>(u);
}


string AMF0::decodeString(const std::uint8_t* buffer, size_t &pos) {
	uint16_t len = bytestodec_be<uint16_t>(buffer + pos);
	pos += 2;
	string s{buffer + pos, buffer + pos + len};
	pos += len;
	return s;
}

vector<uint8_t> AMF0::encodeString(const std::string& s) {
	vector<uint8_t> data;
	array<uint8_t, 2> len = dectobytes_be<uint16_t>(static_cast<uint16_t>(s.size()));
	concatVectorWithContainer(data, len);
	concatVectorWithContainer(data, s);
	return data;
}


// There is no need to compare two arrays or two objects.
// There can be deep copies, so two arrays having the
// same elements does not mean they are the same array object. Same for objects.
bool AMF0_ECMA_ARRAY::operator==(const AMF0_ECMA_ARRAY& rhs) const {
	return std::equal(this->keyValuePairs.begin(), this->keyValuePairs.end(),
	                  rhs.keyValuePairs.begin(), rhs.keyValuePairs.end(),
	                  [](auto& lhs, auto& rhs2) {
		if (lhs.first != rhs2.first) { return false; }
		if (lhs.second != rhs2.second) { return false; }
		return true;
	});
}
bool AMF0_STRICT_ARRAY::operator==(const AMF0_STRICT_ARRAY& rhs) const {
	return std::equal(this->values.begin(), this->values.end(),
	                  rhs.values.begin(), rhs.values.end(),
	                  [](auto& lhs, auto& rhs2) {
		if (lhs != rhs2) { return false; }
		return true;
	});
}
bool AMF0_OBJECT::operator==(const AMF0_OBJECT& rhs) const {
	return std::equal(this->keyValuePairs.begin(), this->keyValuePairs.end(),
	                  rhs.keyValuePairs.begin(), rhs.keyValuePairs.end(),
	                  [](auto& lhs, auto& rhs2) {
		if (lhs.first != rhs2.first) { return false; }
		if (lhs.second != rhs2.second) { return false; }
		return true;
	});
}
bool AMF0_TYPED_OBJECT::operator==(const AMF0_TYPED_OBJECT& rhs) const {
	if (this->className != rhs.className) {
		return false;
	}
	return std::equal(this->keyValuePairs.begin(), this->keyValuePairs.end(),
	                  rhs.keyValuePairs.begin(), rhs.keyValuePairs.end(),
	                  [](auto& lhs, auto& rhs2) {
		if (lhs.first != rhs2.first) { return false; }
		if (lhs.second != rhs2.second) { return false; }
		return true;
	});
}


namespace swf {

	bool operator==(const amf0type_sptr& lhs, const amf0type_sptr& rhs) {
		if (lhs->type != rhs->type) { return false; }
		if (lhs->type == AMF0::NUMBER_MARKER) {
			auto valLhs = static_cast<AMF0_NUMBER *>(lhs.get());
			auto valRhs = static_cast<AMF0_NUMBER *>(rhs.get());
			return *valLhs == *valRhs;
		} else if (lhs->type == AMF0::BOOLEAN_MARKER) {
			auto valLhs = static_cast<AMF0_BOOLEAN *>(lhs.get());
			auto valRhs = static_cast<AMF0_BOOLEAN *>(rhs.get());
			return *valLhs == *valRhs;
		} else if (lhs->type == AMF0::STRING_MARKER) {
			auto valLhs = static_cast<AMF0_STRING *>(lhs.get());
			auto valRhs = static_cast<AMF0_STRING *>(rhs.get());
			return *valLhs == *valRhs;
		} else if (lhs->type == AMF0::OBJECT_MARKER) {
			auto valLhs = static_cast<AMF0_OBJECT *>(lhs.get());
			auto valRhs = static_cast<AMF0_OBJECT *>(rhs.get());
			return *valLhs == *valRhs;
		} else if (lhs->type == AMF0::REFERENCE_MARKER) {
			auto valLhs = static_cast<AMF0_REFERENCE *>(lhs.get());
			auto valRhs = static_cast<AMF0_REFERENCE *>(rhs.get());
			return *valLhs == *valRhs;
		} else if (lhs->type == AMF0::ECMA_ARRAY_MARKER) {
			auto valLhs = static_cast<AMF0_ECMA_ARRAY *>(lhs.get());
			auto valRhs = static_cast<AMF0_ECMA_ARRAY *>(rhs.get());
			return *valLhs == *valRhs;
		} else if (lhs->type == AMF0::STRICT_ARRAY_MARKER) {
			auto valLhs = static_cast<AMF0_STRICT_ARRAY *>(lhs.get());
			auto valRhs = static_cast<AMF0_STRICT_ARRAY *>(rhs.get());
			return *valLhs == *valRhs;
		} else if (lhs->type == AMF0::TYPED_OBJECT_MARKER) {
			auto valLhs = static_cast<AMF0_TYPED_OBJECT *>(lhs.get());
			auto valRhs = static_cast<AMF0_TYPED_OBJECT *>(rhs.get());
			return *valLhs == *valRhs;
		} else {
			std::stringstream stream;
			stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(lhs->type);
			throw swf_exception("Equal: Marker '0x" + stream.str() + "' not valid or not implemented.");
		}
	}

}
