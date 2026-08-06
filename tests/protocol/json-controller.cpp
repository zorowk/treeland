// JSON-driven protocol test controller.
// Starts headless treeland once per JSON, runs all test cases against it.
// Compile with: -DTL_CLIENT_PATH=/path/to/client -DTL_CASE=/path/to/case.json
// Environment: WLR_BACKENDS=headless (QPA is hardcoded in treeland via WServer::initializeQPA)
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QThread>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

static QList<QPair<QString, QJsonArray>> parseEvents(const QString &out) {
    QList<QPair<QString, QJsonArray>> events;
    for (const QString &line : out.split('\n')) {
        if (!line.startsWith("EVENT ")) continue;
        QStringList parts = line.mid(6).split(' ');
        QString ename = parts.takeFirst();
        QJsonArray args;
        for (const QString &f : parts) {
            int eq = f.indexOf('=');
            if (eq <= 0) continue;
            QString val = f.mid(eq + 1);
            bool ok; double d = val.toDouble(&ok);
            args.append(QJsonObject{{"value", ok ? QJsonValue(d) : QJsonValue(val)}});
        }
        events.append({ename, args});
    }
    return events;
}

static QString treelandBin() {
    return QFileInfo(QString::fromUtf8(TL_CLIENT_PATH)).dir().absolutePath()
           + "/../../../src/treeland";
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    QFile f(QString::fromUtf8(TL_CASE));
    if (!f.open(QIODevice::ReadOnly)) { fprintf(stderr, "FAIL: cannot read %s\n", TL_CASE); return 1; }
    QJsonArray tests = QJsonDocument::fromJson(f.readAll()).object()["tests"].toArray();
    if (tests.isEmpty()) { printf("0/0 passed (no tests)\n"); return 0; }

    // ---- Start headless treeland once ----
    char tmpdir[] = "/tmp/treeland-test-XXXXXX";
    if (!mkdtemp(tmpdir)) { perror("mkdtemp"); return 1; }
    QString sockPath = QString::fromUtf8(tmpdir) + "/wayland-0";

    QProcess compositor;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WLR_BACKENDS", "headless");
    env.insert("XDG_RUNTIME_DIR", QString::fromUtf8(tmpdir));
    env.insert("WAYLAND_DISPLAY", "wayland-0");
    compositor.setProcessEnvironment(env);
    compositor.start(treelandBin());

    if (!compositor.waitForStarted(5000)) {
        fprintf(stderr, "FAIL: treeland start: %s\n", qPrintable(compositor.errorString()));
        compositor.kill(); rmdir(tmpdir); return 1;
    }
    for (int i = 0; i < 150 && !QFile::exists(sockPath); i++)
        QThread::msleep(100);
    if (!QFile::exists(sockPath)) {
        fprintf(stderr, "FAIL: socket not created\n");
        compositor.kill(); compositor.waitForFinished(3000);
        rmdir(tmpdir); return 1;
    }

    // ---- Run all test cases against the same compositor ----
    int passed = 0, failed = 0;
    for (const QJsonValue &tv : tests) {
        QJsonObject test = tv.toObject();
        QString name = test["name"].toString();
        QJsonArray steps = test["steps"].toArray();
        QJsonArray expEvents = test["expected"].toObject()["events"].toArray();

        QStringList args; args << "--socket" << sockPath;
        for (const QJsonValue &sv : steps) {
            QJsonObject s = sv.toObject();
            if (s["type"] == "request") {
                args << "--request" << s["name"].toString();
                for (const QJsonValue &a : s["args"].toArray())
                    args << (a.isString() ? a.toString() : QString::number(a.toDouble()));
            } else if (s["type"] == "roundtrip") {
                args << "--roundtrip";
            } else if (s["type"] == "checkpoint") {
                break;
            }
        }

        QProcess client;
        client.setProcessEnvironment(env);
        client.start(QString::fromUtf8(TL_CLIENT_PATH), args);
        if (!client.waitForFinished(5000)) {
            fprintf(stderr, "FAIL [%s]: timeout\n", qPrintable(name));
            client.kill(); failed++; continue;
        }
        if (client.exitCode() != 0) {
            fprintf(stderr, "FAIL [%s]: exit=%d stderr=%s\n", qPrintable(name),
                    client.exitCode(), qPrintable(client.readAllStandardError()));
            failed++; continue;
        }
        auto events = parseEvents(client.readAllStandardOutput());
        bool ok = events.size() == expEvents.size();
        for (int i = 0; i < events.size() && ok; i++) {
            QJsonObject ee = expEvents[i].toObject();
            if (events[i].first != ee["event"].toString()) ok = false;
            QJsonArray ea = ee["args"].toArray();
            if (events[i].second.size() != ea.size()) ok = false;
            for (int j = 0; j < ea.size() && ok; j++)
                if (ea[j].toObject()["value"].toDouble()
                    != events[i].second[j].toObject()["value"].toDouble())
                    ok = false;
        }
        printf("%s [%s]\n", ok ? "PASS" : "FAIL", qPrintable(name));
        ok ? passed++ : failed++;
    }
    printf("%d/%d passed\n", passed, passed + failed);

    compositor.terminate();
    if (!compositor.waitForFinished(5000)) { compositor.kill(); compositor.waitForFinished(3000); }
    rmdir(tmpdir);
    return failed ? 1 : 0;
}
