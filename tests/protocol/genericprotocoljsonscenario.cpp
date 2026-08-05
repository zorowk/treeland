// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocoljsonscenario.h"

#include "clientruntime.h"
#include "tl-test-treeland-test-multi-arg-v1.h"
#include "wayland-treeland-test-multi-arg-v1-client-protocol.h"
#include "wayland-treeland-test-multi-arg-v1-server-protocol.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QThread>

#include <cstring>
#include <sys/socket.h>
#include <wayland-client.h>
#include <wayland-server.h>

namespace {

// ---- Generic echo server ----

struct EchoServer
{
    wl_display *display = nullptr;
    wl_global *global = nullptr;
    std::thread thread;
    int clientFd = -1;

    ~EchoServer() { stop(); }

    void stop()
    {
        if (display) {
            wl_display_terminate(display);
            if (thread.joinable())
                thread.join();
        }
        if (global) {
            wl_global_destroy(global);
            global = nullptr;
        }
        if (display) {
            wl_display_destroy(display);
            display = nullptr;
        }
    }
};

void multiArgEchoHandler(wl_client *, wl_resource *resource,
                          uint32_t id, int32_t offset, const char *name)
{
    treeland_test_multi_arg_v1_send_reply(resource, id, offset, name);
}

void multiArgDestroyHandler(wl_client *, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

const struct treeland_test_multi_arg_v1_interface multiArgImpl{
    .echo = multiArgEchoHandler,
    .destroy = multiArgDestroyHandler,
};

void bindMultiArg(wl_client *client, void *, uint32_t version, uint32_t id)
{
    wl_resource *res = wl_resource_create(
        client, &treeland_test_multi_arg_v1_interface, static_cast<int>(version), id);
    wl_resource_set_implementation(res, &multiArgImpl, nullptr, nullptr);
}

bool startMultiArgEchoServer(EchoServer &server)
{
    server.display = wl_display_create();
    if (!server.display)
        return false;

    server.global = wl_global_create(server.display,
                                     &treeland_test_multi_arg_v1_interface,
                                     1, nullptr, bindMultiArg);
    if (!server.global)
        return false;

    int sockets[2] = { -1, -1 };
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) != 0)
        return false;
    if (!wl_client_create(server.display, sockets[0]))
        return false;

    server.clientFd = sockets[1];
    server.thread = std::thread([&server] { wl_display_run(server.display); });
    return true;
}

// ---- Event normalization ----

QJsonObject normalizeMultiArgReplyEvent(const tl_test_multi_arg_reply_event &e)
{
    QJsonArray args;
    args.append(QJsonObject{ { QStringLiteral("type"), QStringLiteral("uint") },
                             { QStringLiteral("value"), static_cast<qint64>(e.id) } });
    args.append(QJsonObject{ { QStringLiteral("type"), QStringLiteral("int") },
                             { QStringLiteral("value"), static_cast<qint64>(e.offset) } });
    if (e.name)
        args.append(QJsonObject{ { QStringLiteral("type"), QStringLiteral("string") },
                                 { QStringLiteral("value"), QString::fromUtf8(e.name) } });
    else
        args.append(QJsonObject{ { QStringLiteral("type"), QStringLiteral("string") },
                                 { QStringLiteral("value"), QJsonValue::Null } });

    return QJsonObject{
        { QStringLiteral("object"), QStringLiteral("manager") },
        { QStringLiteral("event"), QStringLiteral("reply") },
        { QStringLiteral("args"), args },
    };
}

QJsonObject collectMultiArgCheckpoint(const QString &name,
                                      const tl_test_multi_arg_adapter &adapter)
{
    QJsonArray events;
    for (size_t i = 0; i < adapter.reply_event_count; ++i)
        events.append(normalizeMultiArgReplyEvent(adapter.reply_events[i]));

    QJsonObject checkpoint;
    checkpoint[QStringLiteral("name")] = name;
    checkpoint[QStringLiteral("client_events")] = QJsonObject{
        { QStringLiteral("ordered"), events },
    };
    checkpoint[QStringLiteral("connection")] = QJsonObject{
        { QStringLiteral("display_error"), 0 },
        { QStringLiteral("protocol_error"), QJsonObject{ { QStringLiteral("occurred"), false } } },
    };
    return checkpoint;
}

// ---- JSON step execution ----

struct StepResult { bool ok = true; QString error; };

StepResult executeStep(const QJsonObject &step,
                       tl_test_multi_arg_adapter &adapter,
                       wl_display *display)
{
    const QString type = step.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("request")) {
        const QString name = step.value(QStringLiteral("name")).toString();
        if (name == QStringLiteral("echo")) {
            const QJsonArray args = step.value(QStringLiteral("args")).toArray();
            if (args.size() != 3)
                return { false, QStringLiteral("echo requires 3 args") };
            uint32_t id = static_cast<uint32_t>(args.at(0).toInt());
            int32_t offset = static_cast<int32_t>(args.at(1).toInt());
            QByteArray nameStr = args.at(2).toString().toUtf8();
            int rc = tl_test_multi_arg_echo(&adapter, id, offset,
                                            args.at(2).isNull() ? nullptr : nameStr.constData());
            if (rc != 0)
                return { false, QStringLiteral("echo request failed") };
        }
    } else if (type == QStringLiteral("barrier")) {
        const QString barrierType = step.value(QStringLiteral("barrier_type")).toString();
        if (barrierType == QStringLiteral("client_roundtrip")) {
            if (wl_display_roundtrip(display) < 0)
                return { false, QStringLiteral("roundtrip failed") };
        }
    } else if (type == QStringLiteral("checkpoint")) {
        // Checkpoint data is collected after all steps
    } else if (type == QStringLiteral("disconnect")) {
        tl_test_multi_arg_clear_events(&adapter);
        tl_test_multi_arg_destroy(&adapter);
    }
    return { true, {} };
}


} // namespace


ProtocolJsonRunResult runGenericProtocolJsonCase(const ProtocolJsonCase &testCase)
{
    ProtocolJsonRunResult result;
    QElapsedTimer elapsed;
    elapsed.start();

    const QString protocolName = testCase.input
        .value(QStringLiteral("protocol")).toString();
    if (protocolName != QStringLiteral("treeland_test_multi_arg_v1")) {
        result.failureCategory = QStringLiteral("metadata_validation_error");
        result.failureMessage = QStringLiteral("unsupported protocol: ") + protocolName;
        result.elapsedMs = elapsed.elapsed();
        return result;
    }

    EchoServer server;
    if (!startMultiArgEchoServer(server)) {
        result.failureCategory = QStringLiteral("transport_or_disconnect_error");
        result.failureMessage = QStringLiteral("failed to start echo server");
        result.elapsedMs = elapsed.elapsed();
        return result;
    }

    wl_display *clientDisplay = wl_display_connect_to_fd(server.clientFd);
    if (!clientDisplay) {
        result.failureCategory = QStringLiteral("transport_or_disconnect_error");
        result.failureMessage = QStringLiteral("failed to connect client");
        result.elapsedMs = elapsed.elapsed();
        return result;
    }

    wl_registry *registry = wl_display_get_registry(clientDisplay);
    tl_test_multi_arg_adapter adapter{};
    tl_test_multi_arg_adapter_init(&adapter);
    wl_registry_add_listener(registry, tl_test_multi_arg_registry_listener(), &adapter);
    wl_display_roundtrip(clientDisplay);
    tl_test_multi_arg_bind(&adapter, registry, 1);
    wl_display_roundtrip(clientDisplay);

    result.checks[QStringLiteral("socket_created")] = true;
    result.checks[QStringLiteral("global_advertised")] = true;
    result.checks[QStringLiteral("client_connected")] = true;

    const QJsonArray steps = testCase.input.value(QStringLiteral("steps")).toArray();
    QJsonObject checkpoints;
    bool failed = false;

    for (const QJsonValue &stepVal : steps) {
        const QJsonObject step = stepVal.toObject();
        const QString stepType = step.value(QStringLiteral("type")).toString();

        if (stepType == QStringLiteral("checkpoint")) {
            const QString name = step.value(QStringLiteral("name")).toString();
            checkpoints[name] = collectMultiArgCheckpoint(name, adapter);
            tl_test_multi_arg_clear_events(&adapter);
        } else {
            StepResult sr = executeStep(step, adapter, clientDisplay);
            if (!sr.ok) {
                result.failureCategory = QStringLiteral("adapter_validation_error");
                result.failureMessage = sr.error;
                failed = true;
                break;
            }
        }
    }

    // Build actual
    QJsonObject actual;
    actual[QStringLiteral("case")] = testCase.caseId;
    actual[QStringLiteral("checkpoints")] = checkpoints;

    if (!failed) {
        // Compare with expected
        const QJsonObject expectedCheckpoints = testCase.expected
            .value(QStringLiteral("checkpoints")).toObject();
        for (auto it = expectedCheckpoints.constBegin();
             it != expectedCheckpoints.constEnd(); ++it) {
            const QString cpName = it.key();
            result.checks[QStringLiteral("checkpoint_") + cpName] = checkpoints.contains(cpName);

            if (!checkpoints.contains(cpName)) {
                result.failureCategory = QStringLiteral("checkpoint_event_diff");
                result.failureMessage = QStringLiteral("missing checkpoint: ") + cpName;
                result.failureCheckpoint = cpName;
                failed = true;
                break;
            }

            const QJsonObject actualCp = checkpoints[cpName].toObject();
            const QJsonObject expectedCp = it.value().toObject();

            // Compare client_events
            const QJsonArray actualEvents = actualCp
                .value(QStringLiteral("client_events")).toObject()
                .value(QStringLiteral("ordered")).toArray();
            const QJsonArray expectedEvents = expectedCp
                .value(QStringLiteral("client_events")).toObject()
                .value(QStringLiteral("ordered")).toArray();

            if (actualEvents != expectedEvents) {
                result.failureCategory = QStringLiteral("checkpoint_event_diff");
                result.failureMessage = QStringLiteral("event mismatch at checkpoint: ") + cpName;
                result.failureCheckpoint = cpName;
                result.expectedDifference = QJsonObject{
                    { QStringLiteral("client_events"), QJsonObject{
                        { QStringLiteral("ordered"), expectedEvents } } } };
                result.actualDifference = QJsonObject{
                    { QStringLiteral("client_events"), QJsonObject{
                        { QStringLiteral("ordered"), actualEvents } } } };
                failed = true;
                break;
            }
        }
    }

    // Teardown
    tl_test_multi_arg_adapter_fini(&adapter);
    if (adapter.proxy)
        tl_test_multi_arg_destroy(&adapter);
    wl_registry_destroy(registry);
    wl_display_disconnect(clientDisplay);

    result.passed = !failed;
    result.actual = actual;
    result.elapsedMs = elapsed.elapsed();
    return result;
}
