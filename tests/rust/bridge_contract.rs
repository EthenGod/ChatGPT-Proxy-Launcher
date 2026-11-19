#[allow(dead_code)]
#[path = "../../frontend/src-tauri/src/bootstrap.rs"]
mod bootstrap;
#[allow(dead_code)]
#[path = "../../frontend/src-tauri/src/model.rs"]
mod model;

#[test]
fn native_action_ids_match_the_cpp_contract() {
    use model::Action;
    let header = include_str!("../../src/SetupBridge.h");
    let (_, declaration) = header.split_once("enum SetupBridgeAction").unwrap();
    let (_, body) = declaration.split_once('{').unwrap();
    let (body, _) = body.split_once('}').unwrap();
    let ids: std::collections::BTreeMap<_, _> = body
        .split(',')
        .map(|field| {
            let (name, value) = field.split_once('=').unwrap();
            (name.trim(), value.trim().parse::<u32>().unwrap())
        })
        .collect();
    let actions = [
        ("SETUP_VALIDATE", Action::Validate),
        ("SETUP_DETECT", Action::Detect),
        ("SETUP_SAVE", Action::Save),
        ("SETUP_DIRECTORY", Action::Directory),
        ("SETUP_ADD_PROGRAM", Action::AddProgram),
        ("SETUP_OPEN_LOCATION", Action::OpenLocation),
        ("SETUP_PREFERENCES", Action::Preferences),
        ("SETUP_LIST_PROGRAMS", Action::ListPrograms),
    ];
    assert_eq!(ids.len(), actions.len());
    for (name, action) in actions {
        assert_eq!(ids[name], action as u32, "{name}");
    }
}

#[test]
fn raw_command_tail_preserves_quotes_empty_arguments_and_unicode() {
    let cases = [
        (
            "\"C:\\a b\\setup.exe\" --headless plan --extra-arg \"\" --extra-arg \"中文 \\\"x\"",
            "--headless plan --extra-arg \"\" --extra-arg \"中文 \\\"x\"",
        ),
        ("C:\\setup.exe\t  --headless info", "--headless info"),
        ("\"C:\\a b\\setup.exe\"   ", ""),
        ("C:\\setup.exe", ""),
        ("C:\\setup.exe \"\"", "\"\""),
    ];
    for (raw, expected) in cases {
        let raw: Vec<_> = raw.encode_utf16().collect();
        assert_eq!(
            bootstrap::command_tail(&raw),
            expected.encode_utf16().collect::<Vec<_>>()
        );
    }
    let raw = [b'a' as u16, 32, 0xd800, 32, 0xdc00];
    assert_eq!(bootstrap::command_tail(&raw), &raw[2..]);
}

#[test]
fn native_snapshot_requires_every_field_and_preserves_options() {
    let text = include_str!("../fixtures/setup-snapshot.json");
    let snapshot: model::Snapshot = serde_json::from_str(text).unwrap();
    assert_eq!(snapshot.schema_version, 1);
    assert!(snapshot.options.silent);
    assert_eq!(
        snapshot.options.extra,
        "\"\" \"中文 参数\" \"quote\\\"end\""
    );
    let mut invalid: serde_json::Value = serde_json::from_str(text).unwrap();
    invalid["options"]
        .as_object_mut()
        .unwrap()
        .remove("codexEnv");
    assert!(serde_json::from_value::<model::Snapshot>(invalid).is_err());
}

#[test]
fn compatibility_accepts_only_a_single_explicit_switch() {
    for (tail, expected) in [
        ("--compatibility", true),
        ("\"--compatibility\" \t", true),
        ("", false),
        ("--compatibility --headless info", false),
        ("--headless info --compatibility", false),
        ("--compatibility 中文", false),
        ("--compatibility \"\"", false),
        ("--compatibilityx", false),
    ] {
        assert_eq!(
            bootstrap::is_compatibility(&tail.encode_utf16().collect::<Vec<_>>()),
            expected
        );
    }
}

#[test]
fn window_navigation_matches_the_build_boundary() {
    let packaged = !cfg!(debug_assertions);
    for (url, allowed) in [
        ("http://tauri.localhost/", packaged),
        ("http://tauri.localhost/index.html#settings", packaged),
        (
            "http://tauri.localhost/prototype.html",
            packaged && cfg!(feature = "ui-prototype"),
        ),
        ("http://tauri.localhost:5174/", false),
        ("http://tauri.localhost.example.com/", false),
        ("http://tauri.localhost@127.0.0.1/", false),
        ("http://user@tauri.localhost/", false),
        ("http://127.0.0.1:5174/", !packaged),
        ("http://127.0.0.1:5175/", false),
        ("http://user@127.0.0.1:5174/", false),
        ("https://example.com/", false),
        ("data:text/html,test", false),
        ("file:///C:/test.html", false),
    ] {
        assert_eq!(
            bootstrap::allows_navigation(&url.parse().unwrap()),
            allowed,
            "{url}"
        );
    }
}

#[test]
fn response_failure_preserves_the_independent_native_commit() {
    for (code, expected) in [
        (0, "not-started"),
        (1, "saved"),
        (2, "rolled-back"),
        (3, "rollback-incomplete"),
        (u32::MAX, "rollback-incomplete"),
    ] {
        let response = model::SaveResult::unavailable(code, String::new(), 8);
        let json = serde_json::to_value(response).unwrap();
        assert_eq!(json["commit"], expected);
        assert_eq!(json["nativeError"], 8);
        if code == 1 {
            assert_eq!(json["warnings"][0], "ui.save.response");
        }
        if code == u32::MAX {
            assert_eq!(json["message"], "ui.save.unknown");
        }
    }
}
