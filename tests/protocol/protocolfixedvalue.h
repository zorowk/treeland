// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cstdint>

// Wayland fixed values use an exact signed 24.8 raw representation in JSON:
// { "raw": 384 } represents 1.5. The scalar is copied by value and owns no storage.
bool protocolFixedFromJson(const QJsonValue &json, int32_t &raw, QString &error);
QJsonObject normalizedProtocolFixed(int32_t raw);
