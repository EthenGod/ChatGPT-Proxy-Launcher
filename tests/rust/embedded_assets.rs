#[test]
fn release_page_and_referenced_assets_are_embedded() {
    let context: tauri::Context<tauri::Wry> = tauri::generate_context!();
    assert!(!matches!(
        context.config().build.frontend_dist,
        Some(tauri::utils::config::FrontendDist::Url(_))
    ));
    for entry in ["index.html", "prototype.html"] {
        let page = context
            .assets()
            .get(&entry.into())
            .unwrap_or_else(|| panic!("missing embedded page: {entry}"));
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
}
