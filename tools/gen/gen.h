/*
 * gen.h - configforge-gen's two commands, in contract.c.
 *
 * Both read a JSON Schema contract; neither reads a report.
 */

#ifndef CONFIGFORGE_GEN_H
#define CONFIGFORGE_GEN_H

#include <configforge/cf_flatten.h>
#include <configforge/cf_keyspace.h>
#include <configforge/cf_serialize.h>

/* A document read as a configuration file is - its format named by
   `as`, else from its name and then its content (D-73) - into `ks`,
   resolved; `path` "-" is standard input. 0 when read, else the exit
   code (2) with the reason on stderr. `used`, when not NULL, is the
   codec that read it. */
int gen_read_keys(const char* path, const char* as, cf_arena* ar, cf_keyspace* ks,
                  const cf_codec** used);

/* Files read against a JSON Schema contract, as a program built from
   it would read them (contract.c). argv is what follows `check`. */
int gen_check(int argc, char** argv);

/* The contract as code (contract.c), in the language `--lang` names, C
   by default. argv is what follows the program's name: --schema, --lang,
   --name, --out. */
int gen_code(int argc, char** argv);

/* A JSON Schema contract drafted from a sample configuration file
   (contract.c, D-96). argv is what follows `schema`: --from, --as,
   --title, --out. */
int gen_schema(int argc, char** argv);

#endif /* CONFIGFORGE_GEN_H */
