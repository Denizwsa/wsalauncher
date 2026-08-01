#pragma once

#include <QString>
#include <QStringList>

namespace platform {

QString osName(); // "windows" | "osx" | "linux"
QString archName();

/// java / java.exe suitable for Minecraft 26.2 (25+)
QString findJava();

QString resolveJava(const QString &configured);

/// jar / jar.exe next to the given java binary
QString jarToolForJava(const QString &javaPath);

/// Open a folder in the system file manager
bool openDirectory(const QString &dir);

QStringList javaSearchCandidates();

} // namespace platform
