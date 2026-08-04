// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <wayland-util.h>

// Arrays are byte sequences in JSON: { "bytes": [0, 255] }. The owning
// QByteArray outlives the borrowed wl_array used by the synchronous request.
bool protocolArrayFromJson(const QJsonValue &json, QByteArray &bytes, QString &error);
QJsonObject normalizedProtocolArray(const void *data, qsizetype size);
wl_array borrowedProtocolArray(QByteArray &bytes);
