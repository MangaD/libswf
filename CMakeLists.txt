cmake_minimum_required(VERSION 2.8.12)

if(${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
	message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

project (libswf)

# Directory variables
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries)
set(INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (CMAKE_VERSION VERSION_LESS "3.1")
	if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGCC OR CMAKE_COMPILER_IS_CLANGCXX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
	endif ()
else ()
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif ()

### DEPENDENCIES

# https://cmake.org/cmake/help/v3.0/module/FindZLIB.html
find_package(ZLIB REQUIRED)

if( NOT EXISTS ${LIB_DIR}/lzma/Makefile )
	message(FATAL_ERROR "Unable to find lzma sdk.")
endif()
if( NOT EXISTS ${LIB_DIR}/lodepng/lodepng/Makefile )
	message(FATAL_ERROR "Unable to find lodepng library.")
endif()

# Set Warnings and linking options
if(MSVC)
	## For static linking
	# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	##
	
	## For dynamic linking
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
	##
	
	# Disable secure warnings
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

	if(NOT CMAKE_BUILD_TYPE MATCHES Release)
		# Force to always compile with W4
		if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
			string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
		else()
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
		endif()
	endif(NOT CMAKE_BUILD_TYPE MATCHES Release)
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGCC OR CMAKE_COMPILER_IS_CLANGCXX)
	if(NOT CMAKE_BUILD_TYPE MATCHES Release)
		# Update if necessary
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wmain -Weffc++ -Wswitch-default \
			-Wswitch-enum -Wmissing-include-dirs -Wmissing-declarations -Wunreachable-code -Winline \
			-Wfloat-equal -Wundef -Wcast-align -Wredundant-decls -Winit-self -Wshadow -Wnon-virtual-dtor \
			-Wconversion")
		if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wzero-as-null-pointer-constant") # -Wuseless-cast
		endif()
	endif(NOT CMAKE_BUILD_TYPE MATCHES Release)

	if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
		# Coverage
		SET(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_DEBUG} --coverage")
		SET(CMAKE_EXE_LINKER_FLAGS_COVERAGE "${CMAKE_EXE_LINKER_FLAGS_DEBUG} --coverage")
		SET(CMAKE_SHARED_LINKER_FLAGS_COVERAGE "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} --coverage")
	endif()
endif()

if(WIN32)
	add_compile_definitions(UNICODE _UNICODE)
endif(WIN32)

### BUILD DEPENDENCY LIBRARIES ###
# (Generate the static library from the sources)

# Compile LZMA SDK
add_compile_definitions(_7ZIP_ST)
add_library(lzmasdk STATIC ${LIB_DIR}/lzma/C/LzmaEnc.c ${LIB_DIR}/lzma/C/LzFind.c
	${LIB_DIR}/lzma/C/LzmaDec.c ${LIB_DIR}/lzma/C/Lzma2Dec.c)

# Compile LodePNG
add_compile_definitions(DLODEPNG_NO_COMPILE_ZLIB LODEPNG_NO_COMPILE_DISK)
add_library(lodepng STATIC ${LIB_DIR}/lodepng/lodepng/lodepng.cpp)


### BUILD LIBSWF ###

#cBring the headers into the project
include_directories( SYSTEM ${INC_DIR} )
include_directories( SYSTEM ${ZLIB_INCLUDE_DIRS} ) # From find_package(zlib)
include_directories( SYSTEM ${LIB_DIR}/lodepng )
include_directories( SYSTEM ${LIB_DIR} ) # LZMA, rlutil

# The file(GLOB...) allows for wildcard additions:
file(GLOB SRC_FILES "${SOURCE_DIR}/*.cpp")

#add_library(libswf ${SRC_FILES})

add_library(libswf_shared SHARED ${SRC_FILES})
add_library(libswf STATIC ${SRC_FILES})

target_link_libraries( libswf_shared lzmasdk ) # LZMA SDK
#target_link_libraries(libswf_shared lzma) # XZ Utils
target_link_libraries( libswf_shared lodepng ) # LodePNG
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGCC OR CMAKE_COMPILER_IS_CLANGCXX)
	target_link_libraries( libswf_shared z )
endif()
target_link_libraries( libswf_shared ${ZLIB_LIBRARIES} ) # From find_package(zlib)

IF(CMAKE_BUILD_TYPE MATCHES Coverage)
	file(GLOB TEST_SRC_FILES "${TEST_DIR}/*.cpp")
	list (APPEND SRC_FILES ${TEST_SRC_FILES})

	add_executable(libswf_test ${SRC_FILES})

	if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGCC OR CMAKE_COMPILER_IS_CLANGCXX)
		target_link_libraries( libswf_test z )
	endif()
	target_link_libraries( libswf_test ${ZLIB_LIBRARIES} ) # From find_package(zlib)
	target_link_libraries( libswf_test lzmasdk ) # LZMA SDK
	#target_link_libraries(libswf_test lzma) # XZ Utils
	target_link_libraries( libswf_test lodepng ) # LodePNG

	if( MINGW )
		set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
	endif()

ENDIF(CMAKE_BUILD_TYPE MATCHES Coverage)

