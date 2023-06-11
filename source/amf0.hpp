/**
 * libswf - AMF class
 */

#ifndef AMF0_HPP
#define AMF0_HPP

#include <string>
#include <vector>
#include <array>
#include <cstdint> // uint8_t
#include <json.hpp>
#include <fifo_map.hpp> // https://github.com/nlohmann/fifo_map/blob/master/src/fifo_map.hpp

#include "swf_utils.hpp" // DIAGNOSTIC_WARNING(), bit_cast

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

	class AMF0_TYPE {
	public:
		explicit inline AMF0_TYPE (std::uint8_t t) : type(t) {}
		std::uint8_t type; // U8 marker
	};

	using amf0type_sptr = std::shared_ptr<AMF0_TYPE>;

	bool operator==(const amf0type_sptr& lhs, const amf0type_sptr& rhs);

	class AMF0 {
	public:
		/**
		 * AMF0 marker constants
		 * See section 2.1 of amf0-file-format-specification.pdf
		 */
		static constexpr std::uint8_t NUMBER_MARKER = 0x00;
		static constexpr std::uint8_t BOOLEAN_MARKER = 0x01;
		static constexpr std::uint8_t STRING_MARKER = 0x02;
		static constexpr std::uint8_t OBJECT_MARKER = 0x03;
		static constexpr std::uint8_t MOVIECLIP_MARKER = 0x04;
		static constexpr std::uint8_t NULL_MARKER = 0x05;
		static constexpr std::uint8_t UNDEFINED_MARKER = 0x06;
		static constexpr std::uint8_t REFERENCE_MARKER = 0x07;
		static constexpr std::uint8_t ECMA_ARRAY_MARKER = 0x08;
		static constexpr std::uint8_t OBJECT_END_MARKER = 0x09;
		static constexpr std::uint8_t STRICT_ARRAY_MARKER = 0x0A;
		static constexpr std::uint8_t DATE_MARKER = 0x0B;
		static constexpr std::uint8_t LONG_STRING_MARKER = 0x0C;
		static constexpr std::uint8_t UNSUPPORTED_MARKER = 0x0D;
		static constexpr std::uint8_t RECORDSET_MARKER = 0x0E;
		static constexpr std::uint8_t XML_DOCUMENT_MARKER = 0x0F;
		static constexpr std::uint8_t TYPED_OBJECT_MARKER = 0x10;
		static constexpr std::uint8_t AVMPLUS_OBJECT_MARKER = 0x11;

		explicit AMF0(const std::uint8_t* buffer, size_t& pos);
		explicit AMF0(const std::uint8_t* buffer, size_t&& pos = 0) : AMF0(buffer,pos) {}
		explicit inline AMF0(const amf0type_sptr& type) : object(type) { };
		explicit inline AMF0(const json& j) : object() { this->object = AMF0::from_json(j); };

		static json to_json(const amf0type_sptr &);
		static amf0type_sptr from_json(const json& j);

		inline std::string to_json_str(const int indent=4) {
			json j = AMF0::to_json(this->object);
			return j.dump(indent);
		}

		/// Increases pos by string length.
		static std::string decodeString(const std::uint8_t* buffer, size_t &pos);
		static std::vector<std::uint8_t> encodeString(const std::string& s);

		/// Increases pos by 2
		static std::uint16_t readU16(const std::uint8_t* buffer, size_t &pos);
		static std::array<uint8_t, 2> writeU16(uint16_t u);

		/// Increases pos by 4
		static std::uint32_t readU32(const std::uint8_t* buffer, size_t &pos);
		static std::array<uint8_t, 4> writeU32(uint32_t u);

		/// Increases pos by 8.
		static double readDouble(const std::uint8_t* buffer, size_t &pos);
		static inline double readDouble(const std::uint8_t* buffer, size_t &&pos) {
			auto pos2 = pos;
			return AMF0::readDouble(buffer, pos2);
		}
		static std::array<std::uint8_t, 8> writeDouble(double d);

		amf0type_sptr deserialize(const std::uint8_t* buffer, size_t &pos);
		inline std::vector<std::uint8_t> serialize() { return this->serialize(this->object); };
		std::vector<std::uint8_t> serialize(const amf0type_sptr&);

		amf0type_sptr object;
	};

	class AMF0_NUMBER : public AMF0_TYPE {
	public:
		explicit inline AMF0_NUMBER () : AMF0_TYPE(AMF0::NUMBER_MARKER), d(0) {}
		explicit inline AMF0_NUMBER (double _d) : AMF0_TYPE(AMF0::NUMBER_MARKER), d(_d) {}
		/**
		 * See section 2.2 of amf0-file-format-specification.pdf
		 */
		double d;
		bool operator==(const AMF0_NUMBER& rhs) const {
			// bit_cast is defined in swf_utils.hpp for compatibility with < c++20
			return bit_cast<std::uint64_t>(this->d) == bit_cast<std::uint64_t>(rhs.d);
		}
	};

	class AMF0_BOOLEAN : public AMF0_TYPE {
	public:
		explicit inline AMF0_BOOLEAN () : AMF0_TYPE(AMF0::BOOLEAN_MARKER), b(false) {}
		explicit inline AMF0_BOOLEAN (bool _b) : AMF0_TYPE(AMF0::BOOLEAN_MARKER), b(_b) {}
		/**
		 * See section 2.3 of amf0-file-format-specification.pdf
		 */
		bool b;
		bool operator==(const AMF0_BOOLEAN& rhs) const {
			return this->b == rhs.b;
		}
	};

	class AMF0_STRING : public AMF0_TYPE {
	public:
		explicit inline AMF0_STRING () : AMF0_TYPE(AMF0::STRING_MARKER), s() {}
		explicit inline AMF0_STRING (std::string _s) : AMF0_TYPE(AMF0::STRING_MARKER), s() {
			this->s = _s;
		}
		/**
		 * See section 2.4 of amf0-file-format-specification.pdf
		 */
		std::string s;
		bool operator==(const AMF0_STRING& rhs) const {
			return this->s == rhs.s;
		}
	};

	class AMF0_OBJECT : public AMF0_TYPE {
	public:
		explicit inline AMF0_OBJECT () : AMF0_TYPE(AMF0::OBJECT_MARKER), keyValuePairs() {}
		/**
		 * See section 2.4 of amf0-file-format-specification.pdf
		 */
		std::vector<std::pair <std::string, amf0type_sptr>> keyValuePairs;
		bool operator==(const AMF0_OBJECT& rhs) const;
	};

	class AMF0_REFERENCE : public AMF0_TYPE {
	public:
		explicit inline AMF0_REFERENCE () : AMF0_TYPE(AMF0::REFERENCE_MARKER), index(0) {}
		/**
		 * See section 2.9 of amf0-file-format-specification.pdf
		 */
		std::uint16_t index;
		bool operator==(const AMF0_REFERENCE& rhs) const {
			return this->index == rhs.index;
		}
	};

	class AMF0_ECMA_ARRAY : public AMF0_TYPE {
	public:
		explicit inline AMF0_ECMA_ARRAY () : AMF0_TYPE(AMF0::ECMA_ARRAY_MARKER), associativeCount(), keyValuePairs() {}
		/**
		 * See section 2.10 of amf0-file-format-specification.pdf
		 */
		/**
		 * ECMA arrays work like objects except that they are prefixed with a length.
		 * But sometimes this length is zero and the array contains objects,
		 * I don't know why.
		 */
		std::uint32_t associativeCount;
		std::vector<std::pair <std::string, amf0type_sptr>> keyValuePairs;
		bool operator==(const AMF0_ECMA_ARRAY& rhs) const;
	};

	// Not in used by HFW and not tested
	class AMF0_STRICT_ARRAY : public AMF0_TYPE {
	public:
		explicit inline AMF0_STRICT_ARRAY () : AMF0_TYPE(AMF0::STRICT_ARRAY_MARKER), arrayCount(), values() {}
		/**
		 * See section 2.12 of amf0-file-format-specification.pdf
		 */
		std::uint32_t arrayCount;
		std::vector<amf0type_sptr> values;
		bool operator==(const AMF0_STRICT_ARRAY& rhs) const;
	};

	class AMF0_TYPED_OBJECT : public AMF0_TYPE {
	public:
		explicit inline AMF0_TYPED_OBJECT () : AMF0_TYPE(AMF0::TYPED_OBJECT_MARKER), className(), keyValuePairs() {}
		/**
		 * See section 2.18 of amf0-file-format-specification.pdf
		 */
		std::string className;
		std::vector<std::pair <std::string, amf0type_sptr>> keyValuePairs;
		bool operator==(const AMF0_TYPED_OBJECT& rhs) const;
	};

} // swf

#endif // AMF0_HPP
