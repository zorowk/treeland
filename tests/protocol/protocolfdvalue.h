// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

// Wayland fd values use { "fd": true } in JSON input; normalized actual uses
// { "fd": "valid" } so that raw fd numbers are never serialized as stable
// contract data. The caller is responsible for closing the fd returned by
// protocolFdFromJson after the synchronous request completes.
bool protocolFdFromJson(const QJsonValue &json, int &fd, QString &error);
QJsonObject normalizedProtocolFd(int fd);
