// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocolarrayvalue.h"
#include "tl-test-treeland-test-array-v1.h"
#include "wayland-treeland-test-array-v1-client-protocol.h"
#include "wayland-treeland-test-array-v1-server-protocol.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

#include <thread>

#include <sys/socket.h>
#include <wayland-client.h>
#include <wayland-server.h>

namespace {
void handleEcho(wl_client *, wl_resource *resource, wl_array *value)
{
    treeland_test_array_v1_send_value(resource, value);
}

void handleDestroy(wl_client *, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

const struct treeland_test_array_v1_interface serverImplementation{
    .echo = handleEcho,
    .destroy = handleDestroy,
};

void bindArray(wl_client *client, void *, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(
        client, &treeland_test_array_v1_interface, static_cast<int>(version), id);
    wl_resource_set_implementation(resource, &serverImplementation, nullptr, nullptr);
}
}

class ArrayAdapterSelfTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_serverDisplay = wl_display_create();
        QVERIFY(m_serverDisplay);
        m_global = wl_global_create(m_serverDisplay,
                                    &treeland_test_array_v1_interface,
                                    1,
                                    nullptr,
                                    bindArray);
        QVERIFY(m_global);

        int sockets[2] = { -1, -1 };
        QCOMPARE(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), 0);
        QVERIFY(wl_client_create(m_serverDisplay, sockets[0]));
        m_serverThread = std::thread([this] { wl_display_run(m_serverDisplay); });

        m_clientDisplay = wl_display_connect_to_fd(sockets[1]);
        QVERIFY(m_clientDisplay);
        m_registry = wl_display_get_registry(m_clientDisplay);
        QVERIFY(m_registry);
        tl_test_array_adapter_init(&m_adapter);
        QCOMPARE(wl_registry_add_listener(
                     m_registry, tl_test_array_registry_listener(), &m_adapter),
                 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QCOMPARE(tl_test_array_bind(&m_adapter, m_registry, 1), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
    }

    void cleanupTestCase()
    {
        tl_test_array_adapter_fini(&m_adapter);
        if (m_adapter.proxy)
            tl_test_array_destroy(&m_adapter);
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

    void metadataDeclaresArray()
    {
        QFile file(QStringLiteral(TL_ARRAY_TEST_METADATA));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject metadata = QJsonDocument::fromJson(file.readAll()).object();
        const QJsonObject interface = metadata.value(QStringLiteral("interfaces"))
                                          .toArray().first().toObject();
        QCOMPARE(interface.value(QStringLiteral("requests")).toArray().first().toObject()
                     .value(QStringLiteral("arguments")).toArray().first().toObject()
                     .value(QStringLiteral("type")).toString(),
                 QStringLiteral("array"));
        QCOMPARE(interface.value(QStringLiteral("events")).toArray().first().toObject()
                     .value(QStringLiteral("arguments")).toArray().first().toObject()
                     .value(QStringLiteral("type")).toString(),
                 QStringLiteral("array"));
    }

    void jsonRepresentation_data()
    {
        QTest::addColumn<QJsonValue>("json");
        QTest::addColumn<bool>("valid");

        QTest::newRow("empty") << QJsonValue(QJsonObject{ { "bytes", QJsonArray{} } }) << true;
        QTest::newRow("binary")
            << QJsonValue(QJsonObject{ { "bytes", QJsonArray{ 0, 127, 128, 255 } } }) << true;
        QTest::newRow("null") << QJsonValue(QJsonValue::Null) << false;
        QTest::newRow("bare-array") << QJsonValue(QJsonArray{ 1, 2 }) << false;
        QTest::newRow("floating")
            << QJsonValue(QJsonObject{ { "bytes", QJsonArray{ 1.5 } } }) << false;
        QTest::newRow("negative")
            << QJsonValue(QJsonObject{ { "bytes", QJsonArray{ -1 } } }) << false;
        QTest::newRow("overflow")
            << QJsonValue(QJsonObject{ { "bytes", QJsonArray{ 256 } } }) << false;
        QTest::newRow("string")
            << QJsonValue(QJsonObject{ { "bytes", QJsonArray{ "1" } } }) << false;
        QTest::newRow("extra-member")
            << QJsonValue(QJsonObject{ { "bytes", QJsonArray{} }, { "elements", 0 } }) << false;
    }

    void jsonRepresentation()
    {
        QFETCH(QJsonValue, json);
        QFETCH(bool, valid);
        QByteArray bytes;
        QString error;
        QCOMPARE(protocolArrayFromJson(json, bytes, error), valid);
        if (valid) {
            QCOMPARE(normalizedProtocolArray(bytes.constData(), bytes.size()), json.toObject());
            QVERIFY(error.isEmpty());
        } else {
            QVERIFY(!error.isEmpty());
        }
    }

    void requestAndOwningSnapshot_data()
    {
        QTest::addColumn<QJsonValue>("json");
        QTest::newRow("empty") << QJsonValue(QJsonObject{ { "bytes", QJsonArray{} } });
        QTest::newRow("embedded-zero")
            << QJsonValue(QJsonObject{ { "bytes", QJsonArray{ 1, 0, 2, 255 } } });
        QJsonArray boundary;
        for (int i = 0; i < 1024; ++i)
            boundary.append(i % 256);
        QTest::newRow("one-kibibyte")
            << QJsonValue(QJsonObject{ { "bytes", boundary } });
    }

    void requestAndOwningSnapshot()
    {
        QFETCH(QJsonValue, json);
        QByteArray bytes;
        QString error;
        QVERIFY2(protocolArrayFromJson(json, bytes, error), qPrintable(error));
        const QByteArray expected = bytes;
        wl_array array = borrowedProtocolArray(bytes);

        tl_test_array_clear_events(&m_adapter);
        QCOMPARE(tl_test_array_echo(&m_adapter, &array), 0);
        bytes.fill('\x7f');
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);

        QCOMPARE(m_adapter.array_event_count, size_t(1));
        QVERIFY(!m_adapter.event_snapshot_failed);
        const tl_test_array_array_event &event = m_adapter.array_events[0];
        QCOMPARE(QByteArray(event.name), QByteArray("value"));
        QCOMPARE(event.size, static_cast<size_t>(expected.size()));
        QCOMPARE(QByteArray(reinterpret_cast<const char *>(event.data), static_cast<qsizetype>(event.size)),
                 expected);
        QCOMPARE(normalizedProtocolArray(event.data, static_cast<qsizetype>(event.size)),
                 json.toObject());

        tl_test_array_clear_events(&m_adapter);
        QCOMPARE(m_adapter.array_event_count, size_t(0));
        QCOMPARE(m_adapter.array_events[0].data, nullptr);
        QCOMPARE(m_adapter.array_events[0].size, size_t(0));
    }

    void eventCapacityFailureIsObservable()
    {
        QByteArray byte(1, '\x2a');
        wl_array array = borrowedProtocolArray(byte);
        tl_test_array_clear_events(&m_adapter);
        for (int i = 0; i < TL_TEST_ARRAY_MAX_EVENTS + 1; ++i)
            QCOMPARE(tl_test_array_echo(&m_adapter, &array), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QCOMPARE(m_adapter.array_event_count, size_t(TL_TEST_ARRAY_MAX_EVENTS));
        QVERIFY(m_adapter.event_snapshot_failed);
        tl_test_array_clear_events(&m_adapter);
        QVERIFY(!m_adapter.event_snapshot_failed);
    }

    void nullRequestIsRejected()
    {
        QCOMPARE(tl_test_array_echo(&m_adapter, nullptr), -1);
    }

    void deadTargetIsRejected()
    {
        QCOMPARE(tl_test_array_destroy(&m_adapter), 0);
        QByteArray byte(1, '\x2a');
        wl_array array = borrowedProtocolArray(byte);
        QCOMPARE(tl_test_array_echo(&m_adapter, &array), -1);
    }

private:
    wl_display *m_serverDisplay = nullptr;
    wl_global *m_global = nullptr;
    std::thread m_serverThread;
    wl_display *m_clientDisplay = nullptr;
    wl_registry *m_registry = nullptr;
    tl_test_array_adapter m_adapter{};
};

QTEST_GUILESS_MAIN(ArrayAdapterSelfTest)

#include "arrayadapterselftest.moc"
