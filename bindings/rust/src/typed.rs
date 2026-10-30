//! Reading a value as the type the program wants, and writing its
//! default once (D-46): [`Config::get`] for any [`Value`] type,
//! [`Key`] for a key declared with its type and default, and
//! [`settings!`](crate::settings) for the program's own struct, whose
//! field list is also the contract.

use crate::{c, flag, sys, Builder, Config};
use std::os::raw::c_char;
use std::ptr;

/// A type a key can be read as: `bool`, the integer types and `String`.
///
/// An integer the type cannot hold reads as nothing, never as a wrapped
/// or truncated number - a value is the configured one or absent.
pub trait Value: Sized {
    #[doc(hidden)]
    fn read(cfg: &Config, key: &str) -> Option<Self>;
}

impl Value for bool {
    fn read(cfg: &Config, key: &str) -> Option<bool> {
        let k = c(key);
        let mut v: sys::cf_bool = 0;
        (unsafe { sys::cf_try_bool(cfg.c, k.as_ptr(), &mut v) } == sys::CF_OK).then_some(v != 0)
    }
}

impl Value for String {
    fn read(cfg: &Config, key: &str) -> Option<String> {
        let k = c(key);
        let mut p: *const c_char = ptr::null();
        let mut n: usize = 0;
        if unsafe { sys::cf_try_text(cfg.c, k.as_ptr(), &mut p, &mut n) } != sys::CF_OK {
            return None;
        }
        let bytes = unsafe { std::slice::from_raw_parts(p as *const u8, n) };
        Some(String::from_utf8_lossy(bytes).into_owned())
    }
}

fn read_i64(cfg: &Config, key: &str) -> Option<i64> {
    let k = c(key);
    let mut v: i64 = 0;
    (unsafe { sys::cf_try_int(cfg.c, k.as_ptr(), &mut v) } == sys::CF_OK).then_some(v)
}

macro_rules! integer_value {
    ($($t:ty),*) => {$(
        impl Value for $t {
            fn read(cfg: &Config, key: &str) -> Option<$t> {
                read_i64(cfg, key).and_then(|v| <$t>::try_from(v).ok())
            }
        }
    )*};
}
integer_value!(i8, i16, i32, i64, isize, u8, u16, u32, u64, usize);

impl Config {
    /// The value of `key` as `T`, or None when it is absent, holds
    /// another type, or is an integer `T` cannot hold.
    ///
    /// ```no_run
    /// # let cfg = configforge::Builder::new().create().unwrap();
    /// let port: Option<u16> = cfg.get("server.port");
    /// let host = cfg.get::<String>("server.host");
    /// ```
    pub fn get<T: Value>(&self, key: &str) -> Option<T> {
        <T as Value>::read(self, key)
    }

    /// A declared key, read as its type, with its default when unset.
    pub fn read<T: Field>(&self, key: &Key<T>) -> T {
        <T as Field>::read(self, key.name, &key.default)
    }
}

/// How a type is defined in the contract and read back: what [`Key`]
/// and [`settings!`](crate::settings) are built on.
pub trait Field: Value {
    /// How its default is written: `&'static str` for `String`, so a
    /// key can be a constant.
    type Default;
    #[doc(hidden)]
    fn define(
        b: Builder,
        key: &str,
        default: &Self::Default,
        range: Option<(i64, i64)>,
        flags: u32,
    ) -> Builder;
    #[doc(hidden)]
    fn read(cfg: &Config, key: &str, default: &Self::Default) -> Self;
}

impl Field for bool {
    type Default = bool;
    fn define(b: Builder, key: &str, d: &bool, _: Option<(i64, i64)>, flags: u32) -> Builder {
        b.define_bool(key, *d, flags)
    }
    fn read(cfg: &Config, key: &str, d: &bool) -> bool {
        cfg.get(key).unwrap_or(*d)
    }
}

impl Field for String {
    type Default = &'static str;
    fn define(
        b: Builder,
        key: &str,
        d: &&'static str,
        _: Option<(i64, i64)>,
        flags: u32,
    ) -> Builder {
        b.define_text(key, Some(d), flags)
    }
    fn read(cfg: &Config, key: &str, d: &&'static str) -> String {
        cfg.get(key).unwrap_or_else(|| d.to_string())
    }
}

macro_rules! integer_field {
    ($($t:ty),*) => {$(
        impl Field for $t {
            type Default = $t;
            fn define(b: Builder, key: &str, d: &$t, range: Option<(i64, i64)>, flags: u32)
                -> Builder {
                let (lo, hi) = range.unwrap_or((0, 0));
                b.define_int(key, *d as i64, lo, hi, flags)
            }
            fn read(cfg: &Config, key: &str, d: &$t) -> $t {
                cfg.get(key).unwrap_or(*d)
            }
        }
    )*};
}
integer_field!(i8, i16, i32, i64, u8, u16, u32);

/// A key declared once, with its type and default, and used both to
/// define the contract and to read it - so the default is written in one
/// place and the reader cannot ask for another type:
///
/// ```no_run
/// use configforge::{Builder, Key};
/// const PORT: Key<u16> = Key::new("server.port", 8080).range(1, 65535);
/// const HOST: Key<String> = Key::new("server.host", "0.0.0.0");
///
/// let cfg = Builder::app("myapp").key(&PORT).key(&HOST).create().unwrap();
/// let port: u16 = cfg.read(&PORT);
/// ```
pub struct Key<T: Field> {
    pub name: &'static str,
    default: T::Default,
    range: Option<(i64, i64)>,
    flags: u32,
}

impl<T: Field> Key<T> {
    pub const fn new(name: &'static str, default: T::Default) -> Key<T> {
        Key {
            name,
            default,
            range: None,
            flags: 0,
        }
    }
    /// The permitted range, for an integer key.
    pub const fn range(mut self, lo: i64, hi: i64) -> Key<T> {
        self.range = Some((lo, hi));
        self
    }
    pub const fn flags(mut self, flags: u32) -> Key<T> {
        self.flags = flags;
        self
    }
    /// Cannot change while the program runs (see [`Live`](crate::Live)).
    pub const fn restart(self) -> Key<T> {
        let f = self.flags | flag::RESTART;
        self.flags(f)
    }
    pub fn default_value(&self) -> &T::Default {
        &self.default
    }
}

impl Builder {
    /// Define a declared [`Key`].
    pub fn key<T: Field>(self, k: &Key<T>) -> Builder {
        T::define(self, k.name, &k.default, k.range, k.flags)
    }
}

/// A struct of the program's own, whose fields are read from a
/// configuration and whose field list is the contract:
///
/// ```no_run
/// configforge::settings! {
///     pub struct Settings {
///         pub port: u16 = "server.port" in 1..=65535 with configforge::flag::RESTART => 8080,
///         pub host: String = "server.host" => "0.0.0.0",
///         pub tls: bool = "tls.enabled" => false,
///     }
/// }
///
/// let cfg = Settings::define(configforge::Builder::app("myapp")).create().unwrap();
/// let settings = Settings::read(&cfg);
/// println!("{}:{}", settings.host, settings.port);
/// ```
///
/// A field's type is one [`Field`] has: bool, String, or an integer up to
/// 32 bits unsigned or 64 signed. `in lo..=hi` gives an integer its range,
/// and `with` its definition flags: `with flag::RESTART | flag::SENSITIVE`.
#[macro_export]
macro_rules! settings {
    (
        $(#[$meta:meta])*
        $vis:vis struct $name:ident {
            $(
                $(#[$fmeta:meta])*
                $fvis:vis $field:ident : $ty:ty = $key:literal
                    $(in $lo:literal ..= $hi:literal)? $(with $flags:expr)? => $default:expr
            ),* $(,)?
        }
    ) => {
        $(#[$meta])*
        $vis struct $name {
            $( $(#[$fmeta])* $fvis $field: $ty, )*
        }

        impl $name {
            /// Every field's definition, added to `b`.
            #[allow(unused_mut)]
            pub fn define(mut b: $crate::Builder) -> $crate::Builder {
                $(
                    let d: <$ty as $crate::Field>::Default = $default;
                    #[allow(unused_assignments, unused_mut)]
                    let mut range: Option<(i64, i64)> = None;
                    $( range = Some(($lo, $hi)); )?
                    #[allow(unused_assignments, unused_mut)]
                    let mut flags: u32 = 0;
                    $( flags = $flags; )?
                    b = <$ty as $crate::Field>::define(b, $key, &d, range, flags);
                )*
                b
            }

            /// Every field, read from `cfg`; an unset one has its default.
            pub fn read(cfg: &$crate::Config) -> $name {
                $name {
                    $( $field: {
                        let d: <$ty as $crate::Field>::Default = $default;
                        <$ty as $crate::Field>::read(cfg, $key, &d)
                    }, )*
                }
            }
        }
    };
}

#[cfg(test)]
mod tests {
    use crate::{rank, Builder, Key};

    const PORT: Key<u16> = Key::new("server.port", 8080).range(1, 65535);
    const HOST: Key<String> = Key::new("server.host", "0.0.0.0");
    const TLS: Key<bool> = Key::new("tls.enabled", false);

    crate::settings! {
        #[allow(dead_code)]
        struct Settings {
            port: u16 = "server.port" in 1..=65535 => 8080,
            host: String = "server.host" => "0.0.0.0",
            tls: bool = "tls.enabled" => false,
            workers: u32 = "server.workers" in 1..=64 => 4,
            secret: String = "db.password" with crate::flag::SENSITIVE => "",
        }
    }

    #[test]
    fn get_is_typed_and_never_truncates() {
        let cfg = Builder::new()
            .memory(
                "m",
                rank::USER_FILE,
                &[("big", "5000000000"), ("neg", "-1"), ("t", "x")],
            )
            .create()
            .unwrap();
        assert_eq!(cfg.get::<i64>("big"), Some(5_000_000_000));
        assert_eq!(cfg.get::<i32>("big"), None, "too big for an i32 is absent");
        assert_eq!(cfg.get::<u32>("neg"), None, "a negative unsigned is absent");
        assert_eq!(cfg.get::<i8>("neg"), Some(-1));
        assert_eq!(cfg.get::<String>("t").as_deref(), Some("x"));
        assert_eq!(cfg.get::<i64>("t"), None, "another type is absent");
        assert_eq!(cfg.get::<bool>("nope"), None);
    }

    #[test]
    fn keys_define_and_read() {
        let cfg = Builder::new()
            .memory("m", rank::USER_FILE, &[("server.port", "9443")])
            .key(&PORT)
            .key(&HOST)
            .key(&TLS)
            .create()
            .unwrap();
        assert_eq!(cfg.read(&PORT), 9443u16);
        assert_eq!(cfg.read(&HOST), "0.0.0.0");
        assert!(!cfg.read(&TLS));
        let refused = Builder::new()
            .memory("m", rank::USER_FILE, &[("server.port", "0")])
            .key(&PORT)
            .create();
        assert!(refused.is_err(), "the key's range is the contract's");
    }

    #[test]
    fn a_nul_is_an_error_not_a_panic() {
        let e = Builder::new()
            .define_text("a.b", Some("x\0y"), 0)
            .create()
            .err()
            .expect("a value holding a NUL is refused");
        assert!(e.message.contains("NUL"), "{}", e.message);
        let cfg = Builder::new()
            .memory("m", rank::USER_FILE, &[("a.b", "1")])
            .create()
            .unwrap();
        assert_eq!(
            cfg.get::<i64>("a.b\0"),
            None,
            "a key holding a NUL is absent"
        );
        assert_eq!(cfg.int("a\0b", 7), 7);
    }

    #[test]
    fn settings_struct() {
        let cfg = Settings::define(Builder::new().memory(
            "m",
            rank::USER_FILE,
            &[("server.port", "9443"), ("tls.enabled", "true")],
        ))
        .create()
        .unwrap();
        let s = Settings::read(&cfg);
        assert_eq!(
            (s.port, s.host.as_str(), s.tls, s.workers),
            (9443, "0.0.0.0", true, 4)
        );
        let refused = Settings::define(Builder::new().memory(
            "m",
            rank::USER_FILE,
            &[("server.workers", "65")],
        ))
        .create();
        assert!(refused.is_err(), "a field's range refuses");
    }
}
