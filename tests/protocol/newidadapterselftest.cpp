// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "tl-test-treeland-test-new-id-v1.h"
#include "wayland-treeland-test-new-id-v1-client-protocol.h"
#include "wayland-treeland-test-new-id-v1-server-protocol.h"

#include <QFile>
#include <QJsonDocument>
#include <QtTest>

#include <thread>

#include <sys/socket.h>
#include <wayland-client.h>
#include <wayland-server.h>

namespace {
void handleCreateChild(wl_client *client, wl_resource *resource)
{
    struct wl_resource *child = wl_resource_create(
        client, &treeland_test_child_v1_interface, 1, 0);
    QVERIFY(child);
    treeland_test_new_id_v1_send_child_created(resource, child);
    treeland_test_child_v1_send_done(child, 42);
}

void handleDestroy(wl_client *, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

const struct treeland_test_new_id_v1_interface serverImplementation{
    .create_child = handleCreateChild,
    .destroy = handleDestroy,
};

void bindNewId(wl_client *client, void *, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(
        client, &treeland_test_new_id_v1_interface, static_cast<int>(version), id);
    QVERIFY(resource);
    wl_resource_set_implementation(resource, &serverImplementation, nullptr, nullptr);
}
}

class NewIdAdapterSelfTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_serverDisplay = wl_display_create();
        QVERIFY(m_serverDisplay);
        m_global = wl_global_create(m_serverDisplay,
                                    &treeland_test_new_id_v1_interface,
                                    1,
                                    nullptr,
                                    bindNewId);
        QVERIFY(m_global);

        int sockets[2] = { -1, -1 };
        QCOMPARE(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), 0);
        QVERIFY(wl_client_create(m_serverDisplay, sockets[0]));
        m_serverThread = std::thread([this] { wl_display_run(m_serverDisplay); });

        m_clientDisplay = wl_display_connect_to_fd(sockets[1]);
        QVERIFY(m_clientDisplay);
        m_registry = wl_display_get_registry(m_clientDisplay);
        QVERIFY(m_registry);
        tl_test_new_id_adapter_init(&m_adapter);
        QCOMPARE(wl_registry_add_listener(
                     m_registry, tl_test_new_id_registry_listener(), &m_adapter),
                 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QCOMPARE(tl_test_new_id_bind(&m_adapter, m_registry, 1), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
    }

    void cleanupTestCase()
    {
        tl_test_new_id_adapter_fini(&m_adapter);
        if (m_adapter.proxy)
            tl_test_new_id_destroy(&m_adapter);
        if (m_registry)
            wl_registry_destroy(m_registry);
        if (m_clientDisplay)
            wl_display_disconnect(m_clientDisplay);
        if (m_serverDisplay)
            wl_display_terminate(m_serverDisplay);
        if (m_serverThread.joinable())
            m_serverThread.join();
        if (m_global)
            wl_global_destroy(m_global);
        if (m_serverDisplay)
            wl_display_destroy(m_serverDisplay);
    }

    void metadataDeclaresNewId()
    {
        QFile file(QStringLiteral(TL_NEW_ID_TEST_METADATA));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject metadata = QJsonDocument::fromJson(file.readAll()).object();
        const QJsonArray interfaces = metadata.value(QStringLiteral("interfaces")).toArray();
        QCOMPARE(interfaces.size(), 2);

        const QJsonObject parent = interfaces.at(0).toObject();
        QCOMPARE(parent.value(QStringLiteral("name")).toString(),
                 QStringLiteral("treeland_test_new_id_v1"));
        QCOMPARE(parent.value(QStringLiteral("events")).toArray().first().toObject()
                     .value(QStringLiteral("arguments")).toArray().first().toObject()
                     .value(QStringLiteral("type")).toString(),
                 QStringLiteral("new_id"));
        QCOMPARE(parent.value(QStringLiteral("events")).toArray().first().toObject()
                     .value(QStringLiteral("arguments")).toArray().first().toObject()
                     .value(QStringLiteral("interface")).toString(),
                 QStringLiteral("treeland_test_child_v1"));

        const QJsonObject child = interfaces.at(1).toObject();
        QCOMPARE(child.value(QStringLiteral("name")).toString(),
                 QStringLiteral("treeland_test_child_v1"));
    }

    void childProxyCreatedAndReceivesEvent()
    {
        tl_test_new_id_clear_events(&m_adapter);
        QCOMPARE(tl_test_new_id_create_child(&m_adapter), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);

        QVERIFY(!m_adapter.event_snapshot_failed);
        QVERIFY(m_adapter.test_child_proxy != nullptr);
        QVERIFY(m_adapter.test_child_listener_installed);
        QCOMPARE(m_adapter.test_child_event_count, size_t(1));
        QCOMPARE(m_adapter.test_child_events[0], 42u);
    }

    void clearEventsDestroysChildProxy()
    {
        QCOMPARE(tl_test_new_id_create_child(&m_adapter), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QVERIFY(m_adapter.test_child_proxy != nullptr);

        tl_test_new_id_clear_events(&m_adapter);
        QCOMPARE(m_adapter.test_child_proxy, nullptr);
        QVERIFY(!m_adapter.test_child_listener_installed);
        QCOMPARE(m_adapter.test_child_event_count, size_t(0));
    }

    void duplicateChildIsRejected()
    {
        QCOMPARE(tl_test_new_id_create_child(&m_adapter), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QVERIFY(m_adapter.test_child_proxy != nullptr);

        tl_test_new_id_clear_events(&m_adapter);
        QCOMPARE(tl_test_new_id_create_child(&m_adapter), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QVERIFY(m_adapter.test_child_proxy != nullptr);
        // No event_snapshot_failed since we cleared in between
        QVERIFY(!m_adapter.event_snapshot_failed);
    }

    void deadTargetIsRejected()
    {
        QCOMPARE(tl_test_new_id_destroy(&m_adapter), 0);
        QCOMPARE(tl_test_new_id_create_child(&m_adapter), -1);
    }

private:
    wl_display *m_serverDisplay = nullptr;
    wl_global *m_global = nullptr;
    std::thread m_serverThread;
    wl_display *m_clientDisplay = nullptr;
    wl_registry *m_registry = nullptr;
    tl_test_new_id_adapter m_adapter{};
};

QTEST_GUILESS_MAIN(NewIdAdapterSelfTest)

#include "newidadapterselftest.moc"
