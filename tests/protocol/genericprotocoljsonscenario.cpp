// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocoljsonscenario.h"

#include "tl-test-treeland-test-multi-arg-v1.h"
#include "wayland-treeland-test-multi-arg-v1-client-protocol.h"
#include "wayland-treeland-test-multi-arg-v1-server-protocol.h"

#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <thread>
#include <wayland-client.h>
#include <wayland-server.h>

namespace {

// ---- Uniform registry type (matches generated struct in adapter header) ----
struct ProtocolRegistry {
    const char *protocol_name;
    size_t adapter_size;
    void (*init)(void *);
    void (*fini)(void *);
    int (*bind)(void *, struct wl_registry *, uint32_t);
    void (*clear_events)(void *);
    int (*dispatch)(void *, const char *, const char **, int);
    int (*destroy)(void *);
    const struct wl_registry_listener *(*listener)(void);
};

// ---- Echo server (test fixture protocols only) ----

void multiArgEchoHandler(wl_client *, wl_resource *resource,
                          uint32_t id, int32_t offset, const char *name)
{
    treeland_test_multi_arg_v1_send_reply(resource, id, offset, name);
}
void multiArgDestroyHandler(wl_client *, wl_resource *r) { wl_resource_destroy(r); }

const struct treeland_test_multi_arg_v1_interface multiArgImpl{
    .echo = multiArgEchoHandler, .destroy = multiArgDestroyHandler
};

void bindMultiArg(wl_client *c, void *, uint32_t v, uint32_t id) {
    wl_resource *r = wl_resource_create(c, &treeland_test_multi_arg_v1_interface, (int)v, id);
    wl_resource_set_implementation(r, &multiArgImpl, nullptr, nullptr);
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

bool startEchoServer(EchoServer &s) {
    s.display = wl_display_create();
    if (!s.display) return false;
    s.global = wl_global_create(s.display, &treeland_test_multi_arg_v1_interface, 1, nullptr, bindMultiArg);
    if (!s.global) return false;
    int fds[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) != 0) return false;
    if (!wl_client_create(s.display, fds[0])) return false;
    s.clientFd = fds[1];
    s.thread = std::thread([&s] { wl_display_run(s.display); });
    return true;
}

// ---- Event normalization (protocol-specific, fed from registry metadata) ----

QJsonArray normalizeArgs(const QJsonArray &metaArgs, const tl_test_multi_arg_reply_event &e) {
    QJsonArray out;
    for (int i = 0; i < metaArgs.size(); ++i) {
        QString type = metaArgs[i].toObject().value("type").toString();
        if (type == "uint") out.append(QJsonObject{{"type","uint"},{"value",(qint64)e.id}});
        else if (type == "int") out.append(QJsonObject{{"type","int"},{"value",(qint64)e.offset}});
        else if (type == "string") out.append(QJsonObject{{"type","string"},{"value",e.name?QString::fromUtf8(e.name):QJsonValue()}});
    }
    return out;
}

QJsonObject collectEvents(const tl_test_multi_arg_adapter &adapter, const QJsonArray &metaArgs) {
    QJsonArray events;
    for (size_t i = 0; i < adapter.reply_event_count; ++i)
        events.append(QJsonObject{{"object","manager"},{"event","reply"},{"args",normalizeArgs(metaArgs, adapter.reply_events[i])}});
    return QJsonObject{{"client_events",QJsonObject{{"ordered",events}}},
                       {"connection",QJsonObject{{"display_error",0},{"protocol_error",QJsonObject{{"occurred",false}}}}}};
}

// ---- Step executor (registry-driven) ----

bool execStep(const QJsonObject &step, void *adapter, const ProtocolRegistry &reg, wl_display *dpy) {
    QString type = step.value("type").toString();
    if (type == "request") {
        QString name = step.value("name").toString();
        QJsonArray jargs = step.value("args").toArray();
        QVector<QByteArray> storage;
        QVector<const char *> args;
        for (const QJsonValue &v : jargs) {
            if (v.isNull()) args.append(nullptr);
            else if (v.isString()) { QByteArray b = v.toString().toUtf8(); storage.append(b); args.append(storage.last().constData()); }
            else { QByteArray b = QByteArray::number((qlonglong)v.toDouble()); storage.append(b); args.append(storage.last().constData()); }
        }
        return reg.dispatch(adapter, name.toUtf8().constData(),
                            const_cast<const char **>(args.constData()), args.size()) == 0;
    }
    if (type == "barrier" && step.value("barrier_type").toString() == "client_roundtrip")
        return wl_display_roundtrip(dpy) >= 0;
    return true;
}

} // namespace

// ---- Main entry: registry-driven, protocol-agnostic runner ----

ProtocolJsonRunResult runGenericProtocolJsonCase(const ProtocolJsonCase &testCase)
{
    ProtocolJsonRunResult result;
    QElapsedTimer elapsed;
    elapsed.start();

    // Select registry by protocol name (extend this table for more protocols)
    const ProtocolRegistry *reg = nullptr;
    if (testCase.input.value("protocol").toString() == "treeland_test_multi_arg_v1")
        reg = reinterpret_cast<const ProtocolRegistry *>(&tl_test_multi_arg_registry);

    if (!reg) {
        result.failureCategory = "metadata_validation_error";
        result.failureMessage = "no registry for protocol";
        result.elapsedMs = elapsed.elapsed();
        return result;
    }

    EchoServer server;
    if (!startEchoServer(server)) {
        result.failureCategory = "transport_or_disconnect_error";
        result.elapsedMs = elapsed.elapsed();
        return result;
    }

    wl_display *cdpy = wl_display_connect_to_fd(server.clientFd);
    if (!cdpy) {
        result.failureCategory = "transport_or_disconnect_error";
        result.elapsedMs = elapsed.elapsed();
        return result;
    }

    wl_registry *registry = wl_display_get_registry(cdpy);

    // Allocate adapter from registry
    QByteArray adapterBuf((int)reg->adapter_size, '\0');
    void *adapter = adapterBuf.data();
    reg->init(adapter);

    wl_registry_add_listener(registry, reg->listener(), adapter);
    wl_display_roundtrip(cdpy);
    reg->bind(adapter, registry, 1);
    wl_display_roundtrip(cdpy);

    result.checks["socket_created"] = true;
    result.checks["client_connected"] = true;

    // Collect event metadata for normalization
    QJsonArray eventMetaArgs;
    for (const QJsonValue &iface : testCase.metadata.value("interfaces").toArray()) {
        if (iface.toObject().value("name").toString() == reg->protocol_name) {
            for (const QJsonValue &ev : iface.toObject().value("events").toArray())
                if (ev.toObject().value("name").toString() == "reply")
                    eventMetaArgs = ev.toObject().value("arguments").toArray();
            break;
        }
    }

    QJsonObject checkpoints;
    bool failed = false;

    for (const QJsonValue &sv : testCase.input.value("steps").toArray()) {
        QJsonObject step = sv.toObject();
        QString st = step.value("type").toString();
        if (st == "checkpoint") {
            checkpoints[step.value("name").toString()] =
                collectEvents(*static_cast<tl_test_multi_arg_adapter *>(adapter), eventMetaArgs);
            reg->clear_events(adapter);
        } else if (st == "disconnect") {
            reg->clear_events(adapter);
            reg->destroy(adapter);
        } else {
            if (!execStep(step, adapter, *reg, cdpy)) {
                result.failureCategory = "adapter_validation_error";
                failed = true; break;
            }
        }
    }

    QJsonObject actual;
    actual["case"] = testCase.caseId;
    actual["checkpoints"] = checkpoints;

    if (!failed) {
        QJsonObject expCps = testCase.expected.value("checkpoints").toObject();
        for (auto it = expCps.constBegin(); it != expCps.constEnd(); ++it) {
            if (!checkpoints.contains(it.key())) {
                result.failureCategory = "checkpoint_event_diff";
                result.failureCheckpoint = it.key();
                failed = true; break;
            }
            if (checkpoints[it.key()] != it.value()) {
                result.failureCategory = "checkpoint_event_diff";
                result.failureCheckpoint = it.key();
                result.expectedDifference = it.value().toObject();
                result.actualDifference = checkpoints[it.key()].toObject();
                failed = true; break;
            }
        }
    }

    reg->fini(adapter);
    if (*static_cast<bool *>(adapter)) // proxy check — protocol-specific, simplified
        reg->destroy(adapter);
    wl_registry_destroy(registry);
    wl_display_disconnect(cdpy);

    result.passed = !failed;
    result.actual = actual;
    result.elapsedMs = elapsed.elapsed();
    return result;
}
