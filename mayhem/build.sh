#!/usr/bin/env bash
# mayhem/build.sh — kew fuzz harnesses + the decoder oracle.
#
# kew is a terminal music player; its untrusted input is the media files it plays. The harnesses
# link kew's own parsing code (compiled here with $SANITIZER_FLAGS $DEBUG_FLAGS, the way upstream's
# Makefile compiles it: same -I/-D, gnu99 for libmp4, gnu++20 for C++) without the TUI/audio device:
#   fuzz_m4a   src/sound/m4a.h + include/libmp4 + include/alac + faad2   (MP4/AAC/ALAC/ADTS)
#   fuzz_webm  src/sound/webm.h + include/nestegg + libopus/libvorbis    (WebM)
#   fuzz_tags  src/loader/tagLibWrapper.cpp + lyrics.cpp over TagLib     (tags/cover/lyrics)
#   fuzz_lrc   src/loader/lyrics.cpp                                     (.lrc sidecar lyrics)
#   fuzz_theme     src/data/theme.c                                      (theme files)
#   fuzz_libcache  src/data/directorytree.c                              (library cache file)
#   fuzz_m3u       src/data/playlist.c                                   (m3u playlists)
# Outputs go to mayhem/out/ (gitignored via mayhem/.gitignore). Re-runnable, offline (apt deps only).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${SRC:=/mayhem}"
cd "$SRC"

OUT="$SRC/mayhem/out"
OBJ="$OUT/obj"
mkdir -p "$OBJ/fuzz" "$OBJ/test"

PKGS="glib-2.0 gio-2.0 opusfile opus vorbis vorbisfile ogg taglib"
INC="-Isrc -Iinclude -Iinclude/miniaudio -Iinclude/libmp4/src -Iinclude/libmp4/include -Iinclude/compat -Iinclude/compat/futils -Iinclude/nestegg -Imayhem/harnesses $(pkg-config --cflags $PKGS)"
LIBS="$(pkg-config --libs $PKGS) -lfaad -lm -lpthread -ldl"
DEFS="-DUSE_FAAD -DMP4_API_EXPORTS -D_ISOC11_SOURCE"

# compile <objdir> <flags> -- sources: one object per source, C or C++ by extension.
compile() {
  local dir="$1" flags="$2"; shift 2
  local s o
  for s in "$@"; do
    o="$dir/$(echo "$s" | tr '/' '_').o"
    case "$s" in
      *.cpp) $CXX $flags $DEFS $INC -std=gnu++20 -c "$s" -o "$o" ;;
      include/libmp4/*) $CC $flags $DEFS $INC -std=gnu99 -c "$s" -o "$o" ;;
      # alac.c sign-extends every decoded sample with SIGN_EXTENDED32 ((v << n) >> n), a left
      # shift of a negative value: UBSan shift-base fires on every valid ALAC stream. Relax only
      # that check, only here; ASan and the rest of UBSan stay on and halting.
      include/alac/alac.c) $CC $flags -fno-sanitize=shift-base $DEFS $INC -c "$s" -o "$o" ;;
      *) $CC $flags $DEFS $INC -c "$s" -o "$o" ;;
    esac
  done
}
obj() { local dir="$1"; shift; for s in "$@"; do printf '%s ' "$dir/$(echo "$s" | tr '/' '_').o"; done; }

LIBMP4=$(ls include/libmp4/src/*.c)
M4A_SRC="mayhem/harnesses/fuzz_m4a.c mayhem/harnesses/stubs.c include/alac/alac.c $LIBMP4"
WEBM_SRC="mayhem/harnesses/fuzz_webm.c include/nestegg/nestegg.c"
APP_SRC="src/data/theme.c src/data/directorytree.c src/ops/search_match.c src/utils/file.c src/utils/utils.c src/common/appstate.c src/common/common.c src/utils/k_log.c src/data/artists.c src/utils/term.c"
TAG_SRC="src/loader/tagLibWrapper.cpp src/loader/lyrics.cpp src/utils/utils.c mayhem/harnesses/stubs.c $LIBMP4"

# 1) Sanitized, instrumented project + harness objects.
FZ="$SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link"
compile "$OBJ/fuzz" "$FZ" $M4A_SRC $WEBM_SRC $TAG_SRC mayhem/harnesses/fuzz_tags.cpp mayhem/harnesses/fuzz_lrc.cpp
mkdir -p "$OBJ/app"   # app objects: real k_log/set_error_message, so not mixed with stubs.c
compile "$OBJ/app" "$FZ" $APP_SRC src/data/playlist.c mayhem/harnesses/stubs_app.c mayhem/harnesses/stubs_m3u.c \
  mayhem/harnesses/fuzz_theme.c mayhem/harnesses/fuzz_libcache.c mayhem/harnesses/fuzz_m3u.c
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$OBJ/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OBJ/standalone_main.o"

link() {  # link <name> <objects...>
  local name="$1"; shift
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$@" "$OBJ/lsan_off.o" $LIBS -o "$OUT/$name"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$OBJ/standalone_main.o" "$@" "$OBJ/lsan_off.o" $LIBS -o "$OUT/$name-standalone"
}
link fuzz_m4a  $(obj "$OBJ/fuzz" $M4A_SRC)
link fuzz_webm $(obj "$OBJ/fuzz" $WEBM_SRC mayhem/harnesses/stubs.c)
link fuzz_tags $(obj "$OBJ/fuzz" $TAG_SRC mayhem/harnesses/fuzz_tags.cpp)
link fuzz_lrc  $(obj "$OBJ/fuzz" $TAG_SRC mayhem/harnesses/fuzz_lrc.cpp)
link fuzz_theme    $(obj "$OBJ/app" $APP_SRC mayhem/harnesses/stubs_app.c mayhem/harnesses/fuzz_theme.c)
link fuzz_libcache $(obj "$OBJ/app" $APP_SRC mayhem/harnesses/stubs_app.c mayhem/harnesses/fuzz_libcache.c)
# playlist.c calls getTrackInfo (tagLibWrapper.cpp, which needs lyrics.cpp + libmp4): reuse those objects.
link fuzz_m3u      $(obj "$OBJ/app" $APP_SRC src/data/playlist.c mayhem/harnesses/stubs_m3u.c mayhem/harnesses/fuzz_m3u.c) \
                   $(obj "$OBJ/fuzz" src/loader/tagLibWrapper.cpp src/loader/lyrics.cpp $LIBMP4)

# 2) Oracle: kew's decoders + LRC parser with NORMAL flags (no sanitizers), driven by
#    mayhem/harnesses/kew_selftest.c, which mayhem/test.sh runs against committed samples.
TF="-O2 $COVERAGE_FLAGS"
TEST_SRC="mayhem/harnesses/kew_selftest.c mayhem/harnesses/stubs.c include/alac/alac.c include/nestegg/nestegg.c $LIBMP4 src/loader/lyrics.cpp src/utils/utils.c"
compile "$OBJ/test" "$TF" $TEST_SRC
$CXX $TF $(obj "$OBJ/test" $TEST_SRC) $LIBS -o "$OUT/kew_selftest"
echo "built: $(ls "$OUT" | grep -v obj | tr '\n' ' ')"
