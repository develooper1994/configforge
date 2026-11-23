/* Fixture for cf-detect-compiler: not compiled, not formatted. */
// ruleid: cf-detect-compiler
#if defined(__GNUC__)
int a;
#endif
// ruleid: cf-detect-compiler
#if _MSC_VER >= 1900
int b;
#endif
// ruleid: cf-detect-compiler
#if __STDC_VERSION__ >= 199901L
int c;
#endif
// ok: cf-detect-compiler
#if defined(CF_COMPILER_GNU) || defined(MY__GNUC__X)
int d;
#endif
// ok: cf-detect-compiler
#ifdef CF_STDC_VERSION_ABOVE_89
int e;
#endif
