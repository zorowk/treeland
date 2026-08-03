// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {
QJsonObject readObject(const QString &path, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("cannot open %1: %2").arg(path, file.errorString());
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("cannot parse %1: %2").arg(path, parseError.errorString());
        return {};
    }
    return document.object();
}

QJsonArray normalizeWireEvents(const QJsonArray &events)
{
    QJsonArray normalized;
    for (const QJsonValue &value : events) {
        const QJsonObject event = value.toObject();
        normalized.append(QJsonObject{
            { QStringLiteral("object"), QStringLiteral("window_management") },
            { QStringLiteral("event"), event.value(QStringLiteral("event")) },
            { QStringLiteral("args"),
              QJsonArray{ event.value(QStringLiteral("state")).toInteger() } },
        });
    }
    return normalized;
}

QJsonObject wireContract(const QJsonObject &wire)
{
    const QJsonObject checkpoints = wire.value(QStringLiteral("checkpoints")).toObject();
    const QJsonObject serverState = wire.value(QStringLiteral("server_state")).toObject();
    const QJsonObject lifecycle = wire.value(QStringLiteral("lifecycle")).toObject();
    const QJsonObject metrics = wire.value(QStringLiteral("metrics")).toObject();
    return QJsonObject{
        { QStringLiteral("case"), wire.value(QStringLiteral("case")) },
        { QStringLiteral("request"), wire.value(QStringLiteral("request")) },
        { QStringLiteral("checkpoints"),
          QJsonObject{
              { QStringLiteral("initial-state"),
                QJsonObject{
                    { QStringLiteral("client_events"),
                      normalizeWireEvents(checkpoints.value(QStringLiteral("initial-state"))
                                              .toObject()
                                              .value(QStringLiteral("client_events"))
                                              .toArray()) },
                    { QStringLiteral("server_state"),
                      QJsonObject{
                          { QStringLiteral("desktop_state"), 0 },
                          { QStringLiteral("desktop_state_changed_count"), 0 },
                      } },
                } },
              { QStringLiteral("request-state"),
                QJsonObject{
                    { QStringLiteral("client_events"),
                      normalizeWireEvents(checkpoints.value(QStringLiteral("request-state"))
                                              .toObject()
                                              .value(QStringLiteral("client_events"))
                                              .toArray()) },
                    { QStringLiteral("server_state"), serverState },
                } },
          } },
        { QStringLiteral("lifecycle"),
          QJsonObject{
              { QStringLiteral("client_count_after"),
                metrics.value(QStringLiteral("wayland_client_count_after")) },
              { QStringLiteral("resource_count_after"),
                metrics.value(QStringLiteral("window_management_resource_count_after")) },
              { QStringLiteral("protocol_destructor_sent"),
                lifecycle.value(QStringLiteral("protocol_destructor_sent")) },
              { QStringLiteral("local_proxy_alive_after_destroy"),
                lifecycle.value(QStringLiteral("local_proxy_alive_after_destroy")) },
          } },
    };
}

QJsonObject jsonContract(const QJsonObject &actual)
{
    QJsonObject checkpoints;
    const QJsonObject actualCheckpoints = actual.value(QStringLiteral("checkpoints")).toObject();
    for (const QString &name : { QStringLiteral("initial-state"),
                                QStringLiteral("request-state") }) {
        const QJsonObject checkpoint = actualCheckpoints.value(name).toObject();
        checkpoints.insert(name,
                           QJsonObject{
                               { QStringLiteral("client_events"),
                                 checkpoint.value(QStringLiteral("client_events")) },
                               { QStringLiteral("server_state"),
                                 checkpoint.value(QStringLiteral("server_state")) },
                           });
    }
    const QJsonObject lifecycle = actual.value(QStringLiteral("lifecycle")).toObject();
    return QJsonObject{
        { QStringLiteral("case"), actual.value(QStringLiteral("case")) },
        { QStringLiteral("request"), actual.value(QStringLiteral("request")) },
        { QStringLiteral("checkpoints"), checkpoints },
        { QStringLiteral("lifecycle"),
          QJsonObject{
              { QStringLiteral("client_count_after"),
                lifecycle.value(QStringLiteral("client_count_after")) },
              { QStringLiteral("resource_count_after"),
                lifecycle.value(QStringLiteral("resource_count_after")) },
              { QStringLiteral("protocol_destructor_sent"),
                lifecycle.value(QStringLiteral("protocol_destructor_sent")) },
              { QStringLiteral("local_proxy_alive_after_destroy"),
                lifecycle.value(QStringLiteral("local_proxy_alive_after_destroy")) },
          } },
    };
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 3) {
        QTextStream(stderr) << "usage: protocol-json-contract-diff WIRE JSON_ACTUAL\n";
        return 2;
    }
    QString error;
    const QJsonObject wire = readObject(application.arguments().at(1), error);
    if (!error.isEmpty()) {
        QTextStream(stderr) << error << '\n';
        return 2;
    }
    const QJsonObject actual = readObject(application.arguments().at(2), error);
    if (!error.isEmpty()) {
        QTextStream(stderr) << error << '\n';
        return 2;
    }
    const QJsonObject left = wireContract(wire);
    const QJsonObject right = jsonContract(actual);
    if (left != right) {
        QTextStream(stderr) << "hardcoded/JSON contract mismatch\nhardcoded:\n"
                            << QJsonDocument(left).toJson(QJsonDocument::Indented)
                            << "JSON:\n"
                            << QJsonDocument(right).toJson(QJsonDocument::Indented);
        return 1;
    }
    QTextStream(stdout) << "hardcoded and JSON contracts match\n";
    return 0;
}
