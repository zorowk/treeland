// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocol-test-server.h"
#include "seat/helper.h"

#include <winputdevice.h>
#include <wlr_all.h>
#include <wseat.h>

namespace {
QList<WInputDevice *> g_virtual_pointers;
}

void protocol_test_desktop_setup(Helper *helper)
{
    protocol_test_create_headless_output(helper->backend(), false);

    auto *manager = wlr_virtual_pointer_manager_v1_create(helper->backend()->server()->handle());
    if (!manager)
        return;

    helper->listeners(helper)->add(&manager->events.new_virtual_pointer, helper,
        [helper](wlr_virtual_pointer_v1_new_pointer_event *event) {
            if (!event || !event->new_pointer)
                return;
            auto *device = new WInputDevice(&event->new_pointer->pointer.base, true);
            g_virtual_pointers.append(device);
            auto *seat = helper->seat();
            if (seat)
                seat->attachInputDevice(device);
            device->listeners(helper)->add(&event->new_pointer->pointer.base.events.destroy, helper,
                [device](void *) {
                    g_virtual_pointers.removeOne(device);
                    if (device->seat())
                        device->seat()->detachInputDevice(device);
                    device->deleteLater();
                });
        });
}
