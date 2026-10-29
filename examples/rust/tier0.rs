//! tier0.rs - the smallest complete program, in Rust.
//!
//! tier0.c through the Rust binding: the command line, the environment
//! (MYAPP_), and a value the program reads with its own fallback.
//!
//!     cargo run --example tier0 -- --server.port=7000

fn main() {
    let cfg = match configforge::Builder::app("myapp").create() {
        Ok(cfg) => cfg,
        Err(e) => {
            eprintln!("{}", e);
            std::process::exit(1);
        }
    };
    println!(
        "{}:{} tls={}",
        cfg.text("server.host")
            .unwrap_or_else(|| "0.0.0.0".to_string()),
        cfg.int("server.port", 8080),
        if cfg.boolean("tls.enabled", false) {
            "on"
        } else {
            "off"
        }
    );
}
