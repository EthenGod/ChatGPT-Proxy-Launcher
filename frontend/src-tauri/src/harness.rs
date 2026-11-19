// Compiled only into isolated test executables, never the normal configuration EXE.
use crate::model::Action;
use std::fs::{self, File, OpenOptions};
use std::io::{self, Write};
use std::os::windows::fs::{MetadataExt, OpenOptionsExt};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::OnceLock;
use windows_sys::Win32::Storage::FileSystem::{
    FILE_ATTRIBUTE_REPARSE_POINT, FILE_FLAG_BACKUP_SEMANTICS, FILE_FLAG_OPEN_REPARSE_POINT,
    FILE_READ_ATTRIBUTES, FILE_SHARE_READ, FILE_SHARE_WRITE,
};

fn standalone_sample() -> bool {
    cfg!(feature = "program-sample") && std::env::var_os("CHATGPT_SETUP_TEST_ROOT").is_none()
}

fn hold_directory(path: &Path) -> io::Result<File> {
    // Disallow replacing or renaming the directory as long as this handle is alive.
    let handle = OpenOptions::new()
        .access_mode(FILE_READ_ATTRIBUTES)
        .share_mode(FILE_SHARE_READ | FILE_SHARE_WRITE)
        .custom_flags(FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT)
        .open(path)?;
    let metadata = handle.metadata()?;
    if !metadata.is_dir() || metadata.file_attributes() & FILE_ATTRIBUTE_REPARSE_POINT != 0 {
        return Err(io::Error::other("test directory is redirected"));
    }
    Ok(handle)
}

pub fn root() -> &'static Path {
    static ROOT: OnceLock<(PathBuf, Option<File>)> = OnceLock::new();
    &ROOT.get_or_init(prepare_root).0
}

fn prepare_root() -> (PathBuf, Option<File>) {
    if standalone_sample() {
        // A stable identity preserves the sample's single-instance window.
        return (
            std::env::temp_dir().join("ChatGPTLauncherSetup-CompanionSample"),
            None,
        );
    }
    let path = PathBuf::from(
        std::env::var_os("CHATGPT_SETUP_TEST_ROOT")
            .expect("test harness requires an explicit root"),
    );
    let handle = hold_directory(&path).expect("plain test root required");
    let path = path.canonicalize().expect("test root must already exist");
    // The native contract deliberately accepts ordinary drive paths only.
    let text = path.to_str().expect("test root must be valid Unicode");
    (
        PathBuf::from(text.strip_prefix(r"\\?\").unwrap_or(text)),
        Some(handle),
    )
}

fn unused_path(parent: &Path, prefix: &str) -> PathBuf {
    static NEXT: AtomicUsize = AtomicUsize::new(0);
    let sequence = NEXT.fetch_add(1, Ordering::Relaxed);
    parent.join(format!("{prefix}-{}-{sequence}", std::process::id()))
}

pub fn webview_profile() -> io::Result<PathBuf> {
    static PROFILE: OnceLock<File> = OnceLock::new();
    let (parent, prefix) = if standalone_sample() {
        (std::env::temp_dir(), "ChatGPTLauncherSetup-CompanionSample")
    } else {
        (root().to_owned(), "webview")
    };
    loop {
        let path = unused_path(&parent, prefix);
        match fs::create_dir(&path) {
            Err(error) if error.kind() == io::ErrorKind::AlreadyExists => continue,
            result => result?,
        }
        let handle = hold_directory(&path)?;
        PROFILE
            .set(handle)
            .map_err(|_| io::Error::other("profile already assigned"))?;
        return Ok(path);
    }
}

pub fn write_report(directory: &Path, name: &str, bytes: &[u8]) -> io::Result<()> {
    let (temporary, mut file) = loop {
        let path = unused_path(directory, "observation");
        match OpenOptions::new().write(true).create_new(true).open(&path) {
            Ok(file) => break (path, file),
            Err(error) if error.kind() == io::ErrorKind::AlreadyExists => continue,
            Err(error) => return Err(error),
        }
    };
    let result = file.write_all(bytes);
    drop(file);
    let result = result.and_then(|_| {
        let target = directory.join(name);
        // Windows readers may briefly deny delete sharing while reading an old snapshot.
        for _ in 0..20 {
            match fs::rename(&temporary, &target) {
                Err(error) if error.raw_os_error() == Some(32) => {
                    std::thread::sleep(std::time::Duration::from_millis(5));
                }
                result => return result,
            }
        }
        fs::rename(&temporary, target)
    });
    if result.is_err() {
        let _ = fs::remove_file(temporary);
    }
    result
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
    // root() resolves the directory on disk; case-only siblings can be distinct.
    root().hash(&mut hash);
    format!("ChatGPTLauncherSetupPreview{:016x}", hash.finish())
}
pub fn mark(name: &str) {
    if standalone_sample() {
        return;
    }
    let file = root().join(format!("{name}.json"));
    let mut output = std::fs::OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(file)
        .expect("unique test observation");
    output.write_all(b"{}\n").expect("test observation");
}

pub fn record_read(state: &crate::model::Snapshot) {
    static READS: AtomicUsize = AtomicUsize::new(0);
    let report = serde_json::json!({
        "reads": READS.fetch_add(1, Ordering::Relaxed) + 1,
        "schemaVersion": state.schema_version,
        "setupVersion": state.setup_version,
        "payloadBytes": state.payload_bytes
    });
    write_report(
        root(),
        "read-ok.json",
        &serde_json::to_vec(&report).unwrap(),
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
    write_report(
        root(),
        "close-events.json",
        &serde_json::to_vec(&report).unwrap(),
    )
    .expect("test close observation");
}

pub fn record_action(action: Action) {
    use std::{collections::BTreeMap, sync::Mutex};
    static COUNTS: Mutex<BTreeMap<u32, usize>> = Mutex::new(BTreeMap::new());
    let mut counts = COUNTS.lock().expect("test action counts");
    *counts.entry(action as u32).or_default() += 1;
    write_report(
        root(),
        "actions.json",
        &serde_json::to_vec(&*counts).unwrap(),
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
