//! A configuration that reloads itself, in a mode the program chooses
//! (cf_live, D-48).
//!
//! ```no_run
//! use configforge::{Builder, Live, Mode};
//!
//! let mut live = Live::open(Mode::Auto, || {
//!     Builder::new()
//!         .file("gateway.ini", configforge::rank::USER_FILE, "gateway.ini", 0)
//!         .define_int("server.port", 8080, 1, 65535, configforge::flag::RESTART)
//! })
//! .unwrap();
//!
//! let reader = live.reader();               // Send + Sync + Clone, for the workers
//! std::thread::spawn(move || {
//!     let cfg = reader.get();               // one request, one snapshot
//!     let _ = cfg.int("server.port", 0);
//! });
//! live.poll().unwrap();                     // in the loop that drives reloads
//! ```

use crate::{new_error, sys, Builder, Config, Error, Keep};
use std::ffi::c_void;
use std::sync::{Arc, Mutex};
use std::time::Duration;

/// Who decides that a change is taken.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub enum Mode {
    /// Only [`Live::reload`] rebuilds.
    Manual,
    /// A change is built and waits for [`Live::approve`].
    Approve,
    /// A change is built and swapped in.
    Auto,
}

/// What a poll, a reload or an approval did. A failure is the `Err` of
/// the call, and leaves the running snapshot serving.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub enum Reload {
    Unchanged,
    Swapped,
    Pending,
}

/// What a health page says about the reloads ([`Live::status`]).
#[derive(Clone, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub struct Status {
    /// As [`Live::generation`].
    pub generation: u64,
    /// Rebuilds tried, from poll, reload and wait.
    pub reloads: u64,
    /// Of those, refused or failed.
    pub failures: u64,
    /// What the last rebuild or approval did; `None` when it failed.
    pub last: Option<Reload>,
    /// A change waits for [`Live::approve`].
    pub pending: bool,
    /// Why the last failure failed.
    pub last_error: Option<Error>,
}

/// The strings every builder handed to the C side borrows, kept for as
/// long as any snapshot built from them may be read.
type Kept = Arc<Mutex<Vec<Keep>>>;

struct State {
    build: Box<dyn FnMut() -> Builder + Send>,
    kept: Kept,
}

/// Called by cf_live, on the thread that polls, for each rebuild.
extern "C" fn trampoline(user: *mut c_void) -> *mut sys::cf_builder {
    let state = unsafe { &mut *(user as *mut State) };
    // A panic must not unwind into C; a builder that panicked is a build
    // that failed, which cf_live already reports as one.
    let made = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| (state.build)()));
    match made {
        Ok(b) if b.keep.refusal().is_some() => {
            drop(b); // a string with a NUL: a build that failed
            std::ptr::null_mut()
        }
        Ok(mut b) => {
            let raw = b.b;
            b.b = std::ptr::null_mut();
            state
                .kept
                .lock()
                .unwrap_or_else(|p| p.into_inner())
                .push(std::mem::take(&mut b.keep));
            raw
        }
        Err(_) => std::ptr::null_mut(),
    }
}

struct Inner {
    l: *mut sys::cf_live,
    state: *mut State,
    kept: Kept,
}

// cf_live_get is safe on any thread (cf_live.h); everything else goes
// through &mut Live, of which there is one.
unsafe impl Send for Inner {}
unsafe impl Sync for Inner {}

impl Drop for Inner {
    fn drop(&mut self) {
        unsafe {
            sys::cf_live_close(self.l);
            drop(Box::from_raw(self.state));
        }
    }
}

impl Inner {
    fn get(&self) -> Config {
        let c = unsafe { sys::cf_live_get(self.l) };
        Config::shared(c, self.kept.clone())
    }

    fn status(&self) -> Status {
        let mut s = sys::cf_live_status {
            size: std::mem::size_of::<sys::cf_live_status>(),
            generation: 0,
            reloads: 0,
            failures: 0,
            last: 0,
            pending: 0,
        };
        let mut e = new_error();
        unsafe { sys::cf_live_status_get(self.l, &mut s) };
        let failed = unsafe { sys::cf_live_last_error(self.l, &mut e) } != 0;
        Status {
            generation: s.generation,
            reloads: s.reloads,
            failures: s.failures,
            last: result(s.last, &e).ok(),
            pending: s.pending != 0,
            last_error: failed.then(|| Error::from_raw(&e)),
        }
    }
}

/// The reloads, driven from one place: poll, reload, approve.
pub struct Live {
    inner: Arc<Inner>,
}

/// The read side of a [`Live`], for any number of threads.
#[derive(Clone)]
pub struct LiveReader {
    inner: Arc<Inner>,
}

impl LiveReader {
    /// The current snapshot. A reader keeps the one it got.
    pub fn get(&self) -> Config {
        self.inner.get()
    }

    /// The reloads' status, for a health page on a worker thread.
    pub fn status(&self) -> Status {
        self.inner.status()
    }
}

fn result(r: std::os::raw::c_int, e: &sys::cf_error) -> Result<Reload, Error> {
    match r {
        sys::CF_LIVE_SWAPPED => Ok(Reload::Swapped),
        sys::CF_LIVE_PENDING => Ok(Reload::Pending),
        sys::CF_LIVE_UNCHANGED => Ok(Reload::Unchanged),
        _ => Err(Error::from_raw(e)),
    }
}

impl Live {
    /// Build the first snapshot with `build`, which is called again for
    /// each reload, on the thread that polls.
    pub fn open<F>(mode: Mode, build: F) -> Result<Live, Error>
    where
        F: FnMut() -> Builder + Send + 'static,
    {
        let kept: Kept = Arc::new(Mutex::new(Vec::new()));
        let state = Box::into_raw(Box::new(State {
            build: Box::new(build),
            kept: kept.clone(),
        }));
        let m = match mode {
            Mode::Manual => sys::CF_LIVE_MANUAL,
            Mode::Approve => sys::CF_LIVE_APPROVE,
            Mode::Auto => sys::CF_LIVE_AUTO,
        };
        let mut e = new_error();
        let l = unsafe {
            sys::cf_live_open(
                m,
                trampoline,
                std::ptr::null(),
                state as *mut c_void,
                std::ptr::null(),
                &mut e,
            )
        };
        if l.is_null() {
            unsafe { drop(Box::from_raw(state)) };
            return Err(Error::from_raw(&e));
        }
        Ok(Live {
            inner: Arc::new(Inner { l, state, kept }),
        })
    }

    /// The current snapshot.
    pub fn get(&self) -> Config {
        self.inner.get()
    }

    /// A handle the workers can share.
    pub fn reader(&self) -> LiveReader {
        LiveReader {
            inner: self.inner.clone(),
        }
    }

    /// Rebuild if a file changed (Approve and Auto).
    pub fn poll(&mut self) -> Result<Reload, Error> {
        let mut e = new_error();
        let r = unsafe { sys::cf_live_poll(self.inner.l, &mut e) };
        result(r, &e)
    }

    /// Rebuild now, in any mode.
    pub fn reload(&mut self) -> Result<Reload, Error> {
        let mut e = new_error();
        let r = unsafe { sys::cf_live_reload(self.inner.l, &mut e) };
        result(r, &e)
    }

    /// Wait up to `timeout` for a file to change, then poll.
    pub fn wait(&mut self, timeout: Duration) -> Result<Reload, Error> {
        let mut e = new_error();
        let ms = timeout.as_millis().min(i32::MAX as u128) as std::os::raw::c_long;
        let r = unsafe { sys::cf_live_wait(self.inner.l, ms, &mut e) };
        result(r, &e)
    }

    /// The snapshot waiting for approval.
    pub fn pending(&self) -> Option<Config> {
        let p = unsafe { sys::cf_live_pending(self.inner.l) } as *mut sys::cf_config;
        if p.is_null() {
            return None;
        }
        unsafe { sys::cf_config_ref(p) };
        Some(Config::shared(p, self.inner.kept.clone()))
    }

    pub fn approve(&mut self) -> Result<Reload, Error> {
        let mut e = new_error();
        let r = unsafe { sys::cf_live_approve(self.inner.l, &mut e) };
        result(r, &e)
    }

    pub fn reject(&mut self) {
        unsafe { sys::cf_live_reject(self.inner.l) };
    }

    /// The reloads' status: counts, the last result, the last failure.
    pub fn status(&self) -> Status {
        self.inner.status()
    }

    /// How many snapshots readers have been given.
    pub fn generation(&self) -> u64 {
        unsafe { sys::cf_live_generation(self.inner.l) }
    }

    /// The keys defined RESTART whose new value waits for a restart.
    pub fn restarts(&self) -> Vec<String> {
        let n = unsafe { sys::cf_live_restarts(self.inner.l, std::ptr::null_mut(), 0) };
        if n == 0 {
            return Vec::new();
        }
        let mut buf = vec![0u8; n + 1];
        unsafe { sys::cf_live_restarts(self.inner.l, buf.as_mut_ptr() as *mut _, n + 1) };
        buf.truncate(n);
        String::from_utf8_lossy(&buf)
            .lines()
            .map(str::to_string)
            .collect()
    }
}

#[cfg(test)]
mod tests {
    use super::{Live, Mode, Reload};
    use crate::{flag, rank, Builder};
    use std::sync::atomic::{AtomicUsize, Ordering};
    use std::sync::Arc;

    fn levels() -> (Arc<AtomicUsize>, impl FnMut() -> Builder + Send + 'static) {
        let which = Arc::new(AtomicUsize::new(0));
        let w = which.clone();
        let build = move || {
            let sets: [&[(&str, &str)]; 3] = [
                &[("server.port", "8080"), ("log.level", "info")],
                &[("server.port", "9443"), ("log.level", "debug")],
                &[("server.port", "99999"), ("log.level", "warn")],
            ];
            Builder::new()
                .memory("mem", rank::USER_FILE, sets[w.load(Ordering::SeqCst)])
                .define_int("server.port", 8080, 1, 65535, flag::RESTART)
                .define_text("log.level", Some("info"), 0)
        };
        (which, build)
    }

    #[test]
    fn approve_restart_and_refuse() {
        let (which, build) = levels();
        let mut live = Live::open(Mode::Approve, build).unwrap();
        assert_eq!(live.generation(), 1);
        which.store(1, Ordering::SeqCst);
        assert_eq!(live.reload().unwrap(), Reload::Pending);
        assert_eq!(live.get().text("log.level").as_deref(), Some("info"));
        assert_eq!(
            live.pending().unwrap().text("log.level").as_deref(),
            Some("debug")
        );
        assert_eq!(live.approve().unwrap(), Reload::Swapped);
        let cfg = live.get();
        assert_eq!(cfg.text("log.level").as_deref(), Some("debug"));
        assert_eq!(
            cfg.int("server.port", 0),
            8080,
            "the port waits for a restart"
        );
        assert_eq!(live.restarts(), vec!["server.port".to_string()]);
        assert!(cfg.explain("server.port").contains("running"));

        which.store(2, Ordering::SeqCst);
        let e = live.reload().unwrap_err();
        assert!(e.message.contains("65535"), "{}", e.message);
        let st = live.reader().status();
        assert_eq!((st.generation, st.reloads, st.failures), (2, 2, 1));
        assert_eq!(st.last, None);
        assert!(st.last_error.unwrap().message.contains("65535"));
        assert_eq!(
            live.get().text("log.level").as_deref(),
            Some("debug"),
            "still serving"
        );
        drop(live);
        assert_eq!(
            cfg.text("log.level").as_deref(),
            Some("debug"),
            "a snapshot outlives its Live"
        );
    }

    #[test]
    fn readers_on_threads() {
        let (which, build) = levels();
        let mut live = Live::open(Mode::Manual, build).unwrap();
        let readers: Vec<_> = (0..4)
            .map(|_| {
                let r = live.reader();
                std::thread::spawn(move || {
                    for _ in 0..2000 {
                        let level = r.get().text("log.level").unwrap();
                        assert!(level == "info" || level == "debug");
                    }
                })
            })
            .collect();
        for i in 0..200 {
            which.store(i % 2, Ordering::SeqCst);
            live.reload().unwrap();
        }
        for t in readers {
            t.join().unwrap();
        }
    }

    #[test]
    fn a_panicking_build_is_a_failed_one() {
        let mut first = true;
        let mut live = Live::open(Mode::Manual, move || {
            if !first {
                panic!("no builder today");
            }
            first = false;
            Builder::new()
        })
        .unwrap();
        assert!(live.reload().is_err());
    }
}
