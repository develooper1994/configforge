# ConfigForge for Rust

Configuration whose every value can explain which source won, which
lost, and where it was set. The crate wraps the same C library as the
other interfaces and builds it with CMake and a C compiler. There are
no Cargo dependencies; a normal build does not download anything.

From a checkout, `cargo test --offline --manifest-path
bindings/rust/Cargo.toml` uses the repository's C sources. To create a
standalone crate, run from the repository root:

```sh
cmake -DWORK=/tmp/configforge-crate -P scripts/release/package_rust.cmake
```

`WORK` must not exist. The script stages the C sources, licence and
examples inside the crate, then runs `cargo package --offline` with
verification enabled and tests the unpacked package. The `.crate` is
in `WORK/target/package/`. Nothing is published.

Do not run `cargo publish` directly from `bindings/rust`: that directory
deliberately does not maintain another copy of the C sources. For a
registry publication, use the prepared manifest in
`WORK/package/Cargo.toml`, only after the project's release checks.

Scalar lists use the same definition and indexed reads:

```rust
use configforge::{Builder, Def, ListMode, rank};

let cfg = Builder::new()
    .define(Def::text("tags").list(ListMode::Append, b',')
            .default_text_list(&["base"]).aliases(&["t"]))
    .env("env", rank::ENV, "APP_", &[("APP_TAGS", "one,two")])
    .cli("cli", rank::CLI, &["-t", "three"])
    .create().unwrap();
assert_eq!(cfg.int("tags.count", 0), 4);
assert_eq!(cfg.text("tags.3").as_deref(), Some("three"));
```

`ListMode::Replace` uses the winning count. A zero separator disables
environment splitting; CLI occurrences are separate elements.
`default_int_list` and `default_bool_list` provide typed defaults too;
an empty slice is an empty default. Range, allowed values and policy
remain element constraints. A runtime without the `lists` capability
is refused, not treated as a scalar-only implementation.

See the repository's `docs/Guide.md`, section 13, for typed settings,
editing, reloads and the examples shared with the C interface.
