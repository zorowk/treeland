// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// JSON-driven protocol test controller.
// Starts WServer + attach<module>, then spawns gen-test-client per test case.
// Uses QApplication + nested QEventLoop so server processes events during client wait.
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QEventLoop>
#include <QTimer>
#include <QThread>

#include <WServer>
#include <wsocket.h>
#ifndef NO_MODULE_ATTACH
#include TL_MODULE_HEADER_STR
#endif

#include <unistd.h>
#include <cstdio>

using namespace WAYLIB_SERVER_NAMESPACE;

static QList<QPair<QString, QJsonArray>> parseEvents(const QString &out) {
    QList<QPair<QString, QJsonArray>> events;
    for (const QString &line : out.split('\n')) {
        if (line.startsWith("EVENT ")) {
            QStringList parts = line.mid(6).split(' ');
            QString ename = parts.takeFirst();
            QJsonArray args;
            for (const QString &f : parts) {
                int eq = f.indexOf('='); if (eq <= 0) continue;
                QString val = f.mid(eq + 1);
                bool ok; double d = val.toDouble(&ok);
                args.append(QJsonObject{{"value", ok ? QJsonValue(d) : QJsonValue(val)}});
            }
            events.append({ename, args});
        }
    }
    return events;
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    QFile f(QString::fromUtf8(TL_CASE));
    if (!f.open(QIODevice::ReadOnly)) { fprintf(stderr, "FAIL: read %s\n", TL_CASE); return 1; }
    QJsonObject doc = QJsonDocument::fromJson(f.readAll()).object();
    QJsonArray tests = doc["tests"].toArray();

    char tmpdir[] = "/tmp/treeland-test-XXXXXX";
    if (!mkdtemp(tmpdir)) { perror("mkdtemp"); return 1; }
    QString baseDir = QString::fromUtf8(tmpdir);
    QString sockPath = baseDir + "/wayland-0";

    auto server = std::make_unique<WServer>();
    auto sock = std::make_unique<WSocket>(false);
#ifndef NO_MODULE_ATTACH
    server->attach<TL_MODULE_CLASS>(server.get());
#endif
    if (!sock->autoCreate(baseDir.toUtf8().constData())) {
        fprintf(stderr, "FAIL: socket create\n"); return 1;
    }
    server->addSocket(sock.get());
    server->start();

    // Wait for socket file to appear
    {
        QEventLoop waitLoop;
        QTimer waitTimer;
        waitTimer.setSingleShot(true);
        int count = 0;
        QObject::connect(&waitTimer, &QTimer::timeout, [&] {
            if (QFile::exists(sockPath) || ++count > 40) waitLoop.quit();
            else waitTimer.start(50);
        });
        waitTimer.start(50);
        waitLoop.exec();
    }
    if (!QFile::exists(sockPath)) {
        fprintf(stderr, "FAIL: socket not created\n"); return 1;
    }

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
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&client, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(5000);
        client.start(QString::fromUtf8(TL_CLIENT_PATH), args);
        loop.exec();

        if (!timeout.isActive()) {
            fprintf(stderr, "FAIL [%s]: timeout\n", qPrintable(name));
            client.kill(); failed++; continue;
        }
        timeout.stop();
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
                if (ea[j].toObject()["value"].toDouble() != events[i].second[j].toObject()["value"].toDouble())
                    ok = false;
        }
        printf("%s [%s]\n", ok ? "PASS" : "FAIL", qPrintable(name));
        ok ? passed++ : failed++;
    }
    printf("%d/%d passed\n", passed, passed + failed);
    sock.reset(); server.reset();
    rmdir(tmpdir);
    return failed ? 1 : 0;
}
