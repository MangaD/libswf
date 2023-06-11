# Library: LZMA SDK
# Makefile created by MangaD

CMD = cmd /C

ifeq ($(OS),Windows_NT)
PROG = ..\liblzmasdk.a
else
PROG = ../liblzmasdk.a
endif
CC = gcc
CXX ?= g++
AR      = ar
ARFLAGS = rcvs
SRC_FOLDER = C
LIB =
RM = rm -f

DEFINES = -D_7ZIP_ST

# By default, we build for release
BUILD=release

WARNINGS =

ifeq ($(BUILD),release)
OPTIMIZE = -O3 -s -DNDEBUG
else
OPTIMIZE = -O0 -g -DDEBUG -fsanitize=undefined,address -fno-sanitize-recover=all
endif

# x64 or x86
ifeq ($(ARCH),x64)
ARCHITECTURE = -m64
else ifeq ($(ARCH),x86)
ARCHITECTURE = -m32
endif

CXXFLAGS = $(ARCHITECTURE) -std=c++11 $(DEFINES) $(WARNINGS) $(OPTIMIZE)

CFLAGS = -c $(ARCHITECTURE) $(DEFINES) $(WARNINGS) $(OPTIMIZE)

.PHONY: all clean debug release release32 release64 debug32 debug64

OBJ = $(SRC_FOLDER)/LzmaEnc.o \
	$(SRC_FOLDER)/LzFind.o \
	$(SRC_FOLDER)/LzmaDec.o \
	$(SRC_FOLDER)/Lzma2Dec.o

.PHONY: all clean obj

all: $(PROG)

$(PROG): $(OBJ)
	$(AR) $(ARFLAGS) $@ $?

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
	$(CMD) "del $(PROG) $(SRC_FOLDER)\*.o *.o *.a" 2>nul
else
	rm -f $(PROG) $(SRC_FOLDER)/*.o *.o *.a
endif
