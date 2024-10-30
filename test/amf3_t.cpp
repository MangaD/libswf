#include <catch2/catch_test_macros.hpp>

#include "../source/amf3.hpp"

using namespace swf;

TEST_CASE( "encodeU29 and decodeU29", "[amf3]" ) {
	for (uint32_t i = 0; i < 0x20000000; i+=51) {
		uint8_t a[4];
		size_t pos = 0;
		AMF3::encodeU29(a, i);
		REQUIRE(i == AMF3::decodeU29(a, pos));
	}
}

TEST_CASE( "encodeU29 out_of_range", "[amf3]" ) {
	uint8_t a[4];
	CHECK_THROWS( AMF3::encodeU29(a, 0x20000000) );
}
