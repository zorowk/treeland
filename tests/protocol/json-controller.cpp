// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// JSON-driven protocol test controller.
// Compile per-protocol with:
//   -DTL_CLIENT_PATH="/path/to/client"
//   -DTL_CASE="/path/to/protocol.json"
//   -DTL_MODULE_HEADER="windowmanagementinterfacev1.h"
//   -DTL_MODULE_CLASS=WindowManagementInterfaceV1

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QThread>

#include <WServer>
#include <wsocket.h>
#include TL_MODULE_HEADER_STR

using namespace WAYLIB_SERVER_NAMESPACE;

#define STR(x) #x
#define STRINGIFY(x) STR(x)

static QList<QPair<QString, QJsonArray>> parseEvents(const QString &out) {
    QList<QPair<QString, QJsonArray>> events;
    for (const QString &line : out.split('\n')) {
        if (line.startsWith("EVENT ")) {
            QStringList parts = line.mid(6).split(' ');
            QString ename = parts.takeFirst();
            QJsonArray args;
            for (const QString &f : parts) {
                int eq = f.indexOf('='); if (eq <= 0) continue;
                QString val = f.mid(eq+1);
                bool ok; double d = val.toDouble(&ok);
                args.append(QJsonObject{{"value", ok ? QJsonValue(d) : QJsonValue(val)}});
            }
            events.append({ename, args});
        }
    }
    return events;
}

int main(int, char **) {
    QFile f(QString::fromUtf8(TL_CASE));
    if (!f.open(QIODevice::ReadOnly)) { fprintf(stderr, "FAIL: read\n"); return 1; }
    QJsonObject doc = QJsonDocument::fromJson(f.readAll()).object();
    QJsonArray tests = doc["tests"].toArray();

    QString sockPath = "/tmp/treeland-json-sock";
    QFile::remove(sockPath); QFile::remove(sockPath + ".lock");

    auto server = std::make_unique<WServer>();
    auto socket = std::make_unique<WSocket>(false);
#ifndef NO_MODULE_ATTACH
    server->attach<TL_MODULE_CLASS>(server.get());
#endif
    if (!socket->autoCreate("/tmp/treeland-json")) { fprintf(stderr, "FAIL: socket\n"); return 1; }
    server->addSocket(socket.get()); server->start();
    for (int i = 0; i < 20 && !QFile::exists(sockPath); i++) QThread::msleep(100);

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
        client.start(QString::fromUtf8(TL_CLIENT_PATH), args);
        if (!client.waitForFinished(5000)) { fprintf(stderr, "FAIL [%s]: timeout\n", qPrintable(name)); failed++; continue; }
        auto events = parseEvents(client.readAllStandardOutput());

        bool ok = events.size() == expEvents.size();
        for (int i = 0; i < events.size() && ok; i++) {
            QJsonObject ee = expEvents[i].toObject();
            if (events[i].first != ee["event"].toString()) ok = false;
            QJsonArray ea = ee["args"].toArray();
            if (events[i].second.size() != ea.size()) ok = false;
            for (int j = 0; j < ea.size() && ok; j++)
                if (ea[j].toObject()["value"].toDouble() != events[i].second[j].toObject()["value"].toDouble()) ok = false;
        }
        printf("%s [%s]\n", ok ? "PASS" : "FAIL", qPrintable(name));
        ok ? passed++ : failed++;
    }
    printf("%d/%d passed\n", passed, passed + failed);
    QFile::remove(sockPath); QFile::remove(sockPath + ".lock");
    return failed ? 1 : 0;
}
