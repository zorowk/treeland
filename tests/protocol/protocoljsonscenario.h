// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "protocoljsoncase.h"

#include <QJsonObject>
#include <QMap>
#include <QString>

struct ProtocolJsonRunResult
{
    bool passed = false;
    QString failureCategory;
    QString failureMessage;
    QString failureCheckpoint;
    QJsonObject expectedDifference;
    QJsonObject actualDifference;
    QMap<QString, bool> checks;
    QJsonObject actual;
    qint64 elapsedMs = 0;

    QJsonObject summary() const;
};

ProtocolJsonRunResult runProtocolJsonCase(const ProtocolJsonCase &testCase);
ProtocolJsonRunResult runGenericProtocolJsonCase(const ProtocolJsonCase &testCase);
ProtocolJsonRunResult validationFailureResult(const QString &caseId,
                                              const ProtocolJsonValidationError &error);
bool writeProtocolJsonArtifacts(const ProtocolJsonRunResult &result,
                                const QString &actualPath,
                                const QString &reportDirectory);
