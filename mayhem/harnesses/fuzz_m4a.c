/* M4A/MP4 (AAC, ALAC) and raw ADTS AAC through kew's m4a decoder (src/sound/m4a.h):
 * libmp4 demuxer, ALAC decoder, faad2. Same calls the player makes: init_file, format,
 * length, read, seek, read. The API takes a path only, so the input goes to a scratch file. */
#define MA_NO_DEVICE_IO
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include "sound/m4a.h"
#include "scratch.h"

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
        char path[1024];
        scratch_write("in.m4a", d, n, path, sizeof path);
        static m4a_decoder dec;
        memset(&dec, 0, sizeof dec);
        if (m4a_decoder_init_file(path, NULL, NULL, &dec) == MA_SUCCESS) {
                ma_format f; ma_uint32 ch = 0, sr = 0; ma_channel map[MA_MAX_CHANNELS];
                ma_uint64 len = 0, got = 0, cur = 0;
                m4a_decoder_get_data_format(&dec, &f, &ch, &sr, map, MA_MAX_CHANNELS);
                m4a_decoder_get_length_in_pcm_frames(&dec, &len);
                static float buf[4096 * MA_MAX_CHANNELS * 2];
                for (int i = 0; i < 8; i++)
                        if (m4a_decoder_read_pcm_frames(&dec, buf, 1024, &got) != MA_SUCCESS || !got) break;
                m4a_decoder_get_cursor_in_pcm_frames(&dec, &cur);
                /* Seek target from the input's last bytes, so seeking is fuzzed too. */
                ma_uint64 target = n >= 2 ? ((ma_uint64)d[n - 2] << 8 | d[n - 1]) * 64 : 0;
                if (m4a_decoder_seek_to_pcm_frame(&dec, target) == MA_SUCCESS)
                        m4a_decoder_read_pcm_frames(&dec, buf, 1024, &got);
                m4a_decoder_uninit(&dec, NULL);
        }
        unlink(path);
        return 0;
}
