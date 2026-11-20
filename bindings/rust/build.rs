//! Builds libconfigforge with cargo, as a Rust crate builds its C: the
//! library's sources compiled by the `cc` crate and linked statically. No
//! CMake and no other build system: cargo and a C compiler are all it takes.
//!
//! Every source file guards itself - a reader by its CF_WITH_* option, a
//! platform by the platform - so all of them are compiled, as the
//! amalgamation compiles them in one unit, and the defaults are
//! lib/port/include/configforge/cf_build_options.h's.

use std::env;
use std::fs;
use std::path::{Path, PathBuf};

/// Every `.c` file under `dir`, sorted, so a build is the same build.
fn sources(dir: &Path, out: &mut Vec<PathBuf>) {
    let mut entries: Vec<PathBuf> = fs::read_dir(dir)
        .unwrap_or_else(|e| panic!("cannot read {}: {}", dir.display(), e))
        .map(|e| e.expect("a directory entry").path())
        .collect();
    entries.sort();
    for path in entries {
        if path.is_dir() {
            sources(&path, out);
        } else if path.extension().is_some_and(|x| x == "c") {
            out.push(path);
        }
    }
}

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    // A packaged crate carries the sources in native/; a checkout has them
    // two directories up.
    let native = manifest.join("native");
    let root = if native.join("lib").is_dir() {
        native
    } else {
        manifest.join("..").join("..")
    };
    let lib = root.join("lib");
    assert!(
        lib.join("config")
            .join("include")
            .join("configforge")
            .join("configforge.h")
            .is_file(),
        "the C sources are missing; stage the crate with scripts/release/package_rust.cmake"
    );

    // One directory per component (D-100): its headers in include/, its
    // sources in src/.
    let mut components: Vec<PathBuf> = fs::read_dir(&lib)
        .unwrap_or_else(|e| panic!("cannot read {}: {}", lib.display(), e))
        .map(|e| e.expect("a directory entry").path())
        .filter(|p| p.is_dir())
        .collect();
    components.sort();

    let mut files = Vec::new();
    let mut build = cc::Build::new();
    for component in &components {
        let include = component.join("include");
        if include.is_dir() {
            build.include(&include);
        }
        let src = component.join("src");
        if src.is_dir() {
            sources(&src, &mut files);
        }
    }
    build
        .define("CF_INTERNAL_API", Some("1"))
        // lanes is compiled in under ConfigForge's prefix, so that the
        // crate exports no unprefixed `lanes_` symbol (D-101).
        .define("LANES_PREFIX_CF", Some("1"))
        .files(&files)
        // The library's own warnings are its build's business; this one
        // only compiles it.
        .warnings(false);
    if env::var("CARGO_CFG_TARGET_ENV").as_deref() != Ok("msvc") {
        build.flag_if_supported("-std=c99");
    }
    build.compile("configforge");

    if env::var("CARGO_CFG_WINDOWS").is_ok() {
        // The Registry and the wide command line.
        println!("cargo:rustc-link-lib=advapi32");
        println!("cargo:rustc-link-lib=shell32");
    }
    println!("cargo:rerun-if-changed={}", lib.display());
}
