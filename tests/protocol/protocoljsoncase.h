// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QJsonObject>
#include <QString>

struct ProtocolJsonCase
{
    QString caseId;
    QJsonObject input;
    QJsonObject expected;
    QJsonObject metadata;
    QString xmlSha256;
};

struct ProtocolJsonValidationError
{
    QString category;
    QString message;
};

QString protocolJsonInputInterface(const QString &inputPath);

bool loadProtocolJsonCase(const QString &inputPath,
                          const QString &expectedPath,
                          const QString &metadataPath,
                          const QString &xmlPath,
                          ProtocolJsonCase &testCase,
                          ProtocolJsonValidationError &error);
