// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocoljsonscenario.h"

#include "tl-test-treeland-test-multi-arg-v1.h"
#include "wayland-treeland-test-multi-arg-v1-client-protocol.h"
#include "wayland-treeland-test-multi-arg-v1-server-protocol.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <thread>
#include <wayland-client.h>
#include <wayland-server.h>

// ---- Echo server implementation (protocol-specific) ----

namespace {

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

struct EchoServer {
    wl_display *display = nullptr;
    wl_global *global = nullptr;
    std::thread thread;
    int clientFd = -1;

    ~EchoServer() { stop(); }
    void stop() {
        if (display) wl_display_terminate(display);
        if (thread.joinable()) thread.join();
        if (global) { wl_global_destroy(global); global = nullptr; }
        if (display) { wl_display_destroy(display); display = nullptr; }
    }
};

bool startMultiArgEchoServer(EchoServer &s)
{
    s.display = wl_display_create();
    if (!s.display) return false;
    s.global = wl_global_create(s.display, &treeland_test_multi_arg_v1_interface, 1, nullptr, bindMultiArg);
    if (!s.global) return false;
    int sockets[2] = { -1, -1 };
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) != 0) return false;
    if (!wl_client_create(s.display, sockets[0])) return false;
    s.clientFd = sockets[1];
    s.thread = std::thread([&s] { wl_display_run(s.display); });
    return true;
}

// ---- Event normalization (protocol-specific) ----

QJsonObject normalizeReplyEvent(const tl_test_multi_arg_reply_event &e)
{
    QJsonArray args;
    args.append(QJsonObject{{"type", "uint"}, {"value", static_cast<qint64>(e.id)}});
    args.append(QJsonObject{{"type", "int"}, {"value", static_cast<qint64>(e.offset)}});
    args.append(QJsonObject{{"type", "string"}, {"value", e.name ? QJsonValue(QString::fromUtf8(e.name)) : QJsonValue()}});
    return QJsonObject{{"object", "manager"}, {"event", "reply"}, {"args", args}};
}

QJsonObject collectCheckpoint(const tl_test_multi_arg_adapter &adapter)
{
    QJsonArray events;
    for (size_t i = 0; i < adapter.reply_event_count; ++i)
        events.append(normalizeReplyEvent(adapter.reply_events[i]));
    return QJsonObject{
        {"client_events", QJsonObject{{"ordered", events}}},
        {"connection", QJsonObject{{"display_error", 0}, {"protocol_error", QJsonObject{{"occurred", false}}}}}
    };
}

// ---- JSON step execution (metadata-driven via dispatch) ----

bool executeStep(const QJsonObject &step, tl_test_multi_arg_adapter &adapter, wl_display *display)
{
    const QString type = step.value("type").toString();
    if (type == "request") {
        const QString name = step.value("name").toString();
        const QJsonArray jsonArgs = step.value("args").toArray();

        QVector<QByteArray> stringStorage;
        QVector<const char *> args;
        for (const QJsonValue &v : jsonArgs) {
            if (v.isNull()) {
                args.append(nullptr);
            } else if (v.isString()) {
                QByteArray ba = v.toString().toUtf8();
                stringStorage.append(ba);
                args.append(stringStorage.last().constData());
            } else {
                QByteArray ba = QByteArray::number(static_cast<qlonglong>(v.toDouble()));
                stringStorage.append(ba);
                args.append(stringStorage.last().constData());
            }
        }
        int rc = tl_test_multi_arg_dispatch(&adapter, name.toUtf8().constData(),
                                             const_cast<const char **>(args.constData()),
                                             args.size());
        return rc == 0;
    }
    if (type == "barrier") {
        if (step.value("barrier_type").toString() == "client_roundtrip")
            return wl_display_roundtrip(display) >= 0;
        return true;
    }
    if (type == "checkpoint" || type == "disconnect")
        return true;
    return false;
}

} // namespace

// ---- Main runner entry point ----

ProtocolJsonRunResult runGenericProtocolJsonCase(const ProtocolJsonCase &testCase)
{
    ProtocolJsonRunResult result;
    QElapsedTimer elapsed;
    elapsed.start();

    const QString protocolName = testCase.input.value("protocol").toString();
    if (protocolName != "treeland_test_multi_arg_v1") {
        result.failureCategory = "metadata_validation_error";
        result.failureMessage = "unsupported protocol: " + protocolName;
        result.elapsedMs = elapsed.elapsed();
        return result;
    }

    EchoServer server;
    if (!startMultiArgEchoServer(server)) {
        result.failureCategory = "transport_or_disconnect_error";
        result.failureMessage = "failed to start echo server";
        result.elapsedMs = elapsed.elapsed();
        return result;
    }

    wl_display *clientDisplay = wl_display_connect_to_fd(server.clientFd);
    if (!clientDisplay) {
        result.failureCategory = "transport_or_disconnect_error";
        result.failureMessage = "failed to connect client";
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

    result.checks["socket_created"] = true;
    result.checks["global_advertised"] = true;
    result.checks["client_connected"] = true;

    const QJsonArray steps = testCase.input.value("steps").toArray();
    QJsonObject checkpoints;
    bool failed = false;

    for (const QJsonValue &sv : steps) {
        const QJsonObject step = sv.toObject();
        const QString stepType = step.value("type").toString();

        if (stepType == "checkpoint") {
            const QString cpName = step.value("name").toString();
            checkpoints[cpName] = collectCheckpoint(adapter);
            tl_test_multi_arg_clear_events(&adapter);
        } else if (stepType == "disconnect") {
            tl_test_multi_arg_clear_events(&adapter);
            tl_test_multi_arg_destroy(&adapter);
        } else {
            if (!executeStep(step, adapter, clientDisplay)) {
                result.failureCategory = "adapter_validation_error";
                result.failureMessage = "step execution failed";
                failed = true;
                break;
            }
        }
    }

    QJsonObject actual;
    actual["case"] = testCase.caseId;
    actual["checkpoints"] = checkpoints;

    if (!failed) {
        const QJsonObject expCheckpoints = testCase.expected.value("checkpoints").toObject();
        for (auto it = expCheckpoints.constBegin(); it != expCheckpoints.constEnd(); ++it) {
            const QString cpName = it.key();
            result.checks["checkpoint_" + cpName] = checkpoints.contains(cpName);
            if (!checkpoints.contains(cpName)) {
                result.failureCategory = "checkpoint_event_diff";
                result.failureMessage = "missing checkpoint: " + cpName;
                result.failureCheckpoint = cpName;
                failed = true; break;
            }
            const QJsonObject actualCp = checkpoints[cpName].toObject();
            const QJsonObject expectedCp = it.value().toObject();
            QJsonArray actualEvents = actualCp.value("client_events").toObject().value("ordered").toArray();
            QJsonArray expectedEvents = expectedCp.value("client_events").toObject().value("ordered").toArray();
            if (actualEvents != expectedEvents) {
                result.failureCategory = "checkpoint_event_diff";
                result.failureMessage = "event mismatch at: " + cpName;
                result.failureCheckpoint = cpName;
                result.expectedDifference = QJsonObject{{"client_events", QJsonObject{{"ordered", expectedEvents}}}};
                result.actualDifference = QJsonObject{{"client_events", QJsonObject{{"ordered", actualEvents}}}};
                failed = true; break;
            }
        }
    }

    tl_test_multi_arg_adapter_fini(&adapter);
    if (adapter.proxy) tl_test_multi_arg_destroy(&adapter);
    wl_registry_destroy(registry);
    wl_display_disconnect(clientDisplay);

    result.passed = !failed;
    result.actual = actual;
    result.elapsedMs = elapsed.elapsed();
    return result;
}
