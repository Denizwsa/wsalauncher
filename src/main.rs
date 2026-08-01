mod app;
mod config;
mod download;
mod install;
mod launch;
mod modrinth;

fn main() -> eframe::Result<()> {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([980.0, 620.0])
            .with_min_inner_size([800.0, 520.0])
            .with_title("Wsalauncher"),
        ..Default::default()
    };
    eframe::run_native(
        "Wsalauncher",
        options,
        Box::new(|cc| {
            app::setup_visuals(&cc.egui_ctx);
            Ok(Box::new(app::LauncherApp::new()))
        }),
    )
}
