// Generic E2E test for gen-test-client generated programs.
// Compile with -DTL_CLIENT_PATH=... -DTL_SERVER_PATH=... -DTL_REQUEST="echo 1.5" -DTL_EXPECT="value=384"
#include <QFile>
#include <QProcess>
#include <QThread>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main() {
    QString sockPath = "/tmp/gen-test-e2e-sock";
    QFile::remove(sockPath);
    QFile::remove(sockPath + ".lock");

    QProcess server;
    server.start(QStringLiteral(TL_SERVER_PATH), {sockPath});
    if (!server.waitForStarted(3000)) {
        fprintf(stderr, "FAIL: server start: %s\n", qPrintable(server.errorString()));
        return 1;
    }
    for (int i = 0; i < 20 && !QFile::exists(sockPath); i++)
        QThread::msleep(100);
    if (!QFile::exists(sockPath)) {
        fprintf(stderr, "FAIL: socket not created\n");
        server.terminate(); return 1;
    }

    // Build client args
    QStringList args = {"--socket", sockPath};
    // Parse TL_REQUEST: "echo 1.5" → ["--request", "echo", "1.5"]
    QString reqStr = QStringLiteral(TL_REQUEST);
    args.append("--request");
    args.append(reqStr.split(' '));

    args.append("--roundtrip");
    args.append("--checkpoint");

    QProcess client;
    client.start(QStringLiteral(TL_CLIENT_PATH), args);
    if (!client.waitForFinished(5000)) {
        fprintf(stderr, "FAIL: client timeout: %s\n", qPrintable(client.readAllStandardError()));
        server.terminate(); return 1;
    }
    if (client.exitCode() != 0) {
        fprintf(stderr, "FAIL: client exit %d: %s\n", client.exitCode(),
                qPrintable(client.readAllStandardError()));
        server.terminate(); return 1;
    }

    QString out = client.readAllStandardOutput();
    if (!out.contains("EVENT ") || !out.contains(QStringLiteral(TL_EXPECT)) || !out.contains("CHECKPOINT")) {
        fprintf(stderr, "FAIL: missing expected '%s'\nGot: %s\n", TL_EXPECT, qPrintable(out));
        server.terminate(); return 1;
    }

    printf("PASS\n");
    server.terminate();
    server.waitForFinished(3000);
    QFile::remove(sockPath);
    QFile::remove(sockPath + ".lock");
    return 0;
}
