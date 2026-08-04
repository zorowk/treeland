// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocolarrayvalue.h"

#include <QJsonArray>

#include <cmath>
#include <cstdint>
#include <utility>

bool protocolArrayFromJson(const QJsonValue &json, QByteArray &bytes, QString &error)
{
    if (!json.isObject()) {
        error = QStringLiteral("array must use the exact {\"bytes\": [uint8...]} representation");
        return false;
    }

    const QJsonObject object = json.toObject();
    if (object.size() != 1 || !object.value(QStringLiteral("bytes")).isArray()) {
        error = QStringLiteral("array must contain only a bytes array");
        return false;
    }

    QByteArray result;
    const QJsonArray values = object.value(QStringLiteral("bytes")).toArray();
    result.reserve(values.size());
    for (qsizetype i = 0; i < values.size(); ++i) {
        const QJsonValue value = values.at(i);
        const double number = value.toDouble(-1);
        if (!value.isDouble() || !std::isfinite(number) || std::floor(number) != number
            || number < 0 || number > UINT8_MAX) {
            error = QStringLiteral("array byte %1 must be an integer in [0, 255]").arg(i);
            return false;
        }
        result.append(static_cast<char>(static_cast<uint8_t>(number)));
    }

    bytes = std::move(result);
    error.clear();
    return true;
}

QJsonObject normalizedProtocolArray(const void *data, qsizetype size)
{
    QJsonArray bytes;
    const auto *begin = static_cast<const uint8_t *>(data);
    for (qsizetype i = 0; i < size; ++i)
        bytes.append(begin[i]);
    return QJsonObject{ { QStringLiteral("bytes"), bytes } };
}

wl_array borrowedProtocolArray(QByteArray &bytes)
{
    return wl_array{
        .size = static_cast<size_t>(bytes.size()),
        .alloc = static_cast<size_t>(bytes.size()),
        .data = bytes.data(),
    };
}
