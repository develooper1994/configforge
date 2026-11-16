/*
 * gen.h - configforge-gen's two commands, in contract.c.
 *
 * Both read a JSON Schema contract; neither reads a report.
 */

#ifndef CONFIGFORGE_GEN_H
#define CONFIGFORGE_GEN_H

/* Files read against a JSON Schema contract, as a program built from
   it would read them (contract.c). argv is what follows `check`. */
int gen_check(int argc, char** argv);

/* The contract as C calls (contract.c). argv is what follows the
   program's name: --schema, --name, --out. */
int gen_code(int argc, char** argv);

#endif /* CONFIGFORGE_GEN_H */
