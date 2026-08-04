// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winemulticlientscenario.h"

#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "modules/wine-window-management/winewindowmanagement.h"
#include "protocoljsoncase.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "wineclientworker.h"

#include <wbackend.h>
#include <woutput.h>
#include <woutputlayout.h>
#include <woutputrenderwindow.h>
#include <wrenderhelper.h>
#include <wseat.h>
#include <wserver.h>
#include <wsocket.h>
#include <wsurface.h>

#include <qwallocator.h>
#include <qwbackend.h>
#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwrenderer.h>
#include <qwsubcompositor.h>

#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QPointer>
#include <QQmlContext>
#include <QSGRendererInterface>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

using namespace WAYLIB_SERVER_NAMESPACE;
QW_USE_NAMESPACE

namespace {
struct ClientContext
{
    QString id;
    QString appId;
    QSize size;
    QThread *thread = nullptr;
    WineClientWorker *worker = nullptr;
    WineClientStepResult snapshot;
    QPointer<SurfaceWrapper> wrapper;
    quint32 windowId = 0;
};

class WineMultiClientScenarioExecutor
{
public:
    ~WineMultiClientScenarioExecutor() { stopClients(); }

    ProtocolJsonRunResult run(const ProtocolJsonCase &testCase)
    {
        ProtocolJsonRunResult result;
        QElapsedTimer elapsed;
        elapsed.start();

        const QJsonObject serverDefinition =
            testCase.input.value(QStringLiteral("server")).toObject();
        const QJsonArray outputGeometry = serverDefinition.value(QStringLiteral("outputs"))
                                              .toArray().first().toObject()
                                              .value(QStringLiteral("geometry")).toArray();
        const QSize outputSize(outputGeometry.at(2).toInt(), outputGeometry.at(3).toInt());

        for (const QJsonValue &value : testCase.input.value(QStringLiteral("clients")).toArray()) {
            const QJsonObject definition = value.toObject();
            const QJsonObject window = definition.value(QStringLiteral("objects"))
                                           .toArray().first().toObject();
            const QJsonArray size = window.value(QStringLiteral("size")).toArray();
            auto client = std::make_unique<ClientContext>();
            client->id = definition.value(QStringLiteral("id")).toString();
            client->appId = window.value(QStringLiteral("app_id")).toString();
            client->size = QSize(size.at(0).toInt(), size.at(1).toInt());
            startClient(*client);
            m_clients.push_back(std::move(client));
        }

        qputenv("WLR_BACKENDS", "headless");
        qputenv("WLR_HEADLESS_OUTPUTS", "1");
        qputenv("QT_QUICK_BACKEND", "software");

        QTemporaryDir runtimeDirectory;
        auto server = std::make_unique<WServer>();
        auto socket = std::make_unique<WSocket>(false);
        auto engine = std::make_unique<QmlEngine>();
        auto *helper = engine->singletonInstance<Helper *>("Treeland", "Helper");
        Q_ASSERT(helper);
        QQmlEngine::setContextForObject(helper->window(), engine->rootContext());
        QQmlEngine::setContextForObject(helper->window()->contentItem(), engine->rootContext());
        helper->rootSurfaceContainer()->setQmlEngine(engine.get());
        helper->rootSurfaceContainer()->init(server.get());

        auto *backend = server->attach<WBackend>();
        auto *seat = server->attach<WSeat>();
        record(result, QStringLiteral("runtime_directory_created"), runtimeDirectory.isValid(),
               QStringLiteral("server_crash_or_exit"),
               QStringLiteral("Unable to create isolated runtime directory"));
        const bool socketCreated = runtimeDirectory.isValid()
            && socket->autoCreate(runtimeDirectory.path());
        record(result, QStringLiteral("socket_created"), socketCreated,
               QStringLiteral("server_crash_or_exit"),
               QStringLiteral("Unable to create isolated Wayland socket"));
        if (socketCreated && result.failureCategory.isEmpty()) {
            server->addSocket(socket.get());
            server->start();
        }

        helper->initShellProtocols(server.get(), seat);
        QPointer<WineWindowManager> wineManager = server->findInterface<WineWindowManager>();
        qsizetype remoteControlCount = wineManager
            ? wineManager->activeControlResourceCount() : -1;
        quint64 destroyedControlCount = wineManager
            ? wineManager->destroyedControlResourceCount() : 0;
        if (wineManager) {
            QObject::connect(wineManager, &WineWindowManager::controlResourceCountChanged,
                             server.get(), [&](qsizetype active, quint64 destroyed) {
                                 remoteControlCount = active;
                                 destroyedControlCount = destroyed;
                             });
        }
        record(result, QStringLiteral("wine_manager_probe_ready"), wineManager,
               QStringLiteral("fixture_error"),
               QStringLiteral("WineWindowManager lifecycle probe is unavailable"));

        std::unique_ptr<qw_renderer> renderer(
            WRenderHelper::createRenderer(backend->handle(), QSGRendererInterface::Software));
        std::unique_ptr<qw_allocator> allocator(
            renderer ? qw_allocator::autocreate(*backend->handle(), *renderer) : nullptr);
        if (renderer)
            renderer->init_wl_display(*server->handle());
        auto *compositor = renderer
            ? qw_compositor::create(*server->handle(), 6, *renderer) : nullptr;
        auto *subcompositor = compositor ? qw_subcompositor::create(*server->handle()) : nullptr;
        QPointer<qw_compositor> compositorGuard(compositor);
        QPointer<qw_subcompositor> subcompositorGuard(subcompositor);
        record(result, QStringLiteral("renderer_created"), renderer && allocator && compositor,
               QStringLiteral("fixture_error"),
               QStringLiteral("Unable to initialize headless compositor renderer"));

        WOutput *fixtureOutput = nullptr;
        QObject::connect(backend, &WBackend::outputAdded, server.get(), [&](WOutput *output) {
            output->handle()->init_render(*allocator, *renderer);
            qw_output_state state;
            state.set_custom_mode(outputSize.width(), outputSize.height(), 60000);
            state.set_enabled(true);
            if (output->handle()->commit_state(state)) {
                helper->rootSurfaceContainer()->outputLayout()->add(
                    output, QPoint(outputGeometry.at(0).toInt(), outputGeometry.at(1).toInt()));
                fixtureOutput = output;
            }
        });
        if (result.failureCategory.isEmpty())
            backend->handle()->start();
        record(result, QStringLiteral("output_ready"),
               waitForCondition([&fixtureOutput] { return fixtureOutput != nullptr; },
                                backend, SIGNAL(outputAdded(WOutput*))),
               QStringLiteral("fixture_error"),
               QStringLiteral("Headless output did not become active"));

        const qsizetype clientCountBefore = socket->clients().size();
        const qsizetype surfaceCountBefore = helper->rootSurfaceContainer()->surfaces().size();
        QJsonObject actualCheckpoints;

        for (auto &client : m_clients) {
            if (!result.failureCategory.isEmpty())
                break;
            client->snapshot = waitForStep(*client, QStringLiteral("create_mapped_xdg_toplevel"),
                                           [path = socket->fullServerName(), c = client.get()] {
                                               QMetaObject::invokeMethod(
                                                   c->worker,
                                                   [c, path] {
                                                       c->worker->createMappedToplevel(
                                                           path, c->appId, c->size);
                                                   }, Qt::QueuedConnection);
                                           });
            recordStep(result, client->id + QStringLiteral("_fixture_created"), client->snapshot);
            client->wrapper = findWrapper(helper->rootSurfaceContainer(), client->appId);
            record(result, client->id + QStringLiteral("_surface_mapped"),
                   client->wrapper && waitForCondition(
                       [c = client.get()] { return c->wrapper->surface()->mapped(); },
                       client->wrapper->surface(), SIGNAL(mappedChanged())),
                   QStringLiteral("fixture_error"),
                   QStringLiteral("%1 xdg_toplevel did not map").arg(client->id));
            client->snapshot = waitForStep(*client, QStringLiteral("create_window_control"),
                                           [c = client.get()] {
                                               QMetaObject::invokeMethod(
                                                   c->worker, &WineClientWorker::createWindowControl,
                                                   Qt::QueuedConnection);
                                           });
            recordStep(result, client->id + QStringLiteral("_control_created"), client->snapshot);
            client->windowId = windowId(client->snapshot.events);
        }
        record(result, QStringLiteral("two_independent_connections"),
               socket->clients().size() == 2,
               QStringLiteral("client_isolation_error"),
               QStringLiteral("Expected two independent Wayland connections"));
        record(result, QStringLiteral("two_remote_controls"),
               wineManager && waitForControlCount(wineManager, 2),
               QStringLiteral("client_isolation_error"),
               QStringLiteral("Expected one remote control per client"));

        const QJsonArray steps = testCase.input.value(QStringLiteral("steps")).toArray();
        for (qsizetype index = 0;
             index < steps.size() && result.failureCategory.isEmpty(); ++index) {
            const QJsonObject step = steps.at(index).toObject();
            if (step.contains(QStringLiteral("request"))) {
                const QJsonObject request = step.value(QStringLiteral("request")).toObject();
                ClientContext *client = findClient(request.value(QStringLiteral("client")).toString());
                const QJsonArray args = request.value(QStringLiteral("args")).toArray();
                const QString name = request.value(QStringLiteral("name")).toString();
                const QString workerStep = name == QStringLiteral("set_position")
                    ? QStringLiteral("set_position") : QStringLiteral("set_z_order");
                client->snapshot = waitForStep(*client, workerStep, [client, name, args] {
                    QMetaObject::invokeMethod(client->worker, [client, name, args] {
                        if (name == QStringLiteral("set_position")) {
                            client->worker->setPosition(args.at(0).toInt(), args.at(1).toInt(),
                                                        args.at(2).toInteger());
                        } else {
                            client->worker->setZOrder(args.at(0).toInteger(),
                                                      args.at(1).toInteger());
                        }
                    }, Qt::QueuedConnection);
                });
                const bool expectsError = request.value(QStringLiteral("expect_protocol_error"))
                                              .toBool();
                record(result, QStringLiteral("step_%1_request_%2").arg(index).arg(client->id),
                       expectsError ? client->snapshot.protocolErrorOccurred
                                    : client->snapshot.ok,
                       expectsError ? QStringLiteral("protocol_error_not_observed")
                                    : client->snapshot.errorCategory,
                       expectsError ? QStringLiteral("Expected client-scoped protocol error")
                                    : client->snapshot.errorMessage);
            } else if (step.contains(QStringLiteral("client_roundtrip"))) {
                const QString id = step.value(QStringLiteral("client_roundtrip"))
                                       .toObject().value(QStringLiteral("client")).toString();
                ClientContext *client = findClient(id);
                client->snapshot = waitForStep(*client, QStringLiteral("client_roundtrip"),
                                               [client] {
                                                   QMetaObject::invokeMethod(
                                                       client->worker,
                                                       &WineClientWorker::clientRoundtrip,
                                                       Qt::QueuedConnection);
                                               });
                recordStep(result, QStringLiteral("step_%1_roundtrip_%2").arg(index).arg(id),
                           client->snapshot);
            } else if (step.contains(QStringLiteral("disconnect"))) {
                const QJsonObject definition = step.value(QStringLiteral("disconnect")).toObject();
                ClientContext *client = findClient(definition.value(QStringLiteral("client")).toString());
                const QString mode = definition.value(QStringLiteral("mode"))
                                         .toString(QStringLiteral("graceful"));
                const QString workerStep = mode == QStringLiteral("abrupt")
                    ? QStringLiteral("disconnect:abrupt") : QStringLiteral("disconnect");
                client->snapshot = waitForStep(*client, workerStep, [client, mode] {
                    if (mode == QStringLiteral("abrupt")) {
                        QMetaObject::invokeMethod(client->worker,
                                                  &WineClientWorker::disconnectAbruptly,
                                                  Qt::QueuedConnection);
                    } else {
                        QMetaObject::invokeMethod(client->worker,
                                                  &WineClientWorker::disconnectClient,
                                                  Qt::QueuedConnection);
                    }
                });
                recordStep(result, QStringLiteral("step_%1_disconnect_%2").arg(index).arg(client->id),
                           client->snapshot);
                record(result, QStringLiteral("step_%1_remaining_connection_count").arg(index),
                       waitForCondition(
                           [&socket, this] { return socket->clients().size() == liveClientCount(); },
                           socket.get(), SIGNAL(clientsChanged())),
                       QStringLiteral("client_isolation_error"),
                       QStringLiteral("Disconnect affected the wrong client connection"));
            } else if (step.contains(QStringLiteral("destroy"))) {
                const QString id = step.value(QStringLiteral("destroy"))
                                       .toObject().value(QStringLiteral("client")).toString();
                ClientContext *client = findClient(id);
                client->snapshot = waitForStep(*client, QStringLiteral("destroy"), [client] {
                    QMetaObject::invokeMethod(client->worker, &WineClientWorker::destroyObjects,
                                              Qt::QueuedConnection);
                });
                recordStep(result, QStringLiteral("step_%1_destroy_%2").arg(index).arg(id),
                           client->snapshot);
            } else if (step.contains(QStringLiteral("checkpoint"))) {
                const QString name = step.value(QStringLiteral("checkpoint")).toString();
                const QJsonObject actual = checkpoint(remoteControlCount,
                                                      destroyedControlCount,
                                                      socket->clients().size(),
                                                      helper->rootSurfaceContainer());
                actualCheckpoints.insert(name, actual);
                compareCheckpoint(testCase, name, actual, result);
                result.checks.insert(QStringLiteral("step_%1_checkpoint").arg(index),
                                     result.failureCategory.isEmpty());
            }
        }

        for (auto &client : m_clients) {
            if (!client->snapshot.localDisplayAlive)
                continue;
            client->snapshot = waitForStep(*client, QStringLiteral("disconnect"), [c = client.get()] {
                QMetaObject::invokeMethod(c->worker, &WineClientWorker::disconnectClient,
                                          Qt::QueuedConnection);
            });
        }
        const bool clientsRestored = waitForCondition(
            [&socket, clientCountBefore] { return socket->clients().size() == clientCountBefore; },
            socket.get(), SIGNAL(clientsChanged()));
        const bool controlsRestored = wineManager && waitForControlCount(wineManager, 0);
        const bool surfacesRestored = waitForCondition(
            [root = helper->rootSurfaceContainer(), surfaceCountBefore] {
                return root->surfaces().size() == surfaceCountBefore;
            }, helper->rootSurfaceContainer(), SIGNAL(surfaceRemoved(SurfaceWrapper*)));
        record(result, QStringLiteral("resources_restored"),
               clientsRestored && controlsRestored && surfacesRestored,
               QStringLiteral("resource_not_restored"),
               QStringLiteral("Multi-client resources did not return to baseline"));

        if (server->isRunning())
            server->stop();
        socket->close();
        stopClients();
        result.actual = {
            { QStringLiteral("stage"), QStringLiteral("mvp-d3") },
            { QStringLiteral("case"), testCase.caseId },
            { QStringLiteral("expectation_source"),
              testCase.expected.value(QStringLiteral("expectation_source")) },
            { QStringLiteral("protocol"),
              QJsonObject{
                  { QStringLiteral("interface"), QStringLiteral("treeland_wine_window_manager_v1") },
                  { QStringLiteral("xml_sha256"), testCase.xmlSha256 },
              } },
            { QStringLiteral("checkpoints"), actualCheckpoints },
            { QStringLiteral("lifecycle"),
              QJsonObject{
                  { QStringLiteral("client_count_before"), clientCountBefore },
                  { QStringLiteral("client_count_after"), socket->clients().size() },
                  { QStringLiteral("surface_count_before"), surfaceCountBefore },
                  { QStringLiteral("surface_count_after"),
                    helper->rootSurfaceContainer()->surfaces().size() },
                  { QStringLiteral("remote_control_resource_count"), remoteControlCount },
                  { QStringLiteral("destroyed_control_resource_count"),
                    qint64(destroyedControlCount) },
                  { QStringLiteral("client_threads_stopped"), m_clients.empty() },
              } },
        };

        QPointer<Helper> helperGuard(helper);
        engine.reset();
        if (helperGuard)
            delete helperGuard;
        server.reset();
        if (subcompositorGuard)
            delete subcompositorGuard;
        if (compositorGuard)
            delete compositorGuard;
        allocator.reset();
        renderer.reset();
        result.elapsedMs = elapsed.elapsed();
        result.passed = result.failureCategory.isEmpty();
        return result;
    }

private:
    void startClient(ClientContext &client)
    {
        qRegisterMetaType<WineClientStepResult>();
        client.thread = new QThread;
        client.worker = new WineClientWorker;
        client.worker->moveToThread(client.thread);
        QObject::connect(client.thread, &QThread::finished,
                         client.worker, &QObject::deleteLater);
        client.thread->start();
    }

    ClientContext *findClient(const QString &id) const
    {
        for (const auto &client : m_clients) {
            if (client->id == id)
                return client.get();
        }
        return nullptr;
    }

    qsizetype liveClientCount() const
    {
        qsizetype count = 0;
        for (const auto &client : m_clients)
            count += client->snapshot.localDisplayAlive;
        return count;
    }

    static quint32 windowId(const QJsonArray &events)
    {
        for (const QJsonValue &value : events) {
            const QJsonObject event = value.toObject();
            if (event.value(QStringLiteral("event")).toString() == QStringLiteral("window_id"))
                return event.value(QStringLiteral("args")).toArray().first().toInteger();
        }
        return 0;
    }

    static SurfaceWrapper *findWrapper(RootSurfaceContainer *root, const QString &appId)
    {
        for (SurfaceWrapper *wrapper : root->surfaces()) {
            if (wrapper->appId() == appId)
                return wrapper;
        }
        return nullptr;
    }

    WineClientStepResult waitForStep(ClientContext &client,
                                     const QString &step,
                                     const std::function<void()> &start,
                                     int timeoutMs = 3000) const
    {
        WineClientStepResult value;
        bool completed = false;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        const auto connection = QObject::connect(
            client.worker, &WineClientWorker::stepFinished, &loop,
            [&](const WineClientStepResult &candidate) {
                if (candidate.step != step)
                    return;
                value = candidate;
                completed = true;
                loop.quit();
            });
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        start();
        loop.exec();
        QObject::disconnect(connection);
        if (!completed) {
            value.step = step;
            value.errorCategory = QStringLiteral("timeout");
            value.errorMessage = QStringLiteral("Timed out waiting for %1 step '%2'")
                                     .arg(client.id, step);
        }
        return value;
    }

    static bool waitForCondition(const std::function<bool()> &condition,
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

    static bool waitForControlCount(WineWindowManager *manager,
                                    qsizetype expected,
                                    int timeoutMs = 3000)
    {
        if (manager->activeControlResourceCount() == expected)
            return true;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        const auto connection = QObject::connect(
            manager, &WineWindowManager::controlResourceCountChanged, &loop,
            [&](qsizetype active, quint64) {
                if (active == expected)
                    loop.quit();
            });
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        loop.exec();
        QObject::disconnect(connection);
        return manager->activeControlResourceCount() == expected;
    }

    static QJsonObject connection(const WineClientStepResult &snapshot)
    {
        QJsonObject value{
            { QStringLiteral("display_error"), snapshot.displayError },
            { QStringLiteral("protocol_error_occurred"), snapshot.protocolErrorOccurred },
        };
        if (snapshot.protocolErrorOccurred) {
            value.insert(QStringLiteral("protocol_error"), QJsonObject{
                { QStringLiteral("interface"), snapshot.protocolErrorInterface },
                { QStringLiteral("code"), qint64(snapshot.protocolErrorCode) },
                { QStringLiteral("object"), snapshot.protocolErrorObject },
            });
        }
        return value;
    }

    QJsonObject checkpoint(qsizetype remoteControlCount,
                           quint64 destroyedControlCount,
                           qsizetype clientCount,
                           RootSurfaceContainer *root) const
    {
        QJsonObject clients;
        for (const auto &client : m_clients) {
            const bool serverSurfaceAlive = client->wrapper
                && root->surfaces().contains(client->wrapper.data());
            QJsonArray events;
            for (const QJsonValue &value : client->snapshot.events) {
                QJsonObject event = value.toObject();
                event.insert(QStringLiteral("object"),
                             client->id + QStringLiteral("/control"));
                events.append(event);
            }
            clients.insert(client->id, QJsonObject{
                { QStringLiteral("connection"), connection(client->snapshot) },
                { QStringLiteral("client_events"), events },
                { QStringLiteral("objects"), QJsonObject{
                    { QStringLiteral("window_id"), qint64(client->windowId) },
                    { QStringLiteral("display_alive"), client->snapshot.localDisplayAlive },
                    { QStringLiteral("manager_proxy_alive"),
                      client->snapshot.localManagerProxyAlive },
                    { QStringLiteral("control_proxy_alive"),
                      client->snapshot.localControlProxyAlive },
                    { QStringLiteral("surface_proxy_alive"),
                      client->snapshot.localSurfaceProxyAlive },
                } },
                { QStringLiteral("server_state"), QJsonObject{
                    { QStringLiteral("surface.geometry"),
                      serverSurfaceAlive
                          ? QJsonArray{ client->wrapper->x(), client->wrapper->y(),
                                        client->wrapper->width(), client->wrapper->height() }
                          : QJsonArray{} },
                } },
            });
        }
        std::vector<std::pair<int, QString>> orderedClients;
        for (const auto &client : m_clients) {
            if (!client->wrapper || !root->surfaces().contains(client->wrapper.data())
                || !client->wrapper->parentItem()) {
                continue;
            }
            orderedClients.emplace_back(
                client->wrapper->parentItem()->childItems().indexOf(client->wrapper.data()),
                client->id);
        }
        std::ranges::sort(orderedClients);
        QJsonArray stackOrder;
        for (const auto &[index, id] : orderedClients) {
            Q_UNUSED(index);
            stackOrder.append(id);
        }
        return {
            { QStringLiteral("clients"), clients },
            { QStringLiteral("remote"), QJsonObject{
                { QStringLiteral("control_resource_count"), remoteControlCount },
                { QStringLiteral("destroyed_control_resource_count"),
                  qint64(destroyedControlCount) },
                { QStringLiteral("client_count"), clientCount },
                { QStringLiteral("surface_count"), root->surfaces().size() },
                { QStringLiteral("stack_order"), stackOrder },
            } },
        };
    }

    static void compareCheckpoint(const ProtocolJsonCase &testCase,
                                  const QString &name,
                                  const QJsonObject &actual,
                                  ProtocolJsonRunResult &result)
    {
        const QJsonObject expected = testCase.expected.value(QStringLiteral("checkpoints"))
                                         .toObject().value(name).toObject();
        if (expected.value(QStringLiteral("clients"))
            != actual.value(QStringLiteral("clients"))) {
            result.failureCategory = QStringLiteral("checkpoint_client_isolation_diff");
        } else if (expected.value(QStringLiteral("remote"))
                   != actual.value(QStringLiteral("remote"))) {
            result.failureCategory = QStringLiteral("checkpoint_lifecycle_diff");
        }
        if (!result.failureCategory.isEmpty()) {
            result.failureMessage = QStringLiteral("Multi-client checkpoint differs: %1").arg(name);
            result.failureCheckpoint = name;
            result.expectedDifference = expected;
            result.actualDifference = actual;
        }
    }

    static void record(ProtocolJsonRunResult &result,
                       const QString &name,
                       bool passed,
                       const QString &category,
                       const QString &message)
    {
        result.checks.insert(name, passed);
        if (!passed && result.failureCategory.isEmpty()) {
            result.failureCategory = category.isEmpty()
                ? QStringLiteral("client_isolation_error") : category;
            result.failureMessage = message;
        }
    }

    static void recordStep(ProtocolJsonRunResult &result,
                           const QString &name,
                           const WineClientStepResult &step)
    {
        record(result, name, step.ok,
               step.errorCategory.isEmpty() ? QStringLiteral("fixture_error")
                                            : step.errorCategory,
               step.errorMessage.isEmpty() ? QStringLiteral("Client step failed")
                                           : step.errorMessage);
    }

    void stopClients()
    {
        for (auto &client : m_clients) {
            if (!client->thread)
                continue;
            client->thread->quit();
            client->thread->wait();
            delete client->thread;
            client->thread = nullptr;
            client->worker = nullptr;
        }
        m_clients.clear();
    }

    std::vector<std::unique_ptr<ClientContext>> m_clients;
};
}

ProtocolJsonRunResult runWineMultiClientProtocolJsonCase(const ProtocolJsonCase &testCase)
{
    WineMultiClientScenarioExecutor executor;
    return executor.run(testCase);
}
