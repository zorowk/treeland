// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocolfdvalue.h"
#include "tl-test-treeland-test-fd-v1.h"
#include "wayland-treeland-test-fd-v1-client-protocol.h"
#include "wayland-treeland-test-fd-v1-server-protocol.h"

#include <QFile>
#include <QJsonDocument>
#include <QtTest>

#include <thread>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-server.h>

namespace {
void handleEcho(wl_client *, wl_resource *resource, int32_t fd)
{
    treeland_test_fd_v1_send_value(resource, fd);
    if (fd >= 0)
        close(fd);
}

void handleDestroy(wl_client *, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

const struct treeland_test_fd_v1_interface serverImplementation{
    .echo = handleEcho,
    .destroy = handleDestroy,
};

void bindFd(wl_client *client, void *, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(
        client, &treeland_test_fd_v1_interface, static_cast<int>(version), id);
    wl_resource_set_implementation(resource, &serverImplementation, nullptr, nullptr);
}
}

class FdAdapterSelfTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_serverDisplay = wl_display_create();
        QVERIFY(m_serverDisplay);
        m_global = wl_global_create(m_serverDisplay,
                                    &treeland_test_fd_v1_interface,
                                    1,
                                    nullptr,
                                    bindFd);
        QVERIFY(m_global);

        int sockets[2] = { -1, -1 };
        QCOMPARE(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), 0);
        QVERIFY(wl_client_create(m_serverDisplay, sockets[0]));
        m_serverThread = std::thread([this] { wl_display_run(m_serverDisplay); });

        m_clientDisplay = wl_display_connect_to_fd(sockets[1]);
        QVERIFY(m_clientDisplay);
        m_registry = wl_display_get_registry(m_clientDisplay);
        QVERIFY(m_registry);
        tl_test_fd_adapter_init(&m_adapter);
        QCOMPARE(wl_registry_add_listener(
                     m_registry, tl_test_fd_registry_listener(), &m_adapter),
                 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QCOMPARE(tl_test_fd_bind(&m_adapter, m_registry, 1), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
    }

    void cleanupTestCase()
    {
        tl_test_fd_adapter_fini(&m_adapter);
        if (m_adapter.proxy)
            tl_test_fd_destroy(&m_adapter);
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

    void metadataDeclaresFd()
    {
        QFile file(QStringLiteral(TL_FD_TEST_METADATA));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject metadata = QJsonDocument::fromJson(file.readAll()).object();
        const QJsonObject interface = metadata.value(QStringLiteral("interfaces"))
                                          .toArray().first().toObject();
        QCOMPARE(interface.value(QStringLiteral("requests")).toArray().first().toObject()
                     .value(QStringLiteral("arguments")).toArray().first().toObject()
                     .value(QStringLiteral("type")).toString(),
                 QStringLiteral("fd"));
        QCOMPARE(interface.value(QStringLiteral("events")).toArray().first().toObject()
                     .value(QStringLiteral("arguments")).toArray().first().toObject()
                     .value(QStringLiteral("type")).toString(),
                 QStringLiteral("fd"));
    }

    void jsonRepresentation_data()
    {
        QTest::addColumn<QJsonValue>("json");
        QTest::addColumn<bool>("valid");

        QTest::newRow("present") << QJsonValue(QJsonObject{ { "fd", true } }) << true;
        QTest::newRow("null") << QJsonValue(QJsonValue::Null) << true;
        QTest::newRow("bare-true") << QJsonValue(true) << false;
        QTest::newRow("bare-int") << QJsonValue(42) << false;
        QTest::newRow("wrong-member")
            << QJsonValue(QJsonObject{ { "raw", 42 } }) << false;
        QTest::newRow("fd-false") << QJsonValue(QJsonObject{ { "fd", false } }) << false;
        QTest::newRow("extra-member")
            << QJsonValue(QJsonObject{ { "fd", true }, { "extra", 0 } }) << false;
    }

    void jsonRepresentation()
    {
        QFETCH(QJsonValue, json);
        QFETCH(bool, valid);
        int fd = -1;
        QString error;
        QCOMPARE(protocolFdFromJson(json, fd, error), valid);
        if (valid) {
            QVERIFY(error.isEmpty());
            if (json.isNull()) {
                QCOMPARE(fd, -1);
            } else {
                QVERIFY(fd >= 0);
                QCOMPARE(normalizedProtocolFd(fd),
                         QJsonObject({ { QStringLiteral("fd"), QStringLiteral("valid") } }));
                ::close(fd);
            }
        } else {
            QVERIFY(!error.isEmpty());
        }
    }

    void requestAndOwningSnapshot()
    {
        int fd = -1;
        QString error;
        QVERIFY2(protocolFdFromJson(QJsonObject{ { "fd", true } }, fd, error),
                 qPrintable(error));
        QVERIFY(fd >= 0);

        tl_test_fd_clear_events(&m_adapter);
        QCOMPARE(tl_test_fd_echo(&m_adapter, fd), 0);
        ::close(fd);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);

        QCOMPARE(m_adapter.fd_event_count, size_t(1));
        QVERIFY(!m_adapter.event_snapshot_failed);
        const tl_test_fd_fd_event &event = m_adapter.fd_events[0];
        QCOMPARE(QByteArray(event.name), QByteArray("value"));
        QVERIFY(event.fd >= 0);
        QVERIFY(::fcntl(event.fd, F_GETFD) >= 0);

        QCOMPARE(normalizedProtocolFd(event.fd),
                 QJsonObject({ { QStringLiteral("fd"), QStringLiteral("valid") } }));

        tl_test_fd_clear_events(&m_adapter);
        QCOMPARE(m_adapter.fd_event_count, size_t(0));
        QCOMPARE(m_adapter.fd_events[0].fd, -1);
    }

    void eventCapacityFailureIsObservable()
    {
        int fd = -1;
        QString error;
        QVERIFY(protocolFdFromJson(QJsonObject{ { "fd", true } }, fd, error));

        tl_test_fd_clear_events(&m_adapter);
        for (int i = 0; i < TL_TEST_FD_MAX_EVENTS + 1; ++i) {
            int extra = -1;
            protocolFdFromJson(QJsonObject{ { "fd", true } }, extra, error);
            QCOMPARE(tl_test_fd_echo(&m_adapter, extra), 0);
            ::close(extra);
        }
        ::close(fd); // unused

        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QCOMPARE(m_adapter.fd_event_count, size_t(TL_TEST_FD_MAX_EVENTS));
        QVERIFY(m_adapter.event_snapshot_failed);
        tl_test_fd_clear_events(&m_adapter);
        QVERIFY(!m_adapter.event_snapshot_failed);
    }

    void deadTargetIsRejected()
    {
        QCOMPARE(tl_test_fd_destroy(&m_adapter), 0);
        int fd = -1;
        QString error;
        QVERIFY(protocolFdFromJson(QJsonObject{ { "fd", true } }, fd, error));
        QCOMPARE(tl_test_fd_echo(&m_adapter, fd), -1);
        ::close(fd);
    }

private:
    wl_display *m_serverDisplay = nullptr;
    wl_global *m_global = nullptr;
    std::thread m_serverThread;
    wl_display *m_clientDisplay = nullptr;
    wl_registry *m_registry = nullptr;
    tl_test_fd_adapter m_adapter{};
};

QTEST_GUILESS_MAIN(FdAdapterSelfTest)

#include "fdadapterselftest.moc"
