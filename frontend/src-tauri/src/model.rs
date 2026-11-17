use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Deserialize, Serialize)]
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
    pub default_directory: String,
    pub programs: Vec<ProgramEntry>,
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

#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct ProgramEntry {
    pub name: String,
    pub path: String,
    pub arguments: String,
}
#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Validation {
    pub valid: bool,
    pub arguments: String,
    pub errors: std::collections::BTreeMap<String, String>,
    pub detail: String,
    pub native_error: u32,
}
#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum Commit {
    NotStarted,
    Saved,
    RolledBack,
    RollbackIncomplete,
}
#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct SaveResult {
    pub commit: Commit,
    pub message: String,
    pub detail: String,
    pub native_error: u32,
    pub tasks: Vec<String>,
    pub warnings: Vec<String>,
    pub conflict_directory: String,
    pub options: Option<Options>,
}
impl SaveResult {
    // The independent ABI outcome survives JSON encoding/decoding/allocation failures.
    pub fn unavailable(commit: u32, detail: String, native_error: u32) -> Self {
        Self {
            commit: match commit {
                1 => Commit::Saved,
                2 => Commit::RolledBack,
                3 => Commit::RollbackIncomplete,
                _ => Commit::NotStarted,
            },
            message: match commit {
                1 => "ui.save.response",
                2 => "ui.save.rollback",
                3 => "ui.save.incomplete",
                _ => "ui.save.failed",
            }
            .into(),
            detail,
            native_error,
            tasks: vec![],
            warnings: if commit == 1 {
                vec!["ui.save.response".into()]
            } else {
                vec![]
            },
            conflict_directory: String::new(),
            options: None,
        }
    }
}
#[derive(Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct Detection {
    pub port: String,
    pub message: String,
    pub source: String,
}
#[derive(Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct Selection {
    pub path: String,
    pub entry: Option<ProgramEntry>,
}
#[derive(Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct PreferenceResult {
    pub preferences: Preferences,
    pub warnings: Vec<String>,
}
