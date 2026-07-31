// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "clientruntime.h"

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVector>

#include <functional>

class ClientWorker;
class QThread;

struct WireScenarioResult
{
    bool passed = false;
    QString failureCategory;
    QString failureMessage;
    QMap<QString, bool> checks;
    qint64 elapsedMs = 0;
    qsizetype clientCountBefore = 0;
    qsizetype clientCountAfter = 0;
    qsizetype resourceCountBefore = 0;
    qsizetype resourceCountAfter = 0;
    quint32 advertisedVersion = 0;
    quint32 boundVersion = 0;
    QVector<quint32> initialEvents;
    QVector<quint32> requestEvents;
    quint32 requestedDesktopState = 0;
    quint32 expectedRequestEvent = 0;
    int desktopStateChangedCount = 0;
    quint32 serverDesktopState = 0;
    bool localProxyAliveBeforeDestroy = false;
    bool localProxyAliveAfterDestroy = false;
    bool protocolDestructorSent = false;
    qsizetype resourceCountAfterDestroy = 0;
    int displayError = 0;
    bool protocolErrorOccurred = false;
    QString protocolErrorInterface;
    quint32 protocolErrorObjectId = 0;
    quint32 protocolErrorCode = 0;

    QJsonObject toJson() const;
};

class WireScenario
{
public:
    WireScenario();
    ~WireScenario();

    WireScenarioResult run(quint32 expectedRequestEvent);

private:
    ClientStepResult waitForStep(const QString &step,
                                 const std::function<void()> &start,
                                 int timeoutMs = 3000);
    bool waitForCondition(const std::function<bool()> &condition,
                          QObject *notifier,
                          const char *signal,
                          int timeoutMs = 3000);
    void recordCheck(WireScenarioResult &result,
                     const QString &name,
                     bool passed,
                     const QString &category,
                     const QString &message);

    QThread *m_clientThread = nullptr;
    ClientWorker *m_worker = nullptr;
};

bool writeSummary(const WireScenarioResult &result, const QString &reportDirectory);
