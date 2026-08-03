// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocoljsonscenario.h"

#include "clientruntime.h"
#include "modules/window-management/windowmanagementinterfacev1.h"

#include <wserver.h>
#include <wsocket.h>

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <functional>
#include <memory>

using namespace WAYLIB_SERVER_NAMESPACE;

namespace {
QJsonArray clientEvents(const QString &objectName, const QVector<quint32> &states)
{
    QJsonArray events;
    for (const quint32 state : states) {
        events.append(QJsonObject{
            { QStringLiteral("object"), objectName },
            { QStringLiteral("event"), QStringLiteral("show_desktop") },
            { QStringLiteral("args"), QJsonArray{ static_cast<qint64>(state) } },
        });
    }
    return events;
}

bool saveObject(const QString &path, const QJsonObject &object)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath()))
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.commit();
}

class ScenarioExecutor
{
public:
    ScenarioExecutor()
        : m_thread(new QThread)
        , m_worker(new ClientWorker)
    {
        qRegisterMetaType<ClientStepResult>();
        m_worker->moveToThread(m_thread);
        QObject::connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
        m_thread->start();
    }

    ~ScenarioExecutor()
    {
        stopThread();
    }

    ProtocolJsonRunResult run(const ProtocolJsonCase &testCase)
    {
        ProtocolJsonRunResult result;
        QElapsedTimer elapsed;
        elapsed.start();

        QTemporaryDir runtimeDirectory;
        auto server = std::make_unique<WServer>();
        auto socket = std::make_unique<WSocket>(false);
        QPointer<WServer> serverGuard(server.get());
        QPointer<WSocket> socketGuard(socket.get());
        auto *protocol = server->attach<WindowManagementInterfaceV1>(server.get());
        QSignalSpy stateSpy(protocol, &WindowManagementInterfaceV1::desktopStateChanged);

        record(result,
               QStringLiteral("runtime_directory_created"),
               runtimeDirectory.isValid(),
               QStringLiteral("server_crash_or_exit"),
               QStringLiteral("Unable to create isolated runtime directory"));
        const bool socketCreated =
            runtimeDirectory.isValid() && socket->autoCreate(runtimeDirectory.path());
        record(result,
               QStringLiteral("socket_created"),
               socketCreated,
               QStringLiteral("server_crash_or_exit"),
               QStringLiteral("Unable to create isolated Wayland socket"));
        if (socketCreated) {
            server->addSocket(socket.get());
            server->start();
        }

        const qsizetype clientCountBefore = socket->clients().size();
        const qsizetype resourceCountBefore = protocol->resourceCount();
        record(result,
               QStringLiteral("baseline_resources"),
               clientCountBefore == 0 && resourceCountBefore == 0,
               QStringLiteral("resource_leak_or_lifecycle_error"),
               QStringLiteral("Client or resource baseline was not zero"));

        ClientStepResult snapshot;
        QString objectName;
        bool displayConnected = false;
        bool disconnected = false;
        QJsonObject actualCheckpoints;
        quint32 advertisedVersion = 0;
        quint32 boundVersion = 0;
        QJsonObject requestContract;

        if (result.failureCategory.isEmpty()) {
            const QJsonArray steps = testCase.input.value(QStringLiteral("steps")).toArray();
            for (qsizetype index = 0; index < steps.size(); ++index) {
                const QJsonObject step = steps.at(index).toObject();
                const QString operation = step.constBegin().key();
                const QString checkName = QStringLiteral("step_%1_%2").arg(index).arg(operation);

                if (operation == QStringLiteral("bind")) {
                    const QJsonObject bind = step.value(operation).toObject();
                    objectName = bind.value(QStringLiteral("object")).toString();
                    const quint32 version = static_cast<quint32>(
                        bind.value(QStringLiteral("version")).toInteger());
                    snapshot = waitForStep(
                        QStringLiteral("bind"),
                        [this, path = socket->fullServerName(), version] {
                            QMetaObject::invokeMethod(
                                m_worker,
                                [worker = m_worker, path, version] {
                                    worker->connectAndBindVersion(path, version);
                                },
                                Qt::QueuedConnection);
                        });
                    displayConnected = snapshot.displayConnected;
                    advertisedVersion = snapshot.advertisedVersion;
                    boundVersion = snapshot.boundVersion;
                    recordStep(result, checkName, snapshot);
                    record(result,
                           QStringLiteral("global_advertised"),
                           snapshot.advertisedVersion
                                   == WindowManagementInterfaceV1::InterfaceVersion
                               && snapshot.boundVersion
                                   == WindowManagementInterfaceV1::InterfaceVersion,
                           QStringLiteral("adapter_validation_error"),
                           QStringLiteral("Advertised or bound version differs from metadata"));
                } else if (operation == QStringLiteral("request")) {
                    const QJsonObject request = step.value(operation).toObject();
                    const QString name = request.value(QStringLiteral("name")).toString();
                    if (name == QStringLiteral("set_desktop")) {
                        const quint32 state = static_cast<quint32>(
                            request.value(QStringLiteral("args")).toArray().at(0).toInteger());
                        requestContract = QJsonObject{
                            { QStringLiteral("name"), name },
                            { QStringLiteral("state"), static_cast<qint64>(state) },
                        };
                        snapshot = waitForStep(
                            QStringLiteral("request:set_desktop"),
                            [this, state] {
                                QMetaObject::invokeMethod(
                                    m_worker,
                                    [worker = m_worker, state] { worker->sendSetDesktop(state); },
                                    Qt::QueuedConnection);
                            });
                    } else {
                        snapshot = waitForStep(
                            QStringLiteral("request:destroy"),
                            [this] {
                                QMetaObject::invokeMethod(
                                    m_worker,
                                    [worker = m_worker] { worker->sendDestroyProtocol(); },
                                    Qt::QueuedConnection);
                            });
                    }
                    recordStep(result, checkName, snapshot);
                } else if (operation == QStringLiteral("client_roundtrip")) {
                    snapshot = waitForStep(
                        QStringLiteral("client_roundtrip"),
                        [this] {
                            QMetaObject::invokeMethod(
                                m_worker,
                                [worker = m_worker] { worker->clientRoundtrip(); },
                                Qt::QueuedConnection);
                        });
                    recordStep(result, checkName, snapshot);
                } else if (operation == QStringLiteral("checkpoint")) {
                    const QString checkpointName = step.value(operation).toString();
                    QJsonObject actualCheckpoint{
                        { QStringLiteral("client_events"),
                          clientEvents(objectName, snapshot.events) },
                        { QStringLiteral("server_state"),
                          QJsonObject{
                              { QStringLiteral("desktop_state"),
                                static_cast<qint64>(protocol->desktopState()) },
                              { QStringLiteral("desktop_state_changed_count"), stateSpy.size() },
                          } },
                        { QStringLiteral("connection"),
                          QJsonObject{
                              { QStringLiteral("display_error"), snapshot.displayError },
                              { QStringLiteral("protocol_error_occurred"),
                                snapshot.protocolErrorOccurred },
                          } },
                    };
                    actualCheckpoints.insert(checkpointName, actualCheckpoint);
                    compareCheckpoint(testCase, checkpointName, actualCheckpoint, result);
                    result.checks.insert(checkName, result.failureCategory.isEmpty());
                } else if (operation == QStringLiteral("disconnect")) {
                    snapshot = waitForStep(
                        QStringLiteral("disconnect"),
                        [this] {
                            QMetaObject::invokeMethod(
                                m_worker,
                                [worker = m_worker] { worker->disconnectClient(); },
                                Qt::QueuedConnection);
                        });
                    const bool clientsRestored = snapshot.ok
                        && waitForCondition([&socket] { return socket->clients().isEmpty(); },
                                            socket.get(),
                                            SIGNAL(clientsChanged()));
                    disconnected = clientsRestored;
                    record(result,
                           checkName,
                           clientsRestored,
                           snapshot.errorCategory.isEmpty()
                               ? QStringLiteral("resource_leak_or_lifecycle_error")
                               : snapshot.errorCategory,
                           snapshot.errorMessage.isEmpty()
                               ? QStringLiteral("Server client count did not return to zero")
                               : snapshot.errorMessage);
                }

                if (!result.failureCategory.isEmpty())
                    break;
            }
        }

        if (displayConnected && !disconnected) {
            const ClientStepResult cleanup = waitForStep(
                QStringLiteral("disconnect"),
                [this] {
                    QMetaObject::invokeMethod(
                        m_worker,
                        [worker = m_worker] { worker->disconnectClient(); },
                        Qt::QueuedConnection);
                });
            disconnected = cleanup.ok
                && waitForCondition([&socket] { return socket->clients().isEmpty(); },
                                    socket.get(),
                                    SIGNAL(clientsChanged()));
        }

        const qsizetype clientCountAfter = socket->clients().size();
        const qsizetype resourceCountAfter = protocol->resourceCount();
        record(result,
               QStringLiteral("resources_restored"),
               clientCountAfter == clientCountBefore
                   && resourceCountAfter == resourceCountBefore,
               QStringLiteral("resource_leak_or_lifecycle_error"),
               QStringLiteral("Lifecycle counters did not return to baseline"));

        if (server->isRunning())
            server->stop();
        const bool serverStopped = !server->isRunning();
        socket->close();
        const bool socketClosed =
            !socket->isListening() && !socket->isValid() && socket->clients().isEmpty();
        socket.reset();
        server.reset();
        const bool environmentDestroyed = socketGuard.isNull() && serverGuard.isNull();
        const bool runtimeDirectoryRemoved = runtimeDirectory.remove();
        const bool clientThreadStopped = stopThread();

        record(result,
               QStringLiteral("server_stopped"),
               serverStopped,
               QStringLiteral("resource_leak_or_lifecycle_error"),
               QStringLiteral("Wayland server did not stop"));
        record(result,
               QStringLiteral("socket_closed"),
               socketClosed,
               QStringLiteral("resource_leak_or_lifecycle_error"),
               QStringLiteral("Wayland socket did not close"));
        record(result,
               QStringLiteral("environment_destroyed"),
               environmentDestroyed && runtimeDirectoryRemoved && clientThreadStopped,
               QStringLiteral("resource_leak_or_lifecycle_error"),
               QStringLiteral("Test environment did not teardown completely"));

        result.actual = QJsonObject{
            { QStringLiteral("stage"), QStringLiteral("poc-2") },
            { QStringLiteral("case"), testCase.caseId },
            { QStringLiteral("expectation_source"),
              testCase.expected.value(QStringLiteral("expectation_source")) },
            { QStringLiteral("protocol"),
              QJsonObject{
                  { QStringLiteral("interface"),
                    QStringLiteral("treeland_window_management_v1") },
                  { QStringLiteral("xml_sha256"), testCase.xmlSha256 },
                  { QStringLiteral("advertised_version"),
                    static_cast<qint64>(advertisedVersion) },
                  { QStringLiteral("bound_version"), static_cast<qint64>(boundVersion) },
              } },
            { QStringLiteral("checkpoints"), actualCheckpoints },
            { QStringLiteral("request"), requestContract },
            { QStringLiteral("lifecycle"),
              QJsonObject{
                  { QStringLiteral("client_count_before"), clientCountBefore },
                  { QStringLiteral("client_count_after"), clientCountAfter },
                  { QStringLiteral("resource_count_before"), resourceCountBefore },
                  { QStringLiteral("resource_count_after"), resourceCountAfter },
                  { QStringLiteral("protocol_destructor_sent"),
                    snapshot.protocolDestructorSent },
                  { QStringLiteral("local_proxy_alive_after_destroy"),
                    snapshot.localProxyAlive },
                  { QStringLiteral("server_stopped"), serverStopped },
                  { QStringLiteral("socket_closed"), socketClosed },
                  { QStringLiteral("environment_destroyed"), environmentDestroyed },
                  { QStringLiteral("runtime_directory_removed"), runtimeDirectoryRemoved },
                  { QStringLiteral("client_thread_stopped"), clientThreadStopped },
              } },
        };
        result.elapsedMs = elapsed.elapsed();
        result.passed = result.failureCategory.isEmpty();
        return result;
    }

private:
    ClientStepResult waitForStep(const QString &step,
                                 const std::function<void()> &start,
                                 int timeoutMs = 3000)
    {
        ClientStepResult received;
        bool completed = false;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        const auto connection = QObject::connect(
            m_worker, &ClientWorker::stepFinished, &loop,
            [&](const ClientStepResult &candidate) {
                if (candidate.step != step)
                    return;
                received = candidate;
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

    bool waitForCondition(const std::function<bool()> &condition,
                          QObject *notifier,
                          const char *signal,
                          int timeoutMs = 3000)
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

    void record(ProtocolJsonRunResult &result,
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

    void recordStep(ProtocolJsonRunResult &result,
                    const QString &name,
                    const ClientStepResult &step)
    {
        record(result,
               name,
               step.ok,
               step.errorCategory.isEmpty() ? QStringLiteral("adapter_validation_error")
                                            : step.errorCategory,
               step.errorMessage.isEmpty() ? QStringLiteral("Client step failed")
                                           : step.errorMessage);
    }

    void compareCheckpoint(const ProtocolJsonCase &testCase,
                           const QString &name,
                           const QJsonObject &actual,
                           ProtocolJsonRunResult &result)
    {
        const QJsonObject expected = testCase.expected.value(QStringLiteral("checkpoints"))
                                         .toObject()
                                         .value(name)
                                         .toObject();
        if (expected.value(QStringLiteral("client_events"))
            != actual.value(QStringLiteral("client_events"))) {
            result.failureCategory = QStringLiteral("checkpoint_event_diff");
            result.failureMessage = QStringLiteral("Client events differ at checkpoint %1").arg(name);
        } else if (expected.value(QStringLiteral("server_state"))
                   != actual.value(QStringLiteral("server_state"))) {
            result.failureCategory = QStringLiteral("checkpoint_probe_diff");
            result.failureMessage = QStringLiteral("Server state differs at checkpoint %1").arg(name);
        }
        if (!result.failureCategory.isEmpty()) {
            result.failureCheckpoint = name;
            result.expectedDifference = expected;
            result.actualDifference = actual;
        }
    }

    bool stopThread()
    {
        if (!m_thread)
            return true;
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
        m_worker = nullptr;
        return true;
    }

    QThread *m_thread = nullptr;
    ClientWorker *m_worker = nullptr;
};
}

QJsonObject ProtocolJsonRunResult::summary() const
{
    QJsonObject checkObject;
    for (auto check = checks.cbegin(); check != checks.cend(); ++check)
        checkObject.insert(check.key(), check.value());
    QJsonObject object{
        { QStringLiteral("stage"), QStringLiteral("poc-2") },
        { QStringLiteral("case"), actual.value(QStringLiteral("case")) },
        { QStringLiteral("result"), passed ? QStringLiteral("pass") : QStringLiteral("fail") },
        { QStringLiteral("checks"), checkObject },
        { QStringLiteral("metrics"), QJsonObject{ { QStringLiteral("elapsed_ms"), elapsedMs } } },
    };
    if (!passed) {
        object.insert(QStringLiteral("failure_category"), failureCategory);
        object.insert(QStringLiteral("failure_message"), failureMessage);
        if (!failureCheckpoint.isEmpty()) {
            object.insert(QStringLiteral("failure_checkpoint"), failureCheckpoint);
            object.insert(QStringLiteral("expected"), expectedDifference);
            object.insert(QStringLiteral("actual"), actualDifference);
        }
    }
    return object;
}

ProtocolJsonRunResult runProtocolJsonCase(const ProtocolJsonCase &testCase)
{
    ScenarioExecutor executor;
    return executor.run(testCase);
}

ProtocolJsonRunResult validationFailureResult(const QString &caseId,
                                              const ProtocolJsonValidationError &error)
{
    ProtocolJsonRunResult result;
    result.failureCategory = error.category;
    result.failureMessage = error.message;
    result.actual = QJsonObject{
        { QStringLiteral("stage"), QStringLiteral("poc-2") },
        { QStringLiteral("case"), caseId },
        { QStringLiteral("checkpoints"), QJsonObject{} },
    };
    return result;
}

bool writeProtocolJsonArtifacts(const ProtocolJsonRunResult &result,
                                const QString &actualPath,
                                const QString &reportDirectory)
{
    return saveObject(actualPath, result.actual)
        && saveObject(QDir(reportDirectory).filePath(QStringLiteral("summary.json")),
                      result.summary());
}
