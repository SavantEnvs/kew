/* m3u playlists (src/data/playlist.c read_m3u_file): line splitting, CR/comment handling,
 * relative/absolute path resolution, de-duplication against the playlist, node creation.
 * Entries must name existing files to be enqueued, so the scratch dir holds a few empty song
 * files, created once and never modified; the playlist is emptied after every input. */
#include "data/playlist.h"
#include "scratch.h"
#include <sys/stat.h>

static void touch(const char *rel) {
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", scratch_dir(), rel);
        FILE *f = fopen(p, "wb");
        if (f) fclose(f);
}

int LLVMFuzzerInitialize(int *argc, char ***argv) {
        (void)argc; (void)argv;
        char p[1024];
        snprintf(p, sizeof p, "%s/sub", scratch_dir());
        mkdir(p, 0700);
        touch("a.mp3"); touch("b.flac"); touch("sub/c.opus"); touch("sub/d e.m4a");
        return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
        char path[1024];
        scratch_write("list.m3u", d, n, path, sizeof path);
        static PlayList pl;
        memset(&pl, 0, sizeof pl);
        pthread_mutex_init(&pl.mutex, NULL);
        read_m3u_file(path, &pl);
        empty_playlist(&pl);
        pthread_mutex_destroy(&pl.mutex);
        unlink(path);
        return 0;
}
