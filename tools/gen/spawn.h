/*
 * spawn.h - what gen.c asks of the file that finds and runs a generator
 * outside the tree (D-110): spawn_posix.c, spawn_win32.c, or spawn_none.c
 * where a program cannot run another.
 */
#ifndef CONFIGFORGE_GEN_SPAWN_H
#define CONFIGFORGE_GEN_SPAWN_H

#include <stddef.h>

/* Finds configforge-gen-<lang>: beside this program (the directory of
   argv[0] when it has a / or \, on Windows always the module's), then on
   the PATH, never the current directory. A malloc'd path, or NULL with
   *dir set to this program's directory (malloc'd; NULL when not known). */
char* spawn_find(const char* argv0, const char* lang, char** dir);

/* Runs `path` with the arguments argv[1..argc-1] but each name in `drop`
   (NULL-terminated; "--lang", "--schema") and the argument after it - the
   arguments are read in pairs from argv[1] - `input` (n bytes) on its
   standard input, its other two handles this program's; waits for it. 0
   when it ran to its end, *code its exit code (0 to 255); 1 when it could
   not be started; 2 when it was stopped, the signal (POSIX) or the code
   (Windows, a code above 255) in *code. */
int spawn_run(const char* path, int argc, char** argv, const char* const* drop,
              const char* input, size_t n, int* code);

#endif /* CONFIGFORGE_GEN_SPAWN_H */
