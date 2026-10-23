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
#ifndef CF_WITH_INI
#define CF_WITH_INI 1
#endif
#ifndef CF_WITH_JSON
#define CF_WITH_JSON 1
#endif
#ifndef CF_WITH_TOML
#define CF_WITH_TOML 0
#endif
#ifndef CF_WITH_XML
#define CF_WITH_XML 0
#endif
#ifndef CF_WITH_SQLITE
#define CF_WITH_SQLITE 0
#endif

#endif /* CONFIGFORGE_CF_BUILD_OPTIONS_H */
