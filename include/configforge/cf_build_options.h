/*
 * cf_build_options.h - which features this build has.
 *
 * Two versions of this file exist and the difference is the point.
 *
 * This one, in the source tree, holds *defaults* behind #ifndef guards.
 * It is what someone compiling the sources themselves gets, and what
 * the amalgamation carries, so a consumer can set any of these on the
 * command line.
 *
 * The other is generated at configure time, holds the values this
 * particular build actually used, defines them unconditionally, and is
 * the one that gets installed. For a library that is already compiled,
 * a consumer overriding one of these is not a choice - it is the bug.
 * The public headers change shape with them: CF_WITH_REAL decides
 * whether cf_value's union has a double in it, so a consumer who
 * disagrees with the library about it has a different sizeof and a
 * quiet kind of corruption.
 *
 * That hazard was real. The defaults used to live in cf_port.h, a
 * consumer compiling with a plain -I saw CF_WITH_REAL as 1 against a
 * library built with 0, and the link error that revealed it was the
 * lucky outcome.
 */

#ifndef CONFIGFORGE_CF_BUILD_OPTIONS_H
#define CONFIGFORGE_CF_BUILD_OPTIONS_H

#ifndef CF_WITH_REAL
#define CF_WITH_REAL 1
#endif
/* A static library unless the build says otherwise; see CF_API. */
#ifndef CF_SHARED
#define CF_SHARED 0
#endif
#ifndef CF_WITH_INI
#define CF_WITH_INI 1
#endif
#ifndef CF_WITH_JSON
#define CF_WITH_JSON 1
#endif
#ifndef CF_WITH_TOML
#define CF_WITH_TOML 1
#endif
#ifndef CF_WITH_XML
#define CF_WITH_XML 0
#endif
#ifndef CF_WITH_SQLITE
#define CF_WITH_SQLITE 0
#endif

/* ------------------------------------------------------------------ *
 * Limits. Each one is a fixed buffer somewhere, so each one is stack:
 * a larger limit is a larger frame in every function that holds one,
 * and a smaller one is a smaller frame and a shorter key. The trade is
 * the builder's to make (D-37); the build measures what it cost and
 * says so, against CF_STACK_BUDGET.
 *
 *   CF_KEY_MAX      longest key, terminator included. Held by every
 *                   error, by the flattener, and by the decoders that
 *                   name the path they are in.
 *   CF_MESSAGE_MAX  longest error message. Every cf_error holds one,
 *                   and a builder call keeps a cf_error on its frame.
 *   CF_PATH_MAX     longest file path. The directory source and the
 *                   platform layer hold two or three at once.
 *   CF_MAX_DEPTH    deepest nesting a document may have. The decoders
 *                   and the flattener keep a small frame per level.
 * ------------------------------------------------------------------ */

#ifndef CF_KEY_MAX
#define CF_KEY_MAX 256
#endif
#ifndef CF_MESSAGE_MAX
#define CF_MESSAGE_MAX 256
#endif
#ifndef CF_PATH_MAX
#define CF_PATH_MAX 512
#endif
#ifndef CF_MAX_DEPTH
#define CF_MAX_DEPTH 32
#endif

#endif /* CONFIGFORGE_CF_BUILD_OPTIONS_H */
