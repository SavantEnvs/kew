/* WebM (Opus, Vorbis) through kew's WebM decoder (src/sound/webm.h):
 * vendored nestegg (EBML/Matroska) demuxer, libopus, libvorbis. Same calls the player makes: init_file, format,
 * length, read, seek, read. The API takes a path only, so the input goes to a scratch file. */
#define MA_NO_DEVICE_IO
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include "sound/webm.h"
#include "scratch.h"

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
        char path[1024];
        scratch_write("in.webm", d, n, path, sizeof path);
        static ma_webm dec;
        memset(&dec, 0, sizeof dec);
        if (ma_webm_init_file(path, NULL, NULL, &dec) == MA_SUCCESS) {
                ma_format f; ma_uint32 ch = 0, sr = 0; ma_channel map[MA_MAX_CHANNELS];
                ma_uint64 len = 0, got = 0, cur = 0;
                ma_webm_get_data_format(&dec, &f, &ch, &sr, map, MA_MAX_CHANNELS);
                ma_webm_get_length_in_pcm_frames(&dec, &len);
                static float buf[4096 * MA_MAX_CHANNELS * 2];
                for (int i = 0; i < 8; i++)
                        if (ma_webm_read_pcm_frames(&dec, buf, 1024, &got) != MA_SUCCESS || !got) break;
                ma_webm_get_cursor_in_pcm_frames(&dec, &cur);
                /* Seek target from the input's last bytes, so seeking is fuzzed too. */
                ma_uint64 target = n >= 2 ? ((ma_uint64)d[n - 2] << 8 | d[n - 1]) * 64 : 0;
                if (ma_webm_seek_to_pcm_frame(&dec, target) == MA_SUCCESS)
                        ma_webm_read_pcm_frames(&dec, buf, 1024, &got);
                ma_webm_uninit(&dec, NULL);
        }
        unlink(path);
        return 0;
}
