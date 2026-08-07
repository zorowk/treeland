/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 * Two-thread protocol test — minimal compositor via initTestServer().
 */
#include "core/treelandinit.h"
#include "treeland-dde-shell-v1.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include <WServer>
#include <WSeat>
#include <wsocket.h>
#include <wthreadutils.h>
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <wayland-client.h>
#include <pthread.h>
#include <signal.h>
WAYLIB_SERVER_USE_NAMESPACE

static struct test_ctx g_ctx;
static volatile int    g_done = 0;

static WindowOverlapCheckerInterface *g_checker = nullptr;
static DDEActiveInterface            *g_active = nullptr;
static WindowPickerInterface         *g_picker = nullptr;

static void setup_signals(DDEShellManagerInterfaceV1 *ds)
{
    QObject::connect(ds, &DDEShellManagerInterfaceV1::windowOverlapCheckerCreated,
                     [](WindowOverlapCheckerInterface *c) { g_checker = c; });
    QObject::connect(ds, &DDEShellManagerInterfaceV1::activeCreated,
                     [](DDEActiveInterface *a) { g_active = a; });
    QObject::connect(ds, &DDEShellManagerInterfaceV1::PickerCreated,
                     [](WindowPickerInterface *p) { g_picker = p; });
}

static void *test_thread(void *arg)
{
    test_init(&g_ctx);
    if (!test_connect(&g_ctx, (const char *)arg)) { g_done = 1; return nullptr; }

    while (!test_phase_factories(&g_ctx)) {
        wl_display_flush(g_ctx.display);
        WThreadUtil::gui().exec([] { QCoreApplication::processEvents(); });
    }
    while (!test_phase_setters(&g_ctx)) {
        wl_display_flush(g_ctx.display);
        WThreadUtil::gui().exec([] { QCoreApplication::processEvents(); });
    }
    WThreadUtil::gui().exec([] {
        QCoreApplication::processEvents();
        if (g_checker) { g_checker->sendOverlapped(true); g_checker->sendOverlapped(false); }
        if (g_active) {
            DDEActiveInterface::sendActiveIn(0, g_active->seat());
            DDEActiveInterface::sendActiveOut(1, g_active->seat());
            DDEActiveInterface::sendStartDrag(g_active->seat());
            DDEActiveInterface::sendDrop(g_active->seat());
        }
        if (g_picker) g_picker->sendWindowPid(42);
        QCoreApplication::processEvents();
    });
    wl_display_roundtrip(g_ctx.display);
    test_phase_verify_events(&g_ctx);
    test_cleanup(&g_ctx);
    g_done = 1;
    return nullptr;
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    Treeland::preInit(Treeland::InitOptions{.headless = true});
    QGuiApplication app(argc, argv);

    WServer server;
    server.start();
    Treeland::initTestServer(&server);
    auto *ds = server.attach<DDEShellManagerInterfaceV1>();
    setup_signals(ds);

    WSocket sock(false);
    if (!sock.autoCreate("/tmp")) { qCritical() << "socket"; return 1; }
    server.addSocket(&sock);
    QByteArray name = sock.fullServerName().toUtf8();

    pthread_t thr;
    pthread_create(&thr, nullptr, test_thread, (void *)name.constData());

    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, [&] { if (g_done) app.quit(); });
    tick.start(100);
    int ret = app.exec();
    pthread_join(thr, nullptr);

    int ok = test_print_results(&g_ctx);
    test_destroy(&g_ctx);
    return ok ? 0 : 1;
}
