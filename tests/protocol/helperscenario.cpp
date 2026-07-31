// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helperscenario.h"

#include "modules/window-management/windowmanagementinterfacev1.h"
#include "seat/helper.h"

#include <wserver.h>
#include <wsocket.h>

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <memory>

using namespace WAYLIB_SERVER_NAMESPACE;

namespace {
constexpr quint32 Normal = 0;
constexpr quint32 Show = 1;

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

QJsonObject HelperScenarioResult::toJson() const
{
    QJsonObject checkObject;
    for (auto it = checks.cbegin(); it != checks.cend(); ++it)
        checkObject.insert(it.key(), it.value());

    QJsonObject object{
        { QStringLiteral("stage"), QStringLiteral("poc-0b") },
        { QStringLiteral("case"), QStringLiteral("window-management.helper-set-desktop-show") },
        { QStringLiteral("result"), passed ? QStringLiteral("pass") : QStringLiteral("fail") },
        { QStringLiteral("checks"), checkObject },
        { QStringLiteral("versions"),
          QJsonObject{
              { QStringLiteral("advertised"), static_cast<qint64>(advertisedVersion) },
              { QStringLiteral("bound"), static_cast<qint64>(boundVersion) },
          } },
        { QStringLiteral("checkpoints"),
          QJsonObject{
              { QStringLiteral("initial-state"),
                QJsonObject{
                    { QStringLiteral("client_events"), desktopEvents(initialEvents) },
                } },
              { QStringLiteral("request-state"),
                QJsonObject{
                    { QStringLiteral("client_events"), desktopEvents(requestEvents) },
                } },
          } },
        { QStringLiteral("server_state"),
          QJsonObject{
              { QStringLiteral("protocol_desktop_state"),
                static_cast<qint64>(protocolDesktopState) },
              { QStringLiteral("helper_desktop_state"),
                static_cast<qint64>(helperDesktopState) },
              { QStringLiteral("desktop_state_changed_count"),
                desktopStateChangedCount },
          } },
        { QStringLiteral("lifecycle"),
          QJsonObject{
              { QStringLiteral("protocol_destructor_sent"), protocolDestructorSent },
              { QStringLiteral("local_proxy_alive_after_destroy"),
                localProxyAliveAfterDestroy },
              { QStringLiteral("helper_destroyed"), helperDestroyed },
              { QStringLiteral("client_count_after"), clientCountAfter },
              { QStringLiteral("resource_count_after"), resourceCountAfter },
          } },
        { QStringLiteral("connection"),
          QJsonObject{
              { QStringLiteral("display_error"), displayError },
              { QStringLiteral("protocol_error_occurred"), protocolErrorOccurred },
          } },
        { QStringLiteral("elapsed_ms"), elapsedMs },
    };
    if (!passed) {
        object.insert(QStringLiteral("failure_category"), failureCategory);
        object.insert(QStringLiteral("failure_message"), failureMessage);
    }
    return object;
}

HelperScenario::HelperScenario()
    : m_clientThread(new QThread)
    , m_worker(new ClientWorker)
{
    qRegisterMetaType<ClientStepResult>();
    m_clientThread->setObjectName(QStringLiteral("ProtocolClient"));
    m_worker->moveToThread(m_clientThread);
    QObject::connect(m_clientThread, &QThread::finished,
                     m_worker, &QObject::deleteLater);
    m_clientThread->start();
}

HelperScenario::~HelperScenario()
{
    const bool quitScheduled = QMetaObject::invokeMethod(
        m_worker,
        [thread = m_clientThread] { thread->quit(); },
        Qt::BlockingQueuedConnection);
    if (!quitScheduled)
        m_clientThread->quit();
    m_clientThread->wait();
    delete m_clientThread;
}

ClientStepResult HelperScenario::waitForStep(const QString &step,
                                             const std::function<void()> &start,
                                             int timeoutMs)
{
    ClientStepResult received;
    bool completed = false;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    const auto connection = QObject::connect(
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
    QObject::disconnect(connection);

    if (!completed) {
        received.step = step;
        received.errorCategory = QStringLiteral("timeout");
        received.errorMessage =
            QStringLiteral("Timed out waiting for client step '%1'").arg(step);
    }
    return received;
}

bool HelperScenario::waitForCondition(const std::function<bool()> &condition,
                                      QObject *notifier,
                                      const char *signal,
                                      int timeoutMs)
{
    if (condition())
        return true;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    const auto connection = QObject::connect(notifier, signal, &loop, SLOT(quit()));
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    while (!condition() && timer.isActive())
        loop.exec();

    QObject::disconnect(connection);
    return condition();
}

void HelperScenario::recordCheck(HelperScenarioResult &result,
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

HelperScenarioResult HelperScenario::run(quint32 expectedHelperState)
{
    HelperScenarioResult result;
    QElapsedTimer elapsed;
    elapsed.start();

    QTemporaryDir runtimeDirectory;
    WServer server;
    WSocket socket(false);
    auto helper = std::make_unique<Helper>();
    QPointer<Helper> helperGuard(helper.get());
    auto *protocol = helper->initWindowManagement(&server);

    recordCheck(result,
                QStringLiteral("production_registration"),
                protocol != nullptr,
                QStringLiteral("helper_registration_error"),
                QStringLiteral("Helper production initializer did not return the protocol"));

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
        result.initialEvents = initial.events;
        result.displayError = initial.displayError;
        result.protocolErrorOccurred = initial.protocolErrorOccurred;
        displayConnected = initial.displayConnected;
        proxyAlive = initial.localProxyAlive;

        recordCheck(result,
                    QStringLiteral("client_connected"),
                    initial.ok,
                    initial.errorCategory.isEmpty()
                        ? QStringLiteral("transport_or_disconnect_error")
                        : initial.errorCategory,
                    initial.errorMessage);
        recordCheck(result,
                    QStringLiteral("global_advertised_by_production_path"),
                    initial.advertisedVersion == WindowManagementInterfaceV1::InterfaceVersion
                        && initial.boundVersion == WindowManagementInterfaceV1::InterfaceVersion,
                    QStringLiteral("helper_registration_error"),
                    QStringLiteral("Production registration did not advertise the expected global"));
        recordCheck(result,
                    QStringLiteral("initial_state_checkpoint"),
                    initial.events == QVector<quint32>{ Normal },
                    QStringLiteral("checkpoint_event_diff"),
                    QStringLiteral("Initial checkpoint did not contain show_desktop(Normal)"));
    }

    std::unique_ptr<QSignalSpy> stateSpy;
    if (protocol) {
        stateSpy =
            std::make_unique<QSignalSpy>(protocol,
                                        &WindowManagementInterfaceV1::desktopStateChanged);
    }
    if (proxyAlive) {
        const ClientStepResult request = waitForStep(
            QStringLiteral("set_desktop"),
            [this] {
                QMetaObject::invokeMethod(
                    m_worker,
                    [worker = m_worker] { worker->setDesktop(Show); },
                    Qt::QueuedConnection);
            });
        result.requestEvents = request.events;
        result.displayError = request.displayError;
        result.protocolErrorOccurred = request.protocolErrorOccurred;
        result.desktopStateChangedCount = stateSpy ? stateSpy->size() : 0;
        result.protocolDesktopState = static_cast<quint32>(protocol->desktopState());
        result.helperDesktopState = static_cast<quint32>(helper->showDesktopState());

        recordCheck(result,
                    QStringLiteral("request_roundtrip"),
                    request.ok,
                    request.errorCategory.isEmpty()
                        ? QStringLiteral("transport_or_disconnect_error")
                        : request.errorCategory,
                    request.errorMessage);
        recordCheck(result,
                    QStringLiteral("request_event_checkpoint"),
                    request.events == QVector<quint32>{ Show },
                    QStringLiteral("checkpoint_event_diff"),
                    QStringLiteral("Request checkpoint did not contain show_desktop(Show)"));
        recordCheck(result,
                    QStringLiteral("protocol_state_changed"),
                    stateSpy && stateSpy->size() == 1
                        && protocol->desktopState()
                            == WindowManagementInterfaceV1::DesktopState::Show,
                    QStringLiteral("checkpoint_probe_diff"),
                    QStringLiteral("Protocol state did not become DesktopState::Show"));
        recordCheck(result,
                    QStringLiteral("helper_authoritative_state"),
                    result.helperDesktopState == expectedHelperState,
                    QStringLiteral("checkpoint_probe_diff"),
                    QStringLiteral("Helper authoritative state differs from expected state"));
        recordCheck(result,
                    QStringLiteral("states_agree"),
                    result.helperDesktopState == result.protocolDesktopState
                        && result.helperDesktopState == Show,
                    QStringLiteral("checkpoint_probe_diff"),
                    QStringLiteral("Client event, protocol state, and Helper state disagree"));
        recordCheck(result,
                    QStringLiteral("connection_clean"),
                    request.displayError == 0 && !request.protocolErrorOccurred,
                    QStringLiteral("wayland_protocol_error"),
                    QStringLiteral("Wayland display or protocol error occurred"));
    }

    if (proxyAlive) {
        const ClientStepResult destroyed = waitForStep(
            QStringLiteral("destroy"),
            [this] {
                QMetaObject::invokeMethod(
                    m_worker,
                    [worker = m_worker] { worker->destroyProtocol(); },
                    Qt::QueuedConnection);
            });
        result.protocolDestructorSent = destroyed.protocolDestructorSent;
        result.localProxyAliveAfterDestroy = destroyed.localProxyAlive;
        recordCheck(result,
                    QStringLiteral("protocol_destructor"),
                    destroyed.ok && destroyed.protocolDestructorSent
                        && !destroyed.localProxyAlive && protocol->resourceCount() == 0,
                    QStringLiteral("resource_leak_or_lifecycle_error"),
                    QStringLiteral("Protocol resource did not retire after destructor"));
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
                    QStringLiteral("resource_leak_or_lifecycle_error"),
                    QStringLiteral("Client count did not return to zero"));
    }

    result.clientCountAfter = socket.clients().size();
    result.resourceCountAfter = protocol ? protocol->resourceCount() : 0;
    recordCheck(result,
                QStringLiteral("resources_restored"),
                result.clientCountAfter == 0 && result.resourceCountAfter == 0,
                QStringLiteral("resource_leak_or_lifecycle_error"),
                QStringLiteral("Helper/socket/protocol lifecycle did not return to baseline"));

    if (server.isRunning())
        server.stop();

    helper.reset();
    result.helperDestroyed = helperGuard.isNull();
    recordCheck(result,
                QStringLiteral("helper_destroyed"),
                result.helperDestroyed,
                QStringLiteral("resource_leak_or_lifecycle_error"),
                QStringLiteral("Helper did not tear down synchronously"));

    result.elapsedMs = elapsed.elapsed();
    result.passed = result.failureCategory.isEmpty();
    return result;
}

bool writeHelperSummary(const HelperScenarioResult &result, const QString &reportDirectory)
{
    if (!QDir().mkpath(reportDirectory))
        return false;

    QSaveFile summaryFile(QDir(reportDirectory).filePath(QStringLiteral("summary.json")));
    if (!summaryFile.open(QIODevice::WriteOnly))
        return false;
    summaryFile.write(QJsonDocument(result.toJson()).toJson(QJsonDocument::Indented));
    return summaryFile.commit();
}
