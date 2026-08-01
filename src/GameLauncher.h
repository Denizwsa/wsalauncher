#pragma once

#include <QString>
#include <QStringList>
#include <functional>

namespace gamelauncher {

using LogFn = std::function<void(const QString &)>;

struct LaunchSpec {
    QString javaPath;
    QStringList args;
    QString workDir;
};

bool isInstalled();

/// Downloads libraries/assets and builds the Java command. Does not start the process.
bool prepareLaunch(const QString &javaPath, const QString &username, int ramMb,
                   const QString &launchToken, LogFn log, LaunchSpec *out, QString *errorOut);

} // namespace gamelauncher
