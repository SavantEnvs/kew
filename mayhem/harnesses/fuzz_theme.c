/* kew theme files (src/data/theme.c load_theme_from_file): section/key=value parsing, comment
 * stripping, colour values (hex, 256-colour index, named). The input is written as fuzz.theme in
 * a per-process scratch dir, which is what the loader reads (dir + file name). */
#include "data/theme.h"
#include "scratch.h"

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
        char path[1024];
        scratch_write("fuzz.theme", d, n, path, sizeof path);
        static Theme theme;
        load_theme_from_file(scratch_dir(), "fuzz.theme", &theme);
        unlink(path);
        return 0;
}
