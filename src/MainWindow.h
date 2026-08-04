#pragma once

#include <QMainWindow>
#include <QProcess>
#include <QString>
#include <QVector>

#include "WsamiawApi.h"

class QStackedWidget;
class QTextEdit;
class QProgressBar;
class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QGraphicsDropShadowEffect;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onInstall();
    void onPlay();
    void onSaveSettings();
    void onOpenMods();
    void appendLog(const QString &line);
    void pulseGlow();
    void onGameOutput();
    void onGameFinished(int exitCode, QProcess::ExitStatus status);
    void tailGameLog();

    void onAccountSave();
    void onOpenAccountSite();
    void refreshAccountStatus();

    void onUpdateLauncher();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void loadConfig();
    void saveConfig();
    void setBusy(bool busy);
    void showPage(int index);
    void applyNavState(int index);
    void setAccountStatusText(const QString &text, bool warn = false);
    wsamiawApi::ClientAuth clientAuth() const;
    void writeClientAuthJson() const;
    void persistLogin();
    QPixmap hiDpiLogo(int logicalPx) const;
    void startGameProcess(const QString &javaPath, const QStringList &args, const QString &workDir);
    void stopLogTail();

    QStackedWidget *m_stack = nullptr;
    QTextEdit *m_console = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_homeLogo = nullptr;
    QLineEdit *m_java = nullptr;
    QLineEdit *m_wsamiawJar = nullptr;
    QSpinBox *m_ram = nullptr;
    QPushButton *m_playBtn = nullptr;
    QPushButton *m_installBtn = nullptr;
    QVector<QPushButton *> m_navBtns;
    QGraphicsDropShadowEffect *m_playGlow = nullptr;
    QGraphicsDropShadowEffect *m_logoGlow = nullptr;
    QTimer *m_glowTimer = nullptr;
    QTimer *m_logTailTimer = nullptr;
    QProcess *m_gameProcess = nullptr;
    qint64 m_logFileOffset = 0;
    qreal m_glowPhase = 0;
    bool m_busy = false;

    QString m_licenseKey;
    QString m_accountUser;
    QString m_cachedHwid;
    QLineEdit *m_accountUserEdit = nullptr;
    QLineEdit *m_accountKey = nullptr;
    QLineEdit *m_minecraftName = nullptr;
    QLabel *m_accountHwid = nullptr;
    QLabel *m_accountStatus = nullptr;
    QPushButton *m_accountSaveBtn = nullptr;
};
