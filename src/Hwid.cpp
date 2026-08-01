#include "Hwid.h"

#include "Paths.h"
#include "Platform.h"

#include <QDir>
#include <QFile>
#include <QProcess>

namespace hwid {

static const char kHwidCliJava[] = R"java(
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
public class HwidCli {
    public static void main(String[] args) { System.out.println(compute()); }
    static String compute() {
        StringBuilder sb = new StringBuilder();
        sb.append(System.getProperty("os.name", ""));
        sb.append('|').append(System.getProperty("os.arch", ""));
        sb.append('|').append(System.getProperty("user.name", ""));
        String proc = System.getenv("PROCESSOR_IDENTIFIER");
        if (proc != null) sb.append('|').append(proc);
        return sha256Hex(sb.toString()).substring(0, 32);
    }
    static String sha256Hex(String input) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] digest = md.digest(input.getBytes(StandardCharsets.UTF_8));
            StringBuilder out = new StringBuilder();
            for (byte b : digest) out.append(String.format("%02x", b));
            return out.toString();
        } catch (Exception e) {
            return Integer.toHexString(input.hashCode());
        }
    }
}
)java";

QString compute(const QString &javaPathIn, QString *errorOut) {
    const QString java = platform::resolveJava(javaPathIn);
    const QString hwidDir = paths::cacheDir() + QStringLiteral("/hwid");
    QDir().mkpath(hwidDir);
    const QString srcPath = hwidDir + QStringLiteral("/HwidCli.java");
    const QString classFile = hwidDir + QStringLiteral("/HwidCli.class");

    QFile src(srcPath);
    if (src.open(QIODevice::WriteOnly | QIODevice::Truncate))
        src.write(QByteArray(kHwidCliJava));

    if (!QFile::exists(classFile) || src.size() > 0) {
        QProcess javac;
        javac.setProgram(java);
        javac.setArguments({QStringLiteral("-d"), hwidDir, srcPath});
        javac.start();
        if (!javac.waitForFinished(60000) || javac.exitCode() != 0) {
            if (errorOut)
                *errorOut = QStringLiteral("Could not compile HwidCli (need JDK?)");
            return {};
        }
    }

    QProcess run;
    run.setProgram(java);
    run.setArguments({QStringLiteral("-cp"), hwidDir, QStringLiteral("HwidCli")});
    run.start();
    if (!run.waitForFinished(30000) || run.exitCode() != 0) {
        if (errorOut)
            *errorOut = QStringLiteral("HwidCli failed");
        return {};
    }
    const QString out = QString::fromUtf8(run.readAllStandardOutput()).trimmed();
    if (out.length() < 16) {
        if (errorOut)
            *errorOut = QStringLiteral("Invalid HWID output");
        return {};
    }
    return out;
}

} // namespace hwid
