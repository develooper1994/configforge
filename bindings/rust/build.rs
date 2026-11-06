//! Builds libconfigforge with cargo, as a Rust crate builds its C: the
//! library's sources compiled by the `cc` crate and linked statically. No
//! CMake and no other build system: cargo and a C compiler are all it takes.
//!
//! Every source file guards itself - a reader by its CF_WITH_* option, a
//! platform by the platform - so all of them are compiled, as the
//! amalgamation compiles them in one unit, and the defaults are
//! include/configforge/cf_build_options.h's.

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
    let root = if native.join("src").is_dir() {
        native
    } else {
        manifest.join("..").join("..")
    };
    let include = root.join("include");
    let src = root.join("src");
    assert!(
        include.join("configforge").join("configforge.h").is_file() && src.is_dir(),
        "the C sources are missing; stage the crate with scripts/release/package_rust.cmake"
    );

    let mut files = Vec::new();
    sources(&src, &mut files);
    let mut build = cc::Build::new();
    build
        .include(&include)
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
    println!("cargo:rerun-if-changed={}", src.display());
    println!("cargo:rerun-if-changed={}", include.display());
}
