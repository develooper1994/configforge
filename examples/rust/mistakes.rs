//! mistakes.rs - what the person running a program sees when its
//! configuration is wrong, in Rust.
//!
//! mistakes.c through the Rust binding: one contract, five files with
//! one mistake each, and the Error create() returns. Then the one mistake
//! a contract can be told to forgive, and explain() showing what was
//! forgiven. The suite checks every message: they are the library's.

use configforge::{flag, rank, Builder, Def, Unknown};
use std::fs;

const FILE: &str = "mistakes.ini";

fn attempt(title: &str, text: &str) {
    fs::write(FILE, text).expect("write the file");
    let cfg = Builder::new()
        .file("file", rank::USER_FILE, FILE, 0)
        .define_int("server.port", 8080, 1, 65535, 0)
        .define(
            Def::text("log.level")
                .allowed(&["debug", "info", "warn", "error"])
                .default_text("info"),
        )
        .define_text("db.url", None, flag::REQUIRED) // no default is right
        .unknown(Unknown::Reject) // a typo is not ignored
        .create();
    println!("{}", title);
    match cfg {
        Err(e) => println!("  refused: {}\n", e),
        Ok(cfg) => println!(
            "  started: port {}, log level {}\n",
            cfg.int("server.port", 0),
            cfg.text("log.level").unwrap_or_default()
        ),
    }
}

/// A tuning knob may fall back to its default when it is nonsense - this
/// key only - and explain() still shows the refusal.
fn forgiving() {
    fs::write(FILE, "[cache]\nsize = lots\n").expect("write the file");
    let cfg = Builder::new()
        .file("file", rank::USER_FILE, FILE, 0)
        .define(Def::int("cache.size").default_int(64).fallback())
        .create();
    println!("6. a value the contract may forgive");
    match cfg {
        Err(e) => println!("  refused: {}", e),
        Ok(cfg) => {
            println!("  started: cache.size {}", cfg.int("cache.size", 0));
            print!("{}", cfg.explain("cache.size"));
        }
    }
}

fn main() {
    attempt(
        "1. a configuration with nothing wrong",
        "[server]\nport = 9000\n[db]\nurl = postgres://db/app\n",
    );
    attempt(
        "2. a key spelt wrong",
        "[server]\npot = 9000\n[db]\nurl = postgres://db/app\n",
    );
    attempt(
        "3. a number out of range",
        "[server]\nport = 99999\n[db]\nurl = postgres://db/app\n",
    );
    attempt(
        "4. a word the contract does not know",
        "[log]\nlevel = verbose\n[db]\nurl = postgres://db/app\n",
    );
    attempt("5. a required setting left out", "[server]\nport = 9000\n");
    forgiving();
    let _ = fs::remove_file(FILE);
}
