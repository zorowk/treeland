// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wineclientworker.h"

#include "wayland-treeland-wine-window-management-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include <wayland-client.h>

#include <QTemporaryFile>
#include <QJsonObject>
#include <QSize>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/mman.h>
#include <sys/socket.h>

WineClientWorker::WineClientWorker(QObject *parent)
    : QObject(parent)
{
}

WineClientWorker::~WineClientWorker()
{
    cleanup();
}

WineClientStepResult WineClientWorker::result(const QString &step,
                                              bool ok,
                                              const QString &category,
                                              const QString &message) const
{
    WineClientStepResult value;
    value.step = step;
    value.ok = ok;
    value.errorCategory = category;
    value.errorMessage = message;
    value.events = m_events;
    value.initialConfigureReceived = m_initialConfigureReceived;
    value.initialConfigureAcknowledged = m_initialConfigureAcknowledged;
    value.bufferCommitted = m_bufferCommitted;
    value.controlCreated = m_controlCreated;
    value.protocolDestructorSent = m_protocolDestructorSent;
    value.displayError = m_display ? wl_display_get_error(m_display) : 0;
    value.protocolErrorOccurred = value.displayError == EPROTO;
    if (value.protocolErrorOccurred) {
        const wl_interface *interface = nullptr;
        uint32_t objectId = 0;
        value.protocolErrorCode =
            wl_display_get_protocol_error(m_display, &interface, &objectId);
        value.protocolErrorObjectId = objectId;
        if (interface)
            value.protocolErrorInterface = QString::fromUtf8(interface->name);

        const auto proxyId = [](const void *proxy) {
            return proxy
                ? wl_proxy_get_id(reinterpret_cast<wl_proxy *>(const_cast<void *>(proxy)))
                : 0;
        };
        if (objectId == proxyId(m_control))
            value.protocolErrorObject = QStringLiteral("control");
        else if (objectId == proxyId(m_manager))
            value.protocolErrorObject = QStringLiteral("manager");
        else if (objectId == proxyId(m_toplevel))
            value.protocolErrorObject = QStringLiteral("window");
    }
    value.localDisplayAlive = m_display != nullptr;
    value.localManagerProxyAlive = m_manager != nullptr;
    value.localControlProxyAlive = m_control != nullptr;
    value.localSurfaceProxyAlive = m_surface != nullptr;
    value.localProxyCount = (m_registry != nullptr) + (m_compositor != nullptr)
        + (m_shm != nullptr) + (m_xdgWmBase != nullptr) + (m_manager != nullptr)
        + (m_surface != nullptr) + (m_xdgSurface != nullptr) + (m_toplevel != nullptr)
        + (m_buffer != nullptr) + (m_control != nullptr);
    value.destroyMode = m_destroyMode;
    value.disconnectMode = m_disconnectMode;
    value.abruptTransportClosed = m_abruptTransportClosed;
    return value;
}

bool WineClientWorker::roundtrip(WineClientStepResult &value)
{
    if (m_display && wl_display_roundtrip(m_display) >= 0)
        return true;
    value = result(value.step, false);
    if (value.protocolErrorOccurred) {
        value.errorCategory = QStringLiteral("wayland_protocol_error");
        value.errorMessage = QStringLiteral("Wayland server reported a protocol error");
    } else {
        value.errorCategory = QStringLiteral("transport_or_disconnect_error");
        value.errorMessage = QStringLiteral("Wayland roundtrip failed");
    }
    return false;
}

void WineClientWorker::createMappedToplevel(const QString &socketPath,
                                            const QString &appId,
                                            const QSize &size)
{
    WineClientStepResult value;
    value.step = QStringLiteral("create_mapped_xdg_toplevel");
    m_display = wl_display_connect(socketPath.toLocal8Bit().constData());
    if (!m_display) {
        Q_EMIT stepFinished(result(value.step,
                                   false,
                                   QStringLiteral("transport_or_disconnect_error"),
                                   QStringLiteral("Unable to connect to isolated socket")));
        return;
    }
    m_registry = wl_display_get_registry(m_display);
    static const wl_registry_listener registryListener{
        [](void *data,
           wl_registry *registry,
           uint32_t name,
           const char *interface,
           uint32_t version) {
            auto *worker = static_cast<WineClientWorker *>(data);
            if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
                worker->m_compositor = static_cast<wl_compositor *>(
                    wl_registry_bind(registry, name, &wl_compositor_interface,
                                     std::min(version, 6u)));
            } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
                worker->m_shm = static_cast<wl_shm *>(
                    wl_registry_bind(registry, name, &wl_shm_interface, 1));
            } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
                worker->m_xdgWmBase = static_cast<xdg_wm_base *>(
                    wl_registry_bind(registry, name, &xdg_wm_base_interface,
                                     std::min(version, 5u)));
            } else if (std::strcmp(interface,
                                   treeland_wine_window_manager_v1_interface.name) == 0) {
                worker->m_manager = static_cast<treeland_wine_window_manager_v1 *>(
                    wl_registry_bind(registry,
                                     name,
                                     &treeland_wine_window_manager_v1_interface,
                                     1));
            }
        },
        [](void *, wl_registry *, uint32_t) { },
    };
    wl_registry_add_listener(m_registry, &registryListener, this);
    if (!roundtrip(value)) {
        Q_EMIT stepFinished(value);
        return;
    }
    if (!m_compositor || !m_shm || !m_xdgWmBase || !m_manager) {
        Q_EMIT stepFinished(result(value.step,
                                   false,
                                   QStringLiteral("fixture_error"),
                                   QStringLiteral("Required fixture globals were not advertised")));
        return;
    }

    static const xdg_wm_base_listener wmBaseListener{
        [](void *, xdg_wm_base *base, uint32_t serial) { xdg_wm_base_pong(base, serial); }
    };
    xdg_wm_base_add_listener(m_xdgWmBase, &wmBaseListener, this);
    m_surface = wl_compositor_create_surface(m_compositor);
    m_xdgSurface = xdg_wm_base_get_xdg_surface(m_xdgWmBase, m_surface);
    static const xdg_surface_listener surfaceListener{
        [](void *data, xdg_surface *surface, uint32_t serial) {
            auto *worker = static_cast<WineClientWorker *>(data);
            worker->m_initialConfigureReceived = true;
            xdg_surface_ack_configure(surface, serial);
            worker->m_initialConfigureAcknowledged = true;
        }
    };
    xdg_surface_add_listener(m_xdgSurface, &surfaceListener, this);
    m_toplevel = xdg_surface_get_toplevel(m_xdgSurface);
    xdg_toplevel_set_app_id(m_toplevel, appId.toUtf8().constData());
    wl_surface_commit(m_surface);
    if (!roundtrip(value) || !m_initialConfigureAcknowledged) {
        if (value.errorCategory.isEmpty())
            value = result(value.step, false, QStringLiteral("fixture_error"),
                           QStringLiteral("Initial configure was not acknowledged"));
        Q_EMIT stepFinished(value);
        return;
    }

    QString bufferError;
    if (!createBuffer(size, bufferError)) {
        Q_EMIT stepFinished(result(value.step, false, QStringLiteral("fixture_error"), bufferError));
        return;
    }
    wl_surface_attach(m_surface, m_buffer, 0, 0);
    wl_surface_damage_buffer(m_surface, 0, 0, size.width(), size.height());
    wl_surface_commit(m_surface);
    m_bufferCommitted = true;
    if (!roundtrip(value)) {
        Q_EMIT stepFinished(value);
        return;
    }
    Q_EMIT stepFinished(result(value.step, true));
}

bool WineClientWorker::createBuffer(const QSize &size, QString &error)
{
    const qsizetype stride = size.width() * 4;
    const qsizetype byteCount = stride * size.height();
    m_shmFile = std::make_unique<QTemporaryFile>();
    if (!m_shmFile->open() || !m_shmFile->resize(byteCount)) {
        error = QStringLiteral("Unable to allocate fixture shm file");
        return false;
    }
    void *mapping = mmap(nullptr,
                         static_cast<size_t>(byteCount),
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED,
                         m_shmFile->handle(),
                         0);
    if (mapping == MAP_FAILED) {
        error = QStringLiteral("Unable to map fixture shm file");
        return false;
    }
    std::memset(mapping, 0x80, static_cast<size_t>(byteCount));
    munmap(mapping, static_cast<size_t>(byteCount));
    wl_shm_pool *pool = wl_shm_create_pool(m_shm, m_shmFile->handle(), byteCount);
    m_buffer = wl_shm_pool_create_buffer(pool,
                                         0,
                                         size.width(),
                                         size.height(),
                                         stride,
                                         WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    return m_buffer;
}

void WineClientWorker::createWindowControl()
{
    m_events = {};
    m_control = treeland_wine_window_manager_v1_get_window_control(m_manager, m_toplevel);
    static const treeland_wine_window_control_v1_listener listener{
        [](void *data, treeland_wine_window_control_v1 *, uint32_t id) {
            auto *worker = static_cast<WineClientWorker *>(data);
            worker->m_events.append(QJsonObject{ { QStringLiteral("event"), QStringLiteral("window_id") },
                                                  { QStringLiteral("args"), QJsonArray{ qint64(id) } } });
        },
        [](void *data, treeland_wine_window_control_v1 *, int32_t x, int32_t y, uint32_t serial) {
            auto *worker = static_cast<WineClientWorker *>(data);
            worker->m_events.append(QJsonObject{
                { QStringLiteral("event"), QStringLiteral("configure_position") },
                { QStringLiteral("args"), QJsonArray{ x, y, qint64(serial) } },
            });
        },
        [](void *data, treeland_wine_window_control_v1 *, uint32_t topmost) {
            auto *worker = static_cast<WineClientWorker *>(data);
            worker->m_events.append(QJsonObject{
                { QStringLiteral("event"), QStringLiteral("configure_stacking") },
                { QStringLiteral("args"), QJsonArray{ qint64(topmost) } },
            });
        },
    };
    treeland_wine_window_control_v1_add_listener(m_control, &listener, this);
    WineClientStepResult value;
    value.step = QStringLiteral("create_window_control");
    if (!roundtrip(value)) {
        Q_EMIT stepFinished(value);
        return;
    }
    m_controlCreated = true;
    Q_EMIT stepFinished(result(value.step, true));
}

void WineClientWorker::setPosition(qint32 x, qint32 y, quint32 serial)
{
    m_events = {};
    treeland_wine_window_control_v1_set_position(m_control, x, y, serial);
    WineClientStepResult value;
    value.step = QStringLiteral("set_position");
    if (!roundtrip(value)) {
        Q_EMIT stepFinished(value);
        return;
    }
    Q_EMIT stepFinished(result(value.step, true));
}

void WineClientWorker::sendPosition(qint32 x, qint32 y, quint32 serial)
{
    m_events = {};
    treeland_wine_window_control_v1_set_position(m_control, x, y, serial);
    wl_display_flush(m_display);
    Q_EMIT stepFinished(result(QStringLiteral("send_position"), true));
}

void WineClientWorker::setZOrder(quint32 operation, quint32 siblingId)
{
    m_events = {};
    treeland_wine_window_control_v1_set_z_order(m_control, operation, siblingId);
    WineClientStepResult value;
    value.step = QStringLiteral("set_z_order");
    if (!roundtrip(value)) {
        Q_EMIT stepFinished(value);
        return;
    }
    Q_EMIT stepFinished(result(value.step, true));
}

void WineClientWorker::destroyObjects()
{
    m_events = {};
    m_destroyMode = QStringLiteral("protocol");
    if (m_control) {
        treeland_wine_window_control_v1_destroy(m_control);
        m_control = nullptr;
    }
    if (m_manager) {
        treeland_wine_window_manager_v1_destroy(m_manager);
        m_manager = nullptr;
    }
    m_protocolDestructorSent = true;
    WineClientStepResult value;
    value.step = QStringLiteral("destroy");
    if (!roundtrip(value)) {
        Q_EMIT stepFinished(value);
        return;
    }
    Q_EMIT stepFinished(result(value.step, true));
}

void WineClientWorker::destroyControlProxyOnly()
{
    m_events = {};
    m_destroyMode = QStringLiteral("proxy-only");
    if (m_control) {
        wl_proxy_destroy(reinterpret_cast<wl_proxy *>(m_control));
        m_control = nullptr;
    }
    Q_EMIT stepFinished(result(QStringLiteral("destroy:proxy-only"), true));
}

void WineClientWorker::disconnectClient()
{
    m_events = {};
    m_disconnectMode = QStringLiteral("graceful");
    cleanup();
    Q_EMIT stepFinished(result(QStringLiteral("disconnect"), true));
}

void WineClientWorker::disconnectAbruptly()
{
    m_events = {};
    m_disconnectMode = QStringLiteral("abrupt");
    if (m_display) {
        const int fd = wl_display_get_fd(m_display);
        m_abruptTransportClosed = shutdown(fd, SHUT_RDWR) == 0 || errno == ENOTCONN;
    }
    cleanup();
    Q_EMIT stepFinished(result(QStringLiteral("disconnect:abrupt"),
                               m_abruptTransportClosed,
                               m_abruptTransportClosed
                                   ? QString{}
                                   : QStringLiteral("transport_or_disconnect_error"),
                               m_abruptTransportClosed
                                   ? QString{}
                                   : QStringLiteral("Unable to shut down Wayland transport")));
}

void WineClientWorker::observeServerShutdown()
{
    m_events = {};
    m_disconnectMode = QStringLiteral("server-shutdown");
    WineClientStepResult value;
    value.step = QStringLiteral("server_shutdown");
    if (m_display && wl_display_roundtrip(m_display) >= 0) {
        Q_EMIT stepFinished(result(value.step,
                                   false,
                                   QStringLiteral("server_shutdown_error"),
                                   QStringLiteral("Client remained connected after server shutdown")));
        return;
    }
    Q_EMIT stepFinished(result(value.step, true));
}

void WineClientWorker::destroyLocalProxies()
{
    const auto destroyProxy = [](void *proxy) {
        if (proxy)
            wl_proxy_destroy(static_cast<wl_proxy *>(proxy));
    };
    destroyProxy(m_control);
    destroyProxy(m_toplevel);
    destroyProxy(m_xdgSurface);
    destroyProxy(m_surface);
    destroyProxy(m_buffer);
    destroyProxy(m_manager);
    destroyProxy(m_xdgWmBase);
    destroyProxy(m_shm);
    destroyProxy(m_compositor);
    destroyProxy(m_registry);
    m_control = nullptr;
    m_toplevel = nullptr;
    m_xdgSurface = nullptr;
    m_surface = nullptr;
    m_buffer = nullptr;
    m_manager = nullptr;
    m_xdgWmBase = nullptr;
    m_shm = nullptr;
    m_compositor = nullptr;
    m_registry = nullptr;
}

void WineClientWorker::cleanup()
{
    destroyLocalProxies();
    if (m_display)
        wl_display_disconnect(m_display);
    m_display = nullptr;
    m_shmFile.reset();
}

#include "moc_wineclientworker.cpp"
