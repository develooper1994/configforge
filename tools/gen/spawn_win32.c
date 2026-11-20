/*
 * spawn_win32.c - finds configforge-gen-<lang>.exe and runs it with the
 * contract on its standard input (D-110).
 *
 * Everything is UTF-16 inside, as the front door's run_win32.c: the
 * program's own command line is read with CommandLineToArgvW and written
 * again by the rule that function reads back, so an argument outside the
 * code page reaches the generator whole. Only `.exe` is looked for: not
 * PATHEXT and no `.cmd` or `.bat`, which CreateProcessW runs only through
 * cmd.exe and its quoting, where batch-file arguments have been injected
 * (2024); a generator written as a script needs an `.exe` to start it.
 */

#include "configforge/cf_platform.h"

#if CF_PLATFORM_WIN32

#include "spawn.h"

#include <windows.h>

#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define SPAWN_PATH_LIMIT 32768

/* Ctrl+C and Ctrl+Break are the generator's to answer, not this
   program's: this one stays to wait for it and return what it exits with.
   A handler routine and not "ignore Ctrl+C" (SetConsoleCtrlHandler with
   NULL), which the generator would inherit. */
static BOOL WINAPI keep_waiting(DWORD event) {
    return (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) ? TRUE : FALSE;
}

/* UTF-16 to UTF-8, malloc'd; NULL when it cannot be had. */
static char* to_utf8(const wchar_t* w) {
    int   n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char* s;

    if (n <= 0) {
        return NULL;
    }
    s = (char*)malloc((size_t)n);
    if (s == NULL) {
        return NULL;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL) <= 0) {
        free(s);
        return NULL;
    }
    return s;
}

/* UTF-8 to UTF-16, malloc'd; NULL when it cannot be had. */
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
        if (size >= SPAWN_PATH_LIMIT) {
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

/* `<dir>\<program>` for the first `n` characters of `dir`, malloc'd; no
   second `\` where the directory ends with one. */
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
    return ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) ? 0 : 1;
}

/* Tries `<dir>\<program>`: 1 found (and *path set), 0 not there, -1 no
   room. */
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

/* Each entry of the PATH: 1 found (and *path set), 0 not found, -1 no
   room. An empty entry is skipped, a pair of quotes around one removed. */
static int find_on_path(const wchar_t* program, wchar_t** path) {
    wchar_t* env;
    size_t   pos;
    size_t   total;
    DWORD    n = GetEnvironmentVariableW(L"PATH", NULL, 0);
    int      r;

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
    /* `total` characters before the NUL; every index below is < total. */
    total      = (size_t)n - 1;
    env[total] = L'\0';
    for (pos = 0; pos < total && env[pos] != L'\0';) {
        size_t start = pos;
        size_t len   = 0;

        while (start + len < total && env[start + len] != L'\0' &&
               env[start + len] != L';') {
            len++;
        }
        pos = start + len;
        if (pos < total && env[pos] == L';') {
            pos++;
        }
        if (len >= 2 && env[start] == L'"' && env[start + len - 1] == L'"') {
            start++;
            len -= 2;
        }
        if (len == 0) {
            continue;
        }
        r = try_dir(env + start, len, program, path);
        if (r != 0) {
            free(env);
            return r;
        }
    }
    free(env);
    return 0;
}

char* spawn_find(const char* argv0, const char* lang, char** dir) {
    static const char prefix[] = "configforge-gen-";
    size_t            llen     = strlen(lang);
    char*             narrow   = (char*)malloc(sizeof prefix + llen + 4);
    wchar_t*          program;
    wchar_t*          own;
    wchar_t*          found  = NULL;
    char*             result = NULL;
    int               r;

    (void)argv0;
    *dir = NULL;
    if (narrow == NULL) {
        return NULL;
    }
    memcpy(narrow, prefix, sizeof prefix - 1);
    memcpy(narrow + sizeof prefix - 1, lang, llen);
    memcpy(narrow + sizeof prefix - 1 + llen, ".exe", 5);
    program = to_wide(narrow);
    free(narrow);
    own = own_dir();
    if (program == NULL || own == NULL) {
        free(program);
        free(own);
        return NULL;
    }
    r = (own[0] != L'\0') ? try_dir(own, wcslen(own), program, &found) : 0;
    if (r == 0) {
        r = find_on_path(program, &found);
    }
    if (r > 0) {
        result = to_utf8(found);
    } else if (own[0] != L'\0') {
        *dir = to_utf8(own);
    }
    free(found);
    free(program);
    free(own);
    return result;
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

/* Whether the argument at `i` is one of the names in `drop`. */
static int is_dropped(const wchar_t* arg, const wchar_t* const* drop, size_t nd) {
    size_t k;

    for (k = 0; k < nd; k++) {
        if (wcscmp(arg, drop[k]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The command line: `path`, then the arguments `args[1..n-1]` but each
   dropped name and the one after it, quoted by the rule above, malloc'd;
   NULL when out of room. */
static wchar_t* build_command_line(const wchar_t* path, wchar_t** args, int n,
                                   const wchar_t* const* drop, size_t nd) {
    size_t   size = put_arg(NULL, path) + 1;
    size_t   at   = 0;
    wchar_t* line = NULL;
    int      pass;
    int      i;

    for (pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            line = (wchar_t*)malloc(size * sizeof *line);
            if (line == NULL) {
                return NULL;
            }
            at = put_arg(line, path);
        }
        for (i = 1; i < n; i += 2) {
            int j;

            if (is_dropped(args[i], drop, nd)) {
                continue;
            }
            for (j = i; j < n && j < i + 2; j++) {
                if (pass == 0) {
                    size += 1 + put_arg(NULL, args[j]);
                } else {
                    line[at++] = L' ';
                    at += put_arg(line + at, args[j]);
                }
            }
        }
    }
    line[at] = L'\0';
    return line;
}

/* `n` bytes to the pipe, in pieces a DWORD holds; any failure, a reader
   that has gone among them, ends the writing and is not reported: the
   generator's own code says how it went. */
static void write_all(HANDLE h, const char* p, size_t n) {
    size_t off = 0;

    while (off < n) {
        DWORD piece = (n - off > 0x40000000u) ? 0x40000000u : (DWORD)(n - off);
        DWORD wrote = 0;

        if (!WriteFile(h, p + off, piece, &wrote, NULL) || wrote == 0) {
            return;
        }
        off += wrote;
    }
}

/* Starts `line` with `rd` as its standard input and this program's own
   output and error; 1 started (and *pi set), 0 not. */
static int start(const wchar_t* path, wchar_t* line, HANDLE rd,
                 PROCESS_INFORMATION* pi) {
    STARTUPINFOW si;

    memset(&si, 0, sizeof si);
    si.cb         = sizeof si;
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdInput  = rd;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    memset(pi, 0, sizeof *pi);
    (void)SetConsoleCtrlHandler(keep_waiting, TRUE);
    return CreateProcessW(path, line, NULL, NULL, TRUE, 0, NULL, NULL, &si, pi) ? 1 : 0;
}

int spawn_run(const char* path, int argc, char** argv, const char* const* drop,
              const char* input, size_t n, int* code) {
    wchar_t*            wpath = to_wide(path);
    wchar_t**           args;
    wchar_t*            line;
    wchar_t*            names[8];
    size_t              nd = 0;
    size_t              i;
    SECURITY_ATTRIBUTES sa;
    HANDLE              rd = NULL;
    HANDLE              wr = NULL;
    PROCESS_INFORMATION pi;
    DWORD               status = 0;
    int                 count  = 0;
    int                 ok;

    (void)argv;
    if (wpath == NULL) {
        return 1;
    }
    /* The wide command line has the narrow argv's indexes. */
    args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (args == NULL || count != argc) {
        if (args != NULL) {
            LocalFree(args);
        }
        free(wpath);
        return 1;
    }
    for (; nd < 8 && drop[nd] != NULL; nd++) {
        names[nd] = to_wide(drop[nd]);
        if (names[nd] == NULL) {
            for (i = 0; i < nd; ++i) {
                free(names[i]);
            }
            LocalFree(args);
            free(wpath);
            return 1;
        }
    }
    line = build_command_line(wpath, args, count, (const wchar_t* const*)names, nd);
    LocalFree(args);
    for (i = 0; i < nd; ++i) {
        free(names[i]);
    }
    if (line == NULL) {
        free(wpath);
        return 1;
    }

    sa.nLength              = sizeof sa;
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle       = TRUE;
    if (!CreatePipe(&rd, &wr, &sa, 0)) {
        free(line);
        free(wpath);
        return 1;
    }
    (void)SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);
    ok = start(wpath, line, rd, &pi);
    free(line);
    free(wpath);
    (void)CloseHandle(rd);
    if (!ok) {
        (void)CloseHandle(wr);
        return 1;
    }

    write_all(wr, input, n);
    (void)CloseHandle(wr);
    (void)WaitForSingleObject(pi.hProcess, INFINITE);
    if (!GetExitCodeProcess(pi.hProcess, &status)) {
        status = 2;
    }
    (void)CloseHandle(pi.hThread);
    (void)CloseHandle(pi.hProcess);
    *code = (int)status;
    return (status > 255u) ? 2 : 0;
}

#else

typedef int spawn_win32_unused;

#endif /* CF_PLATFORM_WIN32 */
