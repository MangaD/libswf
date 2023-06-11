/*
    https://github.com/lieff/minimp3
    To the extent possible under law, the author(s) have dedicated all
    copyright and related and neighboring rights to this software to the
    public domain worldwide.
    This software is distributed without any warranty.
    See <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef MINIMP3_EXT_H
#define MINIMP3_EXT_H

#include "minimp3.h"

#define MP3D_SEEK_TO_BYTE   0
#define MP3D_SEEK_TO_SAMPLE 1
#define MP3D_SEEK_TO_SAMPLE_INDEXED 2

#include <cstddef> // size_t
#include <cstdint> // uint8_t, uint32_t
#include <vector>
#include <string>

namespace mp3 {

	class mp3_exception : public std::exception {
		public:
			explicit mp3_exception(const std::string &message = "mp3_exception")
				: std::exception(), error_message(message) {}
			const char *what() const noexcept
			{
				return error_message.c_str();
			}
		private:
			std::string error_message;
	};

	typedef struct {
		bool stereo;
		int hz, layer;
		size_t id3v2size, avg_bitrate_kbps, total_samples;
		size_t id3v1size;
	} mp3_info;

	typedef struct {
		mp3d_sample_t *buffer;
		std::size_t samples; /* channels included, byte size = samples*sizeof(int16_t) */
		int channels, hz, layer;
		size_t id3v2size, avg_bitrate_kbps;
		size_t id3v1size;
	} mp3dec_file_info_t;

	mp3_info get_mp3_info(const std::vector<uint8_t> &buffer);

	void mp3dec_load_buf(mp3dec_t *dec, const uint8_t *buf, std::size_t buf_size, mp3dec_file_info_t *info);

} // mp3

#endif // MINIMP3_EXT_H
