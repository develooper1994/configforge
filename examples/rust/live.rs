//! live.rs - live.c in Rust: a gateway that reloads itself under the
//! mode its operators chose (configforge::Live).
//!
//! The workers share a `LiveReader`; the loop that drives the reloads
//! owns the `Live`.
//!
//!     cargo run --example live

use configforge::{flag, rank, Builder, Error, Live, Mode, Reload};
use std::fs;
use std::time::Duration;

const FILE: &str = "gateway.ini";

configforge::settings! {
    struct Gateway {
        // The port cannot change under a bound socket: it waits for a restart.
        port: u16 = "server.port" in 1..=65535 with flag::RESTART => 8080,
        level: String = "log.level" => "info",
        per_minute: u32 = "limit.per_minute" in 1..=100000 => 600,
    }
}

fn build() -> Builder {
    Gateway::define(Builder::new().file(FILE, rank::USER_FILE, FILE, 0))
}

fn serve(live: &Live) {
    let g = Gateway::read(&live.get()); // one request, one snapshot
    println!(
        "serving on :{}, log {}, {} requests a minute",
        g.port, g.level, g.per_minute
    );
}

fn main() -> Result<(), Error> {
    fs::write(FILE, "[server]\nport = 8080\n").expect("write");

    // 1. Auto
    {
        let mut live = Live::open(Mode::Auto, build)?;
        serve(&live);
        fs::write(FILE, "[server]\nport = 9443\n[log]\nlevel = debug\n").expect("write");
        let r = live.wait(Duration::from_secs(2))?;
        println!(
            "1. reload: {}",
            if r == Reload::Swapped {
                "swapped"
            } else {
                "not swapped"
            }
        );
        serve(&live);
        println!("waiting for a restart: {}", live.restarts().join(", "));
        print!("{}", live.get().explain("server.port"));
    }

    // 2. Approve
    fs::write(FILE, "[server]\nport = 8080\n").expect("write");
    let mut live = Live::open(Mode::Approve, build)?;
    fs::write(FILE, "[server]\nport = 8080\n[limit]\nper_minute = 60\n").expect("write");
    if live.wait(Duration::from_secs(2))? == Reload::Pending {
        let pending = live.pending().expect("a pending snapshot");
        print!(
            "2. waiting for approval:\n{}",
            pending.changes_since(&live.get())
        );
        serve(&live);
        println!("operator: approved");
        live.approve()?;
    }
    serve(&live);

    // 3. A broken edit
    fs::write(FILE, "[server]\nport = 8080\n[limit]\nper_minute = lots\n").expect("write");
    match live.wait(Duration::from_secs(2)) {
        Err(e) => println!("3. reload refused: {}", e),
        Ok(r) => println!("3. reload accepted? {:?}", r),
    }
    serve(&live);

    let _ = fs::remove_file(FILE);
    Ok(())
}
