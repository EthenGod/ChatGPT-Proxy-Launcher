use serde::{Deserialize, Serialize};

// Numeric IDs are part of the static C ABI in src/SetupBridge.h.
#[derive(Clone, Copy, PartialEq, Eq)]
#[repr(u32)]
pub enum Action {
    Validate = 1,
    Detect = 2,
    Save = 3,
    Directory = 4,
    AddProgram = 5,
    OpenLocation = 6,
    Preferences = 7,
    ListPrograms = 8,
}

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
}

#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct BridgeError {
    pub native_error: u32,
    pub message: String,
    pub commit: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub conflict_directory: Option<String>,
}
impl BridgeError {
    pub fn native(code: u32) -> Self {
        // Empty text lets the frontend use the same translated fallback catalog.
        Self {
            native_error: code,
            message: String::new(),
            commit: "not-started".into(),
            conflict_directory: None,
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
#[serde(rename_all = "kebab-case")]
pub enum ProgramStatus {
    Ordinary,
    Adapted,
    AdaptedDisabled,
    DependencyBlocked,
    Disabled,
    Invalid,
    Unavailable,
}
#[derive(Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct ListedProgram {
    pub id: String,
    pub name: String,
    pub path: String,
    pub arguments: String,
    pub status: ProgramStatus,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub icon: Option<usize>,
}
#[derive(Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct ProgramList {
    pub entries: Vec<ListedProgram>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub icons: Vec<String>,
    pub incomplete: bool,
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
    pub fn unavailable(code: u32, error: BridgeError) -> Self {
        let (commit, message) = match code {
            0 => (Commit::NotStarted, "ui.save.failed"),
            1 => (Commit::Saved, "ui.save.response"),
            2 => (Commit::RolledBack, "ui.save.rollback"),
            3 => (Commit::RollbackIncomplete, "ui.save.incomplete"),
            _ => (Commit::RollbackIncomplete, "ui.save.unknown"),
        };
        Self {
            commit,
            message: message.into(),
            detail: error.message,
            native_error: error.native_error,
            tasks: vec![],
            warnings: if code == 1 {
                vec!["ui.save.response".into()]
            } else {
                vec![]
            },
            conflict_directory: error.conflict_directory.unwrap_or_default(),
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
