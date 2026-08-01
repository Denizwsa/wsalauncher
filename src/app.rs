use crate::config::{
    self, instance_dir, save_config, BUNDLE_MOD_SLUGS, LauncherConfig, MC_VERSION,
};
use crate::install::run_full_install;
use crate::launch::{is_installed, launch_game};
use egui::{Color32, CornerRadius, RichText, Stroke, Vec2};
use std::path::PathBuf;
use std::sync::mpsc::{self, Receiver, Sender};
use std::thread;

#[derive(Clone, Copy, PartialEq, Eq)]
enum Page {
    Home,
    Mods,
    Settings,
}

enum WorkerMsg {
    Log(String),
    InstallDone(Result<(), String>),
    LaunchDone(Result<(), String>),
}

pub struct LauncherApp {
    cfg: LauncherConfig,
    java_path_edit: String,
    wsamiaw_jar_edit: String,
    page: Page,
    logs: Vec<String>,
    busy: bool,
    progress: f32,
    status_line: String,
    tx: Sender<WorkerMsg>,
    rx: Receiver<WorkerMsg>,
}

impl LauncherApp {
    pub fn new() -> Self {
        let cfg = config::load_config();
        let (tx, rx) = mpsc::channel();
        Self {
            java_path_edit: cfg.java_path.to_string_lossy().into_owned(),
            wsamiaw_jar_edit: cfg.wsamiaw_jar.to_string_lossy().into_owned(),
            cfg,
            page: Page::Home,
            logs: vec!["Wsalauncher ready — LiquidLauncher-style UI.".into()],
            busy: false,
            progress: 0.0,
            status_line: if is_installed() {
                "Ready to play".into()
            } else {
                "Install required".into()
            },
            tx,
            rx,
        }
    }

    fn sync_path_edits_to_cfg(&mut self) {
        self.cfg.java_path = PathBuf::from(self.java_path_edit.trim());
        self.cfg.wsamiaw_jar = PathBuf::from(self.wsamiaw_jar_edit.trim());
    }

    fn push_log(&mut self, line: String) {
        self.logs.push(line);
        if self.logs.len() > 400 {
            self.logs.drain(0..100);
        }
    }

    fn start_install(&mut self) {
        if self.busy {
            return;
        }
        self.sync_path_edits_to_cfg();
        self.busy = true;
        self.progress = 0.05;
        self.status_line = "Installing…".into();
        save_config(&self.cfg);
        let tx = self.tx.clone();
        let java = self.cfg.java_path.clone();
        let wsamiaw = self.cfg.wsamiaw_jar.clone();
        thread::spawn(move || {
            let mut logs = |s: &str| {
                let _ = tx.send(WorkerMsg::Log(s.to_string()));
            };
            let result = run_full_install(&java, &wsamiaw, &mut logs);
            let _ = tx.send(WorkerMsg::InstallDone(result));
        });
    }

    fn start_play(&mut self) {
        if self.busy {
            return;
        }
        if !is_installed() {
            self.push_log("Run Install first.".into());
            return;
        }
        self.sync_path_edits_to_cfg();
        save_config(&self.cfg);
        self.busy = true;
        self.status_line = "Launching…".into();
        let tx = self.tx.clone();
        let java = self.cfg.java_path.clone();
        let user = self.cfg.username.clone();
        let ram = self.cfg.ram_mb;
        thread::spawn(move || {
            let mut logs = |s: &str| {
                let _ = tx.send(WorkerMsg::Log(s.to_string()));
            };
            let result = launch_game(&java, &user, ram, &mut logs).map(|_| ()).map_err(|e| e);
            let _ = tx.send(WorkerMsg::LaunchDone(result));
        });
    }

    fn poll_worker(&mut self) {
        while let Ok(msg) = self.rx.try_recv() {
            match msg {
                WorkerMsg::Log(s) => {
                    self.progress = (self.progress + 0.02).min(0.95);
                    self.push_log(s);
                }
                WorkerMsg::InstallDone(r) => {
                    self.busy = false;
                    self.progress = if r.is_ok() { 1.0 } else { 0.0 };
                    self.status_line = match &r {
                        Ok(()) => "Install complete".into(),
                        Err(e) => format!("Install failed: {e}"),
                    };
                    if let Err(e) = r {
                        self.push_log(e);
                    }
                }
                WorkerMsg::LaunchDone(r) => {
                    self.busy = false;
                    self.status_line = match &r {
                        Ok(()) => "Game started".into(),
                        Err(e) => format!("Launch failed: {e}"),
                    };
                    if let Err(e) = r {
                        self.push_log(e);
                    }
                }
            }
        }
    }

    fn sidebar_button(&mut self, ui: &mut egui::Ui, page: Page, label: &str) {
        let selected = self.page == page;
        let fill = if selected {
            Color32::from_rgb(55, 90, 160)
        } else {
            Color32::TRANSPARENT
        };
        if ui
            .add(
                egui::Button::new(RichText::new(label).size(15.0).color(Color32::WHITE))
                    .fill(fill)
                    .min_size(Vec2::new(ui.available_width(), 36.0)),
            )
            .clicked()
        {
            self.page = page;
        }
    }

    fn draw_home(&mut self, ui: &mut egui::Ui) {
        ui.vertical_centered(|ui| {
            ui.add_space(24.0);
            ui.label(
                RichText::new("Wsamiaw")
                    .size(42.0)
                    .strong()
                    .color(Color32::from_rgb(120, 170, 255)),
            );
            ui.label(
                RichText::new(format!("Minecraft {MC_VERSION} · Fabric"))
                    .size(16.0)
                    .color(Color32::from_gray(180)),
            );
            ui.add_space(8.0);
            ui.label(
                RichText::new(&self.status_line)
                    .size(14.0)
                    .color(Color32::from_rgb(140, 220, 160)),
            );
            ui.add_space(20.0);
            if self.busy {
                ui.add(egui::ProgressBar::new(self.progress).show_percentage());
                ui.add_space(12.0);
            }
            let play_label = if is_installed() { "PLAY" } else { "INSTALL & PLAY" };
            let play = ui.add(
                egui::Button::new(RichText::new(play_label).size(22.0).strong())
                    .fill(Color32::from_rgb(45, 120, 220))
                    .min_size(Vec2::new(220.0, 52.0)),
            );
            if play.clicked() && !self.busy {
                if is_installed() {
                    self.start_play();
                } else {
                    self.start_install();
                }
            }
            ui.add_space(10.0);
            ui.horizontal(|ui| {
                if ui
                    .add_enabled(!self.busy, egui::Button::new("Install / update"))
                    .clicked()
                {
                    self.start_install();
                }
                if ui
                    .add_enabled(!self.busy && is_installed(), egui::Button::new("Play only"))
                    .clicked()
                {
                    self.start_play();
                }
            });
        });
    }

    fn draw_mods(&mut self, ui: &mut egui::Ui) {
        ui.heading("Bundled mods (auto on install)");
        ui.add_space(8.0);
        ui.label(RichText::new("Wsamiaw + ViaFabricPlus + FPS mods").color(Color32::GRAY));
        egui::ScrollArea::vertical().show(ui, |ui| {
            ui.label(RichText::new("• Wsamiaw (local JAR)").strong());
            for slug in BUNDLE_MOD_SLUGS {
                ui.label(format!("• {slug}"));
            }
        });
        ui.add_space(12.0);
        if ui.button("Open instance mods folder").clicked() {
            let path = instance_dir().join("mods");
            let _ = open::that(path);
        }
    }

    fn draw_settings(&mut self, ui: &mut egui::Ui) {
        ui.heading("Settings");
        ui.add_space(8.0);
        egui::Grid::new("settings_grid")
            .num_columns(2)
            .spacing([12.0, 8.0])
            .show(ui, |ui| {
                ui.label("Username");
                ui.text_edit_singleline(&mut self.cfg.username);
                ui.end_row();

                ui.label("RAM (MB)");
                ui.add(egui::Slider::new(&mut self.cfg.ram_mb, 1024..=16384).step_by(256.0));
                ui.end_row();

                ui.label("Java");
                ui.text_edit_singleline(&mut self.java_path_edit);
                ui.end_row();

                ui.label("Wsamiaw JAR");
                ui.text_edit_singleline(&mut self.wsamiaw_jar_edit);
                ui.end_row();
            });
        ui.add_space(8.0);
        if ui.button("Save settings").clicked() {
            self.sync_path_edits_to_cfg();
            save_config(&self.cfg);
            self.push_log("Settings saved.".into());
        }
        ui.label(
            RichText::new("Java 25+ required for Minecraft 26.2")
                .small()
                .color(Color32::YELLOW),
        );
    }

    fn draw_console(&mut self, ui: &mut egui::Ui) {
        ui.label(RichText::new("Console").strong());
        egui::Frame::new()
            .fill(Color32::from_rgb(12, 14, 20))
            .stroke(Stroke::new(1.0, Color32::from_gray(40)))
            .corner_radius(CornerRadius::same(6))
            .show(ui, |ui| {
                egui::ScrollArea::vertical()
                    .max_height(140.0)
                    .stick_to_bottom(true)
                    .show(ui, |ui| {
                        for line in &self.logs {
                            ui.label(
                                RichText::new(line)
                                    .monospace()
                                    .size(11.0)
                                    .color(Color32::from_gray(200)),
                            );
                        }
                    });
            });
    }
}

impl eframe::App for LauncherApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.poll_worker();
        if self.busy {
            ctx.request_repaint_after(std::time::Duration::from_millis(100));
        }

        egui::SidePanel::left("nav")
            .resizable(false)
            .exact_width(160.0)
            .frame(
                egui::Frame::NONE
                    .fill(Color32::from_rgb(24, 26, 36))
                    .inner_margin(12.0),
            )
            .show(ctx, |ui| {
                ui.label(
                    RichText::new("WSA")
                        .size(20.0)
                        .strong()
                        .color(Color32::from_rgb(100, 150, 255)),
                );
                ui.add_space(16.0);
                self.sidebar_button(ui, Page::Home, "Home");
                self.sidebar_button(ui, Page::Mods, "Mods");
                self.sidebar_button(ui, Page::Settings, "Settings");
            });

        egui::CentralPanel::default()
            .frame(egui::Frame::NONE.fill(Color32::from_rgb(18, 20, 28)))
            .show(ctx, |ui| {
                ui.add_space(8.0);
                match self.page {
                    Page::Home => self.draw_home(ui),
                    Page::Mods => self.draw_mods(ui),
                    Page::Settings => self.draw_settings(ui),
                }
                ui.add_space(12.0);
                self.draw_console(ui);
            });
    }
}

pub fn setup_visuals(ctx: &egui::Context) {
    let mut v = egui::Visuals::dark();
    v.panel_fill = Color32::from_rgb(18, 20, 28);
    v.window_fill = Color32::from_rgb(22, 24, 34);
    v.widgets.noninteractive.bg_fill = Color32::from_rgb(30, 32, 44);
    v.widgets.inactive.bg_fill = Color32::from_rgb(36, 40, 54);
    v.widgets.hovered.bg_fill = Color32::from_rgb(48, 72, 120);
    v.widgets.active.bg_fill = Color32::from_rgb(55, 90, 160);
    v.selection.bg_fill = Color32::from_rgb(60, 100, 180);
    ctx.set_visuals(v);
}
