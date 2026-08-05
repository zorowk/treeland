// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocolfdvalue.h"

#include <unistd.h>

bool protocolFdFromJson(const QJsonValue &json, int &fd, QString &error)
{
    if (json.isNull()) {
        fd = -1;
        error.clear();
        return true;
    }

    if (!json.isObject()) {
        error = QStringLiteral("fd must be null or {\"fd\": true}");
        return false;
    }

    const QJsonObject object = json.toObject();
    if (object.size() != 1 || !object.contains(QStringLiteral("fd"))) {
        error = QStringLiteral("fd must contain only an fd member");
        return false;
    }

    const QJsonValue value = object.value(QStringLiteral("fd"));
    if (!value.isBool() || !value.toBool()) {
        error = QStringLiteral("fd value must be true");
        return false;
    }

    int pipefd[2];
    if (::pipe(pipefd) != 0) {
        error = QStringLiteral("failed to allocate fd");
        return false;
    }
    ::close(pipefd[1]);
    fd = pipefd[0];
    error.clear();
    return true;
}

QJsonObject normalizedProtocolFd(int fd)
{
    if (fd >= 0)
        return QJsonObject{ { QStringLiteral("fd"), QStringLiteral("valid") } };
    return {};
}
