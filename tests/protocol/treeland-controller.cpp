// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Treeland protocol test controller: starts headless treeland, runs gen-test-client,
// checks stdout for expected output.
//
// Compile with:
//   -DTL_CLIENT_PATH="/path/to/generated-client"
//   -DTL_CLIENT_ARGS="--request set_desktop 1 --roundtrip --checkpoint"
//   -DTL_EXPECT="EVENT show_desktop state=1"

#include <QFile>
#include <QProcess>
#include <QThread>
#include <cstdio>
#include <cstdlib>

#include <WServer>
#include <WSocket>
#include <windowmanagementinterfacev1.h>

using namespace WAYLIB_SERVER_NAMESPACE;

int main() {
    QString sockPath = "/tmp/treeland-test-sock";
    QFile::remove(sockPath);
    QFile::remove(sockPath + ".lock");

    // Start headless treeland
    auto server = std::make_unique<WServer>();
    auto socket = std::make_unique<WSocket>(false);
    auto *wm = server->attach<WindowManagementInterfaceV1>(server.get());

    if (!socket->autoCreate("/tmp/treeland-test")) {
        fprintf(stderr, "FAIL: socket create\n");
        return 1;
    }
    server->addSocket(socket.get());
    server->start();

    // Wait for socket
    for (int i = 0; i < 20 && !QFile::exists(sockPath); i++)
        QThread::msleep(100);
    if (!QFile::exists(sockPath)) {
        fprintf(stderr, "FAIL: socket not ready\n");
        return 1;
    }

    // Run generated client
    QProcess client;
    QStringList args;
    args << "--socket" << sockPath;
    // Parse TL_CLIENT_ARGS: "--request set_desktop 1 --roundtrip --checkpoint"
    for (const QString &a : QString::fromUtf8(TL_CLIENT_ARGS).split(' '))
        if (!a.isEmpty()) args << a;

    client.start(QString::fromUtf8(TL_CLIENT_PATH), args);
    if (!client.waitForFinished(5000)) {
        fprintf(stderr, "FAIL: client timeout: %s\n", qPrintable(client.readAllStandardError()));
        return 1;
    }
    if (client.exitCode() != 0) {
        fprintf(stderr, "FAIL: client exit %d: %s\n", client.exitCode(),
                qPrintable(client.readAllStandardError()));
        return 1;
    }

    QString out = client.readAllStandardOutput();
    if (!out.contains(QString::fromUtf8(TL_EXPECT)) || !out.contains("CHECKPOINT")) {
        fprintf(stderr, "FAIL: expected '%s'\nGot: %s\n", TL_EXPECT, qPrintable(out));
        return 1;
    }

    printf("PASS\n");
    QFile::remove(sockPath);
    QFile::remove(sockPath + ".lock");
    return 0;
}
