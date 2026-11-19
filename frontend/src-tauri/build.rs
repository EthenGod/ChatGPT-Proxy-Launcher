use std::{env, fs, path::PathBuf};

fn main() {
    assert_eq!(
        env::var("TARGET").unwrap(),
        "x86_64-pc-windows-msvc",
        "Windows x64 MSVC only"
    );
    // canonicalize() adds a verbatim Windows prefix that cl.exe misparses.
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let root = manifest.parent().unwrap().parent().unwrap();
    let version = env::var("CARGO_PKG_VERSION").unwrap();
    let version_header = fs::read_to_string(root.join("src/AppVersion.h")).unwrap();
    assert!(
        version_header.contains(&format!("#define APP_RELEASE_VERSION \"{version}\"")),
        "Version differs from AppVersion.h"
    );
    let package: serde_json::Value =
        serde_json::from_str(&fs::read_to_string(root.join("frontend/package.json")).unwrap())
            .unwrap();
    let config: serde_json::Value =
        serde_json::from_str(&fs::read_to_string("tauri.conf.json").unwrap()).unwrap();
    assert_eq!(package["version"], version);
    assert_eq!(config["version"], version);
    if !tauri_build::is_dev() {
        let overrides: serde_json::Value =
            serde_json::from_str(&env::var("TAURI_CONFIG").unwrap_or_else(|_| "{}".into()))
                .unwrap();
        let dist = overrides
            .pointer("/build/frontendDist")
            .or_else(|| config.pointer("/build/frontendDist"))
            .and_then(serde_json::Value::as_str)
            .expect("Local frontend directory required");
        assert!(
            !dist.contains(':'),
            "Use a relative frontendDist; drive paths are interpreted as URLs"
        );
        assert!(
            manifest.join(dist).join("index.html").is_file(),
            "Build the frontend before embedding it"
        );
    }

    // Link the core and compatibility UI, excluding the standalone WinMain.
    let sources = [
        "Localization",
        "SetupCore",
        "SetupArguments",
        "CodexEnv",
        "AutomationJson",
        "AutomationIO",
        "DeploymentDiscovery",
        "SetupCommandLine",
        "CompanionManifest",
        "CompanionRegistry",
        "StartupIntegration",
        "SetupBridge",
        "SetupProgramIcons",
        "SetupUiServices",
        "ChatGPTLauncherSetup",
        "ProxyDetection",
        "CodexClose",
    ];
    let mut native = cc::Build::new();
    native
        .cpp(true)
        .static_crt(true)
        .define("SETUP_UI_EMBEDDED", None)
        .opt_level(1);
    for flag in include_str!("../../tools/msvc-common.rsp").lines() {
        native.flag(flag);
    }
    for name in sources {
        native.file(root.join(format!("src/{name}.cpp")));
    }
    native.compile("setup_native");
    for library in [
        "kernel32",
        "user32",
        "shell32",
        "ole32",
        "uuid",
        "shlwapi",
        "windowscodecs",
        "crypt32",
        "advapi32",
        "xmllite",
        "version",
        "gdi32",
        "gdiplus",
        "comctl32",
        "uxtheme",
        "dwmapi",
        "iphlpapi",
        "ws2_32",
        "runtimeobject",
    ] {
        println!("cargo:rustc-link-lib={library}");
    }
    println!("cargo:rerun-if-changed={}", root.join("src").display());
    println!(
        "cargo:rerun-if-changed={}",
        root.join("work/LocalizationData.h").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        root.join("frontend/package.json").display()
    );
    println!("cargo:rerun-if-env-changed=CHATGPT_UI_TEST_LAUNCHER");
    let launcher_dir = if env::var("CHATGPT_UI_TEST_LAUNCHER").as_deref() == Ok("1") {
        "work/ui-test-native"
    } else {
        "outputs"
    };
    let launcher = root.join(launcher_dir).join("ChatGPTProxyLauncher.exe");
    assert!(
        launcher.is_file(),
        "Build the native launcher first with build.cmd"
    );
    println!("cargo:rerun-if-changed={}", launcher.display());
    // Resource 102 identifies dual-UI builds without running an old EXE.
    let windows = tauri_build::WindowsAttributes::new()
        .static_vc_runtime(true)
        .window_icon_path(root.join("assets/ChatGPT.ico"))
        .app_manifest(fs::read_to_string(root.join("src/Setup.manifest")).unwrap())
        .append_rc_content(format!(
            "101 RCDATA \"{}\"\n102 RCDATA \"{}\"",
            launcher.display().to_string().replace('\\', "\\\\"),
            root.join("src/SetupUiCapabilities.json")
                .display()
                .to_string()
                .replace('\\', "\\\\")
        ));
    tauri_build::try_build(
        tauri_build::Attributes::new()
            .windows_attributes(windows)
            .app_manifest(tauri_build::AppManifest::new().commands(&[
                "read_setup_state",
                "setup_window_ready",
                "close_setup_window",
                "validate_setup",
                "detect_setup_port",
                "save_setup",
                "choose_setup_directory",
                "add_setup_program",
                "list_setup_programs",
                "open_setup_location",
                "set_setup_preferences",
            ])),
    )
    .expect("Tauri build configuration failed");
}
