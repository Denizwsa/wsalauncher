#include "MainWindow.h"

#include "Constants.h"
#include "GameLauncher.h"
#include "Hwid.h"
#include "Installer.h"
#include "LauncherUpdate.h"
#include "Paths.h"
#include "Platform.h"
#include "WsamiawApi.h"

#include <QApplication>
#include <QCloseEvent>
#include <QTimer>
#include <QDesktopServices>
#include <QDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRadialGradient>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTextCursor>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QtMath>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {

class AtmosphereWidget : public QWidget {
public:
    explicit AtmosphereWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground, false);
        setAutoFillBackground(false);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QLinearGradient bg(0, 0, width(), height());
        bg.setColorAt(0.0, QColor(6, 8, 16));
        bg.setColorAt(0.45, QColor(10, 14, 28));
        bg.setColorAt(1.0, QColor(4, 6, 12));
        p.fillRect(rect(), bg);

        auto blob = [&](qreal cx, qreal cy, qreal r, const QColor &c) {
            QRadialGradient g(cx, cy, r);
            QColor soft = c;
            soft.setAlpha(70);
            g.setColorAt(0.0, soft);
            soft.setAlpha(28);
            g.setColorAt(0.45, soft);
            soft.setAlpha(0);
            g.setColorAt(1.0, soft);
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawEllipse(QPointF(cx, cy), r, r);
        };

        blob(width() * 0.18, height() * 0.12, qMin(width(), height()) * 0.42, QColor(40, 110, 255));
        blob(width() * 0.82, height() * 0.28, qMin(width(), height()) * 0.38, QColor(70, 160, 255));
        blob(width() * 0.55, height() * 0.78, qMin(width(), height()) * 0.48, QColor(20, 70, 180));

        QRadialGradient vig(width() * 0.5, height() * 0.45, qMax(width(), height()) * 0.75);
        vig.setColorAt(0.55, QColor(0, 0, 0, 0));
        vig.setColorAt(1.0, QColor(0, 0, 0, 120));
        p.fillRect(rect(), vig);
    }
};

class GlassPanel : public QWidget {
public:
    explicit GlassPanel(QWidget *parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setAutoFillBackground(false);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 18, 18);
        p.fillPath(path, QColor(16, 22, 38, 220));
        p.setPen(QPen(QColor(90, 130, 200, 70), 1.2));
        p.drawPath(path);
    }
};

QString cinematicStyle() {
    return QStringLiteral(R"(
        QMainWindow { background: #060810; }
        QWidget#navRail {
            background-color: #0a101c;
            border-right: 1px solid #243656;
        }
        QLabel#brandMark {
            color: #dce9ff;
            font-size: 13px;
            font-weight: 700;
            letter-spacing: 3px;
        }
        QLabel#heroTitle {
            color: #f2f6ff;
            font-size: 44px;
            font-weight: 800;
            letter-spacing: 1px;
        }
        QLabel#heroSub {
            color: #a8c0e8;
            font-size: 14px;
            font-weight: 500;
            letter-spacing: 1px;
        }
        QLabel#statusLabel {
            color: #8eb8ff;
            font-size: 13px;
        }
        QLabel#sectionTitle {
            color: #e8f0ff;
            font-size: 20px;
            font-weight: 700;
        }
        QLabel#muted {
            color: #9eb0d0;
            font-size: 13px;
        }
        QLabel#fieldLabel {
            color: #9eb4dc;
            font-size: 12px;
            font-weight: 600;
            letter-spacing: 0.5px;
        }
        QLabel#warnLabel {
            color: #ffd27a;
            font-size: 12px;
        }
        QPushButton {
            background-color: #2a3d6e;
            color: #eaf2ff;
            border: 1px solid #4a6eb0;
            padding: 0px 18px;
            border-radius: 10px;
            font-size: 13px;
            font-weight: 600;
            min-height: 40px;
        }
        QPushButton:hover {
            background-color: #35548f;
            border: 1px solid #6a96d8;
        }
        QPushButton:pressed { background-color: #243a6a; }
        QPushButton:disabled {
            color: #6a7588;
            background-color: #1a2233;
        }
        QPushButton#playBtn {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #1e6dff, stop:0.5 #3a9bff, stop:1 #1a5fe0);
            color: #ffffff;
            border: 1px solid #9ac4ff;
            font-size: 20px;
            font-weight: 800;
            letter-spacing: 4px;
            min-height: 56px;
            min-width: 240px;
            border-radius: 14px;
            padding: 0px 36px;
        }
        QPushButton#playBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #2d7dff, stop:0.5 #54b0ff, stop:1 #2a70f0);
        }
        QPushButton#navBtn {
            text-align: left;
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 12px;
            padding: 0px 14px;
            min-height: 42px;
            color: #b8c8e8;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#navBtn:hover {
            background-color: #1e3a6e;
            color: #ffffff;
        }
        QPushButton#navBtn:checked {
            background-color: #2454a8;
            border: 1px solid #6aa0e8;
            color: #ffffff;
        }
        QTextEdit#console {
            background-color: #070b14;
            border: 1px solid #2a4570;
            border-radius: 12px;
            color: #b8d0f8;
            font-family: "JetBrains Mono", "Cascadia Code", "Fira Code", "Consolas", monospace;
            font-size: 12px;
            padding: 10px;
            selection-background-color: #2d5aaa;
        }
        QLineEdit, QSpinBox {
            background-color: #0c1220;
            border: 1px solid #3a5588;
            border-radius: 10px;
            color: #e8f0ff;
            font-size: 13px;
            font-weight: 500;
            padding-left: 12px;
            padding-right: 12px;
            padding-top: 0px;
            padding-bottom: 0px;
            min-height: 38px;
            max-height: 38px;
            selection-background-color: #2d5aaa;
            selection-color: #ffffff;
        }
        QLineEdit:focus, QSpinBox:focus {
            border: 1px solid #6a9ae0;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            subcontrol-origin: border;
            width: 18px;
            border: none;
            background-color: #1a2848;
        }
        QSpinBox::up-button { subcontrol-position: top right; }
        QSpinBox::down-button { subcontrol-position: bottom right; }
        QProgressBar {
            background-color: #0c1220;
            border: 1px solid #2a4570;
            border-radius: 8px;
            text-align: center;
            color: #d0e4ff;
            min-height: 10px;
            max-height: 10px;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #1e6dff, stop:1 #5ec8ff);
            border-radius: 7px;
        }
        QStackedWidget { background: transparent; }
    )");
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Wsalauncher"));
    resize(1080, 700);
    setMinimumSize(900, 600);
    setStyleSheet(cinematicStyle());

    if (QFontDatabase::hasFamily(QStringLiteral("Noto Sans")))
        QApplication::setFont(QFont(QStringLiteral("Noto Sans"), 11));
    else if (QFontDatabase::hasFamily(QStringLiteral("Segoe UI")))
        QApplication::setFont(QFont(QStringLiteral("Segoe UI"), 11));
    else
        QApplication::setFont(QFont(QStringLiteral("Sans Serif"), 11));

    setupUi();
    loadConfig();
    if (!m_licenseKey.isEmpty())
        writeClientAuthJson();
    refreshAccountStatus();

    const QString logo = paths::logoPath();
    if (QFile::exists(logo))
        setWindowIcon(QIcon(logo));

    appendLog(QStringLiteral("Wsalauncher ready (%1 / %2)")
                  .arg(platform::osName(), platform::archName()));
    m_status->setText(gamelauncher::isInstalled() ? QStringLiteral("Ready to play")
                                                    : QStringLiteral("Install required"));
    m_playBtn->setText(QStringLiteral("PLAY"));

    m_glowTimer = new QTimer(this);
    connect(m_glowTimer, &QTimer::timeout, this, &MainWindow::pulseGlow);
    m_glowTimer->start(40);

    QTimer::singleShot(1500, this, [this]() { launcherUpdate::checkAndOfferUpdate(this, true); });
}

QPixmap MainWindow::hiDpiLogo(int logicalPx) const {
    const QString path = paths::logoPath();
    if (!QFile::exists(path))
        return {};
    QPixmap src(path);
    if (src.isNull())
        return {};
    const qreal dpr = devicePixelRatioF();
    const QSize target(qRound(logicalPx * dpr), qRound(logicalPx * dpr));
    QPixmap scaled = src.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    return scaled;
}

void MainWindow::setupUi() {
    auto *atmosphere = new AtmosphereWidget(this);
    setCentralWidget(atmosphere);

    auto *root = new QHBoxLayout(atmosphere);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *navRail = new QWidget;
    navRail->setObjectName(QStringLiteral("navRail"));
    navRail->setFixedWidth(168);
    auto *nav = new QVBoxLayout(navRail);
    nav->setContentsMargins(16, 24, 16, 24);
    nav->setSpacing(8);

    auto *sideLogo = new QLabel;
    const QPixmap sidePx = hiDpiLogo(56);
    if (!sidePx.isNull()) {
        sideLogo->setPixmap(sidePx);
        sideLogo->setAlignment(Qt::AlignCenter);
        sideLogo->setFixedHeight(64);
        auto *sideGlow = new QGraphicsDropShadowEffect;
        sideGlow->setBlurRadius(28);
        sideGlow->setColor(QColor(60, 140, 255, 160));
        sideGlow->setOffset(0, 0);
        sideLogo->setGraphicsEffect(sideGlow);
    }
    auto *brand = new QLabel(QStringLiteral("WSAMIAW"));
    brand->setObjectName(QStringLiteral("brandMark"));
    brand->setAlignment(Qt::AlignCenter);
    nav->addWidget(sideLogo);
    nav->addWidget(brand);
    nav->addSpacing(20);

    auto makeNav = [&](const QString &text) {
        auto *b = new QPushButton(text);
        b->setObjectName(QStringLiteral("navBtn"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        m_navBtns.push_back(b);
        nav->addWidget(b);
        return b;
    };
    auto *btnHome = makeNav(QStringLiteral("Home"));
    auto *btnAccount = makeNav(QStringLiteral("Account"));
    auto *btnMods = makeNav(QStringLiteral("Mods"));
    auto *btnSettings = makeNav(QStringLiteral("Settings"));
    btnHome->setChecked(true);
    nav->addStretch();
    auto *ver = new QLabel(QStringLiteral("MC 26.2"));
    ver->setObjectName(QStringLiteral("muted"));
    ver->setAlignment(Qt::AlignCenter);
    nav->addWidget(ver);
    root->addWidget(navRail);

    auto *rightWrap = new QWidget;
    auto *right = new QVBoxLayout(rightWrap);
    right->setContentsMargins(28, 24, 28, 20);
    right->setSpacing(14);

    m_stack = new QStackedWidget;

    // Home
    auto *home = new QWidget;
    auto *homeLay = new QVBoxLayout(home);
    homeLay->setContentsMargins(20, 10, 20, 10);
    homeLay->setSpacing(10);
    homeLay->addStretch(2);

    m_homeLogo = new QLabel;
    const QPixmap heroPx = hiDpiLogo(128);
    if (!heroPx.isNull()) {
        m_homeLogo->setPixmap(heroPx);
        m_homeLogo->setAlignment(Qt::AlignCenter);
        m_logoGlow = new QGraphicsDropShadowEffect;
        m_logoGlow->setBlurRadius(48);
        m_logoGlow->setColor(QColor(70, 150, 255, 180));
        m_logoGlow->setOffset(0, 4);
        m_homeLogo->setGraphicsEffect(m_logoGlow);
        homeLay->addWidget(m_homeLogo, 0, Qt::AlignCenter);
    }

    auto *title = new QLabel(QStringLiteral("Wsamiaw"));
    title->setObjectName(QStringLiteral("heroTitle"));
    title->setAlignment(Qt::AlignCenter);
    auto *sub = new QLabel(QStringLiteral("MINECRAFT 26.2  ·  FABRIC"));
    sub->setObjectName(QStringLiteral("heroSub"));
    sub->setAlignment(Qt::AlignCenter);
    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("statusLabel"));
    m_status->setAlignment(Qt::AlignCenter);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setFixedWidth(280);
    m_progress->hide();

    m_playBtn = new QPushButton(QStringLiteral("PLAY"));
    m_playBtn->setObjectName(QStringLiteral("playBtn"));
    m_playBtn->setCursor(Qt::PointingHandCursor);
    m_playGlow = new QGraphicsDropShadowEffect;
    m_playGlow->setBlurRadius(36);
    m_playGlow->setColor(QColor(50, 140, 255, 200));
    m_playGlow->setOffset(0, 8);
    m_playBtn->setGraphicsEffect(m_playGlow);

    m_installBtn = new QPushButton(QStringLiteral("Install / update"));
    m_installBtn->setCursor(Qt::PointingHandCursor);
    m_installBtn->setFixedWidth(180);

    homeLay->addWidget(title);
    homeLay->addWidget(sub);
    homeLay->addSpacing(4);
    homeLay->addWidget(m_status);
    homeLay->addSpacing(6);
    homeLay->addWidget(m_progress, 0, Qt::AlignCenter);
    homeLay->addSpacing(10);
    homeLay->addWidget(m_playBtn, 0, Qt::AlignCenter);
    homeLay->addSpacing(8);
    homeLay->addWidget(m_installBtn, 0, Qt::AlignCenter);
    homeLay->addStretch(3);
    m_stack->addWidget(home);

    // Account — username + license key (HWID once on website)
    {
        auto *accountPage = new QWidget;
        auto *outer = new QVBoxLayout(accountPage);
        outer->setContentsMargins(8, 8, 8, 8);
        auto *panel = new GlassPanel;
        auto *lay = new QVBoxLayout(panel);
        lay->setContentsMargins(28, 26, 28, 26);
        lay->setSpacing(10);

        auto *t = new QLabel(QStringLiteral("License"));
        t->setObjectName(QStringLiteral("sectionTitle"));
        auto *hint = new QLabel(
            QStringLiteral("Site login — account username + license key (not your Minecraft name). HWID once on the website."));
        hint->setObjectName(QStringLiteral("muted"));
        hint->setWordWrap(true);

        m_accountStatus = new QLabel;
        m_accountStatus->setObjectName(QStringLiteral("statusLabel"));
        m_accountStatus->setWordWrap(true);

        m_accountUserEdit = new QLineEdit;
        m_accountUserEdit->setPlaceholderText(QStringLiteral("Account username"));
        m_accountKey = new QLineEdit;
        m_accountKey->setPlaceholderText(QStringLiteral("License key"));
        m_accountKey->setEchoMode(QLineEdit::Password);
        m_accountHwid = new QLabel;
        m_accountHwid->setObjectName(QStringLiteral("muted"));
        m_accountHwid->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_accountSaveBtn = new QPushButton(QStringLiteral("Save login"));
        m_accountSaveBtn->setCursor(Qt::PointingHandCursor);
        m_accountSaveBtn->setFixedWidth(160);
        auto *siteBtn = new QPushButton(QStringLiteral("Open website (HWID)"));
        siteBtn->setCursor(Qt::PointingHandCursor);

        auto addField = [&](const QString &label, QWidget *w) {
            auto *l = new QLabel(label);
            l->setObjectName(QStringLiteral("fieldLabel"));
            lay->addWidget(l);
            lay->addWidget(w);
            lay->addSpacing(4);
        };

        lay->addWidget(t);
        lay->addWidget(hint);
        lay->addSpacing(8);
        lay->addWidget(m_accountStatus);
        addField(QStringLiteral("ACCOUNT USERNAME"), m_accountUserEdit);
        addField(QStringLiteral("LICENSE KEY"), m_accountKey);
        addField(QStringLiteral("THIS PC HWID (for site)"), m_accountHwid);
        lay->addWidget(m_accountSaveBtn, 0, Qt::AlignLeft);
        lay->addWidget(siteBtn, 0, Qt::AlignLeft);
        lay->addStretch();
        outer->addWidget(panel);
        m_stack->addWidget(accountPage);

        connect(m_accountSaveBtn, &QPushButton::clicked, this, &MainWindow::onAccountSave);
        connect(siteBtn, &QPushButton::clicked, this, &MainWindow::onOpenAccountSite);

        auto *loginTimer = new QTimer(this);
        loginTimer->setSingleShot(true);
        loginTimer->setInterval(400);
        auto schedulePersist = [this, loginTimer]() { loginTimer->start(); };
        connect(loginTimer, &QTimer::timeout, this, &MainWindow::persistLogin);
        connect(m_accountUserEdit, &QLineEdit::textChanged, this, schedulePersist);
        connect(m_accountKey, &QLineEdit::textChanged, this, schedulePersist);
    }

    // Mods
    QPushButton *openMods = nullptr;
    {
        auto *modsPage = new QWidget;
        auto *outer = new QVBoxLayout(modsPage);
        outer->setContentsMargins(8, 8, 8, 8);
        auto *panel = new GlassPanel;
        auto *lay = new QVBoxLayout(panel);
        lay->setContentsMargins(28, 26, 28, 26);
        lay->setSpacing(10);
        auto *t = new QLabel(QStringLiteral("Bundled mods"));
        t->setObjectName(QStringLiteral("sectionTitle"));
        auto *hint = new QLabel(
            QStringLiteral("Modrinth on install · Wsamiaw JAR from site when license key is set"));
        hint->setObjectName(QStringLiteral("muted"));
        QStringList lines;
        lines << QStringLiteral("• Wsamiaw (licensed download or GitHub fallback)");
        for (const QString &s : constants::bundledModSlugs())
            lines << QStringLiteral("• %1").arg(s);
        auto *list = new QLabel(lines.join(QStringLiteral("<br>")));
        list->setObjectName(QStringLiteral("muted"));
        list->setTextFormat(Qt::RichText);
        openMods = new QPushButton(QStringLiteral("Open mods folder"));
        openMods->setCursor(Qt::PointingHandCursor);
        openMods->setFixedWidth(200);
        lay->addWidget(t);
        lay->addWidget(hint);
        lay->addSpacing(8);
        lay->addWidget(list);
        lay->addSpacing(14);
        lay->addWidget(openMods, 0, Qt::AlignLeft);
        lay->addStretch();
        outer->addWidget(panel);
        m_stack->addWidget(modsPage);
    }

    // Settings
    QPushButton *saveBtn = nullptr;
    QPushButton *updateLauncherBtn = nullptr;
    {
        auto *settingsPage = new QWidget;
        auto *outer = new QVBoxLayout(settingsPage);
        outer->setContentsMargins(8, 8, 8, 8);
        auto *panel = new GlassPanel;
        auto *setLay = new QVBoxLayout(panel);
        setLay->setContentsMargins(28, 26, 28, 26);
        setLay->setSpacing(8);

        auto addField = [&](const QString &label, QWidget *field) {
            auto *l = new QLabel(label);
            l->setObjectName(QStringLiteral("fieldLabel"));
            setLay->addWidget(l);
            setLay->addWidget(field);
            setLay->addSpacing(6);
        };

        m_ram = new QSpinBox;
        m_ram->setRange(1024, 16384);
        m_ram->setSingleStep(256);
        m_minecraftName = new QLineEdit;
        m_minecraftName->setPlaceholderText(QStringLiteral("Player"));
        m_java = new QLineEdit;
        m_wsamiawJar = new QLineEdit;
        auto *gameDir = new QLineEdit(paths::minecraftDir());
        gameDir->setReadOnly(true);

        auto *sec = new QLabel(QStringLiteral("Settings"));
        sec->setObjectName(QStringLiteral("sectionTitle"));
        setLay->addWidget(sec);
        setLay->addSpacing(8);
        addField(QStringLiteral("GAME FOLDER"), gameDir);
        addField(QStringLiteral("MINECRAFT NAME (in-game)"), m_minecraftName);
        addField(QStringLiteral("RAM (MB)"), m_ram);
        addField(QStringLiteral("JAVA (25+)"), m_java);
        addField(QStringLiteral("WSAMIAW JAR (FALLBACK)"), m_wsamiawJar);

        saveBtn = new QPushButton(QStringLiteral("Save settings"));
        saveBtn->setCursor(Qt::PointingHandCursor);
        saveBtn->setFixedWidth(160);
        setLay->addWidget(saveBtn, 0, Qt::AlignLeft);
        updateLauncherBtn = new QPushButton(QStringLiteral("Update Wsalauncher"));
        updateLauncherBtn->setCursor(Qt::PointingHandCursor);
        updateLauncherBtn->setFixedWidth(180);
        setLay->addWidget(updateLauncherBtn, 0, Qt::AlignLeft);
        auto *launcherVer = new QLabel(
            QStringLiteral("Launcher %1").arg(launcherUpdate::currentVersion()));
        launcherVer->setObjectName(QStringLiteral("muted"));
        setLay->addWidget(launcherVer);
        auto *warn = new QLabel(QStringLiteral("Java 25+ is required for Minecraft 26.2"));
        warn->setObjectName(QStringLiteral("warnLabel"));
        setLay->addWidget(warn);
        setLay->addStretch();
        outer->addWidget(panel);
        m_stack->addWidget(settingsPage);
    }

    right->addWidget(m_stack, 1);

    auto *consoleLabel = new QLabel(QStringLiteral("CONSOLE"));
    consoleLabel->setObjectName(QStringLiteral("fieldLabel"));
    m_console = new QTextEdit;
    m_console->setObjectName(QStringLiteral("console"));
    m_console->setReadOnly(true);
    m_console->setMaximumHeight(200);
    m_console->setMinimumHeight(140);
    right->addWidget(consoleLabel);
    right->addWidget(m_console);
    root->addWidget(rightWrap, 1);

    connect(btnHome, &QPushButton::clicked, this, [this]() { showPage(0); });
    connect(btnAccount, &QPushButton::clicked, this, [this]() { showPage(1); });
    connect(btnMods, &QPushButton::clicked, this, [this]() { showPage(2); });
    connect(btnSettings, &QPushButton::clicked, this, [this]() { showPage(3); });
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::onPlay);
    connect(m_installBtn, &QPushButton::clicked, this, &MainWindow::onInstall);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveSettings);
    connect(updateLauncherBtn, &QPushButton::clicked, this, &MainWindow::onUpdateLauncher);
    connect(openMods, &QPushButton::clicked, this, &MainWindow::onOpenMods);
}

void MainWindow::pulseGlow() {
    m_glowPhase += 0.08;
    if (m_glowPhase > 6.28318)
        m_glowPhase -= 6.28318;
    const qreal wave = 0.5 + 0.5 * qSin(m_glowPhase);
    if (m_playGlow) {
        m_playGlow->setBlurRadius(28 + wave * 22);
        m_playGlow->setColor(QColor(50, 140, 255, int(140 + wave * 80)));
    }
    if (m_logoGlow) {
        m_logoGlow->setBlurRadius(36 + wave * 18);
        m_logoGlow->setColor(QColor(70, 150, 255, int(120 + wave * 70)));
    }
}

void MainWindow::applyNavState(int index) {
    for (int i = 0; i < m_navBtns.size(); ++i)
        m_navBtns[i]->setChecked(i == index);
}

void MainWindow::showPage(int index) {
    m_stack->setCurrentIndex(index);
    applyNavState(index);
}

void MainWindow::loadConfig() {
    QFile f(paths::configFile());
    if (!f.open(QIODevice::ReadOnly))
        return;
    QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    m_accountUser = o.value(QStringLiteral("account_user")).toString();
    if (m_accountUser.isEmpty()) {
        const QString legacy = o.value(QStringLiteral("username")).toString();
        if (!legacy.isEmpty() && legacy != QStringLiteral("Player"))
            m_accountUser = legacy;
    }
    if (m_accountUserEdit)
        m_accountUserEdit->setText(m_accountUser);
    m_licenseKey = o.value(QStringLiteral("license_key")).toString();
    if (m_accountKey)
        m_accountKey->setText(m_licenseKey);
    if (m_minecraftName) {
        QString mc = o.value(QStringLiteral("minecraft_name")).toString();
        if (mc.isEmpty())
            mc = QStringLiteral("Player");
        m_minecraftName->setText(mc);
    }
    m_ram->setValue(o.value(QStringLiteral("ram_mb")).toInt(4096));
    QString java = o.value(QStringLiteral("java")).toString();
    if (java.trimmed().isEmpty())
        java = platform::findJava();
    m_java->setText(java);
    m_wsamiawJar->setText(
        o.value(QStringLiteral("wsamiaw_jar")).toString(constants::defaultWsamiawJar()));
    QString hwErr;
    m_cachedHwid = hwid::compute(m_java->text(), &hwErr);
    if (m_accountHwid) {
        if (m_cachedHwid.isEmpty())
            m_accountHwid->setText(hwErr.isEmpty() ? QStringLiteral("—") : hwErr);
        else
            m_accountHwid->setText(m_cachedHwid);
    }
}

void MainWindow::saveConfig() {
    QJsonObject o;
    m_accountUser = m_accountUserEdit ? m_accountUserEdit->text().trimmed() : m_accountUser;
    m_licenseKey = m_accountKey ? m_accountKey->text().trimmed() : m_licenseKey;
    if (!m_accountUser.isEmpty())
        o[QStringLiteral("account_user")] = m_accountUser;
    if (!m_licenseKey.isEmpty())
        o[QStringLiteral("license_key")] = m_licenseKey;
    if (m_minecraftName) {
        QString mc = m_minecraftName->text().trimmed();
        if (mc.isEmpty())
            mc = QStringLiteral("Player");
        o[QStringLiteral("minecraft_name")] = mc;
    }
    o[QStringLiteral("ram_mb")] = m_ram->value();
    o[QStringLiteral("java")] = m_java->text();
    o[QStringLiteral("wsamiaw_jar")] = m_wsamiawJar->text();
    QFile f(paths::configFile());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

void MainWindow::persistLogin() {
    saveConfig();
    writeClientAuthJson();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    persistLogin();
    QMainWindow::closeEvent(event);
}

void MainWindow::onSaveSettings() {
    saveConfig();
    appendLog(QStringLiteral("Settings saved."));
}

void MainWindow::onUpdateLauncher() {
    launcherUpdate::checkAndOfferUpdate(this, false);
}

void MainWindow::onOpenMods() {
    const QString dir = paths::minecraftDir() + QStringLiteral("/mods");
    if (!platform::openDirectory(dir))
        appendLog(QStringLiteral("Could not open mods folder: %1").arg(dir));
}

void MainWindow::setAccountStatusText(const QString &text, bool warn) {
    if (!m_accountStatus)
        return;
    m_accountStatus->setText(text);
    m_accountStatus->setObjectName(warn ? QStringLiteral("warnLabel") : QStringLiteral("statusLabel"));
}

wsamiawApi::ClientAuth MainWindow::clientAuth() const {
    wsamiawApi::ClientAuth a;
    a.licenseKey = m_licenseKey;
    a.hwid = m_cachedHwid;
    return a;
}

void MainWindow::writeClientAuthJson() const {
    if (m_licenseKey.isEmpty())
        return;
    const QString dir = paths::minecraftDir() + QStringLiteral("/config/Wsamiaw");
    QDir().mkpath(dir);
    QJsonObject o;
    o[QStringLiteral("license_key")] = m_licenseKey;
    QFile f(dir + QStringLiteral("/auth.json"));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void MainWindow::onOpenAccountSite() {
    QDesktopServices::openUrl(QUrl(QString::fromUtf8(constants::kSiteAccountUrl)));
}

void MainWindow::onAccountSave() {
    saveConfig();
    writeClientAuthJson();
    appendLog(QStringLiteral("License saved."));
    refreshAccountStatus();
}

void MainWindow::refreshAccountStatus() {
    const wsamiawApi::ClientAuth auth = clientAuth();
    if (!auth.valid()) {
        setAccountStatusText(QStringLiteral("Enter account username + license key. Bind HWID on the website once."), true);
        return;
    }
    auto *thread = QThread::create([this, auth]() {
        QNetworkAccessManager nam;
        QString reason;
        const bool ok = wsamiawApi::verifyClient(nam, auth, &reason);
        QMetaObject::invokeMethod(
            this,
            [this, ok, reason]() {
                if (ok) {
                    setAccountStatusText(
                        QStringLiteral("Key + HWID OK — Install / Play ready."));
                } else if (reason == QStringLiteral("hwid_required")) {
                    setAccountStatusText(
                        QStringLiteral("Bind this PC’s HWID on the website (shown above), then Save."),
                        true);
                } else if (reason == QStringLiteral("hwid_mismatch")) {
                    setAccountStatusText(
                        QStringLiteral("HWID mismatch — re-bind on site with the HWID shown above."),
                        true);
                } else {
                    setAccountStatusText(QStringLiteral("Check failed: %1").arg(reason), true);
                }
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void MainWindow::appendLog(const QString &line) {
    m_console->append(line);
    QTextCursor c = m_console->textCursor();
    c.movePosition(QTextCursor::End);
    m_console->setTextCursor(c);
}

void MainWindow::stopLogTail() {
    if (m_logTailTimer)
        m_logTailTimer->stop();
}

void MainWindow::tailGameLog() {
    const QString path = paths::minecraftDir() + QStringLiteral("/logs/latest.log");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    if (f.size() < m_logFileOffset)
        m_logFileOffset = 0;
    if (!f.seek(m_logFileOffset))
        return;
    const QByteArray chunk = f.readAll();
    m_logFileOffset = f.pos();
    if (chunk.isEmpty())
        return;
    const QString text = QString::fromUtf8(chunk);
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (!line.isEmpty())
            appendLog(line);
    }
}

void MainWindow::onGameOutput() {
    if (!m_gameProcess)
        return;
    const QByteArray data = m_gameProcess->readAllStandardOutput();
    const QString text = QString::fromUtf8(data);
    for (QString line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (!line.isEmpty())
            appendLog(line);
    }
}

void MainWindow::onGameFinished(int exitCode, QProcess::ExitStatus status) {
    stopLogTail();
    tailGameLog();
    if (status == QProcess::CrashExit)
        appendLog(QStringLiteral("Minecraft crashed."));
    else
        appendLog(QStringLiteral("Minecraft exited (%1).").arg(exitCode));
    m_status->setText(QStringLiteral("Game closed"));
    setBusy(false);
}

void MainWindow::startGameProcess(const QString &javaPath, const QStringList &args,
                                  const QString &workDir) {
    if (m_gameProcess) {
        m_gameProcess->kill();
        m_gameProcess->deleteLater();
        m_gameProcess = nullptr;
    }

    const QString logPath = workDir + QStringLiteral("/logs/latest.log");
    QFileInfo info(logPath);
    m_logFileOffset = info.exists() ? info.size() : 0;

    m_gameProcess = new QProcess(this);
    m_gameProcess->setProgram(javaPath);
    m_gameProcess->setArguments(args);
    m_gameProcess->setWorkingDirectory(workDir);
    m_gameProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_gameProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::onGameOutput);
    connect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &MainWindow::onGameFinished);

    if (!m_logTailTimer) {
        m_logTailTimer = new QTimer(this);
        connect(m_logTailTimer, &QTimer::timeout, this, &MainWindow::tailGameLog);
    }
    m_logTailTimer->start(250);

    appendLog(QStringLiteral("Starting Minecraft…"));
    m_gameProcess->start();
    if (!m_gameProcess->waitForStarted(15000)) {
        stopLogTail();
        appendLog(m_gameProcess->errorString());
        m_status->setText(QStringLiteral("Launch failed"));
        setBusy(false);
        m_gameProcess->deleteLater();
        m_gameProcess = nullptr;
        return;
    }
    m_status->setText(QStringLiteral("Game running"));
    setBusy(false);
    m_playBtn->setEnabled(false);
}

void MainWindow::setBusy(bool busy) {
    m_busy = busy;
    m_playBtn->setEnabled(!busy && !(m_gameProcess && m_gameProcess->state() != QProcess::NotRunning));
    if (m_installBtn)
        m_installBtn->setEnabled(!busy);
    if (busy) {
        m_progress->show();
        m_progress->setRange(0, 0);
    } else {
        m_progress->setRange(0, 100);
        m_progress->setValue(100);
        QTimer::singleShot(900, this, [this]() {
            if (!m_busy)
                m_progress->hide();
        });
    }
}

void MainWindow::onInstall() {
    if (m_busy)
        return;
    saveConfig();
    setBusy(true);
    m_status->setText(QStringLiteral("Installing…"));
    const QString java = platform::resolveJava(m_java->text());
    const QString jar = m_wsamiawJar->text();
    const wsamiawApi::ClientAuth auth = clientAuth();

    auto *thread = QThread::create([this, java, jar, auth]() {
        QNetworkAccessManager nam;
        QString err;
        auto logFn = [this](const QString &s) {
            QMetaObject::invokeMethod(this, [this, s]() { appendLog(s); }, Qt::QueuedConnection);
        };
        bool ok = installer::runFullInstall(nam, java, jar, auth, logFn, &err);
        QMetaObject::invokeMethod(
            this,
            [this, ok, err]() {
                setBusy(false);
                if (ok) {
                    m_status->setText(QStringLiteral("Install complete"));
                } else {
                    m_status->setText(QStringLiteral("Install failed"));
                    appendLog(err);
                }
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void MainWindow::onPlay() {
    if (m_busy)
        return;
    if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) {
        appendLog(QStringLiteral("Game already running."));
        return;
    }
    if (!gamelauncher::isInstalled()) {
        QMessageBox::information(this, QStringLiteral("Play"),
                                 QStringLiteral("Run Install / update first."));
        return;
    }
    if (!clientAuth().valid()) {
        QMessageBox::warning(this, QStringLiteral("Play"),
                             QStringLiteral("Account: username + license key. Bind HWID on site."));
        return;
    }
    persistLogin();
    setBusy(true);
    m_status->setText(QStringLiteral("Preparing launch…"));
    const QString java = platform::resolveJava(m_java->text());
    QString username = m_minecraftName ? m_minecraftName->text().trimmed() : QString();
    if (username.isEmpty())
        username = QStringLiteral("Player");
    const int ram = m_ram->value();
    const wsamiawApi::ClientAuth auth = clientAuth();

    auto *thread = QThread::create([this, java, username, ram, auth]() {
        QString err;
        gamelauncher::LaunchSpec spec;
        auto logFn = [this](const QString &s) {
            QMetaObject::invokeMethod(this, [this, s]() { appendLog(s); }, Qt::QueuedConnection);
        };
        QNetworkAccessManager nam;
        const QString launchToken = wsamiawApi::fetchLaunchToken(nam, auth, &err);
        if (launchToken.isEmpty()) {
            QMetaObject::invokeMethod(
                this,
                [this, err]() {
                    setBusy(false);
                    m_status->setText(QStringLiteral("Launch failed"));
                    const QString msg =
                        err.isEmpty()
                            ? QStringLiteral("Could not get launch token — check license + HWID on site.")
                            : err;
                    appendLog(msg);
                    QMessageBox::warning(this, QStringLiteral("Launch"), msg);
                },
                Qt::QueuedConnection);
            return;
        }
        const bool ok = gamelauncher::prepareLaunch(java, username, ram, launchToken, logFn, &spec, &err);
        QMetaObject::invokeMethod(
            this,
            [this, ok, err, spec]() {
                if (!ok) {
                    setBusy(false);
                    m_status->setText(QStringLiteral("Launch failed"));
                    appendLog(err);
                    QMessageBox::warning(this, QStringLiteral("Launch"), err);
                    return;
                }
                startGameProcess(spec.javaPath, spec.args, spec.workDir);
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}
