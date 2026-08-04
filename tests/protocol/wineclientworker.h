// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QJsonArray>
#include <QObject>
#include <QString>

#include <memory>

struct wl_buffer;
struct wl_compositor;
struct wl_display;
struct wl_registry;
struct wl_shm;
struct wl_surface;
struct xdg_surface;
struct xdg_toplevel;
struct xdg_wm_base;
struct treeland_wine_window_control_v1;
struct treeland_wine_window_manager_v1;
class QTemporaryFile;

struct WineClientStepResult
{
    QString step;
    bool ok = false;
    QString errorCategory;
    QString errorMessage;
    QJsonArray events;
    bool initialConfigureReceived = false;
    bool initialConfigureAcknowledged = false;
    bool bufferCommitted = false;
    bool controlCreated = false;
    bool protocolDestructorSent = false;
    int displayError = 0;
    bool protocolErrorOccurred = false;
    QString protocolErrorInterface;
    quint32 protocolErrorObjectId = 0;
    quint32 protocolErrorCode = 0;
    QString protocolErrorObject;
    bool localDisplayAlive = false;
    bool localManagerProxyAlive = false;
    bool localControlProxyAlive = false;
    bool localSurfaceProxyAlive = false;
    qsizetype localProxyCount = 0;
    QString destroyMode = QStringLiteral("none");
    QString disconnectMode = QStringLiteral("none");
    bool abruptTransportClosed = false;
};

Q_DECLARE_METATYPE(WineClientStepResult)

class WineClientWorker : public QObject
{
    Q_OBJECT
public:
    explicit WineClientWorker(QObject *parent = nullptr);
    ~WineClientWorker() override;

public Q_SLOTS:
    void createMappedToplevel(const QString &socketPath,
                              const QString &appId,
                              const QSize &size);
    void createWindowControl();
    void setPosition(qint32 x, qint32 y, quint32 serial);
    void sendPosition(qint32 x, qint32 y, quint32 serial);
    void setZOrder(quint32 operation, quint32 siblingId);
    void clientRoundtrip();
    void destroyObjects();
    void destroyControlProxyOnly();
    void disconnectClient();
    void disconnectAbruptly();
    void observeServerShutdown();

Q_SIGNALS:
    void stepFinished(const WineClientStepResult &result);

private:
    WineClientStepResult result(const QString &step,
                                bool ok,
                                const QString &category = {},
                                const QString &message = {}) const;
    bool roundtrip(WineClientStepResult &result);
    bool createBuffer(const QSize &size, QString &error);
    void destroyLocalProxies();
    void cleanup();

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_compositor *m_compositor = nullptr;
    wl_shm *m_shm = nullptr;
    xdg_wm_base *m_xdgWmBase = nullptr;
    treeland_wine_window_manager_v1 *m_manager = nullptr;
    wl_surface *m_surface = nullptr;
    xdg_surface *m_xdgSurface = nullptr;
    xdg_toplevel *m_toplevel = nullptr;
    wl_buffer *m_buffer = nullptr;
    treeland_wine_window_control_v1 *m_control = nullptr;
    std::unique_ptr<QTemporaryFile> m_shmFile;
    QJsonArray m_events;
    bool m_initialConfigureReceived = false;
    bool m_initialConfigureAcknowledged = false;
    bool m_bufferCommitted = false;
    bool m_controlCreated = false;
    bool m_protocolDestructorSent = false;
    QString m_destroyMode = QStringLiteral("none");
    QString m_disconnectMode = QStringLiteral("none");
    bool m_abruptTransportClosed = false;
};
