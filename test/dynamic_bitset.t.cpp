#include <catch2/catch_test_macros.hpp>

#include "../source/dynamic_bitset.hpp"

#include <sstream>      // std::ostringstream

using namespace swf_utils;

TEST_CASE( "dynamic_bitset size", "[dynamic-bitset]" ) {
	dynamic_bitset db(10);
	REQUIRE( db.size() == 10 );
}

TEST_CASE( "dynamic_bitset insertion operator", "[dynamic-bitset]" ) {
	std::ostringstream oss;
	oss << "0001010"_bits;
	REQUIRE( oss.str() == "0001010" );
	oss.str("");

	dynamic_bitset db(10);
	db[0] = 1;
	oss << db;
	REQUIRE( oss.str() == "0000000001" );
}

TEST_CASE( "dynamic_bitset subscript operator", "[dynamic-bitset]" ) {
	dynamic_bitset db(10);

	db[0] = 1;
	REQUIRE( db[0] == 1 );
	REQUIRE( db == "0000000001"_bits );

	db[2] = db[0];
	REQUIRE( db[2] == 1 );
	REQUIRE( db == "0000000101"_bits );
}

TEST_CASE( "dynamic_bitset shift operator", "[dynamic-bitset]" ) {
	dynamic_bitset db(10);

	db[0] = 1;
	db[2] = db[0];

	db <<= 1;
	REQUIRE( db == "0000001010"_bits );

	db >>= 1;
	REQUIRE( db == "0000000101"_bits );
}

TEST_CASE( "dynamic_bitset to_ulong", "[dynamic-bitset]" ) {
	dynamic_bitset db(10);

	db[0] = 1;
	db[2] = db[0];

	REQUIRE( db.to_ulong() == 5 );
}

TEST_CASE( "dynamic_bitset inverse operator", "[dynamic-bitset]" ) {
	dynamic_bitset db(10);

	db[0] = 1;
	db[2] = db[0];

	REQUIRE( ~db == "1111111010"_bits );
}

TEST_CASE( "dynamic_bitset bitwise operators", "[dynamic-bitset]" ) {
	dynamic_bitset db(10);

	db[0] = 1;
	db[2] = db[0];

	dynamic_bitset db2(5, true);

	REQUIRE( db2 == "11111"_bits );

	db &= db2;
	REQUIRE( db == "0000000101"_bits );

	db ^= db2;
	REQUIRE( db == "0000011010"_bits );

	db |= db2;
	REQUIRE( db == "0000011111"_bits );
}

TEST_CASE( "dynamic_bitset from integer", "[dynamic-bitset]" ) {
	dynamic_bitset db(10, 7);

	REQUIRE( db == "0000000111"_bits );

	CHECK_THROWS( dynamic_bitset (2, 7) );
}

TEST_CASE( "dynamic_bitset from string", "[dynamic-bitset]" ) {
	dynamic_bitset db("0101");

	REQUIRE( db[0] == 1 );

	REQUIRE( db == "0101"_bits );

	CHECK_THROWS( dynamic_bitset ("011a") );
}

TEST_CASE( "dynamic_bitset user-defined literal", "[dynamic-bitset]" ) {
	auto db = "0101"_bits;
	std::ostringstream oss;
	oss << "0101"_bits;
	REQUIRE( oss.str() == "0101" );
	oss.str("");
}
