// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "clientruntime.h"

#ifndef TL_WINDOW_MANAGEMENT_ADAPTER_HEADER
#define TL_WINDOW_MANAGEMENT_ADAPTER_HEADER "windowmanagementadapter.h"
#endif
#include TL_WINDOW_MANAGEMENT_ADAPTER_HEADER

#include <wayland-client.h>

#include <cerrno>

ClientWorker::ClientWorker(QObject *parent)
    : QObject(parent)
    , m_adapter(new tl_window_management_adapter)
{
    tl_window_management_adapter_init(m_adapter);
}

ClientWorker::~ClientWorker()
{
    cleanup();
    delete m_adapter;
}

ClientStepResult ClientWorker::result(const QString &step, bool ok,
                                      const QString &category,
                                      const QString &message) const
{
    ClientStepResult snapshot;
    snapshot.step = step;
    snapshot.ok = ok;
    snapshot.errorCategory = category;
    snapshot.errorMessage = message;
    snapshot.advertisedVersion = m_adapter->advertised_version;
    snapshot.boundVersion = m_adapter->bound_version;
    snapshot.displayConnected = m_display != nullptr;
    snapshot.localProxyAlive = m_adapter->local_proxy_alive;
    snapshot.protocolDestructorSent = m_adapter->protocol_destructor_sent;

    snapshot.events.reserve(static_cast<qsizetype>(m_adapter->event_count));
    for (size_t i = 0; i < m_adapter->event_count; ++i)
        snapshot.events.append(m_adapter->events[i]);

    if (m_display) {
        snapshot.displayError = wl_display_get_error(m_display);
        snapshot.protocolErrorOccurred = snapshot.displayError == EPROTO;
        if (snapshot.protocolErrorOccurred) {
            const wl_interface *interface = nullptr;
            uint32_t objectId = 0;
            snapshot.protocolErrorCode =
                wl_display_get_protocol_error(m_display, &interface, &objectId);
            snapshot.protocolErrorObjectId = objectId;
            if (interface)
                snapshot.protocolErrorInterface = QString::fromUtf8(interface->name);
        }
    }

    return snapshot;
}

bool ClientWorker::roundtrip(ClientStepResult &snapshot)
{
    if (!m_display || wl_display_roundtrip(m_display) < 0) {
        snapshot = result(snapshot.step,
                          false,
                          QStringLiteral("transport_or_disconnect_error"),
                          QStringLiteral("Wayland roundtrip failed"));
        return false;
    }
    return true;
}

void ClientWorker::connectAndBind(const QString &socketPath)
{
    ClientStepResult snapshot;
    snapshot.step = QStringLiteral("bind");

    m_display = wl_display_connect(socketPath.toLocal8Bit().constData());
    if (!m_display) {
        Q_EMIT stepFinished(result(snapshot.step,
                                   false,
                                   QStringLiteral("transport_or_disconnect_error"),
                                   QStringLiteral("Unable to connect to the isolated socket")));
        return;
    }

    m_registry = wl_display_get_registry(m_display);
    if (!m_registry
        || wl_registry_add_listener(m_registry,
                                    tl_window_management_registry_listener(),
                                    m_adapter) != 0) {
        Q_EMIT stepFinished(result(snapshot.step,
                                   false,
                                   QStringLiteral("adapter_validation_error"),
                                   QStringLiteral("Unable to install the registry listener")));
        return;
    }

    if (!roundtrip(snapshot)) {
        Q_EMIT stepFinished(snapshot);
        return;
    }

    if (tl_window_management_bind(m_adapter, m_registry, 1) != 0) {
        Q_EMIT stepFinished(result(snapshot.step,
                                   false,
                                   QStringLiteral("adapter_validation_error"),
                                   QStringLiteral("Window-management global was not advertised")));
        return;
    }

    if (!roundtrip(snapshot)) {
        Q_EMIT stepFinished(snapshot);
        return;
    }

    snapshot = result(snapshot.step, true);
    tl_window_management_clear_events(m_adapter);
    Q_EMIT stepFinished(snapshot);
}

void ClientWorker::setDesktop(quint32 state)
{
    constexpr auto step = "set_desktop";
    tl_window_management_clear_events(m_adapter);
    if (tl_window_management_set_desktop(m_adapter, state) != 0) {
        Q_EMIT stepFinished(result(QString::fromLatin1(step),
                                   false,
                                   QStringLiteral("adapter_validation_error"),
                                   QStringLiteral("set_desktop target is not alive")));
        return;
    }

    ClientStepResult snapshot;
    snapshot.step = QString::fromLatin1(step);
    if (!roundtrip(snapshot)) {
        Q_EMIT stepFinished(snapshot);
        return;
    }

    Q_EMIT stepFinished(result(snapshot.step, true));
}

void ClientWorker::destroyProtocol()
{
    constexpr auto step = "destroy";
    tl_window_management_clear_events(m_adapter);
    if (tl_window_management_destroy(m_adapter) != 0) {
        Q_EMIT stepFinished(result(QString::fromLatin1(step),
                                   false,
                                   QStringLiteral("adapter_validation_error"),
                                   QStringLiteral("Protocol destructor target is not alive")));
        return;
    }

    ClientStepResult snapshot;
    snapshot.step = QString::fromLatin1(step);
    if (!roundtrip(snapshot)) {
        Q_EMIT stepFinished(snapshot);
        return;
    }

    Q_EMIT stepFinished(result(snapshot.step, true));
}

void ClientWorker::disconnectClient()
{
    cleanup();
    Q_EMIT stepFinished(result(QStringLiteral("disconnect"), true));
}

void ClientWorker::cleanup()
{
    if (m_adapter->proxy) {
        wl_proxy_destroy(reinterpret_cast<wl_proxy *>(m_adapter->proxy));
        m_adapter->proxy = nullptr;
        m_adapter->local_proxy_alive = false;
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
    if (m_display) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
    }
}
