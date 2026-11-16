/*
 * front.h - what main.c asks of the file that runs a program.
 */
#ifndef CONFIGFORGE_FRONT_H
#define CONFIGFORGE_FRONT_H

/* Runs `program` (configforge-<command>) with argv[2..argc-1] after
   its name, found beside this program first and then on the PATH,
   and returns its exit code; or prints why on stderr and returns 2
   when it is not found or cannot be started. On POSIX the program
   replaces this one and this returns only on failure. */
int front_run(const char* program, int argc, char** argv);

#endif /* CONFIGFORGE_FRONT_H */
