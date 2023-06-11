# Methodology


## Editing the SWF

We use the [JPEXS Free Flash Decompiler](https://www.free-decompiler.com/flash/) program in order to inspect the SWF file. This program is useful to guide us on the SWF file structure and content so that we can mimic what it does.

More info on the SWF file structure can be found on the file `swf-file-format-spec.pdf` inside the `documentation` folder. Chapter 2 is particularly important. The [SWF Reference by Alexis](https://www.m2osw.com/swf_alexref) - part of the free SSWF project - is also very useful, it is inside the `documentation` folder with the name `SWF File Format Reference.pdf`

Other useful tools are an hex editor and a program to find differences between binary files (eg. [VBinDiff](https://www.cjmweb.net/vbindiff/)) so that we can see what modifications JPEXS does to the SWF.

## Editing images

`swf-file-format-spec.pdf` - Page 139


## Editing sounds

`swf-file-format-spec.pdf` - Page 179


## Editing serialized objects

Flash, objects are serialized in [Action Message Format](<https://en.wikipedia.org/wiki/Action_Message_Format>).

[AMF0 file format](amf0-file-format-specification.pdf)
[AMF3 file format](amf3-file-format-spec.pdf)


## Editing ActionScript

*TODO*

What is **P-code**: [https://en.wikipedia.org/wiki/Bytecode](https://en.wikipedia.org/wiki/Bytecode)


## Compress / Decompress SWF

The SWF file structure<sup>[\[1\]](#swf-format)</sup> starts with a a signature which can be one of the following:

-   `FWS` - Uncompressed
-   `CWS` - zlib compressed
-   `ZWS` - LZMA compressed

The signature is followed by the [SWF version](https://www.adobe.com/devnet/air/articles/versioning-in-flash-runtime-swf-version.html).

### zlib

zlib compression works for SWF files of version >= 6. However, for SWF files with lower version it might work by simply changing the version byte to 6.

The algorithm to compress is as follows:

-   Copy the first 8 bytes of the SWF file which correspond to SWF file signature (3), version (1) and file length (4) to a buffer.
-   Change the 'F' in the signature to 'C'
-   Change the version to 6 if it is lower.
-   zlib compress the remaining bytes of the SWF file and add them to the buffer.

### LZMA

LZMA compression works for SWF files of version >= 13. However, for SWF files with lower version it might work by simply changing the version byte to 13.

LZMA compressed SWF files will work [in Flash Player 11 or AIR 3 or higher](https://www.adobe.com/devnet/flash/articles/concept-lzma-compression.html).

The algorithm to compress is as follows:

-   Copy the first 8 bytes of the SWF file which correspond to SWF file signature (3), version (1) and file length (4) to a buffer.
-   Change the 'F' in the signature to 'Z'
-   Change the version to 13 if it is lower.
-   LZMA compress the remaining bytes of the SWF file but don't add them to the buffer yet.
-   Calculate the length of the compressed bytes and subtract 5 to it (excluding LZMA properties<sup>[\[2\]](#lzma-format)</sup>).
-   Add the length calculated to the buffer in 4 little-endian bytes. Buffer should now have 12 bytes.
-   Add the compressed data to the buffer.

Note: Using XZ tools provides compression/decompression for formats .xz and .lzma. Both seem to not work with Adobe Flash Player. So I used LZMA SDK.

[Note from Adobe on LZMA vs 7z](https://helpx.adobe.com/flash-player/kb/exception-thrown-you-decompress-lzma-compressed.html)

<a name="swf-format"></a>*\[1]: swf-file-format-spec.pdf - Chapter 2*  \
<a name="lzma-format"></a>*\[2]: lzma-file-format.txt - Chapter 1.1*

Source: [https://github.com/OpenGG/swfzip/blob/master/swfzip.py](https://github.com/OpenGG/swfzip/blob/master/swfzip.py)

## SWF 2 EXE

Finding how to programmatically convert a .swf file to a .exe file was not straightforward. There are a few programs out there that do this (e.g. [swf2exe](https://sourceforge.net/projects/swf2exe/)), but they are apparently not open source. It was by chance that I was able to find [this](https://github.com/devil-tamachan/tama-ieeye/blob/master/SWF2EXE/swf2exe/swf2exe/MainDlg.h#L98) piece of code by [devil-tamachan](https://github.com/devil-tamachan) on GitHub that showed me how to do this programmatically.

The algorithm is quite simple to implement, but I don't know yet why it works like this. For starters, there is a software called [Adobe Flash Player projector](https://www.adobe.com/support/flashplayer/debug_downloads.html) (or Stand Alone), which is basically a program (.exe) that can run flash files (.swf). This program also has an option to create an .exe of the .swf file.

To convert programmatically, we do the following:

1.  Create a copy of the Flash Player projector.
2.  Append the .swf file to the projector's copy (merging the binaries basically).
3.  Append the following footer to the file: \[0x56, 0x34, 0x12, 0xFA]
4.  Append the size of the .swf file (4 bytes) to the file in little-endian format (less significant byte first). This can be the compressed length or uncompressed length, although the compressed length will allow us to calculate the start of the swf file afterwards.

There are different versions of the Flash Player projector, but this algorithm should work for all of them.

It is also possible to convert the SWF to a Linux binary, using the Adobe Flash Player projector for Linux. Programmatically it is slightly different than windows:

1.  Create a copy of the Flash Player projector.
2.  Append the size of the .swf file (4 bytes) to the file in little-endian format (less significant byte first). This can be the compressed length or uncompressed length, although the compressed length will allow us to calculate the start of the swf file afterwards.
3.  Append the following footer to the file: \[0x56, 0x34, 0x12, 0xFA]
4.  Append the .swf file to the projector's copy (merging the binaries basically).

_Note: Flash Player 11 Projector consumes less RAM than its successors, and FP 32 no longer works for embedding swf files._

## EXE 2 SWF

This method assumes that the SWF is inside the executable and not encrypted, so it is a matter of detecting the start and the end of the SWF file in order to extract it.

1.  Detect if the file is a Windows executable, Linux executable, or not an executable.
    1.  Windows: The PE file's<sup>[\[1\]](#pe-file-format)</sup> magic number is `MZ`<sup>[\[2\]](#magic-number)</sup>, at offset 0x3C there is a 4 byte long integer that points to the beginning of the PE header, which has the signature `PE`.
    2.  Linux: The ELF file's signature is 0x7F followed by `ELF`<sup>[\[3\]](#elf-file)</sup>.
2.  We could search for the SWF signature, but there's no guarantee that there won't be multiple occurrences of this signature in the file. We could also check the length to make sure it reaches the end of the file where the footer FA123456 is found, although we could have a different executable that doesn't make use of this footer. To make things simple, we'll assume that we are dealing with the Flash Player projector and thus we have the footer FA123456 followed by the SWF file length (compressed) at the end of the file, in the case of Windows. In the case of Linux we have the footer FA123456 followed by the SWF and preceded by the SWF length

<a name="pe-file-format"></a>*\[1]: [PE file format image](https://upload.wikimedia.org/wikipedia/commons/1/1b/Portable_Executable_32_bit_Structure_in_SVG_fixed.svg) and [PE file format documentation](https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#machine-types)*  \
<a name="magic-number"></a>*\[2]: the initials of the designer of the file format, Mark Zbikowski: [Wikipedia - Magic Number](https://en.wikipedia.org/wiki/Magic_number_(programming))*  \
<a name="elf-file"></a>*\[3]: [Executable and Linkable Format](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)*

## Customizing the Flash Projector / HF executable

Adobe Flash Player projector can be downloaded [here](https://helpx.adobe.com/flash-player/kb/archived-flash-player-versions.html)

Change icon with [Resource Hacker](http://www.angusj.com/resourcehacker/) (.exe must not be compressed with UPX).

The Adobe Flash Player projector executable file is about 14-15 MiB in size. By using [UPX](https://upx.github.io/) we can decrease the size to about 5.5 MiB!

[Article on UPX](https://labs.detectify.com/2016/04/12/using-reverse-engineering-techniques-to-see-how-a-common-malware-packer-works/)

*Note: SA stands for Standalone*
