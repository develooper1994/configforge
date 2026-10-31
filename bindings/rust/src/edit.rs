//! Create, update and delete as one change (cf_edit.h).
//!
//! ```no_run
//! # let cfg = configforge::Builder::new().create().unwrap();
//! let mut e = configforge::Edit::begin(&cfg, "store").unwrap();
//! e.set("pool.size", 32).set("log.level", "debug").unset("feature.beta");
//! e.commit(Some("admin, 2026-10-30: incident 42")).unwrap();
//! ```
//!
//! An edit not committed is dropped, and nothing is written.

use crate::{c, new_error, sys, Config, Error};

/// A value an edit sets: the contract types text as it types a file's.
#[derive(Clone, Debug, PartialEq)]
#[non_exhaustive]
pub enum EditValue {
    Bool(bool),
    Int(i64),
    Real(f64),
    Text(String),
}

macro_rules! from_int {
    ($($t:ty),*) => {$(
        impl From<$t> for EditValue {
            fn from(v: $t) -> EditValue {
                EditValue::Int(v as i64)
            }
        }
    )*};
}
from_int!(i8, i16, i32, i64, u8, u16, u32);

impl From<bool> for EditValue {
    fn from(v: bool) -> EditValue {
        EditValue::Bool(v)
    }
}
impl From<f64> for EditValue {
    fn from(v: f64) -> EditValue {
        EditValue::Real(v)
    }
}
impl From<&str> for EditValue {
    fn from(v: &str) -> EditValue {
        EditValue::Text(v.to_string())
    }
}
impl From<String> for EditValue {
    fn from(v: String) -> EditValue {
        EditValue::Text(v)
    }
}

/// A change to one writable source, gathered before anything is written.
pub struct Edit {
    e: *mut sys::cf_edit,
}

impl Edit {
    /// An edit of `target`, a source of `cfg` by name. Whether it can and
    /// may be written is asked at commit, with the rest.
    pub fn begin(cfg: &Config, target: &str) -> Result<Edit, Error> {
        let t = c(target);
        let mut e = new_error();
        let raw = unsafe { sys::cf_edit_begin(cfg.c, t.as_ptr(), &mut e) };
        if raw.is_null() {
            return Err(Error::from_raw(&e));
        }
        Ok(Edit { e: raw })
    }

    /// Set `key`. A key set twice keeps the second.
    pub fn set<V: Into<EditValue>>(&mut self, key: &str, value: V) -> &mut Edit {
        let k = c(key);
        unsafe {
            match value.into() {
                EditValue::Bool(b) => sys::cf_edit_set_bool(self.e, k.as_ptr(), b as sys::cf_bool),
                EditValue::Int(i) => sys::cf_edit_set_int(self.e, k.as_ptr(), i),
                EditValue::Real(r) => sys::cf_edit_set_real(self.e, k.as_ptr(), r),
                EditValue::Text(t) => {
                    let v = c(&t);
                    sys::cf_edit_set_text(self.e, k.as_ptr(), v.as_ptr())
                }
            };
        }
        self
    }

    /// Delete `key` from the target, so the next source down supplies it.
    pub fn unset(&mut self, key: &str) -> &mut Edit {
        let k = c(key);
        unsafe { sys::cf_edit_unset(self.e, k.as_ptr()) };
        self
    }

    /// How many keys the edit changes.
    pub fn len(&self) -> usize {
        unsafe { sys::cf_edit_count(self.e) }
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Check and write the whole change, `note` beside each value; or
    /// nothing, and the reason.
    pub fn commit(mut self, note: Option<&str>) -> Result<(), Error> {
        let raw = std::mem::replace(&mut self.e, std::ptr::null_mut());
        let n = note.map(c);
        let mut e = new_error();
        let ok = unsafe {
            sys::cf_edit_commit(
                raw,
                n.as_ref().map_or(std::ptr::null(), |n| n.as_ptr()),
                &mut e,
            )
        };
        if ok == 0 {
            return Err(Error::from_raw(&e));
        }
        Ok(())
    }

    /// The handle, given up to a call that consumes it.
    pub(crate) fn into_raw(mut self) -> *mut sys::cf_edit {
        std::mem::replace(&mut self.e, std::ptr::null_mut())
    }
}

impl Drop for Edit {
    fn drop(&mut self) {
        if !self.e.is_null() {
            unsafe { sys::cf_edit_abort(self.e) };
        }
    }
}

#[cfg(test)]
mod tests {
    use super::Edit;
    use crate::{rank, Builder};

    #[test]
    fn refused_at_commit_and_dropped_cleanly() {
        let cfg = Builder::new()
            .memory("mem", rank::USER_FILE, &[("a.b", "1")])
            .create()
            .unwrap();
        let mut e = Edit::begin(&cfg, "mem").unwrap();
        e.set("a.b", 2)
            .set("c.d", "x")
            .set("e.f", true)
            .unset("g.h");
        assert_eq!(e.len(), 4);
        let err = e.commit(Some("note")).unwrap_err();
        assert!(err.message.contains("cannot be written"), "{}", err.message);
        let mut dropped = Edit::begin(&cfg, "mem").unwrap();
        dropped.set("a.b", 3);
        drop(dropped); // nothing written, nothing leaked
    }
}
