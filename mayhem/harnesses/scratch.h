/* Per-process scratch directory for file-path-only APIs ($TMPDIR, fallback /tmp). */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
static char g_scratch[512];
static const char *scratch_dir(void) {
        if (!g_scratch[0]) {
                const char *t = getenv("TMPDIR");
                snprintf(g_scratch, sizeof g_scratch, "%s/kewfz_XXXXXX", (t && *t) ? t : "/tmp");
                if (!mkdtemp(g_scratch)) abort();
        }
        return g_scratch;
}
/* Remove every file in the scratch dir so each input starts from the same state. */
static void scratch_clear(void) {
        DIR *d = opendir(scratch_dir());
        if (!d) return;
        struct dirent *e;
        char p[1024];
        while ((e = readdir(d))) {
                if (e->d_name[0] == '.') continue;
                snprintf(p, sizeof p, "%s/%s", g_scratch, e->d_name);
                unlink(p);
        }
        closedir(d);
}
static int scratch_write(const char *name, const unsigned char *d, size_t n, char *out, size_t outsz) {
        snprintf(out, outsz, "%s/%s", scratch_dir(), name);
        FILE *f = fopen(out, "wb");
        if (!f) abort();
        if (n && fwrite(d, 1, n, f) != n) abort();
        fclose(f);
        return 0;
}
