# libswf

[![pipeline status](https://gitlab.com/MangaD/libswf/badges/master/pipeline.svg?style=flat-square)](https://gitlab.com/MangaD/libswf/commits/master) [![Build status](https://ci.appveyor.com/api/projects/status/ikfm27u4h9i9gx61?svg=true)](https://ci.appveyor.com/project/MangaD/libswf) [![coverage report](https://gitlab.com/MangaD/libswf/badges/master/coverage.svg?style=flat-square)](https://gitlab.com/MangaD/libswf/commits/master) [![license](https://img.shields.io/badge/license-MIT-red?style=flat-square)](LICENSE)

libswf is a C++17 library for extracting and replacing SWF resources.

## Implemented

- Export SWF as EXE using Adobe Flash Player [projector](https://www.adobe.com/support/flashplayer/debug_downloads.html).
- Import SWF from EXE created with Adobe Flash Player [projector](https://www.adobe.com/support/flashplayer/debug_downloads.html).
- Zlib and LZMA compress / decompress SWF.
- Export and replace MP3.
- Export and replace images of `DefineBitsLossless` and `DefineBitsLossless2` tags.
- Export and replace binary data of `DefineBinaryData` tag.
- Deserializing AMF3 ByteArrays. AMF3 manipulation needs a lot of work.
- Exporting AMF0 as JSON (not all data types), needs work.

## Usage example

See [HF Workshop](https://gitlab.com/MangaD/hf-workshop).

## Build

See [documentation/BUILD.md](documentation/BUILD.md).

## Methodology

See [documentation/METHODOLOGY.md](documentation/METHODOLOGY.md).

## Contributing

See [documentation/CONTRIBUTING.md](documentation/CONTRIBUTING.md).

## Documentation

Documentation is generated using Doxygen (and doxywizard). Needs code review.

## Copyright

Copyright (c) 2019-2020 David Gonçalves

## License

This project uses the MIT [license](LICENSE).

## Third-Party Libraries

| **Project**                                      | **Author(s)**                                                | **License**                                                  | **Comments**                                                 |
| ------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| [zlib](https://www.zlib.net/)                    | [Jean-loup Gailly](http://gailly.net/) (compression) and [Mark Adler](http://en.wikipedia.org/wiki/Mark_Adler) (decompression) | [zlib](https://zlib.net/zlib_license.html)                   | Used for compressing and decompressing the SWF using the zlib algorithm. Also used for compressing and decompressing image data. |
| [LZMA SDK](https://www.7-zip.org/sdk.html)       | Igor Pavlov                                                  | [public domain](https://www.7-zip.org/sdk.html)              | Used for compressing and decompressing the SWF using the LZMA algorithm. |
| [liblzma](https://tukaani.org/xz/)               | Original by Lasse Collin. Developed by The Tukaani Project.  | [public domain](https://tukaani.org/xz)                      | **Not in use.** Alternative for LZMA SDK.                    |
| [minimp3](https://github.com/lieff/minimp3)      | [lieff](https://github.com/lieff)                            | [CC-0](https://github.com/lieff/minimp3/blob/master/LICENSE) | Used to verify the [sample rate](https://sound.stackexchange.com/questions/31782/why-do-mp3-have-sample-rate) of the MP3 file, and also for removing the MP3 file's [ID3 metadata](https://en.wikipedia.org/wiki/ID3), leaving only the MP3 [frames](https://en.wikipedia.org/wiki/MP3#File_structure). |
| [LodePNG](https://lodev.org/lodepng/)            | [Lode Vandevenne](https://lodev.org/)                        | [zlib](https://github.com/lvandeve/lodepng/blob/master/LICENSE) | Exporting and importing images in PNG format.                |
| [json](https://github.com/nlohmann/json)         | [nlohmann](https://github.com/nlohmann)                      | [MIT](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT) | Used for exporting and importing AMF format to and from JSON. |
| [fifo_map](https://github.com/nlohmann/fifo_map) | [nlohmann](https://github.com/nlohmann)                      | [MIT](https://github.com/nlohmann/fifo_map/blob/master/LICENSE.MIT) | Used for ordered JSON.                                       |
| [Catch2](https://github.com/catchorg/Catch2)     | [Phil Nash](https://ndctechtown.com/speaker/phil-nash). Various contributors. | [BSL-1.0](https://github.com/catchorg/Catch2/blob/master/LICENSE.txt) | Used for testing.                                            |


Table generated with: https://www.tablesgenerator.com/markdown_tables

## Credits

Project badge images created with https://shields.io/.
