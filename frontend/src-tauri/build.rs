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
        version_header.contains(&format!("#define APP_VERSION_TEXT \"{version}\"")),
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
        "ChatGPTLauncherSetup",
        "ProxyDetection",
        "CodexClose",
    ];
    let mut native = cc::Build::new();
    native
        .cpp(true)
        .std("c++17")
        .static_crt(true)
        .flag("/utf-8")
        .flag("/EHsc")
        .flag("/W4")
        .flag("/WX")
        .define("UNICODE", None)
        .define("_UNICODE", None)
        .define("SETUP_UI_EMBEDDED", None)
        .opt_level(1);
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
    let launcher = root.join("outputs/ChatGPTProxyLauncher.exe");
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
                "show_setup_window",
                "close_setup_window",
            ])),
    )
    .expect("Tauri build configuration failed");
}
