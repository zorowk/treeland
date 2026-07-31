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

struct HelperScenarioResult
{
    bool passed = false;
    QString failureCategory;
    QString failureMessage;
    QMap<QString, bool> checks;
    qint64 elapsedMs = 0;
    quint32 advertisedVersion = 0;
    quint32 boundVersion = 0;
    QVector<quint32> initialEvents;
    QVector<quint32> requestEvents;
    quint32 protocolDesktopState = 0;
    quint32 helperDesktopState = 0;
    int desktopStateChangedCount = 0;
    qsizetype clientCountAfter = 0;
    qsizetype resourceCountAfter = 0;
    bool protocolDestructorSent = false;
    bool localProxyAliveAfterDestroy = false;
    bool helperDestroyed = false;
    bool serverStopped = false;
    bool socketClosed = false;
    bool environmentDestroyed = false;
    bool runtimeDirectoryRemoved = false;
    bool clientThreadStopped = false;
    int displayError = 0;
    bool protocolErrorOccurred = false;

    QJsonObject toJson() const;
};

class HelperScenario
{
public:
    HelperScenario();
    ~HelperScenario();

    HelperScenarioResult run(quint32 expectedHelperState);

private:
    ClientStepResult waitForStep(const QString &step,
                                 const std::function<void()> &start,
                                 int timeoutMs = 3000);
    bool waitForCondition(const std::function<bool()> &condition,
                          QObject *notifier,
                          const char *signal,
                          int timeoutMs = 3000);
    void recordCheck(HelperScenarioResult &result,
                     const QString &name,
                     bool passed,
                     const QString &category,
                     const QString &message);
    bool stopClientThread();

    QThread *m_clientThread = nullptr;
    ClientWorker *m_worker = nullptr;
};

bool writeHelperSummary(const HelperScenarioResult &result, const QString &reportDirectory);
