#include "GameLauncher.h"

#include "Bootstrap.h"
#include "LauncherUpdate.h"
#include "Constants.h"
#include "Paths.h"
#include "Platform.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QUuid>

#include "Download.h"

namespace gamelauncher {

static QString classpathSeparator() {
#if defined(Q_OS_WIN)
    return QStringLiteral(";");
#else
    return QStringLiteral(":");
#endif
}

static QString currentOs() {
    return platform::osName();
}

static QString currentArch() {
    return platform::archName();
}

static bool ruleMatches(const QJsonObject &rule, const QHash<QString, bool> &features) {
    const QJsonObject os = rule.value(QStringLiteral("os")).toObject();
    if (!os.isEmpty()) {
        const QString name = os.value(QStringLiteral("name")).toString();
        if (!name.isEmpty() && name != currentOs())
            return false;
        const QString arch = os.value(QStringLiteral("arch")).toString();
        if (!arch.isEmpty() && arch != currentArch())
            return false;
    }
    const QJsonObject feat = rule.value(QStringLiteral("features")).toObject();
    if (!feat.isEmpty()) {
        for (auto it = feat.constBegin(); it != feat.constEnd(); ++it) {
            const bool want = it.value().toBool();
            const bool have = features.value(it.key(), false);
            if (want != have)
                return false;
        }
    }
    return true;
}

static bool rulesAllow(const QJsonArray &rules, const QHash<QString, bool> &features = {}) {
    if (rules.isEmpty())
        return true;
    bool allow = false;
    for (const QJsonValue &rv : rules) {
        const QJsonObject rule = rv.toObject();
        if (!ruleMatches(rule, features))
            continue;
        allow = rule.value(QStringLiteral("action")).toString(QStringLiteral("allow"))
                == QStringLiteral("allow");
    }
    return allow;
}

static QJsonObject loadVersionFile(const QString &instance, const QString &id) {
    const QString path =
        instance + QStringLiteral("/versions/") + id + QLatin1Char('/') + id + QStringLiteral(".json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

static QStringList versionChain(const QString &instance, const QString &versionId) {
    QStringList chain;
    QString current = versionId;
    while (true) {
        chain.prepend(current);
        const QJsonObject obj = loadVersionFile(instance, current);
        if (obj.isEmpty())
            break;
        const QString parent = obj.value(QStringLiteral("inheritsFrom")).toString();
        if (parent.isEmpty())
            break;
        current = parent;
    }
    return chain;
}

static QJsonObject mergeChain(const QString &instance, const QString &versionId) {
    const QStringList chain = versionChain(instance, versionId);
    QJsonObject merged;
    for (const QString &id : chain) {
        const QJsonObject layer = loadVersionFile(instance, id);
        if (layer.contains(QStringLiteral("mainClass")))
            merged[QStringLiteral("mainClass")] = layer[QStringLiteral("mainClass")];
        if (layer.contains(QStringLiteral("minecraftArguments")))
            merged[QStringLiteral("minecraftArguments")] =
                layer[QStringLiteral("minecraftArguments")];
        QJsonArray libs = merged.value(QStringLiteral("libraries")).toArray();
        const QJsonArray add = layer.value(QStringLiteral("libraries")).toArray();
        for (const QJsonValue &v : add)
            libs.append(v);
        merged[QStringLiteral("libraries")] = libs;
        if (layer.contains(QStringLiteral("arguments"))) {
            QJsonObject args = merged.value(QStringLiteral("arguments")).toObject();
            const QJsonObject over = layer.value(QStringLiteral("arguments")).toObject();
            for (const char *key : {"jvm", "game"}) {
                QJsonArray base = args.value(key).toArray();
                const QJsonArray extra = over.value(key).toArray();
                for (const QJsonValue &v : extra)
                    base.append(v);
                args[key] = base;
            }
            merged[QStringLiteral("arguments")] = args;
        }
        if (layer.contains(QStringLiteral("assetIndex")))
            merged[QStringLiteral("assetIndex")] = layer[QStringLiteral("assetIndex")];
    }
    return merged;
}

static bool libraryAllowed(const QJsonObject &lib) {
    return rulesAllow(lib.value(QStringLiteral("rules")).toArray());
}

static QString mavenPath(const QString &name) {
    const QStringList p = name.split(QLatin1Char(':'));
    if (p.size() < 3)
        return name;
    QString group = p[0];
    group.replace(QLatin1Char('.'), QLatin1Char('/'));
    const QString classifier = p.size() > 3 ? QStringLiteral("-%1").arg(p[3]) : QString();
    return QStringLiteral("%1/%2/%3/%2-%3%4.jar").arg(group, p[1], p[2], classifier);
}

static QString libraryDestPath(const QJsonObject &lib) {
    if (lib.contains(QStringLiteral("downloads"))) {
        const QString path = lib.value(QStringLiteral("downloads"))
                                 .toObject()
                                 .value(QStringLiteral("artifact"))
                                 .toObject()
                                 .value(QStringLiteral("path"))
                                 .toString();
        if (!path.isEmpty())
            return path;
    }
    return mavenPath(lib.value(QStringLiteral("name")).toString());
}

static QString libraryUrl(const QJsonObject &lib, const QString &path) {
    if (lib.contains(QStringLiteral("downloads"))) {
        const QString url = lib.value(QStringLiteral("downloads"))
                                .toObject()
                                .value(QStringLiteral("artifact"))
                                .toObject()
                                .value(QStringLiteral("url"))
                                .toString();
        if (!url.isEmpty())
            return url;
    }
    QString base = lib.value(QStringLiteral("url")).toString(QStringLiteral("https://libraries.minecraft.net/"));
    if (!base.endsWith(QLatin1Char('/')))
        base += QLatin1Char('/');
    return base + path;
}

static bool ensureLibraries(const QString &instance, const QJsonObject &merged, LogFn log,
                            QString *errorOut) {
    QNetworkAccessManager nam;
    const QJsonArray libs = merged.value(QStringLiteral("libraries")).toArray();
    for (const QJsonValue &lv : libs) {
        const QJsonObject lib = lv.toObject();
        if (!libraryAllowed(lib))
            continue;
        const QString path = libraryDestPath(lib);
        if (path.isEmpty())
            continue;
        const QString dest = instance + QStringLiteral("/libraries/") + path;
        if (QFile::exists(dest))
            continue;
        const QString url = libraryUrl(lib, path);
        QDir().mkpath(QFileInfo(dest).absolutePath());
        log(QStringLiteral("Library %1").arg(path));
        if (!download::fetchToFile(nam, QUrl(url), dest, errorOut))
            return false;
    }
    return true;
}

static QString substitute(const QString &in, const QHash<QString, QString> &map) {
    QString s = in;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        s.replace(QStringLiteral("${%1}").arg(it.key()), it.value());
    return s;
}

static QStringList expandArgs(const QJsonValue &val, const QHash<QString, QString> &map,
                              const QHash<QString, bool> &features = {}) {
    QStringList out;
    if (!val.isArray())
        return out;
    for (const QJsonValue &item : val.toArray()) {
        if (item.isString()) {
            const QString s = substitute(item.toString(), map);
            if (s.contains(QStringLiteral("${")))
                continue;
            out << s;
        } else if (item.isObject()) {
            const QJsonObject obj = item.toObject();
            if (!rulesAllow(obj.value(QStringLiteral("rules")).toArray(), features))
                continue;
            const QJsonValue v = obj.value(QStringLiteral("value"));
            QStringList parts;
            bool unresolved = false;
            auto push = [&](const QString &raw) {
                const QString s = substitute(raw, map);
                if (s.contains(QStringLiteral("${")))
                    unresolved = true;
                else
                    parts << s;
            };
            if (v.isString()) {
                push(v.toString());
            } else if (v.isArray()) {
                for (const QJsonValue &x : v.toArray()) {
                    if (x.isString())
                        push(x.toString());
                }
            }
            if (!unresolved)
                out << parts;
        }
    }
    return out;
}

bool isInstalled() {
    const QString instance = paths::minecraftDir();
    const QString vid = constants::fabricVersionId();
    return QFile::exists(instance + QStringLiteral("/versions/") + vid + QLatin1Char('/') + vid
                         + QStringLiteral(".json"));
}

bool prepareLaunch(const QString &javaPathIn, const QString &usernameIn, int ramMb,
                   const QString &launchToken, LogFn log, LaunchSpec *out, QString *errorOut) {
    if (!isInstalled()) {
        if (errorOut)
            *errorOut = QStringLiteral("Not installed");
        return false;
    }
    QString username = usernameIn.trimmed();
    if (username.isEmpty())
        username = QStringLiteral("Player");

    const QString javaPath = platform::resolveJava(javaPathIn);
    const QString instance = paths::minecraftDir();
    const QString vid = constants::fabricVersionId();
    QNetworkAccessManager nam;
    if (!bootstrap::ensureVanillaVersion(nam, instance, QString::fromUtf8(constants::kMcVersion),
                                         log, errorOut))
        return false;

    const QJsonObject merged = mergeChain(instance, vid);
    if (merged.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Bad version json");
        return false;
    }

    log(QStringLiteral("Checking libraries…"));
    if (!ensureLibraries(instance, merged, log, errorOut))
        return false;

    const QJsonObject assetIndex = merged.value(QStringLiteral("assetIndex")).toObject();
    if (!bootstrap::ensureAssets(nam, instance, assetIndex, log, errorOut))
        return false;

    QStringList cp;
    const QStringList chain = versionChain(instance, vid);
    for (const QString &id : chain) {
        const QString clientJar =
            instance + QStringLiteral("/versions/") + id + QLatin1Char('/') + id + QStringLiteral(".jar");
        if (QFile::exists(clientJar))
            cp << clientJar;
    }

    QStringList nativeJars;
    for (const QJsonValue &lv : merged.value(QStringLiteral("libraries")).toArray()) {
        const QJsonObject lib = lv.toObject();
        if (!libraryAllowed(lib))
            continue;
        const QString path = libraryDestPath(lib);
        const QString file = instance + QStringLiteral("/libraries/") + path;
        if (QFile::exists(file)) {
            cp << file;
            if (path.contains(QStringLiteral("natives-")))
                nativeJars << file;
        }
    }
    if (cp.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Empty classpath");
        return false;
    }

    log(QStringLiteral("Preparing natives…"));
    QDir().mkpath(instance + QStringLiteral("/natives/java"));
    QDir().mkpath(instance + QStringLiteral("/natives/jna"));
    QDir().mkpath(instance + QStringLiteral("/natives/lwjgl"));
    QDir().mkpath(instance + QStringLiteral("/natives/netty"));
    if (!bootstrap::prepareNatives(javaPath, instance, nativeJars, log, errorOut))
        return false;

    QHash<QString, QString> subs;
    subs[QStringLiteral("auth_player_name")] = username;
    subs[QStringLiteral("auth_uuid")] =
        QUuid::createUuidV3(QUuid(QStringLiteral("{6ba7b810-9dad-11d1-80b4-00c04fd430c8}")), username)
            .toString(QUuid::WithoutBraces);
    subs[QStringLiteral("auth_access_token")] = QStringLiteral("0");
    subs[QStringLiteral("user_type")] = QStringLiteral("legacy");
    subs[QStringLiteral("version_name")] = vid;
    subs[QStringLiteral("game_directory")] = instance;
    subs[QStringLiteral("assets_root")] = instance + QStringLiteral("/assets");
    subs[QStringLiteral("assets_index_name")] =
        assetIndex.value(QStringLiteral("id")).toString(QStringLiteral("26"));
    subs[QStringLiteral("natives_directory")] = instance + QStringLiteral("/natives");
    subs[QStringLiteral("launcher_name")] = QStringLiteral("Wsalauncher");
    subs[QStringLiteral("launcher_version")] = launcherUpdate::currentVersion();
    subs[QStringLiteral("clientid")] = QStringLiteral("0");
    subs[QStringLiteral("auth_xuid")] = QStringLiteral("0");
    subs[QStringLiteral("version_type")] = QStringLiteral("release");
    subs[QStringLiteral("classpath")] = cp.join(classpathSeparator());

    const QHash<QString, bool> features;

    QStringList jvmArgs = expandArgs(
        merged.value(QStringLiteral("arguments")).toObject().value(QStringLiteral("jvm")), subs,
        features);
    QStringList jvmFiltered;
    for (int i = 0; i < jvmArgs.size(); ++i) {
        if (jvmArgs[i] == QStringLiteral("-cp") || jvmArgs[i] == QStringLiteral("-classpath")) {
            ++i;
            continue;
        }
        jvmFiltered << jvmArgs[i];
    }

    QStringList args;
    const int heapMin = qMin(ramMb, 1024);
    args << QStringLiteral("-Xmx%1M").arg(ramMb) << QStringLiteral("-Xms%1M").arg(heapMin)
         << QStringLiteral("-XX:+UseZGC") << QStringLiteral("-XX:+DisableExplicitGC")
         << QStringLiteral("-XX:+AlwaysPreTouch");
    if (!launchToken.isEmpty()) {
        args << QStringLiteral("-Dwsamiaw.launcher=Wsalauncher");
        args << QStringLiteral("-Dwsamiaw.launchToken=%1").arg(launchToken);
    }
    args << jvmFiltered;
    args << QStringLiteral("-cp") << cp.join(classpathSeparator());
    args << merged.value(QStringLiteral("mainClass")).toString(
        QStringLiteral("net.fabricmc.loader.impl.launch.knot.KnotClient"));
    args << expandArgs(merged.value(QStringLiteral("arguments")).toObject().value(QStringLiteral("game")),
                       subs, features);

    if (!out) {
        if (errorOut)
            *errorOut = QStringLiteral("Internal: null LaunchSpec");
        return false;
    }
    out->javaPath = javaPath;
    out->args = args;
    out->workDir = instance;
    return true;
}

} // namespace gamelauncher
