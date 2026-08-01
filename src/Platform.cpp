#include "Platform.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QtGlobal>
#include <QUrl>

namespace platform {

QString osName() {
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("osx");
#else
    return QStringLiteral("linux");
#endif
}

QString archName() {
#if defined(Q_PROCESSOR_ARM_64) || defined(__aarch64__) || defined(_M_ARM64)
    return QStringLiteral("arm64");
#elif defined(Q_PROCESSOR_X86_64) || defined(__x86_64__) || defined(_M_X64)
    return QStringLiteral("x86_64");
#else
    return QStringLiteral("x86");
#endif
}

static void addIfExists(QStringList &out, const QString &path) {
    if (!path.isEmpty() && QFile::exists(path))
        out << QDir::cleanPath(path);
}

QStringList javaSearchCandidates() {
    QStringList out;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString javaHome = env.value(QStringLiteral("JAVA_HOME"));
    if (!javaHome.isEmpty()) {
#if defined(Q_OS_WIN)
        addIfExists(out, javaHome + QStringLiteral("/bin/java.exe"));
#else
        addIfExists(out, javaHome + QStringLiteral("/bin/java"));
#endif
    }

#if defined(Q_OS_WIN)
    const QStringList winRoots = {
        QStringLiteral("C:/Program Files/Java"),
        QStringLiteral("C:/Program Files/Eclipse Adoptium"),
        QStringLiteral("C:/Program Files/Microsoft"),
        QStringLiteral("C:/Program Files/Zulu"),
        QStringLiteral("C:/Program Files/Amazon Corretto"),
        QStringLiteral("C:/Program Files/BellSoft"),
    };
    for (const QString &root : winRoots) {
        QDir dir(root);
        if (!dir.exists())
            continue;
        for (const QString &jdk : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            addIfExists(out, root + QLatin1Char('/') + jdk + QStringLiteral("/bin/java.exe"));
        }
    }
#elif defined(Q_OS_MACOS)
    {
        QProcess p;
        p.start(QStringLiteral("/usr/libexec/java_home"),
                {QStringLiteral("-v"), QStringLiteral("25")});
        if (p.waitForFinished(3000) && p.exitCode() == 0) {
            const QString home = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
            addIfExists(out, home + QStringLiteral("/bin/java"));
        }
        p.start(QStringLiteral("/usr/libexec/java_home"), {});
        if (p.waitForFinished(3000) && p.exitCode() == 0) {
            const QString home = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
            addIfExists(out, home + QStringLiteral("/bin/java"));
        }
    }
    const QStringList macRoots = {
        QStringLiteral("/Library/Java/JavaVirtualMachines"),
        QStringLiteral("/opt/homebrew/opt"),
        QStringLiteral("/usr/local/opt"),
    };
    for (const QString &root : macRoots) {
        QDir dir(root);
        if (!dir.exists())
            continue;
        for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            addIfExists(out,
                        root + QLatin1Char('/') + name + QStringLiteral("/Contents/Home/bin/java"));
            addIfExists(out, root + QLatin1Char('/') + name + QStringLiteral("/bin/java"));
            addIfExists(out, root + QLatin1Char('/') + name
                                 + QStringLiteral("/libexec/openjdk.jdk/Contents/Home/bin/java"));
        }
    }
#else
    const QStringList linuxPaths = {
        QStringLiteral("/usr/lib/jvm/java-25-openjdk/bin/java"),
        QStringLiteral("/usr/lib/jvm/java-25-jdk/bin/java"),
        QStringLiteral("/usr/lib/jvm/jdk-25/bin/java"),
        QStringLiteral("/usr/lib/jvm/temurin-25-jdk/bin/java"),
        QStringLiteral("/usr/lib/jvm/default/bin/java"),
        QStringLiteral("/usr/lib/jvm/default-runtime/bin/java"),
    };
    for (const QString &p : linuxPaths)
        addIfExists(out, p);
#endif

#if defined(Q_OS_WIN)
    addIfExists(out, QStandardPaths::findExecutable(QStringLiteral("java.exe")));
#else
    addIfExists(out, QStandardPaths::findExecutable(QStringLiteral("java")));
#endif

    out.removeDuplicates();
    return out;
}

QString findJava() {
    const QStringList cands = javaSearchCandidates();
    if (!cands.isEmpty())
        return cands.first();
#if defined(Q_OS_WIN)
    return QStringLiteral("java.exe");
#else
    return QStringLiteral("java");
#endif
}

QString resolveJava(const QString &configured) {
    const QString t = configured.trimmed();
    if (!t.isEmpty()) {
        if (QFile::exists(t))
            return t;
#if defined(Q_OS_WIN)
        if (t.compare(QStringLiteral("java"), Qt::CaseInsensitive) == 0
            || t.compare(QStringLiteral("java.exe"), Qt::CaseInsensitive) == 0) {
            const QString found = QStandardPaths::findExecutable(QStringLiteral("java.exe"));
            if (!found.isEmpty())
                return found;
        }
#else
        if (t == QStringLiteral("java")) {
            const QString found = QStandardPaths::findExecutable(QStringLiteral("java"));
            if (!found.isEmpty())
                return found;
        }
#endif
    }
    return findJava();
}

QString jarToolForJava(const QString &javaPath) {
    const QString dir = QFileInfo(javaPath).absolutePath();
#if defined(Q_OS_WIN)
    const QString jar = dir + QStringLiteral("/jar.exe");
#else
    const QString jar = dir + QStringLiteral("/jar");
#endif
    if (QFile::exists(jar))
        return jar;
#if defined(Q_OS_WIN)
    return QStandardPaths::findExecutable(QStringLiteral("jar.exe"));
#else
    return QStandardPaths::findExecutable(QStringLiteral("jar"));
#endif
}

bool openDirectory(const QString &dir) {
    QDir().mkpath(dir);
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(dir)))
        return true;
#if defined(Q_OS_WIN)
    return QProcess::startDetached(QStringLiteral("explorer.exe"),
                                   {QDir::toNativeSeparators(dir)});
#elif defined(Q_OS_MACOS)
    return QProcess::startDetached(QStringLiteral("open"), {dir});
#else
    return QProcess::startDetached(QStringLiteral("xdg-open"), {dir});
#endif
}

} // namespace platform
