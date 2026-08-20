// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "protocol-test-server.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"
#include "workspace/workspace.h"

#include <woutputrenderwindow.h>

#include <QEventLoop>
#include <QQuickItem>

namespace {
SurfaceWrapper *g_wrapper = nullptr;
}

void protocol_test_desktop_setup(Helper *helper)
{
    // Keep production geometry transitions short, but wait for their real
    // completion signal below instead of sampling a timing-dependent frame.
    helper->setAnimationSpeed(0.1f);
    protocol_test_create_headless_output(helper->backend(), false);
    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [helper](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_wrapper = wrapper;
                     });
}

extern "C" void foreign_toplevel_read_server_state(void *data)
{
    foreign_toplevel_server_state state {};
    auto *helper = Helper::instance();
    state.output_ready = !helper->rootSurfaceContainer()->outputs().isEmpty();
    state.wrapper_created = g_wrapper != nullptr;
    state.wrapper_in_workspace = g_wrapper && helper->workspace()->surfaces().contains(g_wrapper);
    state.minimized = g_wrapper && g_wrapper->shellSurface() && g_wrapper->shellSurface()->isMinimized();
    state.maximized = g_wrapper && g_wrapper->isMaximized();
    state.fullscreen = g_wrapper && g_wrapper->surfaceState() == SurfaceWrapper::State::Fullscreen;
    state.activated = g_wrapper && g_wrapper->isActivated();
    const auto *seatContainer = helper->rootSurfaceContainer()->getSeatContainerOrDefault();
    state.focused = seatContainer && seatContainer->keyboardFocusSurface() == g_wrapper;
    *static_cast<foreign_toplevel_server_state *>(data) = state;
}

extern "C" void foreign_toplevel_render(void *)
{
    if (!g_wrapper)
        return;
    auto *helper = Helper::instance();
    helper->window()->render();
    if (!g_wrapper->isAnimationRunning())
        return;

    QQuickItem *geometryAnimation = nullptr;
    for (auto *item : g_wrapper->container()->childItems()) {
        if (item->property("surface").value<SurfaceWrapper *>() == g_wrapper) {
            geometryAnimation = item;
            break;
        }
    }
    if (!geometryAnimation)
        return;
    QEventLoop eventLoop;
    QObject::connect(geometryAnimation, SIGNAL(finished()), &eventLoop, SLOT(quit()));
    eventLoop.exec();
}
