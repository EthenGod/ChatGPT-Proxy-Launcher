#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod bootstrap;
mod bridge;
#[cfg(test)]
#[path = "../../../tests/rust/embedded_assets.rs"]
mod embedded_assets;
#[cfg(feature = "test-harness")]
mod harness;
mod jobs;
mod model;

use model::{
    Action, BridgeError, Detection, Options, PreferenceResult, ProgramList, SaveResult, Selection,
    Snapshot, Validation,
};
use serde::de::DeserializeOwned;
use std::sync::Arc;
use tauri::{Emitter, Manager};
use windows_sys::Win32::Foundation::{ERROR_ACCESS_DENIED, ERROR_INVALID_HANDLE};

type NativeState = Arc<jobs::NativeJobs>;

async fn action<T: DeserializeOwned + Send + 'static>(
    state: &NativeState,
    command: Action,
    request: impl serde::Serialize + Send + 'static,
    owner: usize,
) -> Result<T, BridgeError> {
    // The visual prototype can never write real or isolated deployment data.
    if cfg!(feature = "ui-prototype") {
        return Err(BridgeError::native(ERROR_ACCESS_DENIED));
    }
    state
        .run(move |paths| {
            #[cfg(feature = "test-harness")]
            harness::record_action(command);
            bridge::action(command, &request, paths, owner).1
        })
        .await
}
#[tauri::command]
async fn read_setup_state(state: tauri::State<'_, NativeState>) -> Result<Snapshot, BridgeError> {
    if cfg!(feature = "program-sample") {
        return Err(BridgeError::native(ERROR_ACCESS_DENIED));
    }
    state
        .run(|paths| {
            let snapshot = bridge::read_state(paths)?;
            #[cfg(feature = "test-harness")]
            harness::record_read(&snapshot);
            Ok(snapshot)
        })
        .await
}
#[tauri::command]
async fn validate_setup(
    state: tauri::State<'_, NativeState>,
    options: Options,
) -> Result<Validation, BridgeError> {
    action(&state, Action::Validate, options, 0).await
}
#[tauri::command]
async fn detect_setup_port(
    state: tauri::State<'_, NativeState>,
    protocol: String,
    host: String,
) -> Result<Detection, BridgeError> {
    action(
        &state,
        Action::Detect,
        serde_json::json!({"protocol":protocol,"host":host}),
        0,
    )
    .await
}
#[tauri::command]
async fn save_setup(
    state: tauri::State<'_, NativeState>,
    options: Options,
) -> Result<SaveResult, BridgeError> {
    if cfg!(feature = "ui-prototype") {
        return Err(BridgeError::native(ERROR_ACCESS_DENIED));
    }
    state
        .run(move |paths| {
            #[cfg(feature = "test-harness")]
            harness::record_action(Action::Save);
            Ok(bridge::save(options, paths))
        })
        .await
}
fn owner(window: &tauri::WebviewWindow) -> Result<usize, BridgeError> {
    if window.label() != "main" {
        return Err(BridgeError::native(ERROR_ACCESS_DENIED));
    }
    window
        .hwnd()
        .map(|hwnd| hwnd.0 as usize)
        .map_err(|_| BridgeError::native(ERROR_INVALID_HANDLE))
}
#[tauri::command]
async fn choose_setup_directory(
    state: tauri::State<'_, NativeState>,
    window: tauri::WebviewWindow,
    initial: String,
) -> Result<Selection, BridgeError> {
    action(
        &state,
        Action::Directory,
        serde_json::json!({"initial":initial}),
        owner(&window)?,
    )
    .await
}
#[tauri::command]
async fn add_setup_program(
    state: tauri::State<'_, NativeState>,
    window: tauri::WebviewWindow,
) -> Result<Selection, BridgeError> {
    action(
        &state,
        Action::AddProgram,
        serde_json::json!({}),
        owner(&window)?,
    )
    .await
}
#[tauri::command]
async fn list_setup_programs(
    state: tauri::State<'_, NativeState>,
) -> Result<ProgramList, BridgeError> {
    action(&state, Action::ListPrograms, serde_json::json!({}), 0).await
}
#[tauri::command]
async fn open_setup_location(
    state: tauri::State<'_, NativeState>,
    window: tauri::WebviewWindow,
    location: String,
) -> Result<(), BridgeError> {
    let _: serde_json::Value = action(
        &state,
        Action::OpenLocation,
        serde_json::json!({"location":location}),
        owner(&window)?,
    )
    .await?;
    Ok(())
}
#[tauri::command]
async fn set_setup_preferences(
    state: tauri::State<'_, NativeState>,
    dark: bool,
    language: String,
) -> Result<PreferenceResult, BridgeError> {
    action(
        &state,
        Action::Preferences,
        serde_json::json!({"dark":dark,"language":language}),
        0,
    )
    .await
}
#[tauri::command]
fn setup_window_ready(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, NativeState>,
) -> Result<(), String> {
    if window.label() != "main" {
        return Err("unexpected-window".into());
    }
    state.set_close_listener_ready(true);
    #[cfg(feature = "test-harness")]
    if !harness::root().join("ui-ready.json").exists() {
        harness::mark("ui-ready");
    }
    Ok(())
}
#[tauri::command]
async fn close_setup_window(
    window: tauri::WebviewWindow,
    state: tauri::State<'_, NativeState>,
) -> Result<(), String> {
    if window.label() != "main" {
        return Err("unexpected-window".into());
    }
    if !state.try_close() {
        return Err("busy".into());
    }
    #[cfg(feature = "test-harness")]
    harness::record_close(true);
    if let Err(error) = window.close() {
        state.reopen();
        return Err(error.to_string());
    }
    Ok(())
}

fn main() {
    let tail = bootstrap::current_command_tail();
    if cfg!(feature = "program-sample") {
        if !tail.is_empty() {
            std::process::exit(2);
        }
    } else {
        bridge::initialize();
    }
    #[cfg(not(feature = "test-harness"))]
    let paths: Option<Vec<u8>> = None;
    #[cfg(feature = "test-harness")]
    let paths = Some(harness::paths());
    let compatibility = bootstrap::is_compatibility(&tail);
    // Same binary and CLI contract. Do this before constructing ANY Tauri state,
    // checking WebView2, creating a window, or acquiring a GUI instance lock.
    if !tail.is_empty() && !compatibility {
        std::process::exit(bridge::headless(&tail, paths.as_deref()));
    }
    #[cfg(not(feature = "test-harness"))]
    let window_class = String::from("ChatGPTLauncherSetup");
    #[cfg(feature = "test-harness")]
    let window_class = harness::window_class();
    let _gui_instance = match bridge::acquire_gui(&window_class) {
        Ok(Some(handle)) => handle,
        Ok(None) => return,
        Err(code) => {
            bridge::bootstrap_error(compatibility);
            std::process::exit(code as i32);
        }
    };
    if compatibility {
        run_compatibility(paths.as_deref(), &window_class);
    }
    if tauri::webview_version().is_err() {
        if cfg!(feature = "program-sample") {
            bridge::initialize();
            bridge::bootstrap_error(false);
            std::process::exit(1);
        }
        run_compatibility(paths.as_deref(), &window_class);
    }
    #[cfg(feature = "test-harness")]
    harness::mark("gui-entered");
    // Native operations are serialized. Two async workers can serve IPC without scaling with CPU count.
    let Ok(runtime) = tokio::runtime::Builder::new_multi_thread()
        .worker_threads(2)
        .enable_all()
        .build()
    else {
        bridge::bootstrap_error(false);
        std::process::exit(1);
    };
    tauri::async_runtime::set(runtime.handle().clone());
    let context = tauri::generate_context!();
    let result = tauri::Builder::default()
        .manage(Arc::new(jobs::NativeJobs::new(paths)))
        .setup(move |app| {
            let config = &app.config().app.windows[0];
            #[cfg(feature = "ui-prototype")]
            let config = &{
                let mut config = config.clone();
                let page = if cfg!(feature = "program-sample") {
                    "prototype.html?scenario=programs"
                } else {
                    "prototype.html"
                };
                config.url = tauri::WebviewUrl::App(page.into());
                config.height = 760.0;
                config
            };
            let result =
                tauri::WebviewWindowBuilder::from_config(app, config).and_then(|builder| {
                    // Config dataDirectory is relative to AppData. The builder
                    // is required for an absolute, fully isolated test profile.
                    #[cfg(feature = "test-harness")]
                    let builder = builder.data_directory(harness::webview_profile()?);
                    builder
                        .window_classname(window_class)
                        .on_navigation(bootstrap::allows_navigation)
                        .on_page_load(|window, payload| {
                            if matches!(payload.event(), tauri::webview::PageLoadEvent::Started) {
                                window
                                    .state::<NativeState>()
                                    .set_close_listener_ready(false);
                            }
                        })
                        .build()
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
            setup_window_ready,
            close_setup_window,
            validate_setup,
            detect_setup_port,
            save_setup,
            choose_setup_directory,
            add_setup_program,
            list_setup_programs,
            open_setup_location,
            set_setup_preferences
        ])
        .on_window_event(|window, event| {
            if let tauri::WindowEvent::CloseRequested { api, .. } = event {
                let state = window.state::<NativeState>();
                if !state.allow_native_close() {
                    api.prevent_close();
                    #[cfg(feature = "test-harness")]
                    harness::record_close(false);
                    let request_window = window.clone();
                    tauri::async_runtime::spawn(async move {
                        let _ = request_window.emit("setup-close-requested", ());
                    });
                }
            }
        })
        .run(context);
    if result.is_err() {
        bridge::bootstrap_error(false);
        std::process::exit(1);
    }
}

fn run_compatibility(paths: Option<&[u8]>, window_class: &str) -> ! {
    #[cfg(feature = "test-harness")]
    harness::mark("compatibility-entered");
    let code = bridge::compatibility(paths, window_class);
    if code != 0 {
        bridge::bootstrap_error(true);
    }
    std::process::exit(code);
}
