# Project: libswf
# Makefile created by MangaD

# Notes:
#  - If 'cmd /C' does not work, replace with 'cmd //C'

CMD = cmd /C

# Necessary for echo -e
SHELL = bash

APP_NAME    = libswf.a
TEST_NAME   = libswf_test
CXX        ?= g++
AR          = ar
ARFLAGS     = rcvs
SRC_FOLDER  = source
TEST_FOLDER = test
OBJ_FOLDER  = build
LIB_FOLDER  = libraries
INCLUDES    = -isystem include -isystem $(LIB_FOLDER)/lodepng -isystem $(LIB_FOLDER)
BIN_FOLDER  = bin

# By default, we build for release
BUILD=release

SUBDIRS = $(LIB_FOLDER)

# SRC is a list of the cpp files in the source directory.
SRC = $(wildcard $(SRC_FOLDER)/*.cpp)

# OBJ is a list of the object files to be generated in the objects directory.
OBJ = $(subst $(SRC_FOLDER),$(OBJ_FOLDER),$(SRC:.cpp=.o))

DEFINES=

# Windows should work with Unicode
ifeq ($(OS),Windows_NT)
	DEFINES += -DUNICODE -D_UNICODE
endif

ifneq ($(BUILD),release)
	WARNINGS = -Wall -Wextra -pedantic -Wmain -Weffc++ -Wswitch-default \
		-Wswitch-enum -Wmissing-include-dirs -Wmissing-declarations -Wunreachable-code -Winline \
		-Wfloat-equal -Wundef -Wcast-align -Wredundant-decls -Winit-self -Wshadow -Wnon-virtual-dtor \
		-Wconversion #-Wno-useless-cast
	ifeq ($(CXX),g++)
		WARNINGS += -Wzero-as-null-pointer-constant #-Wuseless-cast
	else
		ifeq ($(CXX),clang++)
			#http://clang.llvm.org/docs/ThreadSafetyAnalysis.html
			WARNINGS += -Wthread-safety
		endif
	endif
endif

# Debug or Release #
ifeq ($(BUILD),release)
	# "Release" build - optimization, no debug symbols and no assertions (if they exist)
	OPTIMIZE    = -O3 -s -DNDEBUG
	ifeq ($(OS),Windows_NT)
		STATIC      = -static -static-libgcc -static-libstdc++
		BIN_FOLDER := $(BIN_FOLDER)\release
	else
		BIN_FOLDER := $(BIN_FOLDER)/release
	endif
else
	# "Debug" build - no optimization, with debugging symbols and assertions (if they exist), and sanitize
	OPTIMIZE    = -O0 -g -DDEBUG
	ifneq ($(OS),Windows_NT)
		# Coverage: --coverage -fprofile-arcs -ftest-coverage
		OPTIMIZE    := -fsanitize=undefined,address -fno-sanitize-recover=all
		ifeq ($(CXX),g++)
			OPTIMIZE    := --coverage -fprofile-arcs -ftest-coverage
		endif # g++
		BIN_FOLDER := $(BIN_FOLDER)/debug
	else
		BIN_FOLDER := $(BIN_FOLDER)\debug
	endif # win
	DEFINES    += -DSWF_DEBUG_BUILD
endif # release

# ARCHITECTURE (x64 or x86) #
# Do NOT use spaces in the filename nor special characters
ifeq ($(ARCH),x64)
	ARCHITECTURE = -m64
else ifeq ($(ARCH),x86)
	ARCHITECTURE = -m32
endif

ifeq ($(OS),Windows_NT)
	BIN = $(BIN_FOLDER)\$(APP_NAME)
	TEST_BIN = $(BIN_FOLDER)\$(TEST_NAME).exe
else
	BIN = $(BIN_FOLDER)/$(APP_NAME)
	TEST_BIN = $(BIN_FOLDER)/$(TEST_NAME)
endif

TARGETS = $(BIN)

### TESTING ###

ifneq ($(BUILD),release)
	TEST_OBJ_FOLDER  = build
	TARGETS += $(TEST_BIN)
	LDFLAGS = -L$(LIB_FOLDER) -L$(BIN_FOLDER)
	LDLIBS += -lz -llzmasdk -llodepng -lCatch2Main -lCatch2 # -llzma

	# sanitize
	LDLIBS += -fsanitize=undefined,address -fno-sanitize-recover=all -lasan

	# coverage
	ifeq ($(CXX),g++)
		LDLIBS += -lgcov
	endif

	TEST_SRC = $(wildcard $(TEST_FOLDER)/*.cpp)
	TEST_OBJ = $(subst $(TEST_FOLDER),$(TEST_OBJ_FOLDER),$(TEST_SRC:.cpp=.o))
endif

CXXFLAGS = $(INCLUDES) $(ARCHITECTURE) -std=c++17 $(DEFINES) $(WARNINGS) $(OPTIMIZE)

all: $(OBJ_FOLDER) $(BIN_FOLDER) $(SUBDIRS) $(TARGETS)
ifeq ($(OS),Windows_NT)
	$(CMD) "copy $(LIB_FOLDER)\*.a $(BIN_FOLDER)" 2>nul
else
	cp $(LIB_FOLDER)/*.a $(BIN_FOLDER)
endif

.PHONY : all clean run run32 run64 debug release release32 release64 debug32 debug64 $(SUBDIRS)

$(BIN): $(OBJ)
	$(AR) $(ARFLAGS) $@ $?

$(TEST_BIN): $(OBJ) $(TEST_OBJ)
	$(CXX) $(OBJ) $(TEST_OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(OBJ_FOLDER)/%.o: $(SRC_FOLDER)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_OBJ_FOLDER)/%.o: $(TEST_FOLDER)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


# DEPENDENCIES #
$(OBJ_FOLDER)/swf_utils.o: $(SRC_FOLDER)/swf_utils.hpp
$(OBJ_FOLDER)/zlib_wrapper.o: $(SRC_FOLDER)/zlib_wrapper.hpp
$(OBJ_FOLDER)/lzmasdk_wrapper.o: $(SRC_FOLDER)/lzmasdk_wrapper.hpp
$(OBJ_FOLDER)/swf.o: $(SRC_FOLDER)/swf.hpp $(SRC_FOLDER)/swf_utils.hpp $(SRC_FOLDER)/tag.hpp \
					$(SRC_FOLDER)/zlib_wrapper.hpp $(SRC_FOLDER)/lzmasdk_wrapper.hpp \
					$(SRC_FOLDER)/minimp3_ex.hpp
# $(SRC_FOLDER)/xz_lzma_wrapper.hpp
$(OBJ_FOLDER)/tag.o: $(SRC_FOLDER)/tag.hpp $(SRC_FOLDER)/swf_utils.hpp
$(OBJ_FOLDER)/minimp3_ex.o: $(SRC_FOLDER)/minimp3_ex.hpp
$(OBJ_FOLDER)/amf3.o: $(SRC_FOLDER)/amf3.hpp
$(OBJ_FOLDER)/amf0.o: $(SRC_FOLDER)/amf0.hpp
$(OBJ_FOLDER)/dynamic_bitset.o: $(SRC_FOLDER)/dynamic_bitset.hpp

# $(OBJ_FOLDER)/xz_lzma_wrapper.o: $(SRC_FOLDER)/xz_lzma_wrapper.hpp

# Test dependencies
$(OBJ_FOLDER)/dynamic_bitset.t.o: $(SRC_FOLDER)/dynamic_bitset.hpp

$(SUBDIRS):
	$(MAKE) -C $@ BUILD=$(BUILD) ARCH=$(ARCH)

debug:
	$(MAKE) BUILD=debug
release:
	$(MAKE) BUILD=release
release32:
	$(MAKE) BUILD=release ARCH=x86
release64:
	$(MAKE) BUILD=release ARCH=x64
debug32:
	$(MAKE) BUILD=debug ARCH=x86
debug64:
	$(MAKE) BUILD=debug ARCH=x64


clean:
ifeq ($(OS),Windows_NT)
	FOR %%D IN ($(SUBDIRS)) DO $(MAKE) -C %%D clean
else
	@$(foreach dir,$(SUBDIRS),$(MAKE) -C $(dir) clean;)
endif
ifeq ($(OS),Windows_NT)
	$(CMD) "del *.o $(SRC_FOLDER)\*.o $(OBJ_FOLDER)\*.o $(OBJ_FOLDER)\*.gcda $(OBJ_FOLDER)\*.gcno $(BIN_FOLDER)\*.a bin\debug\*.a *.gcov" 2>nul
else
	rm -f *.o $(SRC_FOLDER)/*.o $(OBJ_FOLDER)/*.o $(OBJ_FOLDER)/*.gcda $(OBJ_FOLDER)/*.gcno $(BIN_FOLDER)/*.a bin/debug/*.a *.gcov
endif

$(OBJ_FOLDER):
ifeq ($(OS),Windows_NT)
	$(CMD) "mkdir $(OBJ_FOLDER)"
else
	mkdir -p $(OBJ_FOLDER)
endif

$(BIN_FOLDER):
ifeq ($(OS),Windows_NT)
	$(CMD) "mkdir $(BIN_FOLDER)"
else
	mkdir -p $(BIN_FOLDER)
endif
