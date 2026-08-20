// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/shellhandler.h"
#include "input-method-unstable-v2.h"
#include "protocol-test-server.h"
#include "seat/helper.h"
#include "seat/seatsmanager.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>

namespace {
SurfaceWrapper *g_wrapper = nullptr;

WSeat *primarySeat(Helper *helper)
{
    const auto seats = helper->seatManager()->seats();
    return seats.isEmpty() ? nullptr : seats.constFirst();
}
}

void protocol_test_desktop_setup(Helper *helper)
{
    protocol_test_create_headless_output(helper->backend(), false);
    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_wrapper = wrapper;
                     });
}

extern "C" void input_method_focus_window(void *data)
{
    auto *state = static_cast<input_method_desktop_state *>(data);
    auto *helper = Helper::instance();
    if (helper && g_wrapper)
        helper->activateSurface(g_wrapper, Qt::ActiveWindowFocusReason);
    state->wrapper_created = g_wrapper ? 1 : 0;
    if (auto *seat = helper ? primarySeat(helper) : nullptr)
        state->keyboard_focused = seat->keyboardFocusSurface() == (g_wrapper ? g_wrapper->surface() : nullptr);
}
