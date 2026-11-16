#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod bootstrap;
mod bridge;
#[cfg(test)]
#[path = "../../../tests/rust/embedded_assets.rs"]
mod embedded_assets;
#[cfg(feature = "test-harness")]
mod harness;
mod model;

use model::{BridgeError, Snapshot};
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};
use tauri::Manager;

struct NativeState {
    busy: Arc<AtomicBool>,
    paths: Option<Vec<u8>>,
}
struct BusyGuard(Arc<AtomicBool>);
impl Drop for BusyGuard {
    fn drop(&mut self) {
        self.0.store(false, Ordering::Release);
    }
}

#[tauri::command]
async fn read_setup_state(state: tauri::State<'_, NativeState>) -> Result<Snapshot, BridgeError> {
    if state.busy.swap(true, Ordering::Acquire) {
        return Err(BridgeError::native(170));
    }
    let guard = BusyGuard(state.busy.clone());
    let paths = state.paths.clone();
    tauri::async_runtime::spawn_blocking(move || {
        let _guard = guard;
        let snapshot = bridge::read_state(paths.as_deref())?;
        #[cfg(feature = "test-harness")]
        harness::record_read(&snapshot);
        Ok(snapshot)
    })
    .await
    .map_err(|_| BridgeError::native(13))?
}

#[tauri::command]
fn show_setup_window(window: tauri::WebviewWindow) -> Result<(), String> {
    if window.label() != "main" {
        return Err("unexpected-window".into());
    }
    window.show().map_err(|e| e.to_string())?;
    #[cfg(feature = "test-harness")]
    if !harness::root().join("ui-ready.json").exists() {
        harness::mark("ui-ready");
    }
    Ok(())
}

#[tauri::command]
fn close_setup_window(window: tauri::WebviewWindow) -> Result<(), String> {
    window.close().map_err(|e| e.to_string())
}

fn main() {
    bridge::initialize();
    #[cfg(not(feature = "test-harness"))]
    let paths: Option<Vec<u8>> = None;
    #[cfg(feature = "test-harness")]
    let paths = Some(harness::paths());
    let tail = bootstrap::current_command_tail();
    if bootstrap::is_compatibility(&tail) {
        run_compatibility(paths.as_deref());
    }
    // Same binary and CLI contract. Do this before constructing ANY Tauri state,
    // checking WebView2, creating a window, or acquiring a GUI instance lock.
    if !tail.is_empty() {
        std::process::exit(bridge::headless(&tail, paths.as_deref()));
    }

    if tauri::webview_version().is_err() {
        run_compatibility(paths.as_deref());
    }
    #[cfg(feature = "test-harness")]
    harness::mark("gui-entered");
    let context = tauri::generate_context!();
    let result = tauri::Builder::default()
        .manage(NativeState {
            busy: Arc::new(AtomicBool::new(false)),
            paths,
        })
        .setup(|app| {
            let result =
                tauri::WebviewWindowBuilder::from_config(app, &app.config().app.windows[0])
                    .and_then(|builder| {
                        // Config dataDirectory is relative to AppData. The builder
                        // is required for an absolute, fully isolated test profile.
                        #[cfg(feature = "test-harness")]
                        let builder = builder.data_directory(harness::root().join("webview"));
                        builder.build()
                    });
            if result.is_err() {
                bridge::bootstrap_error(false);
                // A failed first window may never run queued exit events.
                // No bridge worker has started at this point.
                std::process::exit(1);
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            read_setup_state,
            show_setup_window,
            close_setup_window
        ])
        .on_window_event(|window, event| {
            if let tauri::WindowEvent::CloseRequested { api, .. } = event {
                // Even read-only native work must finish before the process exits.
                if window.state::<NativeState>().busy.load(Ordering::Acquire) {
                    api.prevent_close();
                }
            }
        })
        .run(context);
    if result.is_err() {
        bridge::bootstrap_error(false);
        std::process::exit(1);
    }
}

fn run_compatibility(paths: Option<&[u8]>) -> ! {
    #[cfg(feature = "test-harness")]
    harness::mark("compatibility-entered");
    let code = bridge::compatibility(paths);
    if code != 0 {
        bridge::bootstrap_error(true);
    }
    std::process::exit(code);
}
