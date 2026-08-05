// Minimal E2E test for gen-test-client generated program
#include <QFile>
#include <QProcess>
#include <cstdlib>
#include <QThread>
#include <cstdio>

int main() {
    QString sockPath = "/tmp/gen-test-e2e-sock";
    QFile::remove(sockPath);
    QFile::remove(sockPath + ".lock");

    QProcess server;
    server.start(QStringLiteral(TL_ECHO_SERVER_PATH), {sockPath});
    if (!server.waitForStarted(3000)) {
        fprintf(stderr, "FAIL: server start: %s\n", qPrintable(server.errorString()));
        return 1;
    }
    // Wait for socket
    for (int i = 0; i < 20 && !QFile::exists(sockPath); i++)
        QThread::msleep(100);
    if (!QFile::exists(sockPath)) {
        fprintf(stderr, "FAIL: socket not created\n");
        server.terminate(); return 1;
    }

    QProcess client;
    client.start(QStringLiteral(TL_GENERATED_CLIENT_PATH), {
        "--socket", sockPath,
        "--request", "echo", "42", "-7", "hello",
        "--roundtrip", "--checkpoint"
    });
    if (!client.waitForFinished(5000)) {
        fprintf(stderr, "FAIL: client timeout: %s\n", qPrintable(client.readAllStandardError()));
        server.terminate(); return 1;
    }
    if (client.exitCode() != 0) {
        fprintf(stderr, "FAIL: client exit %d\n", client.exitCode());
        server.terminate(); return 1;
    }

    QString out = client.readAllStandardOutput();
    if (!out.contains("EVENT reply") || !out.contains("id=42") || !out.contains("CHECKPOINT")) {
        fprintf(stderr, "FAIL: missing expected output\nGot: %s\n", qPrintable(out));
        server.terminate(); return 1;
    }

    printf("PASS\n");
    server.terminate();
    server.waitForFinished(3000);
    QFile::remove(sockPath);
    QFile::remove(sockPath + ".lock");
    return 0;
}
