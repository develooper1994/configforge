/* The size rule and the refusal reasons shared by the readers (D-107). */
#include <configforge/configforge.h>
#include <configforge/cf_serialize.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    const char* severity;
    const char* weakness;
} reason_class;

/* One row a reason, in the order of cf_reason.def. */
static const reason_class classes[] = {
    {"none", ""},           {"error", ""},          {"error", "CWE-176"},
    {"error", ""},          {"error", ""},          {"error", ""},
    {"error", ""},          {"error", ""},          {"warning", "CWE-674"},
    {"warning", "CWE-770"}, {"warning", "CWE-400"}, {"error", ""},
    {"error", ""},          {"error", ""},          {"warning", ""},
    {"error", "CWE-694"},   {"error", ""},          {"error", ""},
    {"warning", ""},        {"warning", ""}};

static const char* const names[] = {"none",
                                    "syntax",
                                    "utf-8",
                                    "encoding",
                                    "truncated",
                                    "trailing",
                                    "empty",
                                    "top-level",
                                    "depth",
                                    "too-long",
                                    "budget",
                                    "escape",
                                    "control-character",
                                    "number",
                                    "number-range",
                                    "duplicate-key",
                                    "key",
                                    "structure",
                                    "unsupported",
                                    "storage"};

static void check_place(const cf_error* e, int reason, unsigned line, unsigned col) {
    check(e->reason == reason, "the refusal's reason");
    check(e->where.file == NULL, "the error owns no file pointer");
    check(e->where.line == line && e->where.col == col, "the refusal's place");
}

static void check_tail(const unsigned char* bytes, size_t first, size_t end) {
    while (first < end && bytes[first] == 0xABu) {
        ++first;
    }
    check(first == end, "bytes beyond the caller's size stay untouched");
}

static void test_size(void) {
    const int    reason   = CF_REASON_SYNTAX;
    const size_t old_size = offsetof(cf_error, where);
    struct {
        cf_error      error;
        unsigned char tail[64];
    } buffer;
    unsigned char* bytes = (unsigned char*)&buffer;
    cf_error*      e     = &buffer.error;
    cf_error       copy  = CF_ERROR_INIT;
    size_t         i;
    int            last = CF_REASON_STORAGE;

    memset(bytes, 0xAB, sizeof buffer);
    e->size = old_size;
    cf_error_set(e, CF_E_SOURCE, "a", "doc", "refused");
    cf_error_place(e, reason, 3, 4);
    check_tail(bytes, old_size, sizeof buffer);

    /* A struct that grows its size says its blocks are empty: they are
       read where `size` reaches them, and nothing wrote them before. */
    memset(e->cfi_heap, 0, sizeof e->cfi_heap);
    e->size = sizeof *e;
    cf_error_set(e, CF_E_SOURCE, "a", "doc", "refused");
    cf_error_place(e, reason, 3, 4);
    check_place(e, reason, 3, 4);
    cf_error_copy(&copy, e);
    check_place(&copy, reason, 3, 4);
    check(e->offset == (size_t)-1, "cf_error_place leaves the offset unknown");
    cf_error_place_at(e, reason, 3, 4, 17);
    check_place(e, reason, 3, 4);
    check(e->offset == 17, "cf_error_place_at stores the offset");
    cf_error_copy(&copy, e);
    check(copy.offset == 17, "a copy keeps the offset");
    cf_error_set(e, CF_E_SOURCE, NULL, NULL, "another refusal");
    check_place(e, CF_REASON_NONE, 0, 0);
    cf_error_place(e, reason, 3, 4);
    cf_error_clear(e);
    check_place(e, CF_REASON_NONE, 0, 0);

    memset(bytes, 0xAB, sizeof buffer);
    e->size = old_size;
    cf_error_copy(e, &copy);
    check_tail(bytes, old_size, sizeof buffer);
    cf_error_copy(&copy, e);
    check_place(&copy, CF_REASON_NONE, 0, 0);

    e->size = 0;
    cf_error_set(e, CF_E_SOURCE, NULL, NULL, "refused again");
    cf_error_place(e, reason, 3, 4);
    cf_error_clear(e);
    cf_error_copy(e, &copy);
    check_tail(bytes, old_size, sizeof buffer);
    cf_error_place(NULL, reason, 3, 4);

    check(last == 19, "the last reason is the twentieth");
    check(COUNT(names) == (size_t)(last + 1), "twenty reasons");
    for (i = 0; i < COUNT(names); ++i) {
        check_text(cf_reason_name((int)i), names[i], "reason name");
    }
    check_text(cf_reason_name(-1), "?", "negative reason name");
    check_text(cf_reason_name(last + 1), "?", "unknown reason name");
    check_text(cf_reason_description(-1), "unknown", "negative reason description");
    check_text(cf_reason_description(last + 1), "unknown",
               "unknown reason description");

    for (i = 0; i < COUNT(classes); ++i) {
        check_text(cf_reason_severity((int)i), classes[i].severity, "reason severity");
        check_text(cf_reason_weakness((int)i), classes[i].weakness, "reason weakness");
    }
    check(COUNT(classes) == (size_t)(last + 1), "a class for every reason");
    check_text(cf_reason_severity(-1), "?", "negative reason severity");
    check_text(cf_reason_severity(last + 1), "?", "unknown reason severity");
    check_text(cf_reason_weakness(-1), "", "negative reason weakness");
    check_text(cf_reason_weakness(last + 1), "", "unknown reason weakness");
}

#if CF_WITH_TLV
static const cf_tlv_tag tags[] = {
    {0x0001, "name", CF_TLV_TEXT, 0, 0, 0, NULL, 0, NULL}};
static cf_codec      tlv;
static cf_tlv_layout layout = CF_TLV_LAYOUT_INIT;

static void init_tlv(void) {
    cf_error e = CF_ERROR_INIT;

    layout.name         = "devcfg";
    layout.tag_width    = 2;
    layout.length_width = 2;
    layout.order        = CF_TLV_LITTLE;
    layout.flags        = CF_TLV_END;
    layout.end_tag      = 0xFFFFu;
    layout.tags         = tags;
    layout.ntags        = COUNT(tags);
    check(cf_tlv_codec(&tlv, &layout, &e), "make the devcfg codec");
}
#endif

static cf_bool accept_end(void* ctx) {
    (void)ctx;
    return CF_TRUE;
}

static cf_bool accept_begin(void* ctx, const char* name, size_t n) {
    (void)name;
    (void)n;
    return accept_end(ctx);
}

static cf_bool accept_scalar(void* ctx, const char* name, size_t n,
                             const cf_value* value, cf_bool textual, const cf_loc* at) {
    (void)value;
    (void)textual;
    (void)at;
    return accept_begin(ctx, name, n);
}

static cf_bool refuse_end(void* ctx) {
    (void)ctx;
    return CF_FALSE;
}

static cf_bool refuse_begin(void* ctx, const char* name, size_t n) {
    (void)name;
    (void)n;
    return refuse_end(ctx);
}

static cf_bool refuse_scalar(void* ctx, const char* name, size_t n,
                             const cf_value* value, cf_bool textual, const cf_loc* at) {
    (void)value;
    (void)textual;
    (void)at;
    return refuse_begin(ctx, name, n);
}

static const cf_doc_sink accepting = {sizeof(cf_doc_sink),
                                      accept_begin,
                                      accept_begin,
                                      accept_scalar,
                                      accept_end,
                                      NULL,
                                      NULL,
                                      NULL,
                                      0};

typedef struct reason_row {
    const char* format;
    const char* bytes;
    size_t      n;
    int         reason;
    unsigned    line;
    unsigned    col;
    int         code;
    int         maybe; /* another backend may read it: then nothing to classify */
    size_t offset; /* the byte offset of the cause, NOOFF when the reader has none */
} reason_row;

#define NOOFF ((size_t)-1)

#define ROW(fmt, doc, reason, line, col, off)                                          \
    {fmt, doc, sizeof(doc) - 1, CF_REASON_##reason, line, col, CF_E_SOURCE, 0, off}
#define KEY_ROW(fmt, doc, reason, line, off)                                           \
    {fmt, doc, sizeof(doc) - 1, CF_REASON_##reason, line, 0, CF_E_KEY, 0, off}
#if (defined(CF_YAML_LIBYAML) && CF_YAML_LIBYAML) ||                                   \
    (defined(CF_YAML_LIBFYAML) && CF_YAML_LIBFYAML)
#define YAML_ROW(doc, reason, off)                                                     \
    {"yaml", doc, sizeof(doc) - 1, CF_REASON_SYNTAX, 0, 0, CF_E_SOURCE, 1, NOOFF}
#else
#define YAML_ROW(doc, reason, off) ROW("yaml", doc, reason, 0, 0, off)
#endif

#define KEY_FIFTY "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define KEY_LONG KEY_FIFTY KEY_FIFTY KEY_FIFTY KEY_FIFTY KEY_FIFTY KEY_FIFTY

static const reason_row rows[] = {
    ROW("json", "{\"a\":\"\xFF\"}", UTF8, 1, 7, 6),
    ROW("json5", "{\"a\":\"\xFF\"}", UTF8, 1, 7, 6),
    ROW("toml", "a = \"\xFF\"\n", UTF8, 1, 6, 5),
    ROW("yaml", "a: \xFF\n", UTF8, 1, 4, 3),
    ROW("ini", "a=\xFF\n", UTF8, 1, 3, 2),
    ROW("properties", "a=\xFF\n", UTF8, 1, 3, 2),
    ROW("cbor", "\xA1\x61\x61\x61\xFF", UTF8, 0, 0, 3),
#if CF_WITH_TLV
    ROW("tlv", "\x01\x00\x01\x00\xFF\xFF\xFF", UTF8, 0, 0, 0),
#endif
    ROW("json", "{\"a\":\"x", TRUNCATED, 0, 0, 7),
    ROW("json5", "{\"a\":\"x", TRUNCATED, 0, 0, 7),
    ROW("toml", "a = [1, 2,", TRUNCATED, 0, 0, 10),
    YAML_ROW("a: \"x", TRUNCATED, 5),
    ROW("ini", "[sec", TRUNCATED, 0, 0, 4),
    ROW("cbor", "\xA1\x61\x61", TRUNCATED, 0, 0, 3),
#if CF_WITH_TLV
    ROW("tlv",
        "\x01\x00\x05\x00"
        "a",
        TRUNCATED, 0, 0, 0),
#endif
#if CF_WITH_XML
    ROW("xml", "<a><b>1</b>", TRUNCATED, 0, 0, 11),
#endif
    ROW("json", "{\"a\":1} x", TRAILING, 0, 0, 8),
    ROW("json5", "{\"a\":1} x", TRAILING, 0, 0, 8),
    ROW("yaml", "a: 1\n---\nb: 2\n", TRAILING, 0, 0, NOOFF),
    ROW("cbor", "\xA0\x00", TRAILING, 0, 0, 1),
#if CF_WITH_TLV
    ROW("tlv", "\xFF\xFF\x00", TRAILING, 0, 0, 2),
#endif
#if CF_WITH_XML
    ROW("xml", "<a/><b/>", TRAILING, 0, 0, 4),
#endif
    ROW("json", "", EMPTY, 0, 0, 0),
    ROW("json5", "", EMPTY, 0, 0, 0),
    ROW("cbor", "", EMPTY, 0, 0, 0),
#if CF_WITH_TLV
    ROW("tlv", "", EMPTY, 0, 0, 0),
#endif
#if CF_WITH_XML
    ROW("xml", "", EMPTY, 0, 0, 0),
#endif
    ROW("json", "[1]", TOP_LEVEL, 0, 0, 0),
    ROW("json5", "[1]", TOP_LEVEL, 0, 0, 0),
    ROW("yaml", "- 1\n", TOP_LEVEL, 0, 0, 0),
    ROW("cbor", "\x81\x01", TOP_LEVEL, 0, 0, 0),
    ROW("json", "{\"a\":01}", NUMBER, 0, 0, 5),
    ROW("json5", "{\"a\":01}", NUMBER, 0, 0, 5),
    ROW("toml", "a = 01\n", NUMBER, 0, 0, 4),
    ROW("toml", "a = 9223372036854775808\n", NUMBER_RANGE, 0, 0, 4),
    ROW("cbor", "\xA1\x61\x61\x1B\x80\x00\x00\x00\x00\x00\x00\x00", NUMBER_RANGE, 0, 0,
        3),
    ROW("toml", "a = 1\na = 2\n", DUPLICATE_KEY, 0, 0, 6),
#if CF_WITH_XML
    ROW("xml", "<a b=\"1\" b=\"2\"/>", DUPLICATE_KEY, 0, 0, 9),
#endif
    ROW("json", "{\"A\":1}", KEY, 0, 0, 5),
    ROW("json5", "{\"A\":1}", KEY, 0, 0, 5),
    ROW("toml", "A = 1\n", KEY, 0, 0, 4),
    ROW("yaml", "A: 1\n", KEY, 0, 0, 3),
    KEY_ROW("ini", "A=1\n", KEY, 0, 0),
    KEY_ROW("properties", "A=1\n", KEY, 0, 0),
    KEY_ROW("cbor", "\xA1\x61\x41\x01", KEY, 0, 3),
#if CF_WITH_XML
    ROW("xml", "<a><B>1</B></a>", KEY, 0, 0, 3),
#endif
    ROW("json", "{\"" KEY_LONG "\":1}", TOO_LONG, 0, 0, CF_KEY_MAX + 1),
    ROW("json5", "{\"" KEY_LONG "\":1}", TOO_LONG, 0, 0, CF_KEY_MAX + 1),
    ROW("toml", KEY_LONG " = 1\n", TOO_LONG, 0, 0, CF_KEY_MAX - 1),
    KEY_ROW("ini", KEY_LONG "[1]=1\n", TOO_LONG, 0, 0),
    ROW("json", "{\"a\":\"\\q\"}", ESCAPE, 0, 0, 7),
    ROW("json5", "{\"a\":\"\\8\"}", ESCAPE, 0, 0, 7),
    ROW("toml", "a = \"\\q\"\n", ESCAPE, 0, 0, 6),
    YAML_ROW("a: \"\\q\"\n", ESCAPE, 5),
    ROW("properties", "a=\\u12\n", ESCAPE, 0, 0, 3),
    ROW("json", "{\"a\":\"\x01\"}", CONTROL, 0, 0, 6),
    ROW("json5", "{\"a\":\"\nx\"}", CONTROL, 0, 0, 6),
    ROW("toml", "a = \"\x01\"\n", CONTROL, 0, 0, 5),
    YAML_ROW("a: \"\x01\"\n", CONTROL, 4),
    YAML_ROW("#\0#", CONTROL, 1),
    YAML_ROW("# a\x01"
             "b\n",
             CONTROL, 3),
    YAML_ROW("a: 1 # x\x7f\n", CONTROL, 8),
    YAML_ROW("# c\x0b\r", CONTROL, 3),
    YAML_ROW("a: 1 # 0123456789abcdefghijk\x01lmnop\n", CONTROL, 28),
    YAML_ROW("a: 1\n# 0123456789abcdefghijklmnopqrstu\x7f\n", CONTROL, 38),
    ROW("toml", "a = 1\n[a]\n", STRUCTURE, 0, 0, 6),
#if CF_WITH_XML
    ROW("xml", "<a x=\"1\"><x>2</x></a>", STRUCTURE, 0, 0, 9),
#endif
    ROW("cbor", "\xA1\x61\x61\x41\x61", UNSUPPORTED, 0, 0, 3),
#if CF_WITH_XML
    ROW("xml", "<!DOCTYPE a><a/>", UNSUPPORTED, 0, 0, 0),
#endif
    ROW("yaml", "a: !foo 1\n", UNSUPPORTED, 0, 0, 3),
    ROW("json", "{\"a\" 1}", SYNTAX, 0, 0, 5),
    ROW("json5", "{\"a\" 1}", SYNTAX, 0, 0, 5),
    ROW("toml", "a 1\n", SYNTAX, 0, 0, 2),
    ROW("yaml", "a: b: c\n", SYNTAX, 0, 0, 3),
    ROW("ini", "a\n", SYNTAX, 0, 0, 1),
    ROW("cbor", "\xA1\x61\x61\x1C", SYNTAX, 0, 0, 3),
#if CF_WITH_XML
    ROW("xml", "<a></b>", SYNTAX, 0, 0, 3),
#endif
};

/* Whether the format is read by another library (expat for XML, libyaml or
 * libfyaml for YAML when chosen): the expected offsets are the built-in
 * readers' own. */
static int external_reader(const char* format) {
    if (strcmp(format, "xml") == 0) {
        return 1;
    }
#if (defined(CF_YAML_LIBYAML) && CF_YAML_LIBYAML) ||                                   \
    (defined(CF_YAML_LIBFYAML) && CF_YAML_LIBFYAML)
    if (strcmp(format, "yaml") == 0) {
        return 1;
    }
#endif
    return 0;
}

static void check_row(const reason_row* row, const cf_error* err) {
    char message[96];

    sprintf(message, "%s refuses with %s (got %s)", row->format,
            cf_reason_name(row->reason), cf_reason_name(err->reason));
    check(err->reason == row->reason, message);
    check(err->code == row->code, "the refusal retains its existing code");
    check(err->where.file == NULL, "the error owns no file pointer");
    if (row->line != 0) {
        check(err->where.line == row->line, "the refusal's expected line");
    }
    if (row->col != 0) {
        check(err->where.col == row->col, "the refusal's expected column");
    }
    sprintf(message, "%s %s: byte offset %ld, expected %ld", row->format,
            cf_reason_name(row->reason), (long)err->offset, (long)row->offset);
    if (external_reader(row->format)) {
        /* the offset is the backend's: unknown, or inside the document */
        check(err->offset == NOOFF || err->offset <= row->n, message);
    } else {
        check(err->offset == row->offset, message);
    }
}

static int refuse_row(const reason_row* row) {
    const char*     format = row->format;
    const cf_codec* codec  = cf_codec_by_name(format);
    cf_error        err    = CF_ERROR_INIT;
    cf_builder*     builder;
    cf_config*      config;

#if CF_WITH_TLV
    if (strcmp(format, "tlv") == 0) {
        codec  = &tlv;
        format = layout.name;
    }
#endif
    if (codec == NULL) {
        return 0;
    }
    if (row->reason == CF_REASON_DEPTH && strcmp(format, "toml") == 0) {
        cf_blob blob;

        cf_blob_mem(&blob, row->bytes, row->n, "doc");
        check(!cf_codec_decode(codec, &blob, &accepting, &err), "TOML refuses nesting");
    } else {
        builder = cf_build(NULL);
#if CF_WITH_TLV
        if (codec == &tlv) {
            check(cf_builder_codec(builder, codec), "register devcfg");
        }
#endif
        check(cf_builder_document(builder, "doc", CF_RANK_USER_FILE, row->bytes, row->n,
                                  format, 0),
              "add the document");
        config = cf_builder_create(builder, &err);
        if (config != NULL && row->maybe) {
            /* libyaml or libfyaml reads what the built-in parser refuses. */
            cf_config_free(config);
            return 1;
        }
        check(config == NULL, "the malformed document is refused");
        cf_config_free(config);
    }
    check_row(row, &err);
    return 1;
}

static unsigned test_depth(const char* only) {
    static const struct {
        const char* format;
        const char* prefix;
        const char* open;
        const char* body;
        const char* close;
    } depth[] = {
        {"json", "", "{\"a\":", "1", "}"},        {"json5", "", "{\"a\":", "1", "}"},
        {"toml", "a=", "{b=", "1", "}"},          {"yaml", "", "{a: ", "1", "}"},
        {"cbor", "", "\xA1\x61\x61", "\x01", ""},
#if CF_WITH_XML
        {"xml", "", "<a>", "", "</a>"},
#endif
    };
    char       doc[16 * (CF_MAX_DEPTH + 2)];
    reason_row row;
    unsigned   count = 0;
    size_t     i;
    unsigned   j;

    row.bytes  = doc;
    row.reason = CF_REASON_DEPTH;
    row.line   = 0;
    row.col    = 0;
    row.code   = CF_E_SOURCE;
    row.maybe  = 0;
    for (i = 0; i < COUNT(depth); ++i) {
        row.format = depth[i].format;
        if (only != NULL && strcmp(only, row.format) != 0) {
            continue;
        }
        strcpy(doc, depth[i].prefix);
        for (j = 0; j < CF_MAX_DEPTH + 2; ++j) {
            strcat(doc, depth[i].open);
        }
        strcat(doc, depth[i].body);
        for (j = 0; j < CF_MAX_DEPTH + 2; ++j) {
            strcat(doc, depth[i].close);
        }
        row.n = strlen(doc);
        /* The first byte of the nest that is one too deep; XML has none. */
        row.offset =
            (strcmp(row.format, "xml") == 0)
                ? NOOFF
                : strlen(depth[i].prefix) + CF_MAX_DEPTH * strlen(depth[i].open);
        count += (unsigned)refuse_row(&row);
    }
    return count;
}

static void test_rows(const char* only) {
    static const int order[] = {
        CF_REASON_UTF8,     CF_REASON_DEPTH,        CF_REASON_TRUNCATED,
        CF_REASON_TRAILING, CF_REASON_EMPTY,        CF_REASON_TOP_LEVEL,
        CF_REASON_NUMBER,   CF_REASON_NUMBER_RANGE, CF_REASON_DUPLICATE_KEY,
        CF_REASON_KEY,      CF_REASON_TOO_LONG,     CF_REASON_ESCAPE,
        CF_REASON_CONTROL,  CF_REASON_STRUCTURE,    CF_REASON_UNSUPPORTED,
        CF_REASON_SYNTAX};
    size_t   reason;
    size_t   i;
    unsigned count;
    unsigned seen;

#if CF_WITH_TLV
    init_tlv();
#endif
    for (reason = 0; reason < COUNT(order); ++reason) {
        if (only != NULL && order[reason] == CF_REASON_UTF8) {
            continue;
        }
        count = 0;
        if (order[reason] == CF_REASON_DEPTH) {
            count = test_depth(only);
        }
        seen = 0u;
        for (i = 0; i < COUNT(rows); ++i) {
            if (rows[i].reason == order[reason] &&
                (only == NULL || strcmp(only, rows[i].format) == 0) &&
                refuse_row(&rows[i])) {
                size_t previous;

                for (previous = 0; previous < i; ++previous) {
                    if (rows[previous].reason == rows[i].reason &&
                        strcmp(rows[previous].format, rows[i].format) == 0) {
                        break;
                    }
                }
                if (previous == i) {
                    ++seen;
                }
            }
        }
        printf("%s: %u formats\n", cf_reason_name(order[reason]), count + seen);
    }
}

static void test_storage(const char* format, const char* doc, cf_bool root_accepted) {
    const cf_codec* codec = cf_codec_by_name(format);
    cf_error        err   = CF_ERROR_INIT;
    cf_doc_sink     sink  = accepting;
    cf_blob         blob;

    if (codec == NULL) {
        return;
    }
    sink.end = refuse_end;
    if (!root_accepted) {
        sink.begin_table = refuse_begin;
        sink.begin_array = refuse_begin;
        sink.scalar      = refuse_scalar;
    }
    cf_blob_mem(&blob, doc, strlen(doc), "doc");
    check(!cf_codec_decode(codec, &blob, &sink, &err), "the sink refuses the read");
    check(err.reason == CF_REASON_STORAGE, "the refusal's reason is storage");
    check(err.code == CF_E_SOURCE, "the sink refusal retains its existing code");
}

/* A comment YAML 1.2 lets through: a tab, UTF-8 and a lone or paired CR. */
static void test_yaml_comment_kept(const char* doc, size_t n) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* builder;
    cf_config*  config;

    if (cf_codec_by_name("yaml") == NULL) {
        return;
    }
    builder = cf_build(NULL);
    check(cf_builder_document(builder, "doc", CF_RANK_USER_FILE, doc, n, "yaml", 0),
          "add the document");
    config = cf_builder_create(builder, &err);
    check(config != NULL, "a printable comment is read");
    cf_config_free(config);
}

static void test_multiline_storage(const char* format, const char* doc, unsigned col) {
    const cf_codec* codec = cf_codec_by_name(format);
    cf_doc_sink     sink  = accepting;
    cf_error        err   = CF_ERROR_INIT;
    cf_blob         blob;

    if (codec == NULL) {
        return;
    }
    sink.scalar = refuse_scalar;
    cf_blob_mem(&blob, doc, strlen(doc), "doc");
    check(!cf_codec_decode(codec, &blob, &sink, &err), "the sink refuses the string");
    check_place(&err, CF_REASON_KEY, 2, col);
}

static void test_file(const char* doc, int reason, int code, unsigned line,
                      unsigned col, cf_bool dotenv) {
    const char* path = dotenv ? "cf_reason.env" : "cf_reason.ini";
    cf_error    err  = CF_ERROR_INIT;
    cf_builder* builder;
    cf_config*  config;

    check(spill(path, doc), "write the input file");
    builder = cf_build(NULL);
    if (dotenv) {
        check(cf_builder_dotenv(builder, "doc", CF_RANK_USER_FILE, path, "", 0),
              "add the dotenv file");
    } else {
        check(cf_builder_file(builder, "doc", CF_RANK_USER_FILE, path, NULL,
                              CF_ENCODING_WINDOWS_1254),
              "add the Windows-1254 file");
    }
    config = cf_builder_create(builder, &err);
    check(config == NULL, "the input file is refused");
    check(err.code == code, "the file refusal retains its existing code");
    check_place(&err, reason, line, col);
    cf_config_free(config);
    remove(path);
}

int main(int argc, char** argv) {
    const char* only = argc > 1 ? argv[1] : NULL;

    test_size();
    if (only == NULL || strcmp(only, "size") != 0) {
        test_rows(only);
    }
    if (only == NULL) {
        test_storage("json", "{\"a\":1}", CF_TRUE);
        test_storage("json5", "{\"a\":1}", CF_TRUE);
        test_storage("toml", "a=1", CF_FALSE);
        test_multiline_storage("toml", "\na = \"\"\"first\nsecond\"\"\"", 5);
        test_multiline_storage("json5", "{\n a: 'first\\\nsecond'}", 5);
        test_storage("ini", "a=1", CF_FALSE);
        test_yaml_comment_kept("a: 1 # tab\there\n", sizeof("a: 1 # tab\there\n") - 1);
        test_yaml_comment_kept("a: 1\n# caf\xc3\xa9\n",
                               sizeof("a: 1\n# caf\xc3\xa9\n") - 1);
        test_yaml_comment_kept("a: 1\r\n# c\r\n", sizeof("a: 1\r\n# c\r\n") - 1);
        test_yaml_comment_kept(
            "a: 1 # 0123456789abcdefghij\tklmnop\xc3\xa9qrstuvwxyz\r# k\n",
            sizeof("a: 1 # 0123456789abcdefghij\tklmnop\xc3\xa9qrstuvwxyz\r# k\n") - 1);
        if (cf_codec_by_name("ini") != NULL) {
            test_file("a=\x81\n", CF_REASON_ENCODING, CF_E_SOURCE, 1, 3, CF_FALSE);
        }
#if CF_WITH_DOTENV
        test_file("A=\xFF\n", CF_REASON_UTF8, CF_E_SOURCE, 1, 3, CF_TRUE);
        test_file("NO EQUALS\n", CF_REASON_SYNTAX, CF_E_SOURCE, 1, 0, CF_TRUE);
        test_file("=1\n", CF_REASON_KEY, CF_E_KEY, 1, 0, CF_TRUE);
#endif
    }
    return cf_test_done();
}
