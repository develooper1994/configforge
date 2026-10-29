//! layers.rs - where a value comes from, with every kind of source at
//! once, in Rust.
//!
//! layers.c through the Rust binding, reading the same files: shipped
//! defaults, conf.d, a .env, the environment and the command line, each
//! above the one before, and then why each value won. The files are in
//! examples/c/layers/; the suite runs this there.

use configforge::{flag, rank, Builder, Unknown, OPTIONAL};

fn main() {
    let cfg = Builder::new()
        .file("app.toml", 35, "app.toml", 0)
        .dir("conf.d", 30, "conf.d", OPTIONAL)
        .dotenv(".env", 15, ".env", "LAYERS_", OPTIONAL)
        .env(
            "env",
            rank::ENV,
            "LAYERS_",
            &[("LAYERS_LOG__LEVEL", "warn")],
        )
        .cli("cli", rank::CLI, &["--server.port=9443"])
        .define_text("server.host", Some("127.0.0.1"), 0)
        .define_int("server.port", 8080, 1, 65535, 0)
        .define_int("server.workers", 2, 1, 64, 0)
        .define_text("db.url", Some("sqlite:dev.db"), flag::SENSITIVE)
        .define_text("log.level", Some("info"), 0)
        .define_int("cache.size", 64, 1, 4096, 0)
        .unknown(Unknown::Reject)
        .create();
    let cfg = match cfg {
        Ok(cfg) => cfg,
        Err(e) => {
            eprintln!("layers: {}", e);
            std::process::exit(1);
        }
    };
    // db.url is sensitive: explain() says where it was set, never what.
    for key in [
        "server.host",
        "server.port",
        "server.workers",
        "db.url",
        "log.level",
        "cache.size",
    ] {
        println!("{}", cfg.explain(key));
    }
}
