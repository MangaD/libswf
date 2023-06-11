/**
 * libswf - AMF3 class
 *
 * Documentation in 'amf3-file-format-spec.pdf' file.
 */

#ifndef AMF3_HPP
#define AMF3_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <functional> // std::reference_wrapper
#include <memory> // std::unique_ptr, std::shared_ptr
#include <utility> // std::pair

#include <json.hpp>
#include <fifo_map.hpp> // https://github.com/nlohmann/fifo_map/blob/master/src/fifo_map.hpp

/**
 * https://github.com/nlohmann/json/issues/485#issuecomment-333652309
 * A workaround to give to use fifo_map as map, we are just ignoring the 'less' compare
 */
template<class K, class V, class dummy_compare, class A>
using my_workaround_fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;

using string_sptr = std::shared_ptr<std::string>;

namespace swf {

	class AMF3_TYPE {
	public:
		explicit inline AMF3_TYPE () : type(0) {}
		uint8_t type; // U8 marker
	};

	using amf3type_sptr = std::shared_ptr<AMF3_TYPE>;

	class AMF3_INTEGER : public AMF3_TYPE {
	public:
		explicit inline AMF3_INTEGER () : i(0) {}
		/**
		 * See section 3.6 of amf3-file-format-spec.pdf
		*/
		int32_t i;
	};

	class AMF3_DOUBLE : public AMF3_TYPE {
	public:
		explicit inline AMF3_DOUBLE () : d(0) {}
		/**
		 * See section 3.7 of amf3-file-format-spec.pdf
		*/
		double d;
	};

	class AMF3_STRING : public AMF3_TYPE {
	public:
		explicit inline AMF3_STRING () : s() {}
		/**
		 * See section 3.8 of amf3-file-format-spec.pdf
		*/
		string_sptr s;
	};

	class AMF3_ARRAY : public AMF3_TYPE {
	public:
		explicit inline AMF3_ARRAY () : associativeNameValues(), denseValues() {}
		/**
		 * See section 3.11 of amf3-file-format-spec.pdf
		*/
		std::vector<std::pair <string_sptr, amf3type_sptr>> associativeNameValues;
		std::vector<amf3type_sptr> denseValues;
	};

	class AMF3_TRAIT {
	public:
		explicit inline AMF3_TRAIT() : className(), isDynamic(false), memberNames() {}
		string_sptr className;
		bool isDynamic;
		std::vector<string_sptr> memberNames;
	};

	using amf3trait_sptr = std::shared_ptr<AMF3_TRAIT>;

	class AMF3_OBJECT : public AMF3_TYPE {
	public:
		explicit inline AMF3_OBJECT () : trait(),
			dynamicNameValues(), sealedValues() {}
		/**
		 * See section 3.11 of amf3-file-format-spec.pdf
		*/
		amf3trait_sptr trait;
		std::vector<std::pair <string_sptr, amf3type_sptr>> dynamicNameValues;
		std::vector<amf3type_sptr> sealedValues;
	};

	class AMF3_BYTEARRAY : public AMF3_TYPE {
	public:
		explicit inline AMF3_BYTEARRAY () : binaryData() {}
		/**
		 * See section 3.14 of amf3-file-format-spec.pdf
		*/
		std::vector<uint8_t> binaryData;
	};

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

		/// Increases pos by the number of bytes it takes to encode this U29.
		static uint32_t decodeU29(const uint8_t* buffer, size_t &pos);
		static uint8_t* encodeU29(uint8_t* r, uint32_t n);

		static uint8_t* encodeBALength(uint8_t* r, uint32_t n);

		/// Increases pos by string length.
		string_sptr readString(const uint8_t* buffer, size_t &pos);

		template <typename T> static std::vector<uint8_t> U29BAToVector(T n) {
			std::vector<uint8_t> vec;
			uint8_t a[4];
			vec.insert(vec.end(), std::begin(a), AMF3::encodeBALength(a, static_cast<uint32_t>(n)));
			return vec;
		}

/*
		inline std::string exportToJSON() {
			return this->object.jsonObj.dump(4);
		}
*/
		amf3type_sptr object;

	private:

		/// Increases pos by the number of bytes read
		amf3type_sptr parseAmf3(const uint8_t* buffer, size_t &pos);
		//void amf3TypeToJSON(nlohmann::basic_json<my_workaround_fifo_map> &, const AMF3_TYPE &);

		/**
		 * AMF3 Reference Tables
		 * See section 2.2 of amf3-file-format-spec.pdf
		 */
		std::vector<string_sptr> stringRefs;
		std::vector<amf3trait_sptr> objTraitsRefs;
		std::vector<amf3type_sptr> objRefs;
	};

} // swf

#endif // AMF3_HPP
