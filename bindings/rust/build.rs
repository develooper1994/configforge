//! Builds libconfigforge from this checkout with CMake - the one tool
//! every build of the project needs - and links it statically. Nothing is
//! downloaded. Release, so that on Windows it uses the same C runtime as
//! Rust does (/MD, not /MDd).

use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

fn run(cmd: &mut Command) {
    let status = cmd
        .status()
        .unwrap_or_else(|e| panic!("cannot run {:?}: {}", cmd, e));
    if !status.success() {
        panic!("{:?} failed: {}", cmd, status);
    }
}

/// A path as CMake and Windows can both spell it: forward slashes, no
/// verbatim prefix, and on Windows no case.
fn same_path(p: &Path) -> String {
    let s = p.to_string_lossy().replace('\\', "/");
    let s = s.strip_prefix("//?/").unwrap_or(&s).to_string();
    if cfg!(windows) {
        s.to_lowercase()
    } else {
        s
    }
}

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let root = manifest.join("..").join("..");
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    let build = out.join("cf-build");
    let prefix = out.join("cf-install");

    // A cache made from another checkout - the same crate moved, or a
    // second copy sharing a CARGO_TARGET_DIR - names its own source, and
    // CMake refuses to reuse it. Start that build over.
    let cache = build.join("CMakeCache.txt");
    if let Ok(text) = std::fs::read_to_string(cache) {
        let ours = same_path(&root.canonicalize().unwrap_or_else(|_| root.clone()));
        let same = text
            .lines()
            .find_map(|l| l.strip_prefix("CMAKE_HOME_DIRECTORY:INTERNAL="))
            .map(|theirs| same_path(Path::new(theirs)) == ours);
        if same != Some(true) {
            let _ = std::fs::remove_dir_all(&build);
        }
    }

    run(Command::new("cmake")
        .arg("-S")
        .arg(&root)
        .arg("-B")
        .arg(&build)
        .arg("-DCMAKE_BUILD_TYPE=Release")
        .arg(format!("-DCMAKE_INSTALL_PREFIX={}", prefix.display()))
        .arg("-DCMAKE_INSTALL_LIBDIR=lib")
        .arg("-DCMAKE_POSITION_INDEPENDENT_CODE=ON")
        .arg("-DCF_BUILD_TESTING=OFF")
        .arg("-DCF_BUILD_EXAMPLES=OFF")
        .arg("-DCF_BUILD_TOOLS=OFF")
        .arg("-DCF_WITH_CXX=OFF"));
    run(Command::new("cmake")
        .arg("--build")
        .arg(&build)
        .arg("--config")
        .arg("Release")
        .arg("--target")
        .arg("configforge"));
    run(Command::new("cmake")
        .arg("--install")
        .arg(&build)
        .arg("--config")
        .arg("Release"));

    println!(
        "cargo:rustc-link-search=native={}",
        prefix.join("lib").display()
    );
    println!("cargo:rustc-link-lib=static=configforge");
    if env::var("CARGO_CFG_WINDOWS").is_ok() {
        // The Registry and the wide command line.
        println!("cargo:rustc-link-lib=advapi32");
        println!("cargo:rustc-link-lib=shell32");
    }
    for dir in ["src", "include", "cmake"] {
        println!("cargo:rerun-if-changed={}", root.join(dir).display());
    }
    println!(
        "cargo:rerun-if-changed={}",
        root.join("CMakeLists.txt").display()
    );
}
