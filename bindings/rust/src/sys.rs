//! The C interface, declared by hand from include/configforge - the part
//! the safe API uses. Written out rather than generated: bindgen would be
//! a dependency, and the build downloads nothing.
//!
//! The struct layouts follow the build's limits, which build.rs leaves at
//! their defaults (cf_build_options.h): CF_KEY_MAX 256, CF_MESSAGE_MAX
//! 256, CF_SOURCE_MAX 64. A library built with others is not this one.

#![allow(non_camel_case_types)]

use std::os::raw::{c_char, c_int};

pub const CF_KEY_MAX: usize = 256;
pub const CF_SOURCE_MAX: usize = 64;
pub const CF_MESSAGE_MAX: usize = 256;

pub type cf_bool = c_int;
pub type cf_i64 = i64;

pub const CF_RANK_OVERRIDE: c_int = -10;
pub const CF_RANK_CLI: c_int = 0;
pub const CF_RANK_ENV: c_int = 10;
pub const CF_RANK_USER_FILE: c_int = 20;
pub const CF_RANK_SYS_FILE: c_int = 30;
pub const CF_RANK_DEFAULT: c_int = 40;

pub const CF_OPTIONAL: u32 = 1;

pub const CF_REQUIRED: u32 = 1 << 0;
pub const CF_SENSITIVE: u32 = 1 << 1;
pub const CF_DEPRECATED: u32 = 1 << 2;
pub const CF_TRANSIENT: u32 = 1 << 3;

pub const CF_UNKNOWN_IGNORE: c_int = 0;
pub const CF_UNKNOWN_WARN: c_int = 1;
pub const CF_UNKNOWN_REJECT: c_int = 2;

#[repr(C)]
pub struct cf_error {
    pub size: usize,
    pub code: c_int,
    pub key: [c_char; CF_KEY_MAX],
    pub source: [c_char; CF_SOURCE_MAX],
    pub message: [c_char; CF_MESSAGE_MAX],
}

#[repr(C)]
pub struct cf_pair {
    pub key: *const c_char,
    pub value: *const c_char,
}

pub const CF_TYPE_NULL: c_int = 0;
pub const CF_TYPE_BOOL: c_int = 1;
pub const CF_TYPE_INT: c_int = 2;
pub const CF_TYPE_REAL: c_int = 3;
pub const CF_TYPE_TEXT: c_int = 4;

pub const CF_INVALID_STRICT: c_int = 0;
pub const CF_INVALID_FALLBACK: c_int = 1;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct cf_text {
    pub p: *const c_char,
    pub n: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub union cf_value_u {
    pub b: cf_bool,
    pub i: cf_i64,
    pub r: f64,
    pub t: cf_text,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct cf_value {
    pub type_: c_int,
    pub u: cf_value_u,
}

/// cf_define.h's descriptor, with CF_WITH_REAL on (the default build).
/// The binding checks the size cf_def_init writes against this one, so a
/// library built another way is refused, not misread.
#[repr(C)]
pub struct cf_def {
    pub size: usize,
    pub type_: c_int,
    pub key: *const c_char,
    pub flags: u32,
    pub on_invalid: c_int,
    pub description: *const c_char,
    pub replaced_by: *const c_char,
    pub has_default: cf_bool,
    pub default_value: cf_value,
    pub has_range: cf_bool,
    pub imin: cf_i64,
    pub imax: cf_i64,
    pub rmin: f64,
    pub rmax: f64,
    pub min_len: usize,
    pub max_len: usize,
    pub allowed: *const *const c_char,
    pub aliases: *const *const c_char,
    pub policy: *const *const c_char,
}

/// cf_iterate.h's iterator: plain fields, filled by the library.
#[repr(C)]
pub struct cf_iter {
    pub cfg: *const cf_config,
    pub prefix: *const c_char,
    pub plen: usize,
    pub pos: usize,
    pub children: cf_bool,
    pub key: *const c_char,
    pub keylen: usize,
    pub name: *const c_char,
    pub namelen: usize,
    pub value: *const cf_value,
}

#[repr(C)]
pub struct cf_builder {
    _opaque: [u8; 0],
}

#[repr(C)]
pub struct cf_config {
    _opaque: [u8; 0],
}

extern "C" {
    pub fn cf_build(alloc: *const std::ffi::c_void) -> *mut cf_builder;
    pub fn cf_app_builder(
        name: *const c_char,
        argc: c_int,
        argv: *const *mut c_char,
    ) -> *mut cf_builder;
    pub fn cf_builder_abort(b: *mut cf_builder);
    pub fn cf_builder_create(b: *mut cf_builder, err: *mut cf_error) -> *mut cf_config;

    pub fn cf_builder_cli(
        b: *mut cf_builder,
        name: *const c_char,
        rank: c_int,
        argc: c_int,
        argv: *const *mut c_char,
    ) -> cf_bool;
    pub fn cf_builder_env(
        b: *mut cf_builder,
        name: *const c_char,
        rank: c_int,
        prefix: *const c_char,
        environment: *const *mut c_char,
    ) -> cf_bool;
    pub fn cf_builder_file(
        b: *mut cf_builder,
        name: *const c_char,
        rank: c_int,
        path: *const c_char,
        format: *const c_char,
        flags: u32,
    ) -> cf_bool;
    pub fn cf_builder_dir(
        b: *mut cf_builder,
        name: *const c_char,
        rank: c_int,
        path: *const c_char,
        format: *const c_char,
        flags: u32,
    ) -> cf_bool;
    pub fn cf_builder_dotenv(
        b: *mut cf_builder,
        name: *const c_char,
        rank: c_int,
        path: *const c_char,
        prefix: *const c_char,
        flags: u32,
    ) -> cf_bool;
    pub fn cf_builder_memory(
        b: *mut cf_builder,
        name: *const c_char,
        rank: c_int,
        pairs: *const cf_pair,
        count: usize,
    ) -> cf_bool;

    pub fn cf_builder_bool(
        b: *mut cf_builder,
        key: *const c_char,
        dflt: cf_bool,
        flags: u32,
    ) -> cf_bool;
    pub fn cf_builder_int(
        b: *mut cf_builder,
        key: *const c_char,
        dflt: cf_i64,
        min: cf_i64,
        max: cf_i64,
        flags: u32,
    ) -> cf_bool;
    pub fn cf_builder_text(
        b: *mut cf_builder,
        key: *const c_char,
        dflt: *const c_char,
        flags: u32,
    ) -> cf_bool;
    pub fn cf_def_init(d: *mut cf_def, type_: c_int, key: *const c_char);
    pub fn cf_value_text(v: *mut cf_value, p: *const c_char, n: usize);
    pub fn cf_value_int(v: *mut cf_value, i: cf_i64);
    pub fn cf_value_bool(v: *mut cf_value, b: cf_bool);
    pub fn cf_builder_define(b: *mut cf_builder, d: *const cf_def) -> cf_bool;
    pub fn cf_builder_open(b: *mut cf_builder, prefix: *const c_char) -> cf_bool;
    pub fn cf_builder_unknown(b: *mut cf_builder, policy: c_int);

    pub fn cf_iter_keys(it: *mut cf_iter, c: *const cf_config, prefix: *const c_char) -> cf_bool;
    pub fn cf_iter_children(
        it: *mut cf_iter,
        c: *const cf_config,
        prefix: *const c_char,
    ) -> cf_bool;
    pub fn cf_iter_next(it: *mut cf_iter) -> cf_bool;
    pub fn cf_config_free(c: *mut cf_config);
    pub fn cf_has(c: *const cf_config, key: *const c_char) -> cf_bool;
    pub fn cf_get_bool(c: *const cf_config, key: *const c_char, fallback: cf_bool) -> cf_bool;
    pub fn cf_get_int(c: *const cf_config, key: *const c_char, fallback: cf_i64) -> cf_i64;
    pub fn cf_get_text(
        c: *const cf_config,
        key: *const c_char,
        fallback: *const c_char,
    ) -> *const c_char;
    pub fn cf_explain(
        c: *const cf_config,
        key: *const c_char,
        buf: *mut c_char,
        cap: usize,
    ) -> usize;
    pub fn cf_inspect(c: *const cf_config, buf: *mut c_char, cap: usize) -> usize;
    pub fn cf_diff_text(
        a: *const cf_config,
        b: *const cf_config,
        buf: *mut c_char,
        cap: usize,
    ) -> usize;
    pub fn cf_derive(
        base: *const cf_config,
        source: *const c_char,
        pairs: *const cf_pair,
        count: usize,
        err: *mut cf_error,
    ) -> *mut cf_config;
    pub fn cf_code_name(code: c_int) -> *const c_char;
    pub fn cf_version() -> *const c_char;
    pub fn cf_build_limit(name: *const c_char) -> usize;
    pub fn cf_build_id() -> *const c_char;
}
