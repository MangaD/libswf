/*
 * https://github.com/lieff/minimp3
 */

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ALLOW_MONO_STEREO_TRANSITION
#define MINIMP3_SKIP_ID3V1

#include "minimp3_ex.hpp"

#include <cstdint> // uint8_t, uint32_t
#include <cstring> // strncmp
#include <cstdlib> // malloc
#include <vector>

using namespace std;

namespace mp3 {

	static std::size_t mp3dec_skip_id3v2(const uint8_t *buf, std::size_t buf_size)
	{
		if (buf_size > 10 && !strncmp((char *)buf, "ID3", 3))
		{
			return (((buf[6] & 0x7f) << 21) | ((buf[7] & 0x7f) << 14) |
				((buf[8] & 0x7f) << 7) | (buf[9] & 0x7f)) + 10;
		}
		return 0;
	}

	void mp3dec_load_buf(mp3dec_t *dec, const uint8_t *buf, std::size_t buf_size, mp3dec_file_info_t *info)
	{
		mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
		mp3dec_frame_info_t frame_info;
		memset(info, 0, sizeof(*info));
		memset(&frame_info, 0, sizeof(frame_info));
		/* skip id3v2 */
		std::size_t id3v2size = mp3dec_skip_id3v2(buf, buf_size);
		if (id3v2size > buf_size)
			return;
		buf      += id3v2size;
		buf_size -= id3v2size;
		info->id3v2size = id3v2size;

		size_t id3v1size = 0;
	#ifdef MINIMP3_SKIP_ID3V1
	    if (buf_size > 128 && !strncmp((char *)buf + buf_size - 128, "TAG", 3)) {
	    	buf_size -= 128;
	        id3v1size += 128;
	        if (buf_size > 227 && !strncmp((char *)buf + buf_size - 227, "TAG+", 4)) {
	        	buf_size -= 227;
	        	id3v1size += 227;
	        }
	    }
	#endif
		info->id3v1size = id3v1size;

		/* try to make allocation size assumption by first frame */
		mp3dec_init(dec);
		int samples;
		do
		{
			samples = mp3dec_decode_frame(dec, buf, static_cast<int>(buf_size), pcm, &frame_info);
			buf      += frame_info.frame_bytes;
			buf_size -= frame_info.frame_bytes;
			if (samples)
				break;
		} while (frame_info.frame_bytes);
		if (!samples)
			return;
		samples *= frame_info.channels;
		std::size_t allocated = (buf_size/frame_info.frame_bytes)*samples*sizeof(mp3d_sample_t) + MINIMP3_MAX_SAMPLES_PER_FRAME*sizeof(mp3d_sample_t);
		info->buffer = (mp3d_sample_t*)malloc(allocated);
		if (!info->buffer)
			return;
		info->samples = samples;
		memcpy(info->buffer, pcm, info->samples*sizeof(mp3d_sample_t));
		/* save info */
		info->channels = frame_info.channels;
		info->hz       = frame_info.hz;
		info->layer    = frame_info.layer;
		std::size_t avg_bitrate_kbps = frame_info.bitrate_kbps;
		std::size_t frames = 1;
		/* decode rest frames */
		int frame_bytes;
		do
		{
			if ((allocated - info->samples*sizeof(mp3d_sample_t)) < MINIMP3_MAX_SAMPLES_PER_FRAME*sizeof(mp3d_sample_t))
			{
				allocated *= 2;
				info->buffer = (mp3d_sample_t*)realloc(info->buffer, allocated);
			}
			samples = mp3dec_decode_frame(dec, buf, static_cast<int>(buf_size), info->buffer + info->samples, &frame_info);
			frame_bytes = frame_info.frame_bytes;
			buf      += frame_bytes;
			buf_size -= frame_bytes;
			if (samples)
			{
				if (info->hz != frame_info.hz || info->layer != frame_info.layer)
				    break;
				if (info->channels && info->channels != frame_info.channels)
	#ifdef MINIMP3_ALLOW_MONO_STEREO_TRANSITION
					info->channels = 0; /* mark file with mono-stereo transition */
	#else
					break;
	#endif
				info->samples += samples*frame_info.channels;
				avg_bitrate_kbps += frame_info.bitrate_kbps;
				++frames;
			}
		} while (frame_bytes);
		/* reallocate to normal buffer size */
		if (allocated != info->samples*sizeof(mp3d_sample_t))
			info->buffer = (mp3d_sample_t*)realloc(info->buffer, info->samples*sizeof(mp3d_sample_t));
		info->avg_bitrate_kbps = avg_bitrate_kbps/frames;

		free(info->buffer);
	}

	mp3_info get_mp3_info(const vector<uint8_t> &buffer) {
		mp3_info info;

		mp3dec_t mp3d;
		size_t total_samples = 0;

		mp3dec_file_info_t info_tmp;

		mp3dec_load_buf(&mp3d, buffer.data(), buffer.size(), &info_tmp);

		if (info_tmp.samples > 0) {
			total_samples += info_tmp.samples;
		} else {
			throw mp3_exception("Error parsing MP3 file.");
		}

		info.stereo = (info_tmp.channels == 2);
		info.hz = info_tmp.hz;
		info.layer = info_tmp.layer;
		info.avg_bitrate_kbps = info_tmp.avg_bitrate_kbps;
		info.total_samples = (info_tmp.channels == 2 ? total_samples/2 : total_samples);
		info.id3v2size = info_tmp.id3v2size;
		info.id3v1size = info_tmp.id3v1size;

		return info;
	}

} // mp3
