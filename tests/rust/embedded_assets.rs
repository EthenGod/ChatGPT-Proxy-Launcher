#[test]
fn release_page_and_referenced_assets_are_embedded() {
    let context: tauri::Context<tauri::Wry> = tauri::generate_context!();
    assert!(!matches!(
        context.config().build.frontend_dist,
        Some(tauri::utils::config::FrontendDist::Url(_))
    ));
    let page = context
        .assets()
        .get(&"index.html".into())
        .expect("embedded index.html");
    let html = std::str::from_utf8(&page).unwrap();
    assert!(html.contains("id=\"root\""));
    for marker in ["src=\"/", "href=\"/"] {
        for reference in html.split(marker).skip(1) {
            let path = reference.split('"').next().unwrap();
            assert!(
                context.assets().get(&path.into()).is_some(),
                "missing embedded asset: {path}"
            );
        }
    }
}
