/* kew_selftest — the functional oracle mayhem/test.sh runs. Built with NORMAL flags (no
 * sanitizers) against kew's own decoders and LRC parser; prints what they decoded/parsed so
 * test.sh can compare it with known answers:
 *   kew_selftest decode <m4a|webm> <file>        format, channels, rate, length, frames, hash
 *   kew_selftest pcmcmp <file.m4a> <ref.s16le>   ALAC is lossless: decoded samples == source
 *   kew_selftest lrc <song.mp3>                  timed lines parsed from the sibling song.lrc */
#define MA_NO_DEVICE_IO
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include "sound/m4a.h"
#include "sound/webm.h"
#include "loader/lyrics.h"
#include <stdio.h>
#include <string.h>

typedef struct {
        int kind; /* 0 m4a, 1 webm */
        m4a_decoder m4a;
        ma_webm webm;
} dec_t;

static ma_result d_init(dec_t *d, const char *p) {
        return d->kind ? ma_webm_init_file(p, NULL, NULL, &d->webm) : m4a_decoder_init_file(p, NULL, NULL, &d->m4a);
}
static void d_fmt(dec_t *d, ma_format *f, ma_uint32 *ch, ma_uint32 *sr) {
        ma_channel map[MA_MAX_CHANNELS];
        if (d->kind) ma_webm_get_data_format(&d->webm, f, ch, sr, map, MA_MAX_CHANNELS);
        else m4a_decoder_get_data_format(&d->m4a, f, ch, sr, map, MA_MAX_CHANNELS);
}
static ma_uint64 d_len(dec_t *d) {
        ma_uint64 l = 0;
        if (d->kind) ma_webm_get_length_in_pcm_frames(&d->webm, &l);
        else m4a_decoder_get_length_in_pcm_frames(&d->m4a, &l);
        return l;
}
static ma_uint64 d_read(dec_t *d, void *buf, ma_uint64 n) {
        ma_uint64 got = 0;
        ma_result r = d->kind ? ma_webm_read_pcm_frames(&d->webm, buf, n, &got) : m4a_decoder_read_pcm_frames(&d->m4a, buf, n, &got);
        return (r == MA_SUCCESS || r == MA_AT_END) ? got : 0;
}
static ma_result d_seek(dec_t *d, ma_uint64 f) {
        return d->kind ? ma_webm_seek_to_pcm_frame(&d->webm, f) : m4a_decoder_seek_to_pcm_frame(&d->m4a, f);
}
static ma_uint64 d_cursor(dec_t *d) {
        ma_uint64 c = 0;
        if (d->kind) ma_webm_get_cursor_in_pcm_frames(&d->webm, &c);
        else m4a_decoder_get_cursor_in_pcm_frames(&d->m4a, &c);
        return c;
}
static void d_uninit(dec_t *d) {
        if (d->kind) ma_webm_uninit(&d->webm, NULL); else m4a_decoder_uninit(&d->m4a, NULL);
}

static dec_t D;
static unsigned char buf[1024 * 8 * 8];

static int decode(const char *kind, const char *path) {
        D.kind = !strcmp(kind, "webm");
        if (d_init(&D, path) != MA_SUCCESS) { printf("init failed\n"); return 1; }
        ma_format f; ma_uint32 ch = 0, sr = 0;
        d_fmt(&D, &f, &ch, &sr);
        printf("format=%s channels=%u rate=%u length=%llu\n", ma_get_format_name(f), ch, sr, (unsigned long long)d_len(&D));
        ma_uint64 total = 0, got; double peak = 0;
        while ((got = d_read(&D, buf, 1024)) > 0) {
                for (ma_uint64 i = 0; i < got * ch; i++) {
                        double v = f == ma_format_f32 ? ((float *)buf)[i] : f == ma_format_s16 ? ((short *)buf)[i] / 32768.0 : 0;
                        if (v < 0) v = -v;
                        if (v > peak) peak = v;
                }
                total += got;
        }
        /* Lossy codecs: report the peak coarsely (the source peaks at 0.5) */
        printf("frames=%llu peak=%.1f\n", (unsigned long long)total, peak);
        ma_result r = d_seek(&D, 2000);
        printf("seek=%d cursor=%llu\n", r, (unsigned long long)d_cursor(&D));
        d_uninit(&D);
        return 0;
}

static int pcmcmp(const char *path, const char *ref) {
        FILE *rf = fopen(ref, "rb");
        if (!rf) return 1;
        static short refpcm[1 << 20];
        size_t nref = fread(refpcm, 2, sizeof refpcm / 2, rf);
        fclose(rf);
        D.kind = 0;
        if (d_init(&D, path) != MA_SUCCESS) { printf("init failed\n"); return 1; }
        ma_format f; ma_uint32 ch = 0, sr = 0;
        d_fmt(&D, &f, &ch, &sr);
        size_t idx = 0, mism = 0; ma_uint64 got;
        while ((got = d_read(&D, buf, 1024)) > 0) {
                for (ma_uint64 i = 0; i < got * ch; i++, idx++) {
                        int s;
                        if (f == ma_format_s16) s = ((short *)buf)[i];
                        else if (f == ma_format_s32) s = ((int *)buf)[i] >> 16;
                        else if (f == ma_format_f32) s = (int)(((float *)buf)[i] * 32768.0f + (((float *)buf)[i] >= 0 ? 0.5f : -0.5f));
                        else s = 0x7fffffff;
                        if (idx >= nref || s != refpcm[idx]) mism++;
                }
        }
        printf("samples=%zu ref=%zu mismatches=%zu\n", idx, nref, mism);
        d_uninit(&D);
        return 0;
}

static int lrc(const char *song) {
        static TagSettings meta;
        static SongData sd;
        strcpy(meta.title, "song");
        sd.metadata = &meta;
        Lyrics *l = loadLyricsFromLRC(song, &sd);
        if (!l) { printf("no lyrics\n"); return 1; }
        printf("count=%zu timed=%d karaoke=%d\n", l->count, l->isTimed, l->isKaraoke);
        for (size_t i = 0; i < l->count; i++)
                printf("%.2f|%d|%s\n", l->lines[i].timestamp, l->lines[i].numberOfTimestamps, l->lines[i].text ? l->lines[i].text : "(null)");
        freeLyrics(l);
        return 0;
}

int main(int argc, char **argv) {
        if (argc == 4 && !strcmp(argv[1], "decode")) return decode(argv[2], argv[3]);
        if (argc == 4 && !strcmp(argv[1], "pcmcmp")) return pcmcmp(argv[2], argv[3]);
        if (argc == 3 && !strcmp(argv[1], "lrc")) return lrc(argv[2]);
        fprintf(stderr, "usage: kew_selftest decode <m4a|webm> FILE | pcmcmp FILE REF | lrc SONG\n");
        return 2;
}
