// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treelandinit.h"
#include <DGuiApplicationHelper>
#include <QGuiApplication>
#include <qwlogging.h>
#include <wrenderhelper.h>
#include <wbackend.h>
#include <qwdisplay.h>
#include <qwcompositor.h>
#include <qwsubcompositor.h>
#include <qwrenderer.h>
#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE
DCORE_USE_NAMESPACE

namespace Treeland {

void preInit(const InitOptions &opts)
{
    qw_log::init();
    if (opts.headless) qputenv("WLR_BACKENDS", "headless");
    DTK_GUI_NAMESPACE::DGuiApplicationHelper::setAttribute(
        DTK_GUI_NAMESPACE::DGuiApplicationHelper::DontSaveApplicationTheme, true);
    WServer::initializeQPA({}, opts.createPlatformTheme);
    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
}

void initTestServer(WServer *server)
{
    auto *backend = server->attach<WBackend>();
    auto *renderer = WRenderHelper::createRenderer(backend->handle());
    qw_compositor::create(server->handle()->handle(), 6, renderer->handle());
    qw_subcompositor::create(server->handle()->handle());
}

void postInit()
{
    WRenderHelper::setupRendererBackend();
}

}
