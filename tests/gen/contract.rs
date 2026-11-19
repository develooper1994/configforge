// The contract of contract.schema.json, in Rust: written by `configforge
// gen --schema --lang rust`. Regenerate it from the schema rather
// than editing it.

use configforge::{Builder, Key};

/// The schema, the contract's one source: [`contract`] gives it to a
/// builder, which reads it as it would read the file.
pub const SCHEMA: &str = "{
  \"$schema\": \"https://json-schema.org/draft/2020-12/schema\",
  \"title\": \"contract\",
  \"type\": \"object\",
  \"additionalProperties\": false,
  \"properties\": {
    \"server\": {
      \"type\": \"object\",
      \"additionalProperties\": false,
      \"properties\": {
        \"port\": {
          \"type\": \"integer\",
          \"minimum\": 1,
          \"maximum\": 65535,
          \"default\": 8080,
          \"description\": \"Where the server listens: a \\\"port\\\", 1 to 65535?\"
        },
        \"host\": {\"type\": \"string\", \"default\": \"0.0.0.0\", \"minLength\": 1, \"maxLength\": 253},
        \"mode\": {\"type\": \"string\", \"enum\": [\"dev\", \"prod\"], \"default\": \"dev\"},
        \"tls\": {\"type\": \"boolean\", \"default\": false},
        \"token\": {\"type\": \"string\", \"writeOnly\": true},
        \"big\": {\"type\": \"integer\", \"minimum\": -9007199254740993, \"maximum\": 9223372036854775807}
      },
      \"required\": [\"token\"]
    },
    \"ratio\": {\"type\": \"number\", \"minimum\": 0, \"maximum\": 1, \"default\": 0.25},
    \"tags\": {\"type\": \"array\", \"items\": {\"type\": \"string\"}, \"default\": [\"a\", \"bé\"]},
    \"old\": {\"type\": \"integer\", \"deprecated\": true},
    \"plugins\": {\"type\": \"object\", \"additionalProperties\": true}
  }
}
";

/// Every definition the schema makes, on `b`.
pub fn contract(b: Builder) -> Builder {
    b.schema(SCHEMA, "contract.schema.json")
}

pub const SERVER_PORT: Key<i64> = Key::new("server.port", 8080);
pub const SERVER_HOST: Key<String> = Key::new("server.host", "0.0.0.0");
pub const SERVER_MODE: Key<String> = Key::new("server.mode", "dev");
pub const SERVER_TLS: Key<bool> = Key::new("server.tls", false);
pub const SERVER_TOKEN: Key<String> = Key::new("server.token", "");
pub const SERVER_BIG: Key<i64> = Key::new("server.big", 0);
// ratio: a real, which the Rust binding does not read yet
// tags: a list; its elements are tags.0, tags.1, ... and tags.count
pub const OLD: Key<i64> = Key::new("old", 0);

configforge::settings! {
    pub struct Settings {
        pub server_port: i64 = "server.port" => 8080,
        pub server_host: String = "server.host" => "0.0.0.0",
        pub server_mode: String = "server.mode" => "dev",
        pub server_tls: bool = "server.tls" => false,
        pub server_token: String = "server.token" => "",
        pub server_big: i64 = "server.big" => 0,
        pub old: i64 = "old" => 0,
    }
}
