#[allow(dead_code)]
#[path = "../../frontend/src-tauri/src/bootstrap.rs"]
mod bootstrap;
#[allow(dead_code)]
#[path = "../../frontend/src-tauri/src/model.rs"]
mod model;

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
fn response_failure_preserves_the_independent_native_commit() {
    for (code, expected) in [
        (0, "not-started"),
        (1, "saved"),
        (2, "rolled-back"),
        (3, "rollback-incomplete"),
    ] {
        let response = model::SaveResult::unavailable(code, String::new());
        let json = serde_json::to_value(response).unwrap();
        assert_eq!(json["commit"], expected);
        if code == 1 {
            assert_eq!(json["warnings"][0], "ui.save.response");
        }
    }
}
