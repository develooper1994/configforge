/*
 * run_win32.c - finds configforge-<command>.exe and runs it.
 *
 * Everything is UTF-16, so an argument outside the code page reaches
 * the program whole: the program's own command line is read with
 * CommandLineToArgvW and written again by the rule that function reads
 * back. Found beside this program first, then on the PATH; not in the
 * current directory unless the PATH names it. _wspawnv is not used: it
 * joins the arguments with spaces and quotes nothing.
 */

#include "configforge/cf_platform.h"

#if CF_PLATFORM_WIN32

#include "front.h"

#include <windows.h>

#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define FRONT_PATH_LIMIT 32768

static int out_of_memory(void) {
    fputs("configforge: out of memory\n", stderr);
    return 2;
}

static int cannot_run(const char* program) {
    fprintf(stderr, "configforge: cannot run %s\n", program);
    return 2;
}

/* `program` is "configforge-" and the name and ".exe". */
static int not_found(const char* program) {
    fprintf(stderr, "configforge: '%.*s' is not a configforge command ",
            (int)strlen(program) - 16, program + 12);
    fprintf(stderr, "(no %s beside this program or on the PATH); ", program);
    fputs("configforge help lists them\n", stderr);
    return 2;
}

/* Ctrl+C and Ctrl+Break are the program's to answer, not this one's:
   this one stays to wait for it and return what it exits with. A
   handler routine and not "ignore Ctrl+C" (SetConsoleCtrlHandler with
   NULL), which the program would inherit. */
static BOOL WINAPI keep_waiting(DWORD event) {
    return (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) ? TRUE : FALSE;
}

/* `program` in UTF-16, or NULL when it cannot be converted. */
static wchar_t* to_wide(const char* s) {
    int      n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
    wchar_t* w;
    if (n <= 0) {
        return NULL;
    }
    w = (wchar_t*)malloc((size_t)n * sizeof *w);
    if (w == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, w, n) <= 0) {
        free(w);
        return NULL;
    }
    return w;
}

/* The directory of this program, without its last `\`; "" when it has
   none; NULL when it cannot be had. */
static wchar_t* own_dir(void) {
    DWORD    size = 260;
    wchar_t* buf  = NULL;
    wchar_t* cut;
    wchar_t* p;

    for (;;) {
        DWORD    got;
        wchar_t* bigger = (wchar_t*)realloc(buf, (size_t)size * sizeof *buf);
        if (bigger == NULL) {
            free(buf);
            return NULL;
        }
        buf = bigger;
        got = GetModuleFileNameW(NULL, buf, size);
        if (got == 0) {
            free(buf);
            return NULL;
        }
        if (got < size) {
            break;
        }
        if (size >= FRONT_PATH_LIMIT) {
            free(buf);
            return NULL;
        }
        size *= 2;
    }
    cut = buf;
    for (p = buf; *p != L'\0'; p++) {
        if (*p == L'\\') {
            cut = p;
        }
    }
    *cut = L'\0';
    return buf;
}

/* `<dir>\<program>` for the first `n` characters of `dir`, malloc'd;
   no second `\` where the directory ends with one. */
static wchar_t* join(const wchar_t* dir, size_t n, const wchar_t* program) {
    size_t   plen = wcslen(program);
    int      bar  = (n > 0 && (dir[n - 1] == L'\\' || dir[n - 1] == L'/')) ? 0 : 1;
    wchar_t* path = (wchar_t*)malloc((n + (size_t)bar + plen + 1) * sizeof *path);
    if (path == NULL) {
        return NULL;
    }
    wmemcpy(path, dir, n);
    if (bar) {
        path[n] = L'\\';
    }
    wmemcpy(path + n + (size_t)bar, program, plen + 1);
    return path;
}

/* A file that exists and is not a directory. */
static int is_file(const wchar_t* path) {
    DWORD attrs = GetFileAttributesW(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return 0;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) ? 0 : 1;
}

/* Tries `<dir>\<program>`: 1 found (and *path set), 0 not there, -1
   out of memory. */
static int try_dir(const wchar_t* dir, size_t n, const wchar_t* program,
                   wchar_t** path) {
    wchar_t* candidate = join(dir, n, program);
    if (candidate == NULL) {
        return -1;
    }
    if (!is_file(candidate)) {
        free(candidate);
        return 0;
    }
    *path = candidate;
    return 1;
}

/* Beside this program, then each entry of the PATH: 1 found (and *path
   set), 0 not found, -1 out of memory. */
static int find_program(const wchar_t* program, wchar_t** path) {
    wchar_t* dir = own_dir();
    wchar_t* env;
    wchar_t* p;
    DWORD    n;
    int      r;

    if (dir == NULL) {
        return -1;
    }
    r = (dir[0] != L'\0') ? try_dir(dir, wcslen(dir), program, path) : 0;
    free(dir);
    if (r != 0) {
        return r;
    }

    n = GetEnvironmentVariableW(L"PATH", NULL, 0);
    if (n == 0) {
        return 0;
    }
    env = (wchar_t*)malloc((size_t)n * sizeof *env);
    if (env == NULL) {
        return -1;
    }
    if (GetEnvironmentVariableW(L"PATH", env, n) == 0) {
        free(env);
        return 0;
    }
    for (p = env; *p != L'\0';) {
        wchar_t* end   = wcschr(p, L';');
        size_t   len   = (end != NULL) ? (size_t)(end - p) : wcslen(p);
        wchar_t* entry = p;
        p += len;
        if (*p == L';') {
            p++;
        }
        /* A pair of quotes around an entry is not part of it. */
        if (len >= 2 && entry[0] == L'"' && entry[len - 1] == L'"') {
            entry++;
            len -= 2;
        }
        if (len == 0) {
            continue;
        }
        r = try_dir(entry, len, program, path);
        if (r != 0) {
            free(env);
            return r;
        }
    }
    free(env);
    return 0;
}

/* One argument as CommandLineToArgvW reads it back, into `out` when it
   is not NULL; the number of characters, with none for a NUL. An
   argument that is not empty and has no space, tab, newline, vertical
   tab or quote is written as it is; any other inside quotes, where a
   run of n backslashes before a quote is 2n+1 backslashes and the
   quote, a run at the end 2n backslashes, and any other run as it is. */
static size_t put_arg(wchar_t* out, const wchar_t* a) {
    size_t         len = 0;
    size_t         slashes;
    const wchar_t* p;
    int            plain = (a[0] != L'\0') ? 1 : 0;

    for (p = a; *p != L'\0'; p++) {
        if (*p == L' ' || *p == L'\t' || *p == L'\n' || *p == L'\v' || *p == L'"') {
            plain = 0;
        }
    }
    if (plain) {
        len = wcslen(a);
        if (out != NULL) {
            wmemcpy(out, a, len);
        }
        return len;
    }
    if (out != NULL) {
        out[len] = L'"';
    }
    len++;
    p = a;
    while (*p != L'\0') {
        slashes = 0;
        while (*p == L'\\') {
            slashes++;
            p++;
        }
        if (*p == L'\0') {
            /* A run at the end: doubled, before the closing quote. */
            slashes *= 2;
        } else if (*p == L'"') {
            slashes = slashes * 2 + 1;
        }
        while (slashes > 0) {
            if (out != NULL) {
                out[len] = L'\\';
            }
            len++;
            slashes--;
        }
        if (*p != L'\0') {
            if (out != NULL) {
                out[len] = *p;
            }
            len++;
            p++;
        }
    }
    if (out != NULL) {
        out[len] = L'"';
    }
    len++;
    return len;
}

/* The command line: `path`, then each of the `n` arguments, quoted by
   the rule above, malloc'd; NULL when out of memory. */
static wchar_t* build_command_line(const wchar_t* path, wchar_t** args, int n) {
    size_t   size = put_arg(NULL, path) + 1;
    size_t   at;
    wchar_t* line;
    int      i;

    for (i = 0; i < n; i++) {
        size += 1 + put_arg(NULL, args[i]);
    }
    line = (wchar_t*)malloc(size * sizeof *line);
    if (line == NULL) {
        return NULL;
    }
    at = put_arg(line, path);
    for (i = 0; i < n; i++) {
        line[at++] = L' ';
        at += put_arg(line + at, args[i]);
    }
    line[at] = L'\0';
    return line;
}

/* Runs `path` with this program's own arguments after the first two
   (this program and the command), waits and returns its exit code. */
static int spawn(const wchar_t* path, const char* program) {
    STARTUPINFOW        si;
    PROCESS_INFORMATION pi;
    wchar_t**           args;
    wchar_t*            line;
    DWORD               code = 2;
    int                 n    = 0;

    args = CommandLineToArgvW(GetCommandLineW(), &n);
    if (args == NULL) {
        return cannot_run(program);
    }
    if (n < 2) {
        LocalFree(args);
        return cannot_run(program);
    }
    line = build_command_line(path, args + 2, n - 2);
    LocalFree(args);
    if (line == NULL) {
        return out_of_memory();
    }

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    memset(&pi, 0, sizeof pi);
    (void)SetConsoleCtrlHandler(keep_waiting, TRUE);
    if (!CreateProcessW(path, line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        free(line);
        return cannot_run(program);
    }
    free(line);
    (void)WaitForSingleObject(pi.hProcess, INFINITE);
    if (!GetExitCodeProcess(pi.hProcess, &code)) {
        code = 2;
    }
    (void)CloseHandle(pi.hThread);
    (void)CloseHandle(pi.hProcess);
    return (int)code;
}

int front_run(const char* program, int argc, char** argv) {
    wchar_t* wide;
    wchar_t* path = NULL;
    int      found;
    int      rc;

    (void)argc;
    (void)argv;
    wide = to_wide(program);
    if (wide == NULL) {
        return cannot_run(program);
    }
    found = find_program(wide, &path);
    free(wide);
    if (found < 0) {
        return out_of_memory();
    }
    if (found == 0) {
        return not_found(program);
    }
    rc = spawn(path, program);
    free(path);
    return rc;
}

#else

typedef int front_run_win32_unused;

#endif /* CF_PLATFORM_WIN32 */
