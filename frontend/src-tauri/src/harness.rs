// Compiled only into isolated test executables, never the normal configuration EXE.
use crate::model::Action;
use std::path::PathBuf;

fn standalone_sample() -> bool {
    cfg!(feature = "program-sample") && std::env::var_os("CHATGPT_SETUP_TEST_ROOT").is_none()
}

pub fn root() -> PathBuf {
    if standalone_sample() {
        // Only the WebView profile uses this directory. Sample settings live in memory.
        return std::env::temp_dir().join("ChatGPTLauncherSetup-CompanionSample");
    }
    let path = PathBuf::from(
        std::env::var_os("CHATGPT_SETUP_TEST_ROOT")
            .expect("test harness requires an explicit root"),
    )
    .canonicalize()
    .expect("test root must already exist");
    // The native contract deliberately accepts ordinary drive paths only.
    let text = path.to_str().expect("test root must be valid Unicode");
    PathBuf::from(text.strip_prefix(r"\\?\").unwrap_or(text))
}
pub fn paths() -> Vec<u8> {
    let root = root();
    serde_json::to_vec(&serde_json::json!({
        "defaultDirectory": root.join("state"),
        "settingsLink": root.join("state").join("settings.lnk"),
        "startupLink": root.join("startup.lnk"),
        "desktopLink": root.join("desktop.lnk"),
        "codexEnvFile": root.join("codex.env"),
        "legacyStartupLink": root.join("legacy-startup.lnk"),
        "legacyDesktopLink": root.join("legacy-desktop.lnk")
    }))
    .expect("serialize isolated paths")
}
pub fn window_class() -> String {
    use std::hash::{DefaultHasher, Hash, Hasher};
    let mut hash = DefaultHasher::new();
    root().to_string_lossy().to_uppercase().hash(&mut hash);
    format!("ChatGPTLauncherSetupPreview{:016x}", hash.finish())
}
pub fn mark(name: &str) {
    if standalone_sample() {
        return;
    }
    use std::io::Write;
    let file = root().join(format!("{name}.json"));
    let mut output = std::fs::OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(file)
        .expect("unique test observation");
    output.write_all(b"{}\n").expect("test observation");
}

pub fn record_read(state: &crate::model::Snapshot) {
    use std::sync::atomic::{AtomicUsize, Ordering};
    static READS: AtomicUsize = AtomicUsize::new(0);
    let report = serde_json::json!({
        "reads": READS.fetch_add(1, Ordering::Relaxed) + 1,
        "schemaVersion": state.schema_version,
        "setupVersion": state.setup_version,
        "payloadBytes": state.payload_bytes
    });
    std::fs::write(
        root().join("read-ok.json"),
        serde_json::to_vec(&report).unwrap(),
    )
    .expect("test read observation");
}

pub fn record_close(approved: bool) {
    if standalone_sample() {
        return;
    }
    use std::sync::Mutex;
    static COUNTS: Mutex<(usize, usize)> = Mutex::new((0, 0));
    let mut counts = COUNTS.lock().expect("test close counts");
    if approved {
        counts.1 += 1;
    } else {
        counts.0 += 1;
    }
    let report = serde_json::json!({"requested": counts.0, "approved": counts.1});
    std::fs::write(
        root().join("close-events.json"),
        serde_json::to_vec(&report).unwrap(),
    )
    .expect("test close observation");
}

pub fn record_action(action: Action) {
    use std::{collections::BTreeMap, sync::Mutex};
    static COUNTS: Mutex<BTreeMap<u32, usize>> = Mutex::new(BTreeMap::new());
    let mut counts = COUNTS.lock().expect("test action counts");
    *counts.entry(action as u32).or_default() += 1;
    std::fs::write(
        root().join("actions.json"),
        serde_json::to_vec(&*counts).unwrap(),
    )
    .expect("test command observation");
    drop(counts);
    if action == Action::Save {
        if let Some(delay) = std::env::var("CHATGPT_SETUP_TEST_SAVE_DELAY_MS")
            .ok()
            .and_then(|text| text.parse::<u64>().ok())
            .filter(|delay| *delay <= 5000)
        {
            std::thread::sleep(std::time::Duration::from_millis(delay));
        }
    }
}
