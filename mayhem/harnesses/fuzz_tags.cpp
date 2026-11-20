/* Tag, cover-art and embedded-lyrics extraction (src/loader/tagLibWrapper.cpp extractTags):
 * kew's own code over TagLib — base64/FLAC picture blocks, Ogg/Opus/MP3/FLAC/WAV/MP4 cover
 * extraction, SYLT/USLT/Vorbis/MP4 lyrics, replaygain parsing, comment URL extraction.
 * extractTags picks the cover extractor from the file extension, so the first input byte picks
 * the extension and the rest is the file. Cover files land in a per-process scratch dir that is
 * emptied after every input. */
#include <stdint.h>
#include <stddef.h>
#include "loader/lyrics.h"         /* first: tagLibWrapper.h includes glib inside extern "C" */
#include "loader/tagLibWrapper.h"
#include "scratch.h"

static const char *kExt[] = {"mp3", "flac", "ogg", "opus", "m4a", "wav", "mp4", "aac"};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
        if (n < 1) return -1;
        /* extractTags copies up to KEW_PATH_MAX bytes into coverFilePath (the player passes
         * SongData.cover_art_path, which is KEW_PATH_MAX). */
        char name[32], path[1024], cover[KEW_PATH_MAX];
        snprintf(name, sizeof name, "in.%s", kExt[d[0] % (sizeof kExt / sizeof kExt[0])]);
        scratch_write(name, d + 1, n - 1, path, sizeof path);
        snprintf(cover, sizeof cover, "%s/cover", scratch_dir());
        static TagSettings tags;
        double duration = 0;
        Lyrics *lyrics = NULL;
        extractTags(path, &tags, &duration, cover, &lyrics, true);
        if (lyrics) freeLyrics(lyrics);
        scratch_clear();
        return 0;
}
