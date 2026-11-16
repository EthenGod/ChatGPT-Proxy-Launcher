use serde::{Deserialize, Serialize};

#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Options {
    pub startup: bool,
    pub silent: bool,
    pub desktop: bool,
    pub proxy: bool,
    pub codex_env: bool,
    pub protocol: String,
    pub host: String,
    pub port: String,
    pub bypass: String,
    pub extra: String,
    pub directory: String,
}

#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Preferences {
    pub dark: bool,
    pub language: String,
    pub resolved_language: String,
}

#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Snapshot {
    pub schema_version: u32,
    pub setup_version: String,
    pub deployed_version: String,
    pub deployed: bool,
    pub has_settings: bool,
    pub valid_arguments: bool,
    pub notice: String,
    pub options: Options,
    pub preferences: Preferences,
    pub payload_bytes: u32,
}

#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct BridgeError {
    pub native_error: u32,
    pub message: String,
    pub commit: String,
}
impl BridgeError {
    pub fn native(code: u32) -> Self {
        // Empty text lets the frontend use the same translated fallback catalog.
        Self {
            native_error: code,
            message: String::new(),
            commit: "not-started".into(),
        }
    }
}
