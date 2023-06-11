/**
 * libswf - AMF3 class
 *
 * Documentation in 'amf3-file-format-spec.pdf' file.
 */

#ifndef AMF3_HPP
#define AMF3_HPP

#include <string>
#include <vector>
#include <cstdint> // uint32_t...
#include <functional> // std::reference_wrapper
#include <memory> // std::shared_ptr, std::make_shared
#include <utility> // std::pair

#include <json.hpp>
#include <fifo_map.hpp> // https://github.com/nlohmann/fifo_map/blob/master/src/fifo_map.hpp

#include "swf_utils.hpp" // DIAGNOSTIC_WARNING()

namespace swf {

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

	using string_sptr = std::shared_ptr<std::string>;

	class AMF3_TYPE {
	public:
		explicit inline AMF3_TYPE (uint8_t t) : type(t) {}
		uint8_t type; // U8 marker
	};

	using amf3type_sptr = std::shared_ptr<AMF3_TYPE>;

	bool operator==(const amf3type_sptr& lhs, const amf3type_sptr& rhs);

	class AMF3_TRAIT {
	public:
		explicit inline AMF3_TRAIT() : className(), isDynamic(false), memberNames() {}
		string_sptr className;
		bool isDynamic;
		std::vector<string_sptr> memberNames;
		bool operator==(const AMF3_TRAIT& rhs) const;
	};

	using amf3trait_sptr = std::shared_ptr<AMF3_TRAIT>;

	bool operator==(const amf3trait_sptr& lhs, const amf3trait_sptr& rhs);

	class AMF3 {
	public:

		/**
		 * AMF3 marker constants
		 * See section 3.1 of amf3-file-format-spec.pdf
		 */
		static constexpr uint8_t UNDEFINED_MARKER = 0x00;
		static constexpr uint8_t NULL_MARKER = 0x01;
		static constexpr uint8_t FALSE_MARKER = 0x02;
		static constexpr uint8_t TRUE_MARKER = 0x03;
		static constexpr uint8_t INTEGER_MARKER = 0x04;
		static constexpr uint8_t DOUBLE_MARKER = 0x05;
		static constexpr uint8_t STRING_MARKER = 0x06;
		static constexpr uint8_t XML_DOC_MARKER = 0x07;
		static constexpr uint8_t DATE_MARKER = 0x08;
		static constexpr uint8_t ARRAY_MARKER = 0x09;
		static constexpr uint8_t OBJECT_MARKER = 0x0A;
		static constexpr uint8_t XML_MARKER = 0x0B;
		static constexpr uint8_t BYTE_ARRAY_MARKER = 0x0C;
		static constexpr uint8_t VECTOR_INT_MARKER = 0x0D;
		static constexpr uint8_t VECTOR_UINT_MARKER = 0x0E;
		static constexpr uint8_t VECTOR_DOUBLE_MARKER = 0x0F;
		static constexpr uint8_t VECTOR_OBJECT_MARKER = 0x10;
		static constexpr uint8_t DICTIONARY_MARKER = 0x11;

		explicit AMF3(const uint8_t* buffer, size_t &pos);
		explicit inline AMF3(const amf3type_sptr& type) : object(type), stringRefs(),
			objTraitsRefs(), objRefs() { };
		explicit inline AMF3(const json& j) : object(), stringRefs(),
			objTraitsRefs(), objRefs() { this->object = AMF3::from_json(j); };

		/// Increases pos by the number of bytes it takes to encode this U29.
		static uint32_t decodeU29(const uint8_t* buffer, size_t &pos);
		static uint8_t* encodeU29(uint8_t* r, uint32_t n);

		static uint8_t* encodeBALength(uint8_t* r, uint32_t n);

		/// Increases pos by string length.
		string_sptr decodeString(const uint8_t* buffer, size_t &pos);
		std::vector<uint8_t> encodeString(const std::string& s);

		template <typename T> static std::vector<uint8_t> U29BAToVector(T n) {
			std::vector<uint8_t> vec;
			uint8_t a[4];
			vec.insert(vec.end(), std::begin(a), AMF3::encodeBALength(a, static_cast<uint32_t>(n)));
			return vec;
		}

		static json to_json(amf3type_sptr &);
		static amf3type_sptr from_json(const json& j);

		inline std::string to_json_str(const int indent=4) {
			json j = AMF3::to_json(this->object);
			return j.dump(indent);
		}

		inline std::vector<uint8_t> serialize() { return this->serialize(this->object); };

		/// Increases pos by the number of bytes read
		amf3type_sptr deserialize(const uint8_t* buffer, size_t &pos);
		std::vector<uint8_t> serialize(const amf3type_sptr&);

		amf3type_sptr object;

	private:

		/**
		 * AMF3 Reference Tables
		 * See section 2.2 of amf3-file-format-spec.pdf
		 */
		std::vector<string_sptr> stringRefs;
		std::vector<amf3trait_sptr> objTraitsRefs;
		std::vector<amf3type_sptr> objRefs;
	};

	class AMF3_INTEGER : public AMF3_TYPE {
	public:
		explicit inline AMF3_INTEGER () : AMF3_TYPE(AMF3::INTEGER_MARKER), i(0) {}
		explicit inline AMF3_INTEGER (int32_t _i) : AMF3_TYPE(AMF3::INTEGER_MARKER), i(_i) {}
		/**
		 * See section 3.6 of amf3-file-format-spec.pdf
		*/
		int32_t i;
		bool operator==(const AMF3_INTEGER& rhs) const {
			return this->i == rhs.i;
		}
	};

	class AMF3_DOUBLE : public AMF3_TYPE {
	public:
		explicit inline AMF3_DOUBLE () : AMF3_TYPE(AMF3::DOUBLE_MARKER), d(0) {}
		explicit inline AMF3_DOUBLE (double _d) : AMF3_TYPE(AMF3::DOUBLE_MARKER), d(_d) {}
		/**
		 * See section 3.7 of amf3-file-format-spec.pdf
		*/
		double d;
		bool operator==(const AMF3_DOUBLE& rhs) const {
			DIAGNOSTIC_PUSH()
			DIAGNOSTIC_IGNORE("-Wfloat-equal")
			return this->d == rhs.d;
			DIAGNOSTIC_POP()
		}
	};

	class AMF3_STRING : public AMF3_TYPE {
	public:
		explicit inline AMF3_STRING () : AMF3_TYPE(AMF3::STRING_MARKER), s() {}
		explicit inline AMF3_STRING (std::string _s) : AMF3_TYPE(AMF3::STRING_MARKER), s() {
			this->s = std::make_shared<std::string>(_s);
		}
		/**
		 * See section 3.8 of amf3-file-format-spec.pdf
		*/
		string_sptr s;
		bool operator==(const AMF3_STRING& rhs) const {
			return *(this->s) == *(rhs.s);
		}
	};

	class AMF3_ARRAY : public AMF3_TYPE {
	public:
		explicit inline AMF3_ARRAY () : AMF3_TYPE(AMF3::ARRAY_MARKER),
			associativeNameValues(), denseValues() {}
		/**
		 * See section 3.11 of amf3-file-format-spec.pdf
		*/
		std::vector<std::pair <string_sptr, amf3type_sptr>> associativeNameValues;
		std::vector<amf3type_sptr> denseValues;
		bool operator==(const AMF3_ARRAY& rhs) const;
	};

	class AMF3_OBJECT : public AMF3_TYPE {
	public:
		explicit inline AMF3_OBJECT () : AMF3_TYPE(AMF3::OBJECT_MARKER), trait(),
			dynamicNameValues(), sealedValues() {}
		/**
		 * See section 3.11 of amf3-file-format-spec.pdf
		*/
		amf3trait_sptr trait;
		std::vector<std::pair <string_sptr, amf3type_sptr>> dynamicNameValues;
		std::vector<amf3type_sptr> sealedValues;
		bool operator==(const AMF3_OBJECT& rhs) const;
	};

	class AMF3_BYTEARRAY : public AMF3_TYPE {
	public:
		explicit inline AMF3_BYTEARRAY () : AMF3_TYPE(AMF3::BYTE_ARRAY_MARKER), binaryData() {}
		/**
		 * See section 3.14 of amf3-file-format-spec.pdf
		*/
		std::vector<uint8_t> binaryData;
		bool operator==(const AMF3_BYTEARRAY& rhs) const {
			return this->binaryData == rhs.binaryData;
		}
	};


	int u32Toi29(uint32_t u);
	uint32_t i32Tou29(int32_t i);

} // swf

#endif // AMF3_HPP
