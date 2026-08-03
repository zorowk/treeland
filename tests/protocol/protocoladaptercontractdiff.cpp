// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QCoreApplication>
#include <QFile>
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

QJsonObject lifecycleContract(const QJsonObject &lifecycle)
{
    QJsonObject result;
    const QStringList keys{
        QStringLiteral("local_proxy_alive_before_destroy"),
        QStringLiteral("protocol_destructor_sent"),
        QStringLiteral("local_proxy_alive_after_destroy"),
        QStringLiteral("remote_resource_count_after_destroy"),
        QStringLiteral("resource_count_after"),
        QStringLiteral("client_count_after"),
    };
    for (const QString &key : keys) {
        if (lifecycle.contains(key))
            result.insert(key, lifecycle.value(key));
    }
    return result;
}

QJsonObject normalize(const QJsonObject &summary)
{
    QJsonObject result{
        { QStringLiteral("case"), summary.value(QStringLiteral("case")) },
        { QStringLiteral("request"), summary.value(QStringLiteral("request")) },
        { QStringLiteral("checkpoints"), summary.value(QStringLiteral("checkpoints")) },
        { QStringLiteral("validation_category"),
          summary.contains(QStringLiteral("failure_category"))
              ? summary.value(QStringLiteral("failure_category"))
              : QJsonValue(QStringLiteral("none")) },
        { QStringLiteral("lifecycle"),
          lifecycleContract(summary.value(QStringLiteral("lifecycle")).toObject()) },
        { QStringLiteral("connection"), summary.value(QStringLiteral("connection")) },
    };
    return result;
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 3) {
        QTextStream(stderr) << "usage: protocol-adapter-contract-diff LEFT RIGHT\n";
        return 2;
    }

    QString error;
    const QJsonObject left = readObject(application.arguments().at(1), error);
    if (!error.isEmpty()) {
        QTextStream(stderr) << error << '\n';
        return 2;
    }
    const QJsonObject right = readObject(application.arguments().at(2), error);
    if (!error.isEmpty()) {
        QTextStream(stderr) << error << '\n';
        return 2;
    }

    const QJsonObject normalizedLeft = normalize(left);
    const QJsonObject normalizedRight = normalize(right);
    if (normalizedLeft != normalizedRight) {
        QTextStream(stderr) << "adapter contract mismatch\nleft:\n"
                            << QJsonDocument(normalizedLeft).toJson(QJsonDocument::Indented)
                            << "right:\n"
                            << QJsonDocument(normalizedRight).toJson(QJsonDocument::Indented);
        return 1;
    }

    QTextStream(stdout) << "adapter contracts match\n";
    return 0;
}
