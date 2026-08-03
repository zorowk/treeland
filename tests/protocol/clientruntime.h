// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

struct wl_display;
struct wl_registry;
struct tl_window_management_adapter;

struct ClientStepResult
{
    QString step;
    bool ok = false;
    QString errorCategory;
    QString errorMessage;
    QVector<quint32> events;
    quint32 advertisedVersion = 0;
    quint32 boundVersion = 0;
    bool displayConnected = false;
    bool localProxyAlive = false;
    bool protocolDestructorSent = false;
    int displayError = 0;
    bool protocolErrorOccurred = false;
    QString protocolErrorInterface;
    quint32 protocolErrorObjectId = 0;
    quint32 protocolErrorCode = 0;
};

Q_DECLARE_METATYPE(ClientStepResult)

class ClientWorker : public QObject
{
    Q_OBJECT

public:
    explicit ClientWorker(QObject *parent = nullptr);
    ~ClientWorker() override;

public Q_SLOTS:
    void connectAndBind(const QString &socketPath);
    void connectAndBindVersion(const QString &socketPath, quint32 requestedVersion);
    void setDesktop(quint32 state);
    void sendSetDesktop(quint32 state);
    void clientRoundtrip();
    void destroyProtocol();
    void sendDestroyProtocol();
    void disconnectClient();

Q_SIGNALS:
    void stepFinished(const ClientStepResult &result);

private:
    ClientStepResult result(const QString &step, bool ok,
                            const QString &category = {},
                            const QString &message = {}) const;
    bool roundtrip(ClientStepResult &result);
    void cleanup();

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    tl_window_management_adapter *m_adapter = nullptr;
};
