# Makefile created by MangaD

CMD=cmd /C

DIRS = lzma lodepng

.PHONY: all clean

all: liblzmasdk.a liblodepng.a

liblzmasdk.a:
	$(MAKE) -C lzma BUILD=$(BUILD) ARCH=$(ARCH)

liblodepng.a:
	$(MAKE) -C lodepng BUILD=$(BUILD) ARCH=$(ARCH)

clean:
ifeq ($(OS),Windows_NT)
	FOR %%D IN ($(DIRS)) DO $(MAKE) -C %%D clean
	$(CMD) "del *.a" 2>nul
else
	for d in $(DIRS); do $(MAKE) -C $$d clean; done
	rm -f *.a
endif
