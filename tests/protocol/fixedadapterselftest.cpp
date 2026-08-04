// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocolfixedvalue.h"
#include "tl-test-treeland-test-fixed-v1.h"
#include "wayland-treeland-test-fixed-v1-client-protocol.h"
#include "wayland-treeland-test-fixed-v1-server-protocol.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

#include <limits>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-server.h>

namespace {
void handleEcho(wl_client *, wl_resource *resource, wl_fixed_t value)
{
    treeland_test_fixed_v1_send_value(resource, value);
}

void handleDestroy(wl_client *, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

const struct treeland_test_fixed_v1_interface serverImplementation{
    .echo = handleEcho,
    .destroy = handleDestroy,
};

void bindFixed(wl_client *client, void *, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(
        client, &treeland_test_fixed_v1_interface, static_cast<int>(version), id);
    wl_resource_set_implementation(resource, &serverImplementation, nullptr, nullptr);
}
}

class FixedAdapterSelfTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_serverDisplay = wl_display_create();
        QVERIFY(m_serverDisplay);
        m_global = wl_global_create(m_serverDisplay,
                                    &treeland_test_fixed_v1_interface,
                                    1,
                                    nullptr,
                                    bindFixed);
        QVERIFY(m_global);

        int sockets[2] = { -1, -1 };
        QCOMPARE(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), 0);
        QVERIFY(wl_client_create(m_serverDisplay, sockets[0]));
        m_serverThread = std::thread([this] { wl_display_run(m_serverDisplay); });

        m_clientDisplay = wl_display_connect_to_fd(sockets[1]);
        QVERIFY(m_clientDisplay);
        m_registry = wl_display_get_registry(m_clientDisplay);
        QVERIFY(m_registry);
        tl_test_fixed_adapter_init(&m_adapter);
        QCOMPARE(wl_registry_add_listener(
                     m_registry, tl_test_fixed_registry_listener(), &m_adapter),
                 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QCOMPARE(tl_test_fixed_bind(&m_adapter, m_registry, 1), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
    }

    void cleanupTestCase()
    {
        if (m_adapter.proxy)
            tl_test_fixed_destroy(&m_adapter);
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

    void metadataDeclaresFixed()
    {
        QFile file(QStringLiteral(TL_FIXED_TEST_METADATA));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject metadata = QJsonDocument::fromJson(file.readAll()).object();
        const QJsonObject interface = metadata.value(QStringLiteral("interfaces"))
                                          .toArray().first().toObject();
        QCOMPARE(interface.value(QStringLiteral("requests")).toArray().first().toObject()
                     .value(QStringLiteral("arguments")).toArray().first().toObject()
                     .value(QStringLiteral("type")).toString(),
                 QStringLiteral("fixed"));
        QCOMPARE(interface.value(QStringLiteral("events")).toArray().first().toObject()
                     .value(QStringLiteral("arguments")).toArray().first().toObject()
                     .value(QStringLiteral("type")).toString(),
                 QStringLiteral("fixed"));
    }

    void jsonRepresentation_data()
    {
        QTest::addColumn<QJsonValue>("json");
        QTest::addColumn<bool>("valid");
        QTest::addColumn<qint64>("expectedRaw");

        QTest::newRow("zero") << QJsonValue(QJsonObject{ { "raw", 0 } }) << true << qint64(0);
        QTest::newRow("one-and-a-half")
            << QJsonValue(QJsonObject{ { "raw", 384 } }) << true << qint64(384);
        QTest::newRow("minimum")
            << QJsonValue(QJsonObject{ { "raw", qint64(std::numeric_limits<int32_t>::min()) } })
            << true << qint64(std::numeric_limits<int32_t>::min());
        QTest::newRow("maximum")
            << QJsonValue(QJsonObject{ { "raw", qint64(std::numeric_limits<int32_t>::max()) } })
            << true << qint64(std::numeric_limits<int32_t>::max());
        QTest::newRow("null") << QJsonValue(QJsonValue::Null) << false << qint64(0);
        QTest::newRow("floating") << QJsonValue(QJsonObject{ { "raw", 1.5 } })
                                  << false << qint64(0);
        QTest::newRow("out-of-range")
            << QJsonValue(QJsonObject{ { "raw", 2147483648.0 } }) << false << qint64(0);
        QTest::newRow("extra-member")
            << QJsonValue(QJsonObject{ { "raw", 0 }, { "unit", "double" } })
            << false << qint64(0);
    }

    void jsonRepresentation()
    {
        QFETCH(QJsonValue, json);
        QFETCH(bool, valid);
        QFETCH(qint64, expectedRaw);
        int32_t raw = 0;
        QString error;
        QCOMPARE(protocolFixedFromJson(json, raw, error), valid);
        if (valid) {
            QCOMPARE(qint64(raw), expectedRaw);
            QCOMPARE(normalizedProtocolFixed(raw),
                     QJsonObject({ { QStringLiteral("raw"), expectedRaw } }));
            QVERIFY(error.isEmpty());
        } else {
            QVERIFY(!error.isEmpty());
        }
    }

    void requestAndOwningSnapshot_data()
    {
        QTest::addColumn<QJsonValue>("json");
        QTest::newRow("zero") << QJsonValue(QJsonObject{ { "raw", 0 } });
        QTest::newRow("negative-fraction") << QJsonValue(QJsonObject{ { "raw", -128 } });
        QTest::newRow("one-and-a-half") << QJsonValue(QJsonObject{ { "raw", 384 } });
        QTest::newRow("minimum")
            << QJsonValue(QJsonObject{ { "raw", qint64(std::numeric_limits<int32_t>::min()) } });
        QTest::newRow("maximum")
            << QJsonValue(QJsonObject{ { "raw", qint64(std::numeric_limits<int32_t>::max()) } });
    }

    void requestAndOwningSnapshot()
    {
        QFETCH(QJsonValue, json);
        int32_t raw = 0;
        QString error;
        QVERIFY2(protocolFixedFromJson(json, raw, error), qPrintable(error));
        tl_test_fixed_clear_events(&m_adapter);
        QCOMPARE(tl_test_fixed_echo(&m_adapter, raw), 0);
        QVERIFY(wl_display_roundtrip(m_clientDisplay) >= 0);
        QCOMPARE(m_adapter.fixed_event_count, size_t(1));
        QCOMPARE(QByteArray(m_adapter.fixed_events[0].name), QByteArray("value"));
        QCOMPARE(m_adapter.fixed_events[0].raw, raw);
        QCOMPARE(normalizedProtocolFixed(m_adapter.fixed_events[0].raw),
                 normalizedProtocolFixed(raw));
    }

    void deadTargetIsRejected()
    {
        QCOMPARE(tl_test_fixed_destroy(&m_adapter), 0);
        QCOMPARE(tl_test_fixed_echo(&m_adapter, wl_fixed_from_int(1)), -1);
    }

private:
    wl_display *m_serverDisplay = nullptr;
    wl_global *m_global = nullptr;
    std::thread m_serverThread;
    wl_display *m_clientDisplay = nullptr;
    wl_registry *m_registry = nullptr;
    tl_test_fixed_adapter m_adapter{};
};

QTEST_GUILESS_MAIN(FixedAdapterSelfTest)

#include "fixedadapterselftest.moc"
