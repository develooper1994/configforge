/*
 * codec.h - configforge-codec's commands.
 *
 * The program is the serialization library and a command line around
 * it: it reads documents and never a configuration, and includes
 * configforge/cf_serialize.h and nothing else of the library. Each
 * command takes what follows its name and returns the exit code.
 */

#ifndef CONFIGFORGE_CODEC_H
#define CONFIGFORGE_CODEC_H

#include "configforge/cf_serialize.h"

/* Every built-in codec, its extensions and its kind (formats.c). */
int codec_formats(int argc, char** argv);
/* The format of a file and how it was told (detect.c). */
int codec_detect(int argc, char** argv);
/* Whether each file is well formed in its format (detect.c). */
int codec_check(int argc, char** argv);

/* `path` read whole ("-" is standard input, binary) into `ar`, and
   its codec: `as` by name when not NULL, else from its name and then
   its content (cf_codec_detect, D-73). 0 when read, with *out and
   *how set; else the exit code (1 or 2) with the reason on stderr. */
int codec_read(const char* path, const char* as, cf_arena* ar, cf_blob* out,
               const cf_codec** codec, cf_detect* how);

#endif /* CONFIGFORGE_CODEC_H */
