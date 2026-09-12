/*
 * value.c - the value model. C89.
 */

#include "configforge/cf_value.h"

#include <string.h>

const char* cf_type_name(cf_type t) {
    switch (t) {
#define CF_TYPE_X(sym, name, json)                                                     \
    case sym:                                                                          \
        return name;
#include "configforge/cf_type.def"
#undef CF_TYPE_X
    }
    return "?";
}

/* JSON Schema spells the same types differently, and the report and the
   schema generator both need that spelling. Two names, one list. */
const char* cf_type_json_name(cf_type t) {
    switch (t) {
#define CF_TYPE_X(sym, name, json)                                                     \
    case sym:                                                                          \
        return json;
#include "configforge/cf_type.def"
#undef CF_TYPE_X
    }
    return "null";
}

void cf_value_null(cf_value* v) {
    v->type = CF_TYPE_NULL;
    v->u.i  = 0;
}

void cf_value_bool(cf_value* v, cf_bool b) {
    v->type = CF_TYPE_BOOL;
    v->u.b  = b ? CF_TRUE : CF_FALSE;
}

void cf_value_int(cf_value* v, cf_i64 i) {
    v->type = CF_TYPE_INT;
    v->u.i  = i;
}

#if CF_WITH_REAL
void cf_value_real(cf_value* v, double r) {
    v->type = CF_TYPE_REAL;
    v->u.r  = r;
}
#endif

void cf_value_text(cf_value* v, const char* p, size_t n) {
    v->type  = CF_TYPE_TEXT;
    v->u.t.p = p;
    v->u.t.n = n;
}

cf_bool cf_value_from_text(cf_type t, const char* s, size_t n, cf_value* out) {
    if (s == NULL || out == NULL) {
        return CF_FALSE;
    }
    switch (t) {
    case CF_TYPE_NULL:
        if (n == 4 && memcmp(s, "null", 4) == 0) {
            cf_value_null(out);
            return CF_TRUE;
        }
        return CF_FALSE;

    case CF_TYPE_BOOL:
        /* Exactly these two spellings. "yes", "no", "on" and "off"
           are not booleans here; see decision D-24. */
        if (n == 4 && memcmp(s, "true", 4) == 0) {
            cf_value_bool(out, CF_TRUE);
            return CF_TRUE;
        }
        if (n == 5 && memcmp(s, "false", 5) == 0) {
            cf_value_bool(out, CF_FALSE);
            return CF_TRUE;
        }
        return CF_FALSE;

    case CF_TYPE_INT: {
        cf_i64 i;
        if (!cf_text_to_i64(s, n, &i, NULL)) {
            return CF_FALSE;
        }
        cf_value_int(out, i);
        return CF_TRUE;
    }

    case CF_TYPE_REAL: {
#if CF_WITH_REAL
        double r;
        if (!cf_text_to_f64(s, n, &r, NULL)) {
            return CF_FALSE;
        }
        cf_value_real(out, r);
        return CF_TRUE;
#else
        return CF_FALSE;
#endif
    }

    case CF_TYPE_TEXT:
        cf_value_text(out, s, n);
        return CF_TRUE;

    default:
        return CF_FALSE;
    }
}

cf_bool cf_value_infer(const char* s, size_t n, cf_value* out) {
    cf_scalar_kind k;

    if (s == NULL || out == NULL) {
        return CF_FALSE;
    }
    k = cf_infer_scalar(s, n);
    switch (k) {
    case CF_SCALAR_BOOL:
        return cf_value_from_text(CF_TYPE_BOOL, s, n, out);
    case CF_SCALAR_INT:
        return cf_value_from_text(CF_TYPE_INT, s, n, out);
    case CF_SCALAR_REAL:
        return cf_value_from_text(CF_TYPE_REAL, s, n, out);
    case CF_SCALAR_TEXT:
        break;
    }
    cf_value_text(out, s, n);
    return CF_TRUE;
}

size_t cf_value_to_text(const cf_value* v, char* buf, size_t cap) {
    if (v == NULL || buf == NULL || cap == 0) {
        return 0;
    }
    switch (v->type) {
    case CF_TYPE_NULL:
        if (cap < 5) {
            buf[0] = '\0';
            return 0;
        }
        memcpy(buf, "null", 5);
        return 4;

    case CF_TYPE_BOOL:
        if (v->u.b) {
            if (cap < 5) {
                buf[0] = '\0';
                return 0;
            }
            memcpy(buf, "true", 5);
            return 4;
        }
        if (cap < 6) {
            buf[0] = '\0';
            return 0;
        }
        memcpy(buf, "false", 6);
        return 5;

    case CF_TYPE_INT:
        return cf_i64_to_text(v->u.i, buf, cap);

    case CF_TYPE_REAL:
#if CF_WITH_REAL
        return cf_f64_to_text(v->u.r, buf, cap);
#else
        buf[0] = '\0';
        return 0;
#endif

    case CF_TYPE_TEXT:
        if (v->u.t.n + 1 > cap) {
            buf[0] = '\0';
            return 0;
        }
        if (v->u.t.n > 0) {
            memcpy(buf, v->u.t.p, v->u.t.n);
        }
        buf[v->u.t.n] = '\0';
        return v->u.t.n;

    default:
        buf[0] = '\0';
        return 0;
    }
}

int cf_value_cmp(const cf_value* a, const cf_value* b) {
    if (a->type != b->type) {
        return (a->type < b->type) ? -1 : 1;
    }
    switch (a->type) {
    case CF_TYPE_NULL:
        return 0;
    case CF_TYPE_BOOL:
        if (a->u.b == b->u.b) {
            return 0;
        }
        return (a->u.b < b->u.b) ? -1 : 1;
    case CF_TYPE_INT:
        if (a->u.i == b->u.i) {
            return 0;
        }
        return (a->u.i < b->u.i) ? -1 : 1;
    case CF_TYPE_REAL:
#if CF_WITH_REAL
        if (a->u.r == b->u.r) {
            return 0;
        }
        return (a->u.r < b->u.r) ? -1 : 1;
#else
        return 0;
#endif
    case CF_TYPE_TEXT:
        return cf_cmp_bytes(a->u.t.p, a->u.t.n, b->u.t.p, b->u.t.n);
    default:
        return 0;
    }
}

cf_bool cf_value_equal(const cf_value* a, const cf_value* b) {
    return (cf_value_cmp(a, b) == 0) ? CF_TRUE : CF_FALSE;
}
