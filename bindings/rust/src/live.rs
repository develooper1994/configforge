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
    /// Readers have the snapshot before the last swap again.
    RolledBack,
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
    /// Swaps undone, by [`Live::rollback`] or the health function.
    pub rollbacks: u64,
    /// A swap is on probation now.
    pub probation: bool,
}

type Health = Box<dyn FnMut(&Config) -> Result<(), String> + Send>;

/// How a [`Live`] is opened beyond its mode and build: the log, and
/// probation (cf_live.h) - for `polls` polls after each swap the snapshot
/// it replaced is kept, so that [`Live::rollback`] can return to it, and
/// `health`, asked at each of those polls, rolls back by answering `Err`.
#[derive(Default)]
pub struct Options {
    log: Option<(crate::Level, crate::log::SharedLogger)>,
    probation: u32,
    health: Option<Health>,
}

impl Options {
    pub fn new() -> Options {
        Options::default()
    }

    /// Each reload's result, and the events of every configuration built,
    /// to `log` from `min` up.
    pub fn log<L>(mut self, min: crate::Level, log: L) -> Options
    where
        L: Fn(&crate::LogEvent<'_>) + Send + Sync + 'static,
    {
        self.log = Some((min, crate::log::logger(log)));
        self
    }

    /// Polls a swap stays on probation; 0, the default, is none.
    pub fn probation(mut self, polls: u32) -> Options {
        self.probation = polls;
        self
    }

    /// Asked at each poll of a probation; `Err(reason)` rolls it back.
    pub fn health<H>(mut self, health: H) -> Options
    where
        H: FnMut(&Config) -> Result<(), String> + Send + 'static,
    {
        self.health = Some(Box::new(health));
        self
    }
}

/// The strings every builder handed to the C side borrows, kept for as
/// long as any snapshot built from them may be read.
type Kept = Arc<Mutex<Vec<Keep>>>;

struct State {
    build: Box<dyn FnMut() -> Builder + Send>,
    kept: Kept,
    health: Option<Health>,
}

/// Called by cf_live, on the thread that polls, during a probation.
extern "C" fn health_trampoline(
    user: *mut c_void,
    running: *const sys::cf_config,
    err: *mut sys::cf_error,
) -> sys::cf_bool {
    let state = unsafe { &mut *(user as *mut State) };
    let Some(health) = state.health.as_mut() else {
        return 1;
    };
    unsafe { sys::cf_config_ref(running as *mut sys::cf_config) };
    let cfg = Config::shared(running as *mut sys::cf_config, state.kept.clone());
    let said = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| health(&cfg)));
    let reason = match said {
        Ok(Ok(())) => return 1,
        Ok(Err(reason)) => reason,
        Err(_) => "the health function panicked".to_string(),
    };
    // 1.0's fields are always the caller's; cf_error_set is variadic.
    let e = unsafe { &mut *err };
    e.code = sys::CF_E_POLICY;
    let n = reason.len().min(e.message.len() - 1);
    for (d, s) in e.message.iter_mut().zip(reason.as_bytes()[..n].iter()) {
        *d = *s as std::os::raw::c_char;
    }
    e.message[n] = 0;
    0
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
            // The library copies every name and path it is given, and reads
            // the rest only while it creates; each build creates before the
            // next begins. So only this build's strings need to live - not
            // every reload's for as long as the Live does. A log closure is
            // another matter: any snapshot built with it, or derived from
            // one, may still call it, so the closures are kept.
            let mut kept = state.kept.lock().unwrap_or_else(|p| p.into_inner());
            let loggers: Vec<Keep> = kept
                .drain(..)
                .filter_map(|mut k| {
                    k.logger.take().map(|lg| Keep {
                        logger: Some(lg),
                        ..Keep::default()
                    })
                })
                .collect();
            kept.extend(loggers);
            kept.push(std::mem::take(&mut b.keep));
            raw
        }
        Err(_) => std::ptr::null_mut(),
    }
}

struct Inner {
    l: *mut sys::cf_live,
    state: *mut State,
    kept: Kept,
    /// The reloads' log, alive as long as the cf_live that calls it.
    _logger: Option<crate::log::SharedLogger>,
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
            rollbacks: 0,
            probation: 0,
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
            rollbacks: s.rollbacks,
            probation: s.probation != 0,
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
        sys::CF_LIVE_ROLLED_BACK => Ok(Reload::RolledBack),
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
        Live::open_with(mode, build, Options::new())
    }

    /// As [`Live::open`], with each reload's result - and the events of
    /// every configuration built - sent to `log` from `min` up.
    pub fn open_logged<F, L>(mode: Mode, build: F, min: crate::Level, log: L) -> Result<Live, Error>
    where
        F: FnMut() -> Builder + Send + 'static,
        L: Fn(&crate::LogEvent<'_>) + Send + Sync + 'static,
    {
        Live::open_with(mode, build, Options::new().log(min, log))
    }

    /// As [`Live::open`], with the log and probation `options` give.
    pub fn open_with<F>(mode: Mode, build: F, options: Options) -> Result<Live, Error>
    where
        F: FnMut() -> Builder + Send + 'static,
    {
        let Options {
            log,
            probation,
            health,
        } = options;
        let has_health = health.is_some();
        let kept: Kept = Arc::new(Mutex::new(Vec::new()));
        let state = Box::into_raw(Box::new(State {
            build: Box::new(build),
            kept: kept.clone(),
            health,
        }));
        let m = match mode {
            Mode::Manual => sys::CF_LIVE_MANUAL,
            Mode::Approve => sys::CF_LIVE_APPROVE,
            Mode::Auto => sys::CF_LIVE_AUTO,
        };
        let o = sys::cf_live_options {
            size: std::mem::size_of::<sys::cf_live_options>(),
            mode: m,
            build: Some(trampoline),
            build_user: state as *mut c_void,
            check: std::ptr::null(),
            check_user: std::ptr::null_mut(),
            allocator: std::ptr::null(),
            log: log
                .as_ref()
                .map(|_| crate::log::trampoline as sys::cf_log_fn),
            log_user: log
                .as_ref()
                .map_or(std::ptr::null_mut(), |(_, lg)| crate::log::Logger::user(lg)),
            log_min: log.as_ref().map_or(20, |(min, _)| min.raw()),
            health: has_health.then_some(health_trampoline as sys::cf_live_health_fn),
            health_user: state as *mut c_void,
            probation,
        };
        let mut e = new_error();
        let l = unsafe { sys::cf_live_open_with(&o, &mut e) };
        if l.is_null() {
            unsafe { drop(Box::from_raw(state)) };
            return Err(Error::from_raw(&e));
        }
        Ok(Live {
            inner: Arc::new(Inner {
                l,
                state,
                kept,
                _logger: log.map(|(_, lg)| lg),
            }),
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

    /// Back to the snapshot before the swap on probation; `Err` when no
    /// swap is on probation.
    pub fn rollback(&mut self, reason: Option<&str>) -> Result<Reload, Error> {
        let n = reason.map(crate::c);
        let mut e = new_error();
        let r = unsafe {
            sys::cf_live_rollback(
                self.inner.l,
                n.as_ref().map_or(std::ptr::null(), |n| n.as_ptr()),
                &mut e,
            )
        };
        result(r, &e)
    }

    /// End the probation, keeping the snapshot readers have; false when
    /// none was on probation.
    pub fn confirm(&mut self) -> bool {
        unsafe { sys::cf_live_confirm(self.inner.l) != 0 }
    }

    /// Commit `edit` and reload, in one step; nothing is written when the
    /// commit is refused.
    pub fn commit(&mut self, edit: crate::Edit, note: Option<&str>) -> Result<Reload, Error> {
        let raw = edit.into_raw();
        let n = note.map(crate::c);
        let mut e = new_error();
        let r = unsafe {
            sys::cf_live_commit(
                self.inner.l,
                raw,
                n.as_ref().map_or(std::ptr::null(), |n| n.as_ptr()),
                &mut e,
            )
        };
        result(r, &e)
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
    fn probation_and_rollback() {
        use super::Options;
        use std::sync::atomic::AtomicBool;
        let (which, build) = levels();
        let healthy = Arc::new(AtomicBool::new(true));
        let h = healthy.clone();
        let options = Options::new().probation(2).health(move |cfg| {
            if h.load(Ordering::SeqCst) {
                Ok(())
            } else {
                Err(format!(
                    "503 at {}",
                    cfg.text("log.level").unwrap_or_default()
                ))
            }
        });
        let mut live = Live::open_with(Mode::Manual, build, options).unwrap();
        assert!(live.rollback(None).is_err(), "nothing on probation yet");
        which.store(1, Ordering::SeqCst);
        assert_eq!(live.reload().unwrap(), Reload::Swapped);
        assert!(live.status().probation);
        assert_eq!(
            live.rollback(Some("pool did not connect")).unwrap(),
            Reload::RolledBack
        );
        assert_eq!(live.get().text("log.level").as_deref(), Some("info"));
        assert_eq!(live.status().rollbacks, 1);
        assert_eq!(live.reload().unwrap(), Reload::Swapped, "a retry");
        healthy.store(false, Ordering::SeqCst);
        assert_eq!(live.poll().unwrap(), Reload::RolledBack, "health said no");
        assert_eq!(live.get().text("log.level").as_deref(), Some("info"));
        healthy.store(true, Ordering::SeqCst);
        assert_eq!(live.reload().unwrap(), Reload::Swapped);
        assert_eq!(live.poll().unwrap(), Reload::Unchanged);
        assert_eq!(live.poll().unwrap(), Reload::Unchanged);
        assert!(!live.status().probation, "two healthy polls confirm it");
        assert!(!live.confirm());
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

    #[test]
    fn reloads_do_not_keep_every_builds_strings() {
        let (_which, build) = levels();
        let mut live = Live::open(Mode::Manual, build).unwrap();
        for _ in 0..50 {
            live.reload().unwrap();
        }
        let n = live.inner.kept.lock().unwrap().len();
        assert!(n <= 1, "{} builds' strings kept after 50 reloads", n);
        assert_eq!(live.get().text("log.level").as_deref(), Some("info"));
    }

    #[test]
    fn logs_reach_the_closure() {
        use crate::{Level, Unknown};
        use std::sync::Mutex;

        let seen = Arc::new(Mutex::new(Vec::<(Level, String, Option<String>)>::new()));
        let s = seen.clone();
        let cfg = Builder::new()
            .memory("m", rank::USER_FILE, &[("server.prot", "1")])
            .define_int("server.port", 8080, 1, 65535, 0)
            .unknown(Unknown::Warn)
            .log(Level::Info, move |e| {
                s.lock()
                    .unwrap()
                    .push((e.level, e.event.to_string(), e.key.map(str::to_string)))
            })
            .create()
            .unwrap();
        let derived = cfg.derive("admin", &[("another.typo", "1")]).unwrap();
        drop(cfg); // the derived one keeps the closure alive
        drop(derived);
        let got = seen.lock().unwrap().clone();
        assert!(got.contains(&(
            Level::Warn,
            "unknown-key".into(),
            Some("server.prot".into())
        )));
        assert!(got.contains(&(
            Level::Warn,
            "unknown-key".into(),
            Some("another.typo".into())
        )));

        let (which, build) = levels();
        let events = Arc::new(Mutex::new(Vec::<String>::new()));
        let ev = events.clone();
        let mut live = Live::open_logged(Mode::Manual, build, Level::Debug, move |e| {
            ev.lock().unwrap().push(e.event.to_string())
        })
        .unwrap();
        which.store(1, Ordering::SeqCst);
        live.reload().unwrap();
        let events = events.lock().unwrap().clone();
        assert!(events.contains(&"reload".to_string()), "{:?}", events);
        assert!(
            events.contains(&"restart-pending".to_string()),
            "{:?}",
            events
        );
    }
}
