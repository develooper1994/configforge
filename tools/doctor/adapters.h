/*
 * adapters.h - Doctor's foreign-ecosystem adapters.
 *
 * Each reads an ecosystem's own origin data and writes a provenance
 * report; none of them resolves anything. See Doctor.md, section 12.
 */

#ifndef CONFIGFORGE_DOCTOR_ADAPTERS_H
#define CONFIGFORGE_DOCTOR_ADAPTERS_H

/* `git config --list --show-origin --show-scope` on stdin, a report on
   stdout. Returns Doctor's exit code. */
int doctor_import_git(int argc, char** argv);

/* Spring Boot's /actuator/env JSON on stdin, a report on stdout. */
int doctor_import_spring(int argc, char** argv);

/* .NET's IConfigurationRoot.GetDebugView() text on stdin, a report on
   stdout. */
int doctor_import_dotnet(int argc, char** argv);

#endif /* CONFIGFORGE_DOCTOR_ADAPTERS_H */
