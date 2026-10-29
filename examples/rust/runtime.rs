//! runtime.rs - changing a setting while the program runs, in Rust.
//!
//! runtime.c through the Rust binding. An operator turns the log level
//! up from an admin page:
//!
//!   1. a request in flight keeps the snapshot it started with - a Config
//!      is immutable, and Send + Sync, so a request can hold one (an Arc)
//!      while the next is made;
//!   2. the change meets the same contract a file would;
//!   3. explain() says who changed it, and what it was before.

use configforge::{rank, Builder};
use std::fs;
use std::sync::Arc;

fn main() {
    fs::write(
        "service.ini",
        "[log]\nlevel = info\n[server]\nworkers = 4\n",
    )
    .expect("write the file");
    let live = Builder::new()
        .file("service.ini", rank::SYS_FILE, "service.ini", 0)
        .define_text("log.level", Some("warn"), 0)
        .define_int("server.workers", 1, 1, 64, 0)
        .create()
        .unwrap_or_else(|e| panic!("runtime: {}", e));
    let live = Arc::new(live);
    println!(
        "started: log level {}, {} workers",
        live.text("log.level").unwrap_or_default(),
        live.int("server.workers", 0)
    );

    // A request takes the configuration as it is now.
    let in_flight = Arc::clone(&live);

    // The operator's change, as a source of its own named "admin".
    let next = live
        .derive("admin", &[("log.level", "debug")])
        .unwrap_or_else(|e| panic!("runtime: {}", e));
    println!(
        "\nthe request in flight still sees: {}",
        in_flight.text("log.level").unwrap_or_default()
    );
    println!(
        "a new request sees:               {}",
        next.text("log.level").unwrap_or_default()
    );

    print!("\nchanged:\n{}", next.changes_since(&live));
    print!("\n{}", next.explain("log.level"));

    // A mistake typed into the admin page is refused, and the running
    // configuration stays as it was.
    match next.derive("admin", &[("server.workers", "0")]) {
        Err(e) => println!("\n0 workers: {}", e),
        Ok(_) => println!("\n0 workers: accepted"),
    }
    println!(
        "still running with {} workers",
        next.int("server.workers", 0)
    );
    let _ = fs::remove_file("service.ini");
}
