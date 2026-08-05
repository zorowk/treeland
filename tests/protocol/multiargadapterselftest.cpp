// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "tl-test-treeland-test-multi-arg-v1.h"
#include "wayland-treeland-test-multi-arg-v1-client-protocol.h"
#include "wayland-treeland-test-multi-arg-v1-server-protocol.h"

#include <QFile>
#include <QJsonDocument>
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

const struct treeland_test_multi_arg_v1_interface serverImplementation{
    .echo = handleEcho,
    .destroy = handleDestroy,
};

void bindMultiArg(wl_client *client, void *, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(
        client, &treeland_test_multi_arg_v1_interface, static_cast<int>(version), id);
    wl_resource_set_implementation(resource, &serverImplementation, nullptr, nullptr);
}
}

class MultiArgAdapterSelfTest : public QObject
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
                     m_registry, tl_test_multi_arg_registry_listener(), &m_adapter),
                 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QCOMPARE(tl_test_multi_arg_bind(&m_adapter, m_registry, 1), 0);
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

    void metadataDeclaresFields()
    {
        QFile file(QStringLiteral(TL_MULTI_ARG_TEST_METADATA));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject metadata = QJsonDocument::fromJson(file.readAll()).object();
        const QJsonObject interface = metadata.value(QStringLiteral("interfaces"))
                                          .toArray().first().toObject();

        // Verify event argument types
        const QJsonObject replyEvent = interface.value(QStringLiteral("events"))
                                           .toArray().first().toObject();
        const QJsonArray args = replyEvent.value(QStringLiteral("arguments")).toArray();
        QCOMPARE(args.size(), 3);
        QCOMPARE(args.at(0).toObject().value(QStringLiteral("type")).toString(),
                 QStringLiteral("uint"));
        QCOMPARE(args.at(1).toObject().value(QStringLiteral("type")).toString(),
                 QStringLiteral("int"));
        QCOMPARE(args.at(2).toObject().value(QStringLiteral("type")).toString(),
                 QStringLiteral("string"));
        QCOMPARE(args.at(2).toObject().value(QStringLiteral("allow_null")).toBool(), true);

        // Verify enum metadata
        const QJsonArray enums = interface.value(QStringLiteral("enums")).toArray();
        QCOMPARE(enums.size(), 1);
        const QJsonObject enumObj = enums.at(0).toObject();
        QCOMPARE(enumObj.value(QStringLiteral("name")).toString(),
                 QStringLiteral("test_kind"));
        const QJsonArray entries = enumObj.value(QStringLiteral("entries")).toArray();
        QCOMPARE(entries.size(), 3);
    }

    void multiArgEchoRoundTrip()
    {
        tl_test_multi_arg_clear_events(&m_adapter);
        QCOMPARE(tl_test_multi_arg_echo(&m_adapter, 42, -7, "hello"), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);

        QVERIFY(!m_adapter.event_snapshot_failed);
        QCOMPARE(m_adapter.reply_event_count, size_t(1));

        const tl_test_multi_arg_reply_event &e = m_adapter.reply_events[0];
        QCOMPARE(e.id, 42u);
        QCOMPARE(e.offset, -7);
        QVERIFY(e.name != nullptr);
        QCOMPARE(QByteArray(e.name), QByteArray("hello"));
    }

    void stringOwnershipIsIndependent()
    {
        tl_test_multi_arg_clear_events(&m_adapter);
        QCOMPARE(tl_test_multi_arg_echo(&m_adapter, 1, 0, "before-clear"), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QCOMPARE(m_adapter.reply_event_count, size_t(1));

        tl_test_multi_arg_clear_events(&m_adapter);
        QCOMPARE(m_adapter.reply_event_count, size_t(0));

        QCOMPARE(tl_test_multi_arg_echo(&m_adapter, 2, 0, "after-clear"), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QCOMPARE(m_adapter.reply_event_count, size_t(1));
        QCOMPARE(QByteArray(m_adapter.reply_events[0].name), QByteArray("after-clear"));
    }

    void nullStringIsAccepted()
    {
        tl_test_multi_arg_clear_events(&m_adapter);
        QCOMPARE(tl_test_multi_arg_echo(&m_adapter, 0, 0, nullptr), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);

        QCOMPARE(m_adapter.reply_event_count, size_t(1));
        QCOMPARE(m_adapter.reply_events[0].name, nullptr);
    }

    void eventCapacityFailureIsObservable()
    {
        tl_test_multi_arg_clear_events(&m_adapter);
        for (int i = 0; i < TL_TEST_MULTI_ARG_MAX_EVENTS + 1; ++i)
            QCOMPARE(tl_test_multi_arg_echo(&m_adapter, uint32_t(i), 0, "overflow"), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);

        QCOMPARE(m_adapter.reply_event_count,
                 size_t(TL_TEST_MULTI_ARG_MAX_EVENTS));
        QVERIFY(m_adapter.event_snapshot_failed);

        tl_test_multi_arg_clear_events(&m_adapter);
        QVERIFY(!m_adapter.event_snapshot_failed);
        QCOMPARE(m_adapter.reply_event_count, size_t(0));
    }

    void deadTargetIsRejected()
    {
        QCOMPARE(tl_test_multi_arg_destroy(&m_adapter), 0);
        QCOMPARE(tl_test_multi_arg_echo(&m_adapter, 0, 0, "dead"), -1);
    }

private:
    wl_display *m_serverDisplay = nullptr;
    wl_global *m_global = nullptr;
    std::thread m_serverThread;
    wl_display *m_clientDisplay = nullptr;
    wl_registry *m_registry = nullptr;
    tl_test_multi_arg_adapter m_adapter{};
};

QTEST_GUILESS_MAIN(MultiArgAdapterSelfTest)

#include "multiargadapterselftest.moc"
