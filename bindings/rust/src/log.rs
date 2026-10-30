//! What the library says that is not an error (cf_log.h): an unknown
//! key under `Unknown::Warn`, a deprecated key in use, a value refused
//! and fallen back from, each reload of a [`Live`](crate::Live).
//!
//! ```no_run
//! use configforge::{Builder, Level};
//! let cfg = Builder::new()
//!     .log(Level::Info, |e| eprintln!("configforge {} {:?}: {}", e.event, e.key, e.message))
//!     .create();
//! ```

use crate::sys;
use std::ffi::{c_void, CStr};
use std::os::raw::c_char;
use std::sync::Arc;

/// How loud an event is; the numbers are the library's, which are
/// Python's and syslog-like: higher is louder.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[non_exhaustive]
pub enum Level {
    Debug,
    Info,
    Warn,
    Error,
}

impl Level {
    pub(crate) fn raw(self) -> std::os::raw::c_int {
        match self {
            Level::Debug => 10,
            Level::Info => 20,
            Level::Warn => 30,
            Level::Error => 40,
        }
    }

    fn from_raw(v: std::os::raw::c_int) -> Level {
        match v {
            v if v >= 40 => Level::Error,
            v if v >= 30 => Level::Warn,
            v if v >= 20 => Level::Info,
            _ => Level::Debug,
        }
    }
}

/// One event, borrowed for the call: copy what outlives it.
#[derive(Clone, Copy, Debug)]
#[non_exhaustive]
pub struct LogEvent<'a> {
    pub level: Level,
    /// Stable, for a log pipeline to match: "unknown-key", "deprecated-key",
    /// "fallback", "reload", "reload-pending", "reload-failed",
    /// "restart-pending".
    pub event: &'a str,
    /// The library's error code: 0 for information.
    pub code: i32,
    pub key: Option<&'a str>,
    pub source: Option<&'a str>,
    pub message: &'a str,
}

type Sink = dyn Fn(&LogEvent<'_>) + Send + Sync;

/// The function behind a log, shared by every configuration that may
/// call it: the builder's, its snapshot's, and every one derived.
pub(crate) struct Logger {
    sink: Box<Sink>,
}

pub(crate) type SharedLogger = Arc<Logger>;

pub(crate) fn logger<F>(f: F) -> SharedLogger
where
    F: Fn(&LogEvent<'_>) + Send + Sync + 'static,
{
    Arc::new(Logger { sink: Box::new(f) })
}

impl Logger {
    pub(crate) fn user(this: &SharedLogger) -> *mut c_void {
        Arc::as_ptr(this) as *mut c_void
    }
}

fn opt<'a>(p: *const c_char) -> Option<&'a str> {
    if p.is_null() {
        None
    } else {
        unsafe { CStr::from_ptr(p) }.to_str().ok()
    }
}

/// Called by the library, on the thread that creates or reloads.
pub(crate) extern "C" fn trampoline(user: *mut c_void, e: *const sys::cf_log_event) {
    if user.is_null() || e.is_null() {
        return;
    }
    let logger = unsafe { &*(user as *const Logger) };
    let raw = unsafe { &*e };
    let ev = LogEvent {
        level: Level::from_raw(raw.level),
        event: opt(raw.event).unwrap_or(""),
        code: raw.code,
        key: opt(raw.key),
        source: opt(raw.source),
        message: opt(raw.message).unwrap_or(""),
    };
    // A panic must not unwind into C; a log line that panicked is lost.
    let _ = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| (logger.sink)(&ev)));
}
