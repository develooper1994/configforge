/* Fixture for cf-detect-platform: not compiled, not formatted. */
// ruleid: cf-detect-platform
#ifdef _WIN32
int a;
#endif
// ruleid: cf-detect-platform
#if defined(__linux__) || defined(CF_X)
int b;
#endif
// ruleid: cf-detect-platform
#if defined(__APPLE__)
int c;
#endif
// ok: cf-detect-platform
#ifdef CF_ATOMIC_WIN32
int d;
#endif
// ok: cf-detect-platform
#ifdef CF_PLATFORM_FREEBSD_LIKE
int e;
#endif
