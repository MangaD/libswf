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

/**
 * https://github.com/nlohmann/json/issues/485#issuecomment-333652309
 * A workaround to give to use fifo_map as map, we are just ignoring the 'less' compare
 */
template<class K, class V, class dummy_compare, class A>
using my_workaround_fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;

namespace swf {

	class AMF0 {
	public:
		/**
		 * AMF0 marker constants
		 * See section 2.1 of amf0-file-format-specification.pdf
		 */
		static constexpr uint8_t NUMBER_MARKER = 0x00;
		static constexpr uint8_t BOOLEAN_MARKER = 0x01;
		static constexpr uint8_t STRING_MARKER = 0x02;
		static constexpr uint8_t OBJECT_MARKER = 0x03;
		static constexpr uint8_t MOVIECLIP_MARKER = 0x04;
		static constexpr uint8_t NULL_MARKER = 0x05;
		static constexpr uint8_t UNDEFINED_MARKER = 0x06;
		static constexpr uint8_t REFERENCE_MARKER = 0x07;
		static constexpr uint8_t ECMA_ARRAY_MARKER = 0x08;
		static constexpr uint8_t OBJECT_END_MARKER = 0x09;
		static constexpr uint8_t STRICT_ARRAY_MARKER = 0x0A;
		static constexpr uint8_t DATE_MARKER = 0x0B;
		static constexpr uint8_t LONG_STRING_MARKER = 0x0C;
		static constexpr uint8_t UNSUPPORTED_MARKER = 0x0D;
		static constexpr uint8_t RECORDSET_MARKER = 0x0E;
		static constexpr uint8_t XML_DOCUMENT_MARKER = 0x0F;
		static constexpr uint8_t TYPED_OBJECT_MARKER = 0x10;
		static constexpr uint8_t AVMPLUS_OBJECT_MARKER = 0x11;

		explicit AMF0(const uint8_t *buffer, const size_t buffer_size);
		inline std::string to_json_str(const int indent=4) {
			return jsonObj.dump(indent);
		}
		static std::vector<uint8_t> fromJSON(const std::string &js);
		static void writeStringWithLenPrefixU16(std::vector<uint8_t> &data, const std::string &s);

		/// Increases pos by 8.
		static double readDouble(const uint8_t* buffer, size_t &pos);
		static inline double readDouble(const uint8_t* buffer, size_t &&pos) {
			auto pos2 = pos;
			return AMF0::readDouble(buffer, pos2);
		}
		static std::array<uint8_t, 8> writeDouble(double d);
	private:
		void parseAMF0Object(const uint8_t *buffer, const size_t buffer_size, size_t &pos,
		                     nlohmann::basic_json<my_workaround_fifo_map> &obj);
		static void parseJSONElement(std::vector<uint8_t> &buffer,
		                             const nlohmann::basic_json<my_workaround_fifo_map> &el);
		nlohmann::basic_json<my_workaround_fifo_map> jsonObj;
	};

} // swf

#endif // AMF0_HPP
