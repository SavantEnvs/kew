#!/usr/bin/env bash
#
# mayhem/test.sh — kew has no upstream test suite, so this is a known-answer oracle over kew's own
# decoders and LRC parser, run through mayhem/out/kew_selftest (built by build.sh, normal flags).
# The ALAC check is exact: kew's decode must equal ffmpeg's reference decode sample for sample.
# exit 0 = pass. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}


T="$SRC/mayhem/out/kew_selftest"
D="$SRC/mayhem/testdata"
[ -x "$T" ] || { echo "missing $T — run mayhem/build.sh" >&2; emit_ctrf kew-selftest 0 1; exit 1; }

passed=0; failed=0
check() {  # check <name> <expected output> <command...>
  local name="$1" want="$2"; shift 2
  local got; got="$("$@" 2>/dev/null)"
  if [ "$got" = "$want" ]; then passed=$((passed+1)); echo "PASS $name"
  else failed=$((failed+1)); echo "FAIL $name"; diff <(printf '%s\n' "$want") <(printf '%s\n' "$got") | sed 's/^/    /'; fi
}

check alac-lossless 'samples=44100 ref=44100 mismatches=0' "$T" pcmcmp "$D/alac.m4a" "$D/alac.s16le"
check alac-decode 'format=32-bit IEEE Floating Point channels=2 rate=44100 length=22050
frames=22050 peak=0.5
seek=0 cursor=2000' "$T" decode m4a "$D/alac.m4a"
check aac-m4a 'format=32-bit IEEE Floating Point channels=2 rate=8000 length=5024
frames=4096 peak=1.0
seek=0 cursor=2000' "$T" decode m4a "$D/aac.m4a"
check aac-fragmented-m4a 'format=32-bit IEEE Floating Point channels=2 rate=8000 length=5024
frames=4096 peak=1.0
seek=0 cursor=2000' "$T" decode m4a "$D/frag.m4a"
check webm-opus 'format=32-bit IEEE Floating Point channels=2 rate=48000 length=24072
frames=24576 peak=0.4
seek=0 cursor=0' "$T" decode webm "$D/opus.webm"
check webm-vorbis 'format=32-bit IEEE Floating Point channels=1 rate=8000 length=8256
frames=8192 peak=0.6
seek=0 cursor=2000' "$T" decode webm "$D/vorbis.webm"
check lrc 'count=4 timed=1 karaoke=1
1.50|0|First line
3.25|0|Second line
5.00|0|[00:07.00]Repeated line
9.10|2|Kara oke' "$T" lrc "$D/song.mp3"

emit_ctrf kew-selftest "$passed" "$failed"
