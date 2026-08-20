// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "protocol-test-server.h"
#include "seat/helper.h"
#include "wlr-output-power-management-unstable-v1.h"

#include <woutput.h>

void protocol_test_desktop_setup(Helper *helper)
{
    protocol_test_create_headless_output(helper->backend(), false);
}

extern "C" void output_power_read_server_state(void *data)
{
    output_power_server_state state {};
    const auto outputs = Helper::instance()->rootSurfaceContainer()->outputs();
    state.output_count = outputs.size();
    if (!outputs.isEmpty())
        state.enabled = outputs.constFirst()->output()->isEnabled();
    *static_cast<output_power_server_state *>(data) = state;
}
