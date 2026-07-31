// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wirescenario.h"

#include "modules/window-management/windowmanagementinterfacev1.h"

#include <wserver.h>
#include <wsocket.h>

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

using namespace WAYLIB_SERVER_NAMESPACE;

namespace {
constexpr quint32 Normal = 0;
constexpr quint32 Show = 1;

QString buildType()
{
#ifdef TL_PROTOCOL_TEST_BUILD_TYPE
    const QString configuredType = QStringLiteral(TL_PROTOCOL_TEST_BUILD_TYPE);
    return configuredType.isEmpty() ? QStringLiteral("unspecified") : configuredType;
#else
    return QStringLiteral("unknown");
#endif
}

QString gitCommit()
{
#ifdef TL_PROTOCOL_TEST_GIT_COMMIT
    return QStringLiteral(TL_PROTOCOL_TEST_GIT_COMMIT);
#else
    return QStringLiteral("unknown");
#endif
}

QJsonArray desktopEvents(const QVector<quint32> &states)
{
    QJsonArray events;
    for (const quint32 state : states) {
        events.append(QJsonObject{
            { QStringLiteral("event"), QStringLiteral("show_desktop") },
            { QStringLiteral("state"), static_cast<qint64>(state) },
        });
    }
    return events;
}
}

QJsonObject WireScenarioResult::toJson() const
{
    QJsonObject checkObject;
    for (auto it = checks.cbegin(); it != checks.cend(); ++it)
        checkObject.insert(it.key(), it.value());

    QJsonObject metrics{
        { QStringLiteral("elapsed_ms"), elapsedMs },
        { QStringLiteral("wayland_client_count_before"), clientCountBefore },
        { QStringLiteral("wayland_client_count_after"), clientCountAfter },
        { QStringLiteral("window_management_resource_count_before"), resourceCountBefore },
        { QStringLiteral("window_management_resource_count_after"), resourceCountAfter },
    };

    QJsonObject versions{
        { QStringLiteral("advertised"), static_cast<qint64>(advertisedVersion) },
        { QStringLiteral("bound"), static_cast<qint64>(boundVersion) },
    };

    QJsonObject connection{
        { QStringLiteral("display_error"), displayError },
        { QStringLiteral("protocol_error_occurred"), protocolErrorOccurred },
        { QStringLiteral("protocol_error_interface"), protocolErrorInterface },
        { QStringLiteral("protocol_error_object_id"),
          static_cast<qint64>(protocolErrorObjectId) },
        { QStringLiteral("protocol_error_code"), static_cast<qint64>(protocolErrorCode) },
    };

    QJsonObject checkpoints{
        { QStringLiteral("initial-state"),
          QJsonObject{ { QStringLiteral("client_events"), desktopEvents(initialEvents) } } },
        { QStringLiteral("request-state"),
          QJsonObject{ { QStringLiteral("client_events"), desktopEvents(requestEvents) } } },
    };

    QJsonObject request{
        { QStringLiteral("name"), QStringLiteral("set_desktop") },
        { QStringLiteral("state"), static_cast<qint64>(requestedDesktopState) },
    };

    QJsonObject serverState{
        { QStringLiteral("desktop_state_changed_count"), desktopStateChangedCount },
        { QStringLiteral("desktop_state"), static_cast<qint64>(serverDesktopState) },
    };

    QJsonObject lifecycle{
        { QStringLiteral("local_proxy_alive_before_destroy"), localProxyAliveBeforeDestroy },
        { QStringLiteral("protocol_destructor_sent"), protocolDestructorSent },
        { QStringLiteral("local_proxy_alive_after_destroy"), localProxyAliveAfterDestroy },
        { QStringLiteral("remote_resource_count_after_destroy"), resourceCountAfterDestroy },
    };

    QJsonObject object{
        { QStringLiteral("stage"), QStringLiteral("poc-0a") },
        { QStringLiteral("case"), QStringLiteral("window-management.set-desktop-show") },
        { QStringLiteral("result"), passed ? QStringLiteral("pass") : QStringLiteral("fail") },
        { QStringLiteral("git_commit"), gitCommit() },
        { QStringLiteral("build_type"), buildType() },
        { QStringLiteral("sanitizer"), QStringLiteral("none") },
        { QStringLiteral("checks"), checkObject },
        { QStringLiteral("versions"), versions },
        { QStringLiteral("checkpoints"), checkpoints },
        { QStringLiteral("request"), request },
        { QStringLiteral("server_state"), serverState },
        { QStringLiteral("lifecycle"), lifecycle },
        { QStringLiteral("connection"), connection },
        { QStringLiteral("metrics"), metrics },
    };

    if (!passed) {
        object.insert(QStringLiteral("failure_category"), failureCategory);
        object.insert(QStringLiteral("failure_message"), failureMessage);
        if (failureCategory == QStringLiteral("checkpoint_event_diff")) {
            object.insert(QStringLiteral("failure_checkpoint"),
                          QStringLiteral("request-state"));
            object.insert(QStringLiteral("expected"),
                          QJsonObject{
                              { QStringLiteral("client_events"),
                                desktopEvents({ expectedRequestEvent }) },
                          });
            object.insert(QStringLiteral("actual"),
                          QJsonObject{
                              { QStringLiteral("client_events"),
                                desktopEvents(requestEvents) },
                          });
        }
    }
    return object;
}

WireScenario::WireScenario()
    : m_clientThread(new QThread)
    , m_worker(new ClientWorker)
{
    qRegisterMetaType<ClientStepResult>();
    m_worker->moveToThread(m_clientThread);
    QObject::connect(m_clientThread, &QThread::finished,
                     m_worker, &QObject::deleteLater);
    m_clientThread->start();
}

WireScenario::~WireScenario()
{
    m_clientThread->quit();
    m_clientThread->wait();
    delete m_clientThread;
}

ClientStepResult WireScenario::waitForStep(const QString &step,
                                           const std::function<void()> &start,
                                           int timeoutMs)
{
    ClientStepResult received;
    bool completed = false;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    const auto stepConnection = QObject::connect(
        m_worker, &ClientWorker::stepFinished, &loop,
        [&](const ClientStepResult &result) {
            if (result.step != step)
                return;
            received = result;
            completed = true;
            loop.quit();
        });
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    start();
    loop.exec();
    QObject::disconnect(stepConnection);

    if (!completed) {
        received.step = step;
        received.errorCategory = QStringLiteral("timeout");
        received.errorMessage =
            QStringLiteral("Timed out waiting for client step '%1'").arg(step);
    }
    return received;
}

bool WireScenario::waitForCondition(const std::function<bool()> &condition,
                                    QObject *notifier,
                                    const char *signal,
                                    int timeoutMs)
{
    if (condition())
        return true;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    const auto conditionConnection = QObject::connect(
        notifier, signal, &loop, SLOT(quit()));
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    while (!condition() && timer.isActive())
        loop.exec();

    QObject::disconnect(conditionConnection);
    return condition();
}

void WireScenario::recordCheck(WireScenarioResult &result,
                               const QString &name,
                               bool passed,
                               const QString &category,
                               const QString &message)
{
    result.checks.insert(name, passed);
    if (passed || !result.failureCategory.isEmpty())
        return;

    result.failureCategory = category;
    result.failureMessage = message;
}

WireScenarioResult WireScenario::run(quint32 expectedRequestEvent)
{
    WireScenarioResult result;
    result.requestedDesktopState = Show;
    result.expectedRequestEvent = expectedRequestEvent;
    QElapsedTimer elapsed;
    elapsed.start();

    QTemporaryDir runtimeDirectory;
    WServer server;
    WSocket socket(false);
    auto *protocol = server.attach<WindowManagementInterfaceV1>(&server);

    recordCheck(result,
                QStringLiteral("runtime_directory_created"),
                runtimeDirectory.isValid(),
                QStringLiteral("server_crash_or_exit"),
                QStringLiteral("Unable to create the isolated runtime directory"));
    const bool socketCreated =
        runtimeDirectory.isValid() && socket.autoCreate(runtimeDirectory.path());
    recordCheck(result,
                QStringLiteral("socket_created"),
                socketCreated,
                QStringLiteral("server_crash_or_exit"),
                QStringLiteral("Unable to create the isolated Wayland socket"));

    if (socketCreated) {
        server.addSocket(&socket);
        server.start();
    }

    result.clientCountBefore = socket.clients().size();
    result.resourceCountBefore = protocol->resourceCount();
    recordCheck(result,
                QStringLiteral("baseline_resources"),
                result.clientCountBefore == 0 && result.resourceCountBefore == 0,
                QStringLiteral("resource_leak_or_lifecycle_error"),
                QStringLiteral("Client or resource baseline was not zero"));

    bool clientConnected = false;
    bool displayConnected = false;
    bool proxyAlive = false;
    if (socketCreated) {
        const ClientStepResult initial = waitForStep(
            QStringLiteral("bind"),
            [this, path = socket.fullServerName()] {
                QMetaObject::invokeMethod(
                    m_worker,
                    [worker = m_worker, path] { worker->connectAndBind(path); },
                    Qt::QueuedConnection);
            });
        result.advertisedVersion = initial.advertisedVersion;
        result.boundVersion = initial.boundVersion;
        result.displayError = initial.displayError;
        result.protocolErrorOccurred = initial.protocolErrorOccurred;
        result.protocolErrorInterface = initial.protocolErrorInterface;
        result.protocolErrorObjectId = initial.protocolErrorObjectId;
        result.protocolErrorCode = initial.protocolErrorCode;
        result.initialEvents = initial.events;
        clientConnected = initial.ok;
        displayConnected = initial.displayConnected;
        proxyAlive = initial.localProxyAlive;

        recordCheck(result,
                    QStringLiteral("client_connected"),
                    initial.ok,
                    initial.errorCategory.isEmpty() ? QStringLiteral("transport_or_disconnect_error")
                                                    : initial.errorCategory,
                    initial.errorMessage);
        recordCheck(result,
                    QStringLiteral("global_advertised"),
                    initial.advertisedVersion == WindowManagementInterfaceV1::InterfaceVersion,
                    QStringLiteral("adapter_validation_error"),
                    QStringLiteral("Unexpected advertised protocol version"));
        recordCheck(result,
                    QStringLiteral("version_bound"),
                    initial.boundVersion == WindowManagementInterfaceV1::InterfaceVersion,
                    QStringLiteral("adapter_validation_error"),
                    QStringLiteral("Unexpected bound protocol version"));
        recordCheck(result,
                    QStringLiteral("initial_state_checkpoint"),
                    initial.events == QVector<quint32>{ Normal },
                    QStringLiteral("checkpoint_event_diff"),
                    QStringLiteral("Initial checkpoint did not contain exactly show_desktop(Normal)"));
        recordCheck(result,
                    QStringLiteral("resource_created"),
                    protocol->resourceCount() == 1 && socket.clients().size() == 1,
                    QStringLiteral("resource_leak_or_lifecycle_error"),
                    QStringLiteral("Bound client/resource count did not reach one"));
    }

    QSignalSpy stateSpy(protocol, &WindowManagementInterfaceV1::desktopStateChanged);
    if (clientConnected && proxyAlive) {
        const ClientStepResult request = waitForStep(
            QStringLiteral("set_desktop"),
            [this] {
                QMetaObject::invokeMethod(
                    m_worker,
                    [worker = m_worker] { worker->setDesktop(Show); },
                    Qt::QueuedConnection);
            });
        result.displayError = request.displayError;
        result.protocolErrorOccurred = request.protocolErrorOccurred;
        result.protocolErrorInterface = request.protocolErrorInterface;
        result.protocolErrorObjectId = request.protocolErrorObjectId;
        result.protocolErrorCode = request.protocolErrorCode;
        result.requestEvents = request.events;
        result.desktopStateChangedCount = stateSpy.size();
        result.serverDesktopState = static_cast<quint32>(protocol->desktopState());

        recordCheck(result,
                    QStringLiteral("request_roundtrip"),
                    request.ok,
                    request.errorCategory.isEmpty() ? QStringLiteral("transport_or_disconnect_error")
                                                    : request.errorCategory,
                    request.errorMessage);
        recordCheck(result,
                    QStringLiteral("request_event_checkpoint"),
                    request.events == QVector<quint32>{ expectedRequestEvent },
                    QStringLiteral("checkpoint_event_diff"),
                    QStringLiteral("Request checkpoint event differs from expected state"));
        recordCheck(result,
                    QStringLiteral("request_received"),
                    stateSpy.size() == 1,
                    QStringLiteral("checkpoint_probe_diff"),
                    QStringLiteral("desktopStateChanged was not emitted exactly once"));
        recordCheck(result,
                    QStringLiteral("server_state_changed"),
                    protocol->desktopState() == WindowManagementInterfaceV1::DesktopState::Show,
                    QStringLiteral("checkpoint_probe_diff"),
                    QStringLiteral("Protocol state did not become DesktopState::Show"));
        recordCheck(result,
                    QStringLiteral("connection_clean"),
                    request.displayError == 0 && !request.protocolErrorOccurred,
                    QStringLiteral("wayland_protocol_error"),
                    QStringLiteral("Wayland display or protocol error occurred"));
    }

    if (proxyAlive) {
        result.localProxyAliveBeforeDestroy = true;
        const ClientStepResult destroyed = waitForStep(
            QStringLiteral("destroy"),
            [this] {
                QMetaObject::invokeMethod(
                    m_worker,
                    [worker = m_worker] { worker->destroyProtocol(); },
                    Qt::QueuedConnection);
            });
        proxyAlive = destroyed.localProxyAlive;
        result.localProxyAliveAfterDestroy = destroyed.localProxyAlive;
        result.protocolDestructorSent = destroyed.protocolDestructorSent;
        result.resourceCountAfterDestroy = protocol->resourceCount();
        recordCheck(result,
                    QStringLiteral("protocol_destructor"),
                    destroyed.ok && destroyed.protocolDestructorSent && !destroyed.localProxyAlive,
                    destroyed.errorCategory.isEmpty()
                        ? QStringLiteral("resource_leak_or_lifecycle_error")
                        : destroyed.errorCategory,
                    destroyed.errorMessage.isEmpty()
                        ? QStringLiteral("Protocol destructor did not retire the local proxy")
                        : destroyed.errorMessage);
        recordCheck(result,
                    QStringLiteral("remote_resource_destroyed"),
                    protocol->resourceCount() == 0,
                    QStringLiteral("resource_leak_or_lifecycle_error"),
                    QStringLiteral("Server resource count did not return to zero after destructor"));
    }

    if (displayConnected) {
        const ClientStepResult disconnected = waitForStep(
            QStringLiteral("disconnect"),
            [this] {
                QMetaObject::invokeMethod(
                    m_worker,
                    [worker = m_worker] { worker->disconnectClient(); },
                    Qt::QueuedConnection);
            });
        const bool clientsRestored = disconnected.ok
            && waitForCondition([&socket] { return socket.clients().isEmpty(); },
                                &socket,
                                SIGNAL(clientsChanged()));
        recordCheck(result,
                    QStringLiteral("client_disconnected"),
                    clientsRestored,
                    disconnected.errorCategory.isEmpty()
                        ? QStringLiteral("resource_leak_or_lifecycle_error")
                        : disconnected.errorCategory,
                    disconnected.errorMessage.isEmpty()
                        ? QStringLiteral("Server client count did not return to zero")
                        : disconnected.errorMessage);
    }

    result.clientCountAfter = socket.clients().size();
    result.resourceCountAfter = protocol->resourceCount();
    recordCheck(result,
                QStringLiteral("resources_restored"),
                result.clientCountAfter == result.clientCountBefore
                    && result.resourceCountAfter == result.resourceCountBefore,
                QStringLiteral("resource_leak_or_lifecycle_error"),
                QStringLiteral("Lifecycle counters did not return to baseline"));

    if (server.isRunning())
        server.stop();

    result.elapsedMs = elapsed.elapsed();
    result.passed = result.failureCategory.isEmpty();
    return result;
}

bool writeSummary(const WireScenarioResult &result, const QString &reportDirectory)
{
    if (!QDir().mkpath(reportDirectory))
        return false;

    QSaveFile summaryFile(QDir(reportDirectory).filePath(QStringLiteral("summary.json")));
    if (!summaryFile.open(QIODevice::WriteOnly))
        return false;
    summaryFile.write(QJsonDocument(result.toJson()).toJson(QJsonDocument::Indented));
    return summaryFile.commit();
}
