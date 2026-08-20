// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "protocol-test-server.h"
#include "seat/helper.h"
#include "wlr-output-management-unstable-v1.h"

#include <woutput.h>
#include <woutputitem.h>

void protocol_test_desktop_setup(Helper *helper)
{
    protocol_test_create_headless_output(helper->backend(), false);
}

extern "C" void output_management_read_server_state(void *data)
{
    output_management_server_state state {};
    const auto outputs = Helper::instance()->rootSurfaceContainer()->outputs();
    state.output_count = outputs.size();

    if (!outputs.isEmpty()) {
        auto *output = outputs.constFirst()->output();
        const auto *item = WOutputItem::getOutputItem(output);
        state.enabled = output->isEnabled();
        state.transform = static_cast<int>(output->orientation());
        state.scale_milli = static_cast<int>(output->scale() * 1000.0f);
        if (item) {
            state.x = static_cast<int>(item->x());
            state.y = static_cast<int>(item->y());
        }
    }

    *static_cast<output_management_server_state *>(data) = state;
}
