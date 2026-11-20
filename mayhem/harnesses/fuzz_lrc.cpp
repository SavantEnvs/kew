/* LRC sidecar lyrics (src/loader/lyrics.cpp loadLyricsFromLRC): timestamps, timed lines,
 * enhanced/karaoke word timing, untimed fallback. The player looks for <song>.lrc next to the
 * song, so the input is written as song.lrc and the song path song.mp3 is passed. */
#include <stdint.h>
#include <stddef.h>
#include "loader/lyrics.h"
#include "scratch.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
        char lrc[1024], song[1024];
        scratch_write("song.lrc", d, n, lrc, sizeof lrc);
        snprintf(song, sizeof song, "%s/song.mp3", scratch_dir());
        static TagSettings meta;
        static SongData sd;
        memset(&sd, 0, sizeof sd);
        memset(&meta, 0, sizeof meta);
        strcpy(meta.title, "song");
        sd.metadata = &meta;
        Lyrics *l = loadLyricsFromLRC(song, &sd);
        if (l) freeLyrics(l);
        unlink(lrc);
        return 0;
}
