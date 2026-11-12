//! ConfigForge for Rust: configuration whose every value can say why it
//! is what it is - which source won, which lost, and where each was set.
//!
//! A safe layer over the C library, which build.rs builds from this
//! checkout: a [`Builder`] that consumes itself into a [`Config`], reads
//! that cannot fail, and [`Config::explain`]. The C library does the work,
//! so a Rust program and a C one built from the same contract resolve,
//! refuse and explain identically - the examples in `examples/rust` are
//! checked for the same lines as `examples/c`.
//!
//! ```no_run
//! let cfg = configforge::Builder::app("myapp")
//!     .define_int("server.port", 8080, 1, 65535, 0)
//!     .create()
//!     .unwrap_or_else(|e| panic!("{}", e));
//! let port = cfg.int("server.port", 8080);
//! print!("{}", cfg.explain("server.port"));
//! # let _ = port;
//! ```
//!
//! A [`Config`] is an immutable snapshot (D-14), so it is `Send` and
//! `Sync`: read it from any thread, change it with [`Config::derive`].

mod edit;
mod live;
mod log;
/// The C declarations the safe layer is written over. Hidden, and not part
/// of this crate's semver: they follow the C headers, and a program that
/// reaches for them has left what the crate promises.
#[doc(hidden)]
pub mod sys;
mod typed;

pub use edit::{Edit, EditValue};
pub use live::{Live, LiveReader, Mode, Options as LiveOptions, Reload, Status};
pub use log::{Level, LogEvent};
pub use typed::{key_is_valid, DefDefault, Field, Key, Value};

use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::{c_char, c_int};
use std::ptr;

/// Source ranks: lower wins.
pub mod rank {
    pub const OVERRIDE: i32 = super::sys::CF_RANK_OVERRIDE;
    pub const CLI: i32 = super::sys::CF_RANK_CLI;
    pub const ENV: i32 = super::sys::CF_RANK_ENV;
    pub const USER_FILE: i32 = super::sys::CF_RANK_USER_FILE;
    pub const SYS_FILE: i32 = super::sys::CF_RANK_SYS_FILE;
    pub const DEFAULT: i32 = super::sys::CF_RANK_DEFAULT;
}

/// Definition flags.
pub mod flag {
    pub const REQUIRED: u32 = super::sys::CF_REQUIRED;
    pub const SENSITIVE: u32 = super::sys::CF_SENSITIVE;
    pub const DEPRECATED: u32 = super::sys::CF_DEPRECATED;
    pub const TRANSIENT: u32 = super::sys::CF_TRANSIENT;
    /// Cannot change while the program runs; see [`Live`](crate::Live).
    pub const RESTART: u32 = super::sys::CF_RESTART;
}

/// A file or directory that may be absent.
pub const OPTIONAL: u32 = sys::CF_OPTIONAL;

/// What to do with a key nobody defined.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub enum Unknown {
    Ignore,
    Warn,
    Reject,
}

/// What went wrong, with the key and the place - the C library's one
/// error, built once.
#[derive(Clone, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub struct Error {
    pub code: i32,
    pub name: String,
    pub key: String,
    pub source: String,
    pub message: String,
}

fn text_of(p: *const c_char) -> String {
    if p.is_null() {
        return String::new();
    }
    unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned()
}

impl Error {
    fn from_raw(e: &sys::cf_error) -> Error {
        Error {
            code: e.code,
            name: text_of(unsafe { sys::cf_code_name(e.code) }),
            key: text_of(e.key.as_ptr()),
            source: text_of(e.source.as_ptr()),
            message: text_of(e.message.as_ptr()),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.message)
    }
}

impl std::error::Error for Error {}

/// The library's limits against the ones this binding's cf_error was
/// written for (D-45): a library built with others would be misread, so
/// it is refused, once, before the first call that fills an error.
fn check_limits() {
    static CHECKED: std::sync::Once = std::sync::Once::new();
    CHECKED.call_once(|| {
        for (name, ours) in [
            ("key", sys::CF_KEY_MAX),
            ("source", sys::CF_SOURCE_MAX),
            ("message", sys::CF_MESSAGE_MAX),
        ] {
            let n = c(name);
            let theirs = unsafe { sys::cf_build_limit(n.as_ptr()) };
            assert_eq!(
                theirs,
                ours,
                "libconfigforge was built with CF_{}_MAX {}, this binding expects {}",
                name.to_uppercase(),
                theirs,
                ours
            );
        }
        // Config and LiveReader are Send + Sync because the library's
        // reference counts are atomic; in a single-threaded build they are
        // not, and nothing here could say so to the compiler.
        let single = c("single-threaded");
        assert!(
            unsafe { sys::cf_has_feature(single.as_ptr()) } == 0,
            "libconfigforge was built CF_SINGLE_THREADED; this binding's types are Send + Sync"
        );
        // cf_def's layout has rmin and rmax only where reals are built.
        let real = c("real");
        assert!(
            unsafe { sys::cf_has_feature(real.as_ptr()) } != 0,
            "libconfigforge was built without CF_WITH_REAL; this binding's cf_def has reals"
        );
    });
}

fn new_error() -> sys::cf_error {
    check_limits();
    sys::cf_error {
        size: std::mem::size_of::<sys::cf_error>(),
        code: 0,
        key: [0; sys::CF_KEY_MAX],
        source: [0; sys::CF_SOURCE_MAX],
        message: [0; sys::CF_MESSAGE_MAX],
    }
}

/// A string for the C side. No configuration string can hold a NUL, so a
/// Rust string with one never panics here: each NUL becomes U+0001,
/// which no key may contain, so a read of such a key finds nothing and a
/// definition of one is refused by the library. A *value* with a NUL is
/// refused before it reaches the library (Keep::s, Edit::set).
fn c(s: &str) -> CString {
    CString::new(s).unwrap_or_else(|_| {
        CString::new(s.replace('\0', "\u{1}")).expect("no NUL is left to refuse")
    })
}

/// Everything the C side borrows: kept alive with the configuration that
/// may point at it. Moving a CString or a Vec does not move its buffer.
#[derive(Default)]
struct Keep {
    strings: Vec<CString>,
    pairs: Vec<Vec<sys::cf_pair>>,
    lists: Vec<Vec<*const c_char>>,
    argv: Vec<Vec<*mut c_char>>,
    /// Where the first string handed over that held a NUL had it (the
    /// byte offset): refused at create or derive, rather than silently
    /// changed. Only the offset is kept - the string may be a secret.
    nul: Option<usize>,
    /// The log this builder's configurations call, kept with each of
    /// them - and with every one derived - so it outlives the calls.
    logger: Option<log::SharedLogger>,
}

impl Keep {
    /// The error for a string with a NUL, if one was handed over.
    fn refusal(&self) -> Option<Error> {
        self.nul.map(|at| nul_refusal("a string", at))
    }
}

/// The error for a string with a NUL at byte `at`, naming only what kind
/// of string it was: the string itself may be a password or a token, and
/// an error message is what ends up in a log.
pub(crate) fn nul_refusal(what: &str, at: usize) -> Error {
    Error {
        code: sys::CF_E_CONVERT,
        name: text_of(unsafe { sys::cf_code_name(sys::CF_E_CONVERT) }),
        key: String::new(),
        source: String::new(),
        message: format!(
            "{what} holds a NUL byte (at byte {at}), which no configuration string can"
        ),
    }
}

// The raw pointers point into the CStrings and Vecs this same value
// owns, whose buffers do not move when the Keep does; nothing else
// reaches them. Moving a Keep to another thread moves its owner too.
unsafe impl Send for Keep {}

impl Keep {
    fn s(&mut self, v: &str) -> *const c_char {
        if self.nul.is_none() {
            self.nul = v.find('\0');
        }
        let cs = c(v);
        let p = cs.as_ptr();
        self.strings.push(cs);
        p
    }

    fn opt(&mut self, v: Option<&str>) -> *const c_char {
        v.map_or(ptr::null(), |v| self.s(v))
    }

    /// A NULL-terminated array of strings, as argv and environ are.
    fn argv(&mut self, items: &[String]) -> *const *mut c_char {
        let mut v: Vec<*mut c_char> = items.iter().map(|i| self.s(i) as *mut c_char).collect();
        v.push(ptr::null_mut());
        let p = v.as_ptr();
        self.argv.push(v);
        p
    }

    fn list(&mut self, items: &[&str]) -> *const *const c_char {
        let mut v: Vec<*const c_char> = items.iter().map(|i| self.s(i)).collect();
        v.push(ptr::null());
        let p = v.as_ptr();
        self.lists.push(v);
        p
    }

    fn pairs(&mut self, items: &[(&str, &str)]) -> (*const sys::cf_pair, usize) {
        let v: Vec<sys::cf_pair> = items
            .iter()
            .map(|(k, val)| sys::cf_pair {
                key: self.s(k),
                value: self.s(val),
            })
            .collect();
        let r = (v.as_ptr(), v.len());
        self.pairs.push(v);
        r
    }
}

/// A full definition, for what the define_* shorthands do not say: a
/// permitted set, a fallback, a policy, a replacement.
pub struct Def {
    key: String,
    kind: c_int,
    flags: u32,
    fallback: bool,
    default: Option<DefValue>,
    allowed: Vec<String>,
    policy: Vec<String>,
    replaced_by: Option<String>,
    range: Option<(i64, i64)>,
    list_mode: Option<ListMode>,
    list_separator: u8,
    list_defaults: Option<Vec<DefValue>>,
    aliases: Vec<String>,
}

/// How a declared scalar list combines its sources.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub enum ListMode {
    Replace,
    Append,
}

enum DefValue {
    Text(String),
    Int(i64),
    Bool(bool),
}

impl DefValue {
    fn native(&self, keep: &mut Keep) -> sys::cf_value {
        unsafe {
            let mut value: sys::cf_value = std::mem::zeroed();
            match self {
                DefValue::Text(t) => sys::cf_value_text(&mut value, keep.s(t), t.len()),
                DefValue::Int(i) => sys::cf_value_int(&mut value, *i),
                DefValue::Bool(b) => sys::cf_value_bool(&mut value, *b as sys::cf_bool),
            }
            value
        }
    }
}

impl Def {
    fn new(kind: c_int, key: &str) -> Def {
        Def {
            key: key.to_string(),
            kind,
            flags: 0,
            fallback: false,
            default: None,
            allowed: Vec::new(),
            policy: Vec::new(),
            replaced_by: None,
            range: None,
            list_mode: None,
            list_separator: 0,
            list_defaults: None,
            aliases: Vec::new(),
        }
    }
    pub fn text(key: &str) -> Def {
        Def::new(sys::CF_TYPE_TEXT, key)
    }
    pub fn int(key: &str) -> Def {
        Def::new(sys::CF_TYPE_INT, key)
    }
    pub fn boolean(key: &str) -> Def {
        Def::new(sys::CF_TYPE_BOOL, key)
    }
    /// Make this element contract a list. A zero separator disables
    /// environment splitting; CLI occurrences are separate elements.
    pub fn list(mut self, mode: ListMode, separator: u8) -> Def {
        self.list_mode = Some(mode);
        self.list_separator = separator;
        self
    }
    pub fn default_text_list(mut self, values: &[&str]) -> Def {
        self.list_defaults = Some(
            values
                .iter()
                .map(|v| DefValue::Text(v.to_string()))
                .collect(),
        );
        self
    }
    pub fn default_int_list(mut self, values: &[i64]) -> Def {
        self.list_defaults = Some(values.iter().map(|v| DefValue::Int(*v)).collect());
        self
    }
    pub fn default_bool_list(mut self, values: &[bool]) -> Def {
        self.list_defaults = Some(values.iter().map(|v| DefValue::Bool(*v)).collect());
        self
    }
    /// Command-line aliases, without dashes.
    pub fn aliases(mut self, items: &[&str]) -> Def {
        self.aliases = items.iter().map(|s| s.to_string()).collect();
        self
    }
    pub fn flags(mut self, flags: u32) -> Def {
        self.flags = flags;
        self
    }
    pub fn default_text(mut self, v: &str) -> Def {
        self.default = Some(DefValue::Text(v.to_string()));
        self
    }
    pub fn default_int(mut self, v: i64) -> Def {
        self.default = Some(DefValue::Int(v));
        self
    }
    pub fn default_bool(mut self, v: bool) -> Def {
        self.default = Some(DefValue::Bool(v));
        self
    }
    pub fn range(mut self, min: i64, max: i64) -> Def {
        self.range = Some((min, max));
        self
    }
    /// The permitted spellings of a text value: an enum.
    pub fn allowed(mut self, items: &[&str]) -> Def {
        self.allowed = items.iter().map(|s| s.to_string()).collect();
        self
    }
    /// A refused value hands over to the next candidate (D-05).
    pub fn fallback(mut self) -> Def {
        self.fallback = true;
        self
    }
    /// The only sources that may set it, in decreasing precedence (D-33).
    pub fn policy(mut self, sources: &[&str]) -> Def {
        self.policy = sources.iter().map(|s| s.to_string()).collect();
        self
    }
    /// Renamed: the old key's values feed `key` (D-43).
    pub fn replaced_by(mut self, key: &str) -> Def {
        self.replaced_by = Some(key.to_string());
        self.flags |= flag::DEPRECATED;
        self
    }
}

/// A configuration being assembled. Each call adds a source or a
/// definition; [`Builder::create`] resolves it all, once, and is where
/// every error is reported.
pub struct Builder {
    b: *mut sys::cf_builder,
    keep: Keep,
}

impl Builder {
    pub fn new() -> Builder {
        Builder {
            b: unsafe { sys::cf_build(ptr::null()) },
            keep: Keep::default(),
        }
    }

    /// The usual sources for a program: command line, environment
    /// (NAME_ prefix) and defaults, as cf_app installs them.
    pub fn app(name: &str) -> Builder {
        let mut keep = Keep::default();
        let args: Vec<String> = std::env::args().collect();
        let argc = args.len() as c_int;
        let argv = keep.argv(&args);
        let n = keep.s(name);
        Builder {
            b: unsafe { sys::cf_app_builder(n, argc, argv) },
            keep,
        }
    }

    /// Expand `@file` arguments on the command line of a builder made by
    /// [`Builder::app`] (off by default: an argument that starts with `@`
    /// is an ordinary argument, so an untrusted one cannot inject options
    /// or tell which files exist). A file that cannot be read is an error
    /// from [`Builder::create`]. Needs libconfigforge 1.4.1.
    pub fn response_files(self) -> Builder {
        unsafe { sys::cf_builder_response_files(self.b) };
        self
    }

    /// Arguments as a program would see them after its own name.
    pub fn cli(mut self, name: &str, rank: i32, args: &[&str]) -> Builder {
        let mut all = vec!["program".to_string()];
        all.extend(args.iter().map(|a| a.to_string()));
        let argc = all.len() as c_int;
        let argv = self.keep.argv(&all);
        let n = self.keep.s(name);
        unsafe { sys::cf_builder_cli(self.b, n, rank, argc, argv) };
        self
    }

    /// Variables given here, as NAME=value pairs - the process's own with
    /// [`Builder::process_env`].
    pub fn env(mut self, name: &str, rank: i32, prefix: &str, vars: &[(&str, &str)]) -> Builder {
        let items: Vec<String> = vars.iter().map(|(k, v)| format!("{}={}", k, v)).collect();
        let envp = self.keep.argv(&items);
        let (n, p) = (self.keep.s(name), self.keep.s(prefix));
        unsafe { sys::cf_builder_env(self.b, n, rank, p, envp) };
        self
    }

    pub fn process_env(self, name: &str, rank: i32, prefix: &str) -> Builder {
        let vars: Vec<(String, String)> = std::env::vars().collect();
        let refs: Vec<(&str, &str)> = vars.iter().map(|(k, v)| (k.as_str(), v.as_str())).collect();
        self.env(name, rank, prefix, &refs)
    }

    /// A file, its format by extension unless given.
    pub fn file(mut self, name: &str, rank: i32, path: &str, flags: u32) -> Builder {
        let (n, p) = (self.keep.s(name), self.keep.s(path));
        unsafe { sys::cf_builder_file(self.b, n, rank, p, ptr::null(), flags) };
        self
    }

    /// A drop-in directory: every file in byte order, at one rank.
    pub fn dir(mut self, name: &str, rank: i32, path: &str, flags: u32) -> Builder {
        let (n, p) = (self.keep.s(name), self.keep.s(path));
        unsafe { sys::cf_builder_dir(self.b, n, rank, p, ptr::null(), flags) };
        self
    }

    pub fn dotenv(
        mut self,
        name: &str,
        rank: i32,
        path: &str,
        prefix: &str,
        flags: u32,
    ) -> Builder {
        let (n, p, x) = (self.keep.s(name), self.keep.s(path), self.keep.s(prefix));
        unsafe { sys::cf_builder_dotenv(self.b, n, rank, p, x, flags) };
        self
    }

    /// systemd's credentials: the directory `$CREDENTIALS_DIRECTORY` names,
    /// one file a key, read as a directory of secrets is. Every value's
    /// place in [`Config::explain`] is "systemd credential <id> (<file>)".
    /// With the variable unset, [`Builder::create`] fails naming it, and
    /// with [`OPTIONAL`] the source contributes nothing. `name` of `None` is
    /// "credentials".
    pub fn credentials(
        mut self,
        name: Option<&str>,
        rank: i32,
        prefix: Option<&str>,
        flags: u32,
    ) -> Builder {
        let (n, x) = (self.keep.opt(name), self.keep.opt(prefix));
        unsafe { sys::cf_builder_credentials(self.b, n, rank, x, flags) };
        self
    }

    /// Pairs from the program itself.
    pub fn memory(mut self, name: &str, rank: i32, pairs: &[(&str, &str)]) -> Builder {
        let n = self.keep.s(name);
        let (p, count) = self.keep.pairs(pairs);
        unsafe { sys::cf_builder_memory(self.b, n, rank, p, count) };
        self
    }

    pub fn define_int(mut self, key: &str, dflt: i64, min: i64, max: i64, flags: u32) -> Builder {
        let k = self.keep.s(key);
        unsafe { sys::cf_builder_int(self.b, k, dflt, min, max, flags) };
        self
    }

    pub fn define_text(mut self, key: &str, dflt: Option<&str>, flags: u32) -> Builder {
        let (k, d) = (self.keep.s(key), self.keep.opt(dflt));
        unsafe { sys::cf_builder_text(self.b, k, d, flags) };
        self
    }

    pub fn define_bool(mut self, key: &str, dflt: bool, flags: u32) -> Builder {
        let k = self.keep.s(key);
        unsafe { sys::cf_builder_bool(self.b, k, dflt as sys::cf_bool, flags) };
        self
    }

    /// A full definition ([`Def`]).
    pub fn define(mut self, def: Def) -> Builder {
        check_limits();
        if def.list_mode.is_some() || def.list_defaults.is_some() {
            assert!(
                unsafe { sys::cf_has_feature(c("lists").as_ptr()) } != 0,
                "libconfigforge does not support list contracts"
            );
        }
        unsafe {
            let mut d: sys::cf_def = std::mem::zeroed();
            let k = self.keep.s(&def.key);
            sys::cf_def_init_sized(&mut d, std::mem::size_of::<sys::cf_def>(), def.kind, k);
            d.flags = def.flags;
            if def.fallback {
                d.on_invalid = sys::CF_INVALID_FALLBACK;
            }
            if let Some(v) = &def.default {
                d.has_default = 1;
                d.default_value = v.native(&mut self.keep);
            }
            if let Some(mode) = def.list_mode {
                d.list_mode = match mode {
                    ListMode::Replace => sys::CF_LIST_REPLACE,
                    ListMode::Append => sys::CF_LIST_APPEND,
                };
            }
            d.list_separator = def.list_separator as c_char;
            let list_defaults: Option<Vec<sys::cf_value>> = def
                .list_defaults
                .as_ref()
                .map(|values| values.iter().map(|v| v.native(&mut self.keep)).collect());
            if let Some(values) = &list_defaults {
                d.has_default = 1;
                d.list_defaults = values.as_ptr();
                d.list_default_count = values.len();
            }
            if let Some((lo, hi)) = def.range {
                d.has_range = 1;
                d.imin = lo;
                d.imax = hi;
            }
            if !def.allowed.is_empty() {
                let items: Vec<&str> = def.allowed.iter().map(|s| s.as_str()).collect();
                d.allowed = self.keep.list(&items);
            }
            if !def.policy.is_empty() {
                let items: Vec<&str> = def.policy.iter().map(|s| s.as_str()).collect();
                d.policy = self.keep.list(&items);
            }
            if !def.aliases.is_empty() {
                let items: Vec<&str> = def.aliases.iter().map(|s| s.as_str()).collect();
                d.aliases = self.keep.list(&items);
            }
            if let Some(r) = &def.replaced_by {
                d.replaced_by = self.keep.s(r);
            }
            sys::cf_builder_define(self.b, &d);
        }
        self
    }

    /// A prefix whose keys the program cannot know in advance.
    pub fn open(mut self, prefix: &str) -> Builder {
        let p = self.keep.s(prefix);
        unsafe { sys::cf_builder_open(self.b, p) };
        self
    }

    /// Where what is not an error goes, from `min` up (cf_log.h); a
    /// configuration derived from this one logs there too.
    pub fn log<F>(mut self, min: Level, f: F) -> Builder
    where
        F: Fn(&LogEvent<'_>) + Send + Sync + 'static,
    {
        let lg = log::logger(f);
        unsafe { sys::cf_builder_log(self.b, log::trampoline, log::Logger::user(&lg), min.raw()) };
        self.keep.logger = Some(lg);
        self
    }

    pub fn unknown(self, policy: Unknown) -> Builder {
        let p = match policy {
            Unknown::Ignore => sys::CF_UNKNOWN_IGNORE,
            Unknown::Warn => sys::CF_UNKNOWN_WARN,
            Unknown::Reject => sys::CF_UNKNOWN_REJECT,
        };
        unsafe { sys::cf_builder_unknown(self.b, p) };
        self
    }

    /// Record which keys the program reads: a getter and `has` count;
    /// iterating, explaining and reporting do not. A derived snapshot
    /// tracks too, from nothing.
    pub fn track_reads(self) -> Builder {
        unsafe { sys::cf_builder_track_reads(self.b) };
        self
    }

    /// Resolve everything, once. The error names the key and the place.
    pub fn create(mut self) -> Result<Config, Error> {
        if let Some(refused) = self.keep.refusal() {
            return Err(refused); /* Drop aborts the builder */
        }
        let mut e = new_error();
        let c = unsafe { sys::cf_builder_create(self.b, &mut e) };
        self.b = ptr::null_mut(); /* consumed either way */
        if c.is_null() {
            return Err(Error::from_raw(&e));
        }
        Ok(Config {
            c,
            _keep: std::mem::take(&mut self.keep),
            _shared: None,
        })
    }
}

impl Default for Builder {
    fn default() -> Builder {
        Builder::new()
    }
}

impl Drop for Builder {
    fn drop(&mut self) {
        if !self.b.is_null() {
            unsafe { sys::cf_builder_abort(self.b) };
        }
    }
}

/// A resolved configuration: an immutable snapshot whose reads cannot
/// fail, and which can say why each value is what it is.
pub struct Config {
    c: *mut sys::cf_config,
    _keep: Keep,
    /// For a snapshot a [`Live`] built: the strings of every builder it
    /// was given, which the snapshot's sources may point into.
    _shared: Option<std::sync::Arc<std::sync::Mutex<Vec<Keep>>>>,
}

// Immutable after construction, its reference count atomic (D-14).
unsafe impl Send for Config {}
unsafe impl Sync for Config {}

fn render(f: impl Fn(*mut c_char, usize) -> usize) -> String {
    let n = f(ptr::null_mut(), 0);
    if n == 0 {
        return String::new();
    }
    let mut buf = vec![0u8; n + 1];
    f(buf.as_mut_ptr() as *mut c_char, n + 1);
    buf.truncate(n);
    String::from_utf8_lossy(&buf).into_owned()
}

impl Config {
    fn shared(c: *mut sys::cf_config, kept: std::sync::Arc<std::sync::Mutex<Vec<Keep>>>) -> Config {
        Config {
            c,
            _keep: Keep::default(),
            _shared: Some(kept),
        }
    }

    /// What identifies this configuration in a log or a trace; a secret
    /// counts only by the digest a value-blind report shows for it.
    pub fn fingerprint(&self) -> u64 {
        unsafe { sys::cf_fingerprint(self.c) }
    }

    /// [`Config::fingerprint`] as a report's `fingerprint` field says it.
    pub fn fingerprint_text(&self) -> String {
        fingerprint_text(self.fingerprint())
    }

    pub fn has(&self, key: &str) -> bool {
        let k = c(key);
        unsafe { sys::cf_has(self.c, k.as_ptr()) != 0 }
    }

    /// Whether the builder asked for [`Builder::track_reads`].
    pub fn tracks_reads(&self) -> bool {
        unsafe { sys::cf_tracks_reads(self.c) != 0 }
    }

    /// Whether the program has read `key`; asking is not a read.
    pub fn was_read(&self, key: &str) -> bool {
        let k = c(key);
        unsafe { sys::cf_was_read(self.c, k.as_ptr()) != 0 }
    }

    /// How many keys the program has read, each counted once.
    pub fn read_count(&self) -> usize {
        unsafe { sys::cf_read_count(self.c) }
    }

    pub fn int(&self, key: &str, fallback: i64) -> i64 {
        let k = c(key);
        unsafe { sys::cf_get_int(self.c, k.as_ptr(), fallback) }
    }

    pub fn boolean(&self, key: &str, fallback: bool) -> bool {
        let k = c(key);
        unsafe { sys::cf_get_bool(self.c, k.as_ptr(), fallback as sys::cf_bool) != 0 }
    }

    /// The text of a key, or None when it has no value.
    pub fn text(&self, key: &str) -> Option<String> {
        let k = c(key);
        let p = unsafe { sys::cf_get_text(self.c, k.as_ptr(), ptr::null()) };
        if p.is_null() {
            None
        } else {
            Some(text_of(p))
        }
    }

    /// The text of a key without a copy: a view into this snapshot,
    /// borrowed for as long as the `Config` is - which the compiler holds
    /// the caller to. None when the key is absent, holds another type,
    /// or is not UTF-8.
    pub fn str(&self, key: &str) -> Option<&str> {
        let k = c(key);
        let mut p: *const c_char = ptr::null();
        let mut n: usize = 0;
        if unsafe { sys::cf_try_text(self.c, k.as_ptr(), &mut p, &mut n) } != sys::CF_OK {
            return None;
        }
        // The bytes live in the snapshot's arena, which `self` keeps alive
        // and nothing changes: a snapshot is immutable (D-14).
        let bytes = unsafe { std::slice::from_raw_parts(p as *const u8, n) };
        std::str::from_utf8(bytes).ok()
    }

    fn walk(&self, prefix: &str, children: bool) -> Vec<String> {
        let p = c(prefix);
        let mut it: sys::cf_iter = unsafe { std::mem::zeroed() };
        let mut out = Vec::new();
        unsafe {
            if children {
                sys::cf_iter_children(&mut it, self.c, p.as_ptr());
            } else {
                sys::cf_iter_keys(&mut it, self.c, p.as_ptr());
            }
            while sys::cf_iter_next(&mut it) != 0 {
                let (s, n) = if children {
                    (it.name, it.namelen)
                } else {
                    (it.key, it.keylen)
                };
                let bytes = std::slice::from_raw_parts(s as *const u8, n);
                out.push(String::from_utf8_lossy(bytes).into_owned());
            }
        }
        out
    }

    /// Each distinct immediate child of a prefix, once: how a program
    /// discovers what is configured rather than being told in advance.
    pub fn children(&self, prefix: &str) -> Vec<String> {
        self.walk(prefix, true)
    }

    /// Every key under a prefix.
    pub fn keys(&self, prefix: &str) -> Vec<String> {
        self.walk(prefix, false)
    }

    /// Every candidate for one key, the winner and why.
    pub fn explain(&self, key: &str) -> String {
        let k = c(key);
        render(|buf, cap| unsafe { sys::cf_explain(self.c, k.as_ptr(), buf, cap) })
    }

    /// Every key, explained.
    pub fn inspect(&self) -> String {
        render(|buf, cap| unsafe { sys::cf_inspect(self.c, buf, cap) })
    }

    /// What changed since `before`, a line per key, secrets redacted.
    pub fn changes_since(&self, before: &Config) -> String {
        render(|buf, cap| unsafe { sys::cf_diff_text(before.c, self.c, buf, cap) })
    }

    /// A new snapshot with `pairs` applied as a source named `source`;
    /// this one is unchanged.
    pub fn derive(&self, source: &str, pairs: &[(&str, &str)]) -> Result<Config, Error> {
        let mut keep = Keep {
            logger: self._keep.logger.clone(),
            ..Keep::default()
        };
        let s = keep.s(source);
        let (p, n) = keep.pairs(pairs);
        if let Some(refused) = keep.refusal() {
            return Err(refused);
        }
        let mut e = new_error();
        let d = unsafe { sys::cf_derive(self.c, s, p, n, &mut e) };
        if d.is_null() {
            return Err(Error::from_raw(&e));
        }
        // The derived configuration copies the name and the values; the
        // strings are kept with it all the same, as the builder's are.
        Ok(Config {
            c: d,
            _keep: keep,
            _shared: self._shared.clone(),
        })
    }
}

/// The keys and the fingerprint, not the values: a value may be a secret,
/// and a `{:?}` in a log line is where secrets leak. Not the digest
/// either, which hashes a secret's value itself.
impl fmt::Debug for Config {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Config")
            .field("keys", &self.keys(""))
            .field("fingerprint", &format_args!("{}", self.fingerprint_text()))
            .finish()
    }
}

/// A fingerprint as reports and logs write it: "fnv1a:" and sixteen hex
/// digits (cf_fingerprint_format).
pub fn fingerprint_text(fp: u64) -> String {
    format!("fnv1a:{:016x}", fp)
}

impl Drop for Config {
    fn drop(&mut self) {
        unsafe { sys::cf_config_free(self.c) };
    }
}

/// The library's version, and the commit it was built from.
pub fn version() -> String {
    format!(
        "{} ({})",
        text_of(unsafe { sys::cf_version() }),
        text_of(unsafe { sys::cf_build_id() })
    )
}

#[cfg(test)]
mod list_tests {
    use super::{rank, Builder, Def, ListMode};

    #[test]
    fn lists_append_and_keep_their_contract_when_derived() {
        let cfg = Builder::new()
            .define(
                Def::text("tags")
                    .list(ListMode::Append, b',')
                    .default_text_list(&["base"])
                    .aliases(&["t"]),
            )
            .env("env", rank::ENV, "APP_", &[("APP_TAGS", "e1,e2")])
            .cli("cli", rank::CLI, &["--tags=c1", "-t", "c2"])
            .create()
            .unwrap();
        assert_eq!(cfg.int("tags.count", -1), 5);
        for (i, expected) in ["base", "e1", "e2", "c1", "c2"].iter().enumerate() {
            assert_eq!(cfg.text(&format!("tags.{i}")).as_deref(), Some(*expected));
        }
        let derived = cfg.derive("view", &[]).unwrap();
        drop(cfg);
        assert_eq!(derived.int("tags.count", -1), 5);
        assert_eq!(derived.text("tags.4").as_deref(), Some("c2"));
    }

    #[test]
    fn list_defaults_are_typed_and_can_be_empty() {
        let cfg = Builder::new()
            .define(
                Def::int("ports")
                    .list(ListMode::Replace, b',')
                    .default_int_list(&[1, 2])
                    .range(1, 9),
            )
            .define(
                Def::boolean("switches")
                    .list(ListMode::Append, 0)
                    .default_bool_list(&[false, true]),
            )
            .define(
                Def::text("empty")
                    .list(ListMode::Replace, b',')
                    .default_text_list(&[]),
            )
            .create()
            .unwrap();
        assert_eq!(cfg.int("ports.1", 0), 2);
        assert!(!cfg.boolean("switches.0", true));
        assert!(cfg.boolean("switches.1", false));
        assert_eq!(cfg.int("empty.count", -1), 0);
        assert!(Builder::new()
            .define(
                Def::int("ports")
                    .list(ListMode::Append, b',')
                    .default_int_list(&[10])
                    .range(1, 9)
            )
            .create()
            .is_err());
        assert!(Builder::new()
            .define(
                Def::int("ports")
                    .list(ListMode::Append, b',')
                    .default_text_list(&["1"])
            )
            .create()
            .is_err());
        assert!(Builder::new()
            .define(
                Def::text("tags")
                    .list(ListMode::Append, b',')
                    .default_text("not a list")
            )
            .create()
            .is_err());
    }

    #[test]
    fn list_policy_and_unsplit_environment_are_preserved() {
        let cfg = Builder::new()
            .define(
                Def::text("tags")
                    .list(ListMode::Append, 0)
                    .default_text_list(&["base"])
                    .policy(&["env"]),
            )
            .env("env", rank::ENV, "APP_", &[("APP_TAGS", "a,b")])
            .cli("cli", rank::CLI, &["--tags=ignored"])
            .create()
            .unwrap();
        assert_eq!(cfg.int("tags.count", -1), 2);
        assert_eq!(cfg.text("tags.1").as_deref(), Some("a,b"));
    }
}

#[cfg(test)]
mod nul_tests {
    use super::{rank, Builder};

    #[test]
    fn a_string_with_a_nul_is_refused_without_being_quoted() {
        let refused = Builder::new()
            .memory("mem", rank::USER_FILE, &[("db.password", "hunter\0two")])
            .create();
        let err = match refused {
            Ok(_) => panic!("a value with a NUL was accepted"),
            Err(err) => err,
        };
        assert_eq!(err.code, super::sys::CF_E_CONVERT);
        assert!(err.message.contains("byte 6"), "{}", err.message);
        assert!(!err.message.contains("hunter"), "{}", err.message);
    }
}

#[cfg(test)]
mod read_tests {
    use super::{Builder, Def};

    #[test]
    fn reads_are_recorded_when_asked_for() {
        let plain = Builder::new()
            .define(Def::int("port").default_int(8080))
            .create()
            .unwrap();
        assert!(!plain.tracks_reads());
        let cfg = Builder::new()
            .define(Def::int("port").default_int(8080))
            .define(Def::text("host").default_text("web1"))
            .define(Def::boolean("tls").default_bool(false))
            .track_reads()
            .create()
            .unwrap();
        assert!(cfg.tracks_reads());
        cfg.explain("port");
        cfg.keys("");
        assert!(
            !cfg.was_read("port"),
            "explaining and listing are not reads"
        );
        assert_eq!(cfg.int("port", 0), 8080);
        assert!(cfg.has("tls"));
        assert!(cfg.was_read("port") && cfg.was_read("tls"));
        assert!(!cfg.was_read("host"));
        assert_eq!(cfg.read_count(), 2);
        let derived = cfg.derive("admin", &[]).unwrap();
        assert!(derived.tracks_reads());
        assert_eq!(derived.read_count(), 0, "a derived snapshot starts afresh");
    }
}

/// `Builder::app` reads the process's real command line, so the test runs
/// this binary again with an `@file` argument (a test filter that matches
/// nothing, to the harness) and reads what the child says.
#[cfg(test)]
mod response_file_tests {
    use super::Builder;
    use std::process::Command;

    #[test]
    fn response_files_child() {
        if std::env::var("RSP_CHILD").is_err() {
            return;
        }
        let mut b = Builder::app("rsp");
        if std::env::var("RSP_ON").as_deref() == Ok("1") {
            b = b.response_files();
        }
        match b.create() {
            Ok(c) => println!("RSP:ok {:?}", c.text("rsp.key")),
            Err(e) => println!("RSP:err {}", e.message),
        }
    }

    fn ask(on: bool, arg: &str) -> String {
        let out = Command::new(std::env::current_exe().unwrap())
            .args(["response_files_child", "--nocapture"])
            .arg(arg)
            .env("RSP_CHILD", "1")
            .env("RSP_ON", if on { "1" } else { "0" })
            .output()
            .unwrap();
        String::from_utf8_lossy(&out.stdout)
            .lines()
            .find_map(|l| l.strip_prefix("RSP:").map(str::to_string))
            .unwrap_or_else(|| "child said nothing".to_string())
    }

    #[test]
    fn response_files_are_opt_in() {
        let dir = std::env::temp_dir();
        let file = dir.join(format!("cf_rsp_args_{}.txt", std::process::id()));
        std::fs::write(&file, "--rsp.key=from-file\n").unwrap();
        let there = format!("@{}", file.display());
        let absent = format!("@{}", dir.join("cf_rsp_absent.txt").display());

        assert_eq!(ask(false, &there), "ok None", "off: the file is not read");
        assert_eq!(ask(false, &absent), "ok None", "off: a missing one is fine");
        assert_eq!(ask(true, &there), "ok Some(\"from-file\")");
        let refused = ask(true, &absent);
        assert!(
            refused.starts_with("err ") && refused.contains("cf_rsp_absent"),
            "{refused}"
        );
        std::fs::remove_file(&file).unwrap();
    }

    #[test]
    fn a_builder_with_no_command_line_takes_it_harmlessly() {
        assert!(Builder::new()
            .response_files()
            .response_files()
            .create()
            .is_ok());
    }
}

/// The library reads `$CREDENTIALS_DIRECTORY` through the C runtime, so the
/// test runs this binary again with the variable set (or not) for the
/// child, as the response-file test does, and `set_var` stays out of it.
#[cfg(test)]
mod credentials_tests {
    use super::{rank, Builder, OPTIONAL};
    use std::process::Command;

    #[test]
    fn credentials_child() {
        if std::env::var("CRED_CHILD").is_err() {
            return;
        }
        let flags = if std::env::var("CRED_OPTIONAL").as_deref() == Ok("1") {
            OPTIONAL
        } else {
            0
        };
        match Builder::new()
            .credentials(None, rank::ENV, None, flags)
            .create()
        {
            Ok(c) => println!(
                "CRED:ok {:?} {:?} {}",
                c.text("db.password"),
                c.text("token"),
                c.explain("db.password").replace('\n', " ")
            ),
            Err(e) => println!("CRED:err {}", e.message),
        }
    }

    fn ask(directory: Option<&str>, optional: bool) -> String {
        let mut cmd = Command::new(std::env::current_exe().unwrap());
        cmd.args(["credentials_child", "--nocapture"])
            .env("CRED_CHILD", "1")
            .env("CRED_OPTIONAL", if optional { "1" } else { "0" })
            .env_remove("CREDENTIALS_DIRECTORY");
        if let Some(d) = directory {
            cmd.env("CREDENTIALS_DIRECTORY", d);
        }
        let out = cmd.output().unwrap();
        String::from_utf8_lossy(&out.stdout)
            .lines()
            .find_map(|l| l.strip_prefix("CRED:").map(str::to_string))
            .unwrap_or_else(|| "child said nothing".to_string())
    }

    #[test]
    fn a_directory_of_credentials_loads_and_says_where_from() {
        let dir = std::env::temp_dir().join(format!("cf_credentials_{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("db.password"), "hunter2\n").unwrap();
        std::fs::write(dir.join("token"), "abc").unwrap();
        let said = ask(Some(dir.to_str().unwrap()), false);
        assert!(said.starts_with("ok Some(\"hunter2\") Some(\"abc\")"), "{said}");
        assert!(said.contains("systemd credential"), "{said}");
        std::fs::remove_dir_all(&dir).unwrap();
    }

    #[test]
    fn unset_is_nothing_when_optional_and_names_the_variable_otherwise() {
        assert!(ask(None, true).starts_with("ok None None"));
        let refused = ask(None, false);
        assert!(
            refused.starts_with("err ") && refused.contains("CREDENTIALS_DIRECTORY"),
            "{refused}"
        );
    }
}
