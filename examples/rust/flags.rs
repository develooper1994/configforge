//! flags.rs - feature flags, rolled out by host and killed in an
//! incident, in Rust.
//!
//! flags.c through the Rust binding, reading the same files: every
//! feature shipped off, a canary drop-in turning two on, an environment
//! variable killing one - and the flags discovered by iterating an open
//! prefix, not listed in the program.

use configforge::{rank, Builder, Unknown, OPTIONAL};

fn main() {
    let cfg = Builder::new()
        .file("features.ini", rank::SYS_FILE, "features.ini", 0)
        .dir("conf.d", rank::USER_FILE, "conf.d", OPTIONAL)
        .env(
            "env",
            rank::ENV,
            "SHOP_",
            &[("SHOP_FEATURES__SEARCH_V2", "false")],
        )
        .open("features") // discovered, not declared
        .unknown(Unknown::Reject)
        .create()
        .unwrap_or_else(|e| panic!("flags: {}", e));
    for name in cfg.children("features") {
        let on = cfg.boolean(&format!("features.{}", name), false);
        println!("{:<14} {}", name, if on { "on" } else { "off" });
    }
    println!();
    print!("{}", cfg.explain("features.search_v2"));
}
