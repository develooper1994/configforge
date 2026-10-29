//! references.rs - one value made from others, ${key} (D-10), in Rust.
//!
//! references.c through the Rust binding: a URL written once from the
//! host and port, a flag that moves it, explain() saying what it was made
//! from, and a cycle refused by name.

use configforge::{rank, Builder, Config, Error};
use std::fs;

const FILE: &str = "app.ini";

fn load(args: &[&str]) -> Result<Config, Error> {
    Builder::new()
        .file("app.ini", rank::USER_FILE, FILE, 0)
        .cli("cli", rank::CLI, args)
        .define_text("server.host", Some("localhost"), 0)
        .define_int("server.port", 8080, 1, 65535, 0)
        .define_text("server.url", None, 0)
        .create()
}

fn main() {
    fs::write(
        FILE,
        "[server]\nhost = web1\nport = 9443\nurl = https://${server.host}:${server.port}/\n",
    )
    .expect("write the file");
    let cfg = load(&[]).unwrap_or_else(|e| panic!("references: {}", e));
    println!("url: {}", cfg.text("server.url").unwrap_or_default());
    print!("{}", cfg.explain("server.url"));

    let flagged = load(&["--server.host=web2"]).unwrap_or_else(|e| panic!("references: {}", e));
    println!(
        "\nwith --server.host=web2, url: {}",
        flagged.text("server.url").unwrap_or_default()
    );

    fs::write(
        FILE,
        "[server]\nhost = ${server.url}\nurl = https://${server.host}/\n",
    )
    .expect("write the file");
    match load(&[]) {
        Err(e) => println!("\na cycle: {}", e),
        Ok(_) => println!("\na cycle: accepted"),
    }
    let _ = fs::remove_file(FILE);
}
