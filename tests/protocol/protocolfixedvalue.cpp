// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocolfixedvalue.h"

#include <cmath>
#include <limits>

bool protocolFixedFromJson(const QJsonValue &json, int32_t &raw, QString &error)
{
    if (!json.isObject()) {
        error = QStringLiteral("fixed must use the exact {\"raw\": int32} representation");
        return false;
    }

    const QJsonObject object = json.toObject();
    if (object.size() != 1 || !object.contains(QStringLiteral("raw"))) {
        error = QStringLiteral("fixed must contain only a raw member");
        return false;
    }

    const QJsonValue value = object.value(QStringLiteral("raw"));
    if (!value.isDouble()) {
        error = QStringLiteral("fixed raw must be an integer");
        return false;
    }

    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < std::numeric_limits<int32_t>::min()
        || number > std::numeric_limits<int32_t>::max()) {
        error = QStringLiteral("fixed raw is outside the signed 32-bit integer range");
        return false;
    }

    raw = static_cast<int32_t>(number);
    error.clear();
    return true;
}

QJsonObject normalizedProtocolFixed(int32_t raw)
{
    return QJsonObject{ { QStringLiteral("raw"), static_cast<qint64>(raw) } };
}
