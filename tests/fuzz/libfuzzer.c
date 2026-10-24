/*
 * libfuzzer.c - coverage-guided fuzzing of every reader of untrusted
 * bytes, with libFuzzer.
 *
 * test_fuzz.c mutates blindly from fixed seeds, which is reproducible on
 * every platform and good at regressions, and weak at reaching a path no
 * mutation of the seed happens to reach. libFuzzer keeps the inputs that
 * reached new code and mutates those, so it walks into the parser rather
 * than around it. The two share everything but the loop: this file is
 * test_fuzz.c's readers, fed by libFuzzer instead of by xorshift, and
 * its seeds are test_fuzz.c's corpora (written by fuzz_seeds).
 *
 * One binary per reader, chosen by CF_FUZZ_TARGET, because libFuzzer
 * measures coverage per process and a binary that feeds every reader
 * spends its time on whichever is cheapest.
 */

#define CF_LIBFUZZER 1
#include "../unit/test_fuzz.c"

int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size);

int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
    static blob v;

    v.n = (size < MAX_INPUT) ? size : MAX_INPUT;
    if (v.n > 0) {
        memcpy(v.b, data, v.n);
    }
#if CF_FUZZ_TARGET == 1
    feed_codec("json", &v);
#elif CF_FUZZ_TARGET == 2
    feed_codec("ini", &v);
#elif CF_FUZZ_TARGET == 3
    feed_codec("toml", &v);
#elif CF_FUZZ_TARGET == 4
    feed_dotenv(&v);
#elif CF_FUZZ_TARGET == 5
    feed_image(&v);
#elif CF_FUZZ_TARGET == 6
    feed_replay(&v);
#elif CF_FUZZ_TARGET == 7
    feed_argv(&v);
#elif CF_FUZZ_TARGET == 8
    feed_codec("yaml", &v);
#elif CF_FUZZ_TARGET == 9
    feed_codec("xml", &v);
#else
#error "CF_FUZZ_TARGET names no reader"
#endif
    return 0;
}
