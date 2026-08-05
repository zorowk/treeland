// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "tl-test-treeland-test-multi-arg-v1.h"
#include "wayland-treeland-test-multi-arg-v1-client-protocol.h"
#include "wayland-treeland-test-multi-arg-v1-server-protocol.h"

#include <QtTest>

#include <thread>

#include <sys/socket.h>
#include <wayland-client.h>
#include <wayland-server.h>

namespace {

void handleEcho(wl_client *, wl_resource *resource,
                uint32_t id, int32_t offset, const char *name)
{
    treeland_test_multi_arg_v1_send_reply(resource, id, offset, name);
}

void handleDestroy(wl_client *, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

const struct treeland_test_multi_arg_v1_interface serverImpl{
    .echo = handleEcho,
    .destroy = handleDestroy,
};

void bindMultiArg(wl_client *client, void *, uint32_t version, uint32_t id)
{
    wl_resource *r = wl_resource_create(
        client, &treeland_test_multi_arg_v1_interface, static_cast<int>(version), id);
    wl_resource_set_implementation(r, &serverImpl, nullptr, nullptr);
}
}

class GlobalVersionSelfTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_serverDisplay = wl_display_create();
        QVERIFY(m_serverDisplay);
        m_global = wl_global_create(m_serverDisplay,
                                    &treeland_test_multi_arg_v1_interface,
                                    1, nullptr, bindMultiArg);
        QVERIFY(m_global);

        int sockets[2] = { -1, -1 };
        QCOMPARE(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), 0);
        QVERIFY(wl_client_create(m_serverDisplay, sockets[0]));
        m_serverThread = std::thread([this] { wl_display_run(m_serverDisplay); });

        m_clientDisplay = wl_display_connect_to_fd(sockets[1]);
        QVERIFY(m_clientDisplay);
        m_registry = wl_display_get_registry(m_clientDisplay);
        QVERIFY(m_registry);
        tl_test_multi_arg_adapter_init(&m_adapter);
        QCOMPARE(wl_registry_add_listener(
                     m_registry, tl_test_multi_arg_registry_listener(), &m_adapter), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
    }

    void cleanupTestCase()
    {
        tl_test_multi_arg_adapter_fini(&m_adapter);
        if (m_adapter.proxy)
            tl_test_multi_arg_destroy(&m_adapter);
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

    void advertisedVersionIsRecorded()
    {
        QCOMPARE(m_adapter.advertised_version, 1u);
        QVERIFY(m_adapter.global_name > 0);
    }

    void bindVersionClampedToAdvertised()
    {
        QCOMPARE(tl_test_multi_arg_bind(&m_adapter, m_registry, 5), 0);
        QCOMPARE(m_adapter.bound_version, 1u);
        tl_test_multi_arg_destroy(&m_adapter);
    }

    void bindVersionExact()
    {
        QCOMPARE(tl_test_multi_arg_bind(&m_adapter, m_registry, 1), 0);
        QCOMPARE(m_adapter.bound_version, 1u);
        tl_test_multi_arg_destroy(&m_adapter);
    }

    void globalRemovePreventsNewBind()
    {
        QCOMPARE(tl_test_multi_arg_bind(&m_adapter, m_registry, 1), 0);

        wl_global_remove(m_global);
        wl_display_roundtrip(m_clientDisplay);

        QCOMPARE(tl_test_multi_arg_echo(&m_adapter, 1, 0, "still-alive"), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QCOMPARE(m_adapter.reply_event_count, size_t(1));

        tl_test_multi_arg_destroy(&m_adapter);

        // New adapter cannot bind
        tl_test_multi_arg_adapter a2{};
        tl_test_multi_arg_adapter_init(&a2);
        wl_registry_add_listener(m_registry, tl_test_multi_arg_registry_listener(), &a2);
        wl_display_roundtrip(m_clientDisplay);
        QCOMPARE(a2.global_name, uint32_t(0));
        QCOMPARE(tl_test_multi_arg_bind(&a2, m_registry, 1), -1);
        tl_test_multi_arg_adapter_fini(&a2);

        // Re-create for subsequent tests
        wl_global_destroy(m_global);
        m_global = wl_global_create(m_serverDisplay,
                                    &treeland_test_multi_arg_v1_interface,
                                    1, nullptr, bindMultiArg);
        QVERIFY(m_global);
        wl_display_roundtrip(m_clientDisplay);
    }

    void rebindAfterRecreate()
    {
        QCOMPARE(tl_test_multi_arg_bind(&m_adapter, m_registry, 1), 0);
        QVERIFY(m_adapter.proxy != nullptr);
        QCOMPARE(m_adapter.bound_version, 1u);
        tl_test_multi_arg_destroy(&m_adapter);
    }

private:
    wl_display *m_serverDisplay = nullptr;
    wl_global *m_global = nullptr;
    std::thread m_serverThread;
    wl_display *m_clientDisplay = nullptr;
    wl_registry *m_registry = nullptr;
    tl_test_multi_arg_adapter m_adapter{};
};

QTEST_GUILESS_MAIN(GlobalVersionSelfTest)

#include "globalversionselftest.moc"
