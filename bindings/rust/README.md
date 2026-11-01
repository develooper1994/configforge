# ConfigForge for Rust

Configuration whose every value can explain which source won, which
lost, and where it was set. The crate wraps the same C library as the
other interfaces and builds it with CMake and a C compiler. There are
no Cargo dependencies; a normal build does not download anything.

From a checkout, `cargo test --offline --manifest-path
bindings/rust/Cargo.toml` uses the repository's C sources. To create a
standalone crate, run from the repository root:

```sh
cmake -DWORK=/tmp/configforge-crate -P scripts/package-rust.cmake
```

`WORK` must not exist. The script stages the C sources, licence and
examples inside the crate, then runs `cargo package --offline` with
verification enabled and tests the unpacked package. The `.crate` is
in `WORK/target/package/`. Nothing is published.

Do not run `cargo publish` directly from `bindings/rust`: that directory
deliberately does not maintain another copy of the C sources. For a
registry publication, use the prepared manifest in
`WORK/package/Cargo.toml`, only after the project's release checks.

See the repository's `docs/Guide.md`, section 13, for typed settings,
editing, reloads and the examples shared with the C interface.
