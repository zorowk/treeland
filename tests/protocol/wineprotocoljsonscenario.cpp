// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wineprotocoljsonscenario.h"

#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "modules/wine-window-management/winewindowmanagement.h"
#include "protocoljsoncase.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "wineclientworker.h"
#include "winemulticlientscenario.h"

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

#include <functional>
#include <memory>

using namespace WAYLIB_SERVER_NAMESPACE;
QW_USE_NAMESPACE

namespace {
class WineScenarioExecutor
{
public:
    WineScenarioExecutor()
        : m_thread(new QThread)
        , m_worker(new WineClientWorker)
    {
        qRegisterMetaType<WineClientStepResult>();
        m_worker->moveToThread(m_thread);
        QObject::connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
        m_thread->start();
    }

    ~WineScenarioExecutor() { stopThread(); }

    ProtocolJsonRunResult run(const ProtocolJsonCase &testCase)
    {
        ProtocolJsonRunResult result;
        QElapsedTimer elapsed;
        elapsed.start();

        const QJsonObject serverDefinition =
            testCase.input.value(QStringLiteral("server")).toObject();
        const QJsonObject outputDefinition =
            serverDefinition.value(QStringLiteral("outputs")).toArray().first().toObject();
        const QJsonArray outputGeometry =
            outputDefinition.value(QStringLiteral("geometry")).toArray();
        const QSize outputSize(outputGeometry.at(2).toInt(), outputGeometry.at(3).toInt());
        const QJsonArray objects = testCase.input.value(QStringLiteral("client"))
                                       .toObject()
                                       .value(QStringLiteral("objects"))
                                       .toArray();
        const QJsonObject windowDefinition = objects.at(0).toObject();
        const QString appId = windowDefinition.value(QStringLiteral("app_id")).toString();
        const QJsonArray windowSizeJson = windowDefinition.value(QStringLiteral("size")).toArray();
        const QSize windowSize(windowSizeJson.at(0).toInt(), windowSizeJson.at(1).toInt());

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
            QObject::connect(
                wineManager,
                &WineWindowManager::controlResourceCountChanged,
                server.get(),
                [&](qsizetype active, quint64 destroyed) {
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
            ? qw_compositor::create(*server->handle(), 6, *renderer)
            : nullptr;
        auto *subcompositor = compositor ? qw_subcompositor::create(*server->handle()) : nullptr;
        QPointer<qw_compositor> compositorGuard(compositor);
        QPointer<qw_subcompositor> subcompositorGuard(subcompositor);
        record(result, QStringLiteral("renderer_created"),
               renderer && allocator && compositor,
               QStringLiteral("fixture_error"),
               QStringLiteral("Unable to initialize headless compositor renderer"));

        WOutput *fixtureOutput = nullptr;
        QObject::connect(backend, &WBackend::outputAdded, server.get(),
                         [&](WOutput *output) {
                             output->handle()->init_render(*allocator, *renderer);
                             qw_output_state state;
                             state.set_custom_mode(outputSize.width(), outputSize.height(), 60000);
                             state.set_enabled(true);
                             if (output->handle()->commit_state(state)) {
                                 helper->rootSurfaceContainer()->outputLayout()->add(
                                     output,
                                     QPoint(outputGeometry.at(0).toInt(),
                                            outputGeometry.at(1).toInt()));
                                 fixtureOutput = output;
                             }
                         });

        if (result.failureCategory.isEmpty()) {
            backend->handle()->start();
        }
        record(result, QStringLiteral("output_ready"),
               waitForCondition([&fixtureOutput] { return fixtureOutput != nullptr; },
                                backend, SIGNAL(outputAdded(WOutput*))),
               QStringLiteral("fixture_error"),
               QStringLiteral("Headless output did not become active"));

        const qsizetype clientCountBefore = socket->clients().size();
        const qsizetype surfaceCountBefore = helper->rootSurfaceContainer()->surfaces().size();
        WineClientStepResult snapshot;
        QPointer<SurfaceWrapper> wrapper;
        QJsonObject actualCheckpoints;
        QHash<QString, quint32> serials;
        QRectF geometryBefore;
        QRectF geometryAfter;
        bool surfaceWasMapped = false;
        bool disconnected = false;
        bool serverShutdownRequested = false;

        if (result.failureCategory.isEmpty()) {
            snapshot = waitForStep(QStringLiteral("create_mapped_xdg_toplevel"),
                                   [this, path = socket->fullServerName(), appId, windowSize] {
                                       QMetaObject::invokeMethod(
                                           m_worker,
                                           [worker = m_worker, path, appId, windowSize] {
                                               worker->createMappedToplevel(path, appId, windowSize);
                                           },
                                           Qt::QueuedConnection);
                                   });
            recordStep(result, QStringLiteral("fixture_created"), snapshot);
            wrapper = findWrapper(helper->rootSurfaceContainer(), appId);
            record(result, QStringLiteral("surface_mapped"),
                   wrapper && waitForCondition(
                                  [wrapper] { return wrapper->surface()->mapped(); },
                                  wrapper->surface(), SIGNAL(mappedChanged())),
                   QStringLiteral("fixture_error"),
                   QStringLiteral("xdg_toplevel did not reach mapped state"));
            if (wrapper)
                geometryBefore = wrapper->geometry();
            surfaceWasMapped = wrapper && wrapper->surface()->mapped();
        }

        if (result.failureCategory.isEmpty()) {
            snapshot = waitForStep(QStringLiteral("create_window_control"), [this] {
                QMetaObject::invokeMethod(m_worker,
                                          &WineClientWorker::createWindowControl,
                                          Qt::QueuedConnection);
            });
            recordStep(result, QStringLiteral("control_created"), snapshot);
            record(result,
                   QStringLiteral("control_resource_created"),
                   wineManager && waitForControlCount(wineManager, 1),
                   QStringLiteral("resource_leak_or_lifecycle_error"),
                   QStringLiteral("Remote control resource was not observed"));
        }

        const QJsonArray steps = testCase.input.value(QStringLiteral("steps")).toArray();
        for (qsizetype index = 0;
             index < steps.size() && result.failureCategory.isEmpty();
             ++index) {
            const QJsonObject step = steps.at(index).toObject();
            if (step.contains(QStringLiteral("capture"))) {
                const QJsonObject capture = step.value(QStringLiteral("capture")).toObject();
                serials.insert(capture.value(QStringLiteral("name")).toString(),
                               static_cast<quint32>(capture.value(QStringLiteral("value")).toInteger()));
                result.checks.insert(QStringLiteral("step_%1_capture").arg(index), true);
            } else if (step.contains(QStringLiteral("request"))) {
                const QJsonObject request = step.value(QStringLiteral("request")).toObject();
                const QJsonArray args = request.value(QStringLiteral("args")).toArray();
                const QString requestName = request.value(QStringLiteral("name")).toString();
                const bool disconnectBeforeCompletion =
                    request.value(QStringLiteral("disconnect_before_completion")).toBool();
                if (requestName == QStringLiteral("set_position")) {
                    const QString serialReference = args.at(2).toString().mid(1);
                    const QString requestStep = disconnectBeforeCompletion
                        ? QStringLiteral("send_position") : QStringLiteral("set_position");
                    snapshot = waitForStep(
                        requestStep,
                        [this, args, serial = serials.value(serialReference),
                         disconnectBeforeCompletion] {
                                           QMetaObject::invokeMethod(
                                               m_worker,
                                               [worker = m_worker, args, serial,
                                                disconnectBeforeCompletion] {
                                                   if (disconnectBeforeCompletion) {
                                                       worker->sendPosition(args.at(0).toInt(),
                                                                            args.at(1).toInt(),
                                                                            serial);
                                                   } else {
                                                       worker->setPosition(args.at(0).toInt(),
                                                                           args.at(1).toInt(),
                                                                           serial);
                                                   }
                                               },
                                               Qt::QueuedConnection);
                        });
                } else {
                    snapshot = waitForStep(QStringLiteral("set_z_order"), [this, args] {
                        QMetaObject::invokeMethod(
                            m_worker,
                            [worker = m_worker, args] {
                                worker->setZOrder(static_cast<quint32>(args.at(0).toInteger()),
                                                  static_cast<quint32>(args.at(1).toInteger()));
                            },
                            Qt::QueuedConnection);
                    });
                }
                const bool expectedProtocolError = expectedHasProtocolError(testCase);
                if (snapshot.protocolErrorOccurred && expectedProtocolError) {
                    result.checks.insert(QStringLiteral("step_%1_request").arg(index), true);
                } else {
                    recordStep(result, QStringLiteral("step_%1_request").arg(index), snapshot);
                }
            } else if (step.contains(QStringLiteral("barrier"))) {
                const QJsonObject barrier = step.value(QStringLiteral("barrier")).toObject();
                if (barrier.value(QStringLiteral("type")).toString()
                    == QStringLiteral("server_condition")) {
                    const QJsonArray expected = barrier.value(QStringLiteral("equals")).toArray();
                    const QRectF target(expected.at(0).toDouble(), expected.at(1).toDouble(),
                                        expected.at(2).toDouble(), expected.at(3).toDouble());
                    const bool matched = wrapper && waitForGeometry(
                        wrapper, target, barrier.value(QStringLiteral("timeout_ms")).toInt(1000));
                    record(result, QStringLiteral("step_%1_server_condition").arg(index), matched,
                           QStringLiteral("checkpoint_probe_diff"),
                           QStringLiteral("surface.geometry did not satisfy server_condition"));
                } else {
                    result.checks.insert(QStringLiteral("step_%1_client_roundtrip").arg(index),
                                         snapshot.ok);
                }
            } else if (step.contains(QStringLiteral("checkpoint"))) {
                const QString name = step.value(QStringLiteral("checkpoint")).toString();
                const QJsonObject actual = checkpoint(snapshot,
                                                      wrapper.data(),
                                                      remoteControlCount,
                                                      destroyedControlCount,
                                                      socket->clients().size(),
                                                      helper->rootSurfaceContainer()
                                                          ->surfaces().size(),
                                                      server->isRunning());
                actualCheckpoints.insert(name, actual);
                compareCheckpoint(testCase, name, actual, result);
                result.checks.insert(QStringLiteral("step_%1_checkpoint").arg(index),
                                     result.failureCategory.isEmpty());
            } else if (step.contains(QStringLiteral("destroy"))) {
                if (wrapper)
                    geometryAfter = wrapper->geometry();
                const QString mode = step.value(QStringLiteral("destroy")).toObject()
                                         .value(QStringLiteral("mode"))
                                         .toString(QStringLiteral("protocol"));
                if (mode == QStringLiteral("proxy-only")) {
                    snapshot = waitForStep(QStringLiteral("destroy:proxy-only"), [this] {
                        QMetaObject::invokeMethod(m_worker,
                                                  &WineClientWorker::destroyControlProxyOnly,
                                                  Qt::QueuedConnection);
                    });
                } else {
                    snapshot = waitForStep(QStringLiteral("destroy"), [this] {
                        QMetaObject::invokeMethod(m_worker,
                                                  &WineClientWorker::destroyObjects,
                                                  Qt::QueuedConnection);
                    });
                    if (snapshot.ok && wineManager) {
                        record(result,
                               QStringLiteral("step_%1_remote_destroy").arg(index),
                               waitForControlCount(wineManager, 0),
                               QStringLiteral("resource_not_restored"),
                               QStringLiteral("Protocol destroy did not release remote control"));
                    }
                }
                recordStep(result, QStringLiteral("step_%1_destroy").arg(index), snapshot);
            } else if (step.contains(QStringLiteral("disconnect"))) {
                if (wrapper)
                    geometryAfter = wrapper->geometry();
                const QString mode = step.value(QStringLiteral("disconnect")).toObject()
                                         .value(QStringLiteral("mode"))
                                         .toString(QStringLiteral("graceful"));
                const QString disconnectStep = mode == QStringLiteral("abrupt")
                    ? QStringLiteral("disconnect:abrupt") : QStringLiteral("disconnect");
                snapshot = waitForStep(disconnectStep, [this, mode] {
                    if (mode == QStringLiteral("abrupt")) {
                        QMetaObject::invokeMethod(m_worker,
                                                  &WineClientWorker::disconnectAbruptly,
                                                  Qt::QueuedConnection);
                    } else {
                        QMetaObject::invokeMethod(m_worker,
                                                  &WineClientWorker::disconnectClient,
                                                  Qt::QueuedConnection);
                    }
                });
                disconnected = snapshot.ok
                    && waitForCondition([&socket] { return socket->clients().isEmpty(); },
                                        socket.get(), SIGNAL(clientsChanged()));
                record(result, QStringLiteral("step_%1_disconnect").arg(index), disconnected,
                       QStringLiteral("resource_leak_or_lifecycle_error"),
                       QStringLiteral("Client did not disconnect cleanly"));
                if (disconnected && wineManager) {
                    record(result,
                           QStringLiteral("step_%1_remote_resources_restored").arg(index),
                           waitForControlCount(wineManager, 0),
                           QStringLiteral("resource_not_restored"),
                           QStringLiteral("Remote control resource did not return to baseline"));
                }
                if (disconnected) {
                    record(result,
                           QStringLiteral("step_%1_surfaces_restored").arg(index),
                           waitForCondition(
                               [root = helper->rootSurfaceContainer(), surfaceCountBefore] {
                                   return root->surfaces().size() == surfaceCountBefore;
                               },
                               helper->rootSurfaceContainer(),
                               SIGNAL(surfaceRemoved(SurfaceWrapper*))),
                           QStringLiteral("resource_not_restored"),
                           QStringLiteral("Surface resource did not return to baseline"));
                }
            } else if (step.contains(QStringLiteral("server_shutdown"))) {
                serverShutdownRequested = true;
                server->stop();
                snapshot = waitForStep(QStringLiteral("server_shutdown"), [this] {
                    QMetaObject::invokeMethod(m_worker,
                                              &WineClientWorker::observeServerShutdown,
                                              Qt::QueuedConnection);
                });
                disconnected = snapshot.ok && socket->clients().isEmpty();
                recordStep(result, QStringLiteral("step_%1_server_shutdown").arg(index), snapshot);
                record(result,
                       QStringLiteral("step_%1_remote_resources_restored").arg(index),
                       remoteControlCount == 0,
                       QStringLiteral("resource_not_restored"),
                       QStringLiteral("Server shutdown did not destroy remote control resource"));
            }
        }

        if (snapshot.localDisplayAlive) {
            if (wrapper)
                geometryAfter = wrapper->geometry();
            snapshot = waitForStep(QStringLiteral("disconnect"), [this] {
                QMetaObject::invokeMethod(m_worker,
                                          &WineClientWorker::disconnectClient,
                                          Qt::QueuedConnection);
            });
            disconnected = snapshot.ok && (serverShutdownRequested || waitForCondition(
                [&socket] { return socket->clients().isEmpty(); },
                socket.get(), SIGNAL(clientsChanged())));
        }
        const bool surfacesRestored = waitForCondition(
            [root = helper->rootSurfaceContainer(), surfaceCountBefore] {
                return root->surfaces().size() == surfaceCountBefore;
            },
            helper->rootSurfaceContainer(), SIGNAL(surfaceRemoved(SurfaceWrapper*)));
        record(result, QStringLiteral("resources_restored"),
               socket->clients().size() == clientCountBefore && surfacesRestored
                   && remoteControlCount == 0,
               QStringLiteral("resource_not_restored"),
               QStringLiteral("Client, surface, or remote resource count did not return to baseline"));

        if (server->isRunning())
            server->stop();
        socket->close();
        stopThread();
        const QString stage = testCase.input.value(QStringLiteral("lifecycle_test")).toBool()
            ? QStringLiteral("mvp-d2")
            : testCase.input.value(QStringLiteral("validation_mode")).toString()
                    == QStringLiteral("wire")
                ? QStringLiteral("mvp-d1")
                : QStringLiteral("poc-3");
        result.actual = QJsonObject{
            { QStringLiteral("stage"), stage },
            { QStringLiteral("case"), testCase.caseId },
            { QStringLiteral("expectation_source"),
              testCase.expected.value(QStringLiteral("expectation_source")) },
            { QStringLiteral("protocol"),
              QJsonObject{
                  { QStringLiteral("interface"),
                    QStringLiteral("treeland_wine_window_manager_v1") },
                  { QStringLiteral("xml_sha256"), testCase.xmlSha256 },
              } },
            { QStringLiteral("fixture"),
              QJsonObject{
                  { QStringLiteral("wl_surface_created"), snapshot.ok },
                  { QStringLiteral("initial_configure_received"),
                    snapshot.initialConfigureReceived },
                  { QStringLiteral("initial_configure_acknowledged"),
                    snapshot.initialConfigureAcknowledged },
                  { QStringLiteral("shm_buffer_committed"), snapshot.bufferCommitted },
                  { QStringLiteral("surface_mapped"), surfaceWasMapped },
                  { QStringLiteral("window_control_created"), snapshot.controlCreated },
              } },
            { QStringLiteral("geometry_before"), geometryArray(geometryBefore) },
            { QStringLiteral("geometry_after"), geometryArray(geometryAfter) },
            { QStringLiteral("checkpoints"), actualCheckpoints },
            { QStringLiteral("lifecycle"),
              QJsonObject{
                  { QStringLiteral("client_count_before"), clientCountBefore },
                  { QStringLiteral("client_count_after"), socket->clients().size() },
                  { QStringLiteral("surface_count_before"), surfaceCountBefore },
                  { QStringLiteral("surface_count_after"),
                    helper->rootSurfaceContainer()->surfaces().size() },
                  { QStringLiteral("protocol_destructor_sent"),
                    snapshot.protocolDestructorSent },
                  { QStringLiteral("remote_control_resource_count"), remoteControlCount },
                  { QStringLiteral("destroyed_control_resource_count"),
                    qint64(destroyedControlCount) },
                  { QStringLiteral("server_shutdown_requested"), serverShutdownRequested },
                  { QStringLiteral("client_thread_stopped"), m_thread == nullptr },
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
    static bool expectedHasProtocolError(const ProtocolJsonCase &testCase)
    {
        const QJsonObject checkpoints =
            testCase.expected.value(QStringLiteral("checkpoints")).toObject();
        for (const QJsonValue &value : checkpoints) {
            if (value.toObject().value(QStringLiteral("connection")).toObject()
                    .value(QStringLiteral("protocol_error_occurred")).toBool()) {
                return true;
            }
        }
        return false;
    }

    static QJsonArray geometryArray(const QRectF &geometry)
    {
        return { geometry.x(), geometry.y(), geometry.width(), geometry.height() };
    }

    static SurfaceWrapper *findWrapper(RootSurfaceContainer *root, const QString &appId)
    {
        for (SurfaceWrapper *wrapper : root->surfaces()) {
            if (wrapper->appId() == appId)
                return wrapper;
        }
        return nullptr;
    }

    WineClientStepResult waitForStep(const QString &step,
                                     const std::function<void()> &start,
                                     int timeoutMs = 3000)
    {
        WineClientStepResult value;
        bool completed = false;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        const auto connection = QObject::connect(
            m_worker, &WineClientWorker::stepFinished, &loop,
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
            value.errorMessage = QStringLiteral("Timed out waiting for client step '%1'").arg(step);
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

    static bool waitForGeometry(SurfaceWrapper *wrapper, const QRectF &target, int timeoutMs)
    {
        if (wrapper->geometry() == target)
            return true;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        const auto check = [&] {
            if (wrapper->geometry() == target)
                loop.quit();
        };
        const auto x = QObject::connect(wrapper, &QQuickItem::xChanged, &loop, check);
        const auto y = QObject::connect(wrapper, &QQuickItem::yChanged, &loop, check);
        const auto width = QObject::connect(wrapper, &QQuickItem::widthChanged, &loop, check);
        const auto height = QObject::connect(wrapper, &QQuickItem::heightChanged, &loop, check);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        loop.exec();
        QObject::disconnect(x);
        QObject::disconnect(y);
        QObject::disconnect(width);
        QObject::disconnect(height);
        return wrapper->geometry() == target;
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
            manager,
            &WineWindowManager::controlResourceCountChanged,
            &loop,
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

    static QJsonObject checkpoint(const WineClientStepResult &snapshot,
                                  SurfaceWrapper *wrapper,
                                  qsizetype remoteControlCount,
                                  quint64 destroyedControlCount,
                                  qsizetype clientCount,
                                  qsizetype surfaceCount,
                                  bool serverRunning)
    {
        QJsonArray events;
        for (const QJsonValue &value : snapshot.events) {
            QJsonObject event = value.toObject();
            event.insert(QStringLiteral("object"), QStringLiteral("control"));
            events.append(event);
        }
        QJsonObject connection{
            { QStringLiteral("display_error"), snapshot.displayError },
            { QStringLiteral("protocol_error_occurred"), snapshot.protocolErrorOccurred },
        };
        if (snapshot.protocolErrorOccurred) {
            connection.insert(
                QStringLiteral("protocol_error"),
                QJsonObject{
                    { QStringLiteral("interface"), snapshot.protocolErrorInterface },
                    { QStringLiteral("object_id"), qint64(snapshot.protocolErrorObjectId) },
                    { QStringLiteral("code"), qint64(snapshot.protocolErrorCode) },
                    { QStringLiteral("object"), snapshot.protocolErrorObject },
                });
        }
        return {
            { QStringLiteral("connection"),
              connection },
            { QStringLiteral("client_events"), events },
            { QStringLiteral("server_state"),
              QJsonObject{
                  { QStringLiteral("surface.geometry"),
                    wrapper ? geometryArray(wrapper->geometry()) : QJsonArray{} },
              } },
            { QStringLiteral("lifecycle"),
              QJsonObject{
                  { QStringLiteral("destroy_mode"), snapshot.destroyMode },
                  { QStringLiteral("disconnect_mode"), snapshot.disconnectMode },
                  { QStringLiteral("protocol_destructor_sent"),
                    snapshot.protocolDestructorSent },
                  { QStringLiteral("abrupt_transport_closed"),
                    snapshot.abruptTransportClosed },
                  { QStringLiteral("local"),
                    QJsonObject{
                        { QStringLiteral("display_alive"), snapshot.localDisplayAlive },
                        { QStringLiteral("manager_proxy_alive"),
                          snapshot.localManagerProxyAlive },
                        { QStringLiteral("control_proxy_alive"),
                          snapshot.localControlProxyAlive },
                        { QStringLiteral("surface_proxy_alive"),
                          snapshot.localSurfaceProxyAlive },
                        { QStringLiteral("proxy_count"), snapshot.localProxyCount },
                    } },
                  { QStringLiteral("remote"),
                    QJsonObject{
                        { QStringLiteral("control_resource_count"), remoteControlCount },
                        { QStringLiteral("destroyed_control_resource_count"),
                          qint64(destroyedControlCount) },
                        { QStringLiteral("client_count"), clientCount },
                        { QStringLiteral("surface_count"), surfaceCount },
                        { QStringLiteral("server_running"), serverRunning },
                    } },
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
        const QJsonObject expectedConnection =
            expected.value(QStringLiteral("connection")).toObject();
        const QJsonObject actualConnection = actual.value(QStringLiteral("connection")).toObject();
        const QJsonObject expectedProtocolError =
            expectedConnection.value(QStringLiteral("protocol_error")).toObject();
        const QJsonObject actualProtocolError =
            actualConnection.value(QStringLiteral("protocol_error")).toObject();
        const bool connectionMatches =
            expectedConnection.value(QStringLiteral("display_error"))
                == actualConnection.value(QStringLiteral("display_error"))
            && expectedConnection.value(QStringLiteral("protocol_error_occurred"))
                == actualConnection.value(QStringLiteral("protocol_error_occurred"))
            && (!expectedConnection.value(QStringLiteral("protocol_error_occurred")).toBool()
                || (expectedProtocolError.value(QStringLiteral("interface"))
                        == actualProtocolError.value(QStringLiteral("interface"))
                    && expectedProtocolError.value(QStringLiteral("code"))
                        == actualProtocolError.value(QStringLiteral("code"))
                    && expectedProtocolError.value(QStringLiteral("object"))
                        == actualProtocolError.value(QStringLiteral("object"))));
        if (!connectionMatches) {
            result.failureCategory = QStringLiteral("checkpoint_protocol_error_diff");
        } else if (expected.value(QStringLiteral("client_events"))
            != actual.value(QStringLiteral("client_events"))) {
            result.failureCategory = QStringLiteral("checkpoint_event_diff");
        } else if (expected.value(QStringLiteral("server_state"))
                   != actual.value(QStringLiteral("server_state"))) {
            result.failureCategory = QStringLiteral("checkpoint_probe_diff");
        } else if (expected.contains(QStringLiteral("lifecycle"))
                   && expected.value(QStringLiteral("lifecycle"))
                       != actual.value(QStringLiteral("lifecycle"))) {
            result.failureCategory = QStringLiteral("checkpoint_lifecycle_diff");
        }
        if (!result.failureCategory.isEmpty()) {
            result.failureMessage = QStringLiteral("Checkpoint differs: %1").arg(name);
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
            result.failureCategory = category;
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
    WineClientWorker *m_worker = nullptr;
};
}

ProtocolJsonRunResult runWineProtocolJsonCase(const ProtocolJsonCase &testCase)
{
    if (testCase.input.value(QStringLiteral("multi_client_test")).toBool())
        return runWineMultiClientProtocolJsonCase(testCase);
    WineScenarioExecutor executor;
    return executor.run(testCase);
}
