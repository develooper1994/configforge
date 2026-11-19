//! A module written by `configforge gen --lang rust` (D-95), compiled and
//! run: the contract it gives a builder, the `Key` constants and the struct.
//! The golden file is checked against the program's output by `gen.rust`.

use configforge::{rank, Builder};

#[allow(dead_code)]
mod generated {
    include!("../../../tests/gen/contract.rs");
}

fn built(pairs: &[(&str, &str)]) -> Result<configforge::Config, configforge::Error> {
    generated::contract(Builder::new())
        .memory("m", rank::USER_FILE, pairs)
        .create()
}

#[test]
fn the_contract_is_the_schemas() {
    assert!(built(&[("server.token", "t")]).is_ok());
}

#[test]
fn settings_read_their_defaults() {
    let cfg = built(&[("server.token", "t")]).unwrap();
    let s = generated::Settings::read(&cfg);
    assert_eq!(s.server_port, 8080);
    assert_eq!(s.server_host, "0.0.0.0");
}

#[test]
fn a_key_reads_what_is_set() {
    let cfg = built(&[("server.token", "t"), ("server.port", "9443")]).unwrap();
    assert_eq!(cfg.read(&generated::SERVER_PORT), 9443);
}

#[test]
fn a_value_outside_the_schema_is_refused() {
    assert!(built(&[("server.token", "t"), ("server.port", "70000")]).is_err());
}
