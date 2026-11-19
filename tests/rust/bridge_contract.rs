#[allow(dead_code)]
#[path = "../../frontend/src-tauri/src/bootstrap.rs"]
mod bootstrap;
#[path = "../../frontend/src-tauri/src/jobs.rs"]
mod jobs;
#[allow(dead_code)]
#[path = "../../frontend/src-tauri/src/model.rs"]
mod model;

#[test]
fn only_one_close_can_be_admitted_and_failure_reopens_the_queue() {
    use std::sync::{Arc, Barrier};

    let jobs = Arc::new(jobs::NativeJobs::new(None));
    let start = Arc::new(Barrier::new(2));
    let attempts = [0, 1].map(|_| {
        let (jobs, start) = (Arc::clone(&jobs), Arc::clone(&start));
        std::thread::spawn(move || {
            start.wait();
            jobs.try_close()
        })
    });
    let [first, second] = attempts.map(|thread| thread.join().unwrap());
    assert_ne!(first, second);
    assert!(jobs.allow_native_close());
    let runtime = tokio::runtime::Builder::new_current_thread()
        .build()
        .unwrap();
    runtime.block_on(async {
        let error = jobs.run(|_| Ok(())).await.unwrap_err();
        assert_eq!(
            error.native_error,
            windows_sys::Win32::Foundation::ERROR_BUSY
        );
        jobs.reopen();
        jobs.run(|_| Ok(())).await.unwrap();
        assert!(jobs.try_close());
    });
}

#[test]
fn native_close_is_available_until_the_page_installs_its_listener() {
    let unread = jobs::NativeJobs::new(None);
    assert!(unread.allow_native_close());
    let ready = jobs::NativeJobs::new(None);
    ready.set_close_listener_ready(true);
    assert!(!ready.allow_native_close());
    ready.set_close_listener_ready(false);
    assert!(ready.allow_native_close());
    let ready = jobs::NativeJobs::new(None);
    ready.set_close_listener_ready(true);
    assert!(ready.try_close());
    assert!(ready.allow_native_close());
}

#[test]
fn cancelled_waiters_leave_but_running_work_still_blocks_close() {
    use std::future::Future;
    use std::sync::{mpsc, Arc};
    use std::task::{Context, Waker};
    use std::time::Duration;

    let runtime = tokio::runtime::Builder::new_current_thread()
        .build()
        .unwrap();
    runtime.block_on(async {
        let jobs = Arc::new(jobs::NativeJobs::new(Some(vec![42])));
        let (started, running) = tokio::sync::oneshot::channel();
        let (release, wait) = mpsc::channel();
        let active = Arc::clone(&jobs);
        let caller = tokio::spawn(async move {
            active
                .run(move |paths| {
                    assert_eq!(paths, Some([42u8].as_slice()));
                    started.send(()).unwrap();
                    wait.recv_timeout(Duration::from_secs(10)).unwrap();
                    Ok(())
                })
                .await
        });
        running.await.unwrap();
        caller.abort();
        assert!(caller.await.unwrap_err().is_cancelled());
        assert!(!jobs.try_close());

        let (unexpected, observer) = mpsc::channel();
        let mut cancelled = Box::pin(jobs.run(move |_| {
            unexpected.send(()).unwrap();
            Ok(())
        }));
        assert!(cancelled
            .as_mut()
            .poll(&mut Context::from_waker(Waker::noop()))
            .is_pending());
        drop(cancelled);
        assert_eq!(observer.try_recv(), Err(mpsc::TryRecvError::Disconnected));

        let mut next = Box::pin(jobs.run(|_| Ok(42)));
        assert!(next
            .as_mut()
            .poll(&mut Context::from_waker(Waker::noop()))
            .is_pending());
        release.send(()).unwrap();
        assert_eq!(next.await.unwrap(), 42);
        assert!(jobs.try_close());
    });
}

#[test]
fn failed_worker_releases_both_guards_and_keeps_commit_unknown() {
    let jobs = std::sync::Arc::new(jobs::NativeJobs::new(None));
    let runtime = tokio::runtime::Builder::new_current_thread()
        .build()
        .unwrap();
    runtime.block_on(async {
        let error = jobs
            .run::<(), _>(|_| panic!("test worker"))
            .await
            .unwrap_err();
        assert_eq!(error.commit, "unknown");
        jobs.run(|_| Ok(())).await.unwrap();
        assert!(jobs.try_close());
    });
}

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
fn load_errors_preserve_the_native_conflict_directory() {
    let mut error =
        serde_json::json!({"nativeError":183, "message":"conflict", "commit":"not-started"});
    assert!(serde_json::from_value::<model::BridgeError>(error.clone())
        .unwrap()
        .conflict_directory
        .is_none());
    error["conflictDirectory"] = "C:\\Example".into();
    let decoded: model::BridgeError = serde_json::from_value(error.clone()).unwrap();
    assert_eq!(serde_json::to_value(decoded).unwrap(), error);
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
