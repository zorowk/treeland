// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct wl_registry;
struct wl_registry_listener;
struct treeland_window_management_v1;

#ifdef __cplusplus
extern "C" {
#endif

enum {
    TL_WINDOW_MANAGEMENT_MAX_EVENTS = 8,
};

struct tl_window_management_adapter {
    struct treeland_window_management_v1 *proxy;
    uint32_t global_name;
    uint32_t advertised_version;
    uint32_t bound_version;
    uint32_t events[TL_WINDOW_MANAGEMENT_MAX_EVENTS];
    size_t event_count;
    bool local_proxy_alive;
    bool protocol_destructor_sent;
};

void tl_window_management_adapter_init(struct tl_window_management_adapter *adapter);
const struct wl_registry_listener *tl_window_management_registry_listener(void);
int tl_window_management_bind(struct tl_window_management_adapter *adapter,
                              struct wl_registry *registry,
                              uint32_t requested_version);
void tl_window_management_clear_events(struct tl_window_management_adapter *adapter);
int tl_window_management_set_desktop(struct tl_window_management_adapter *adapter,
                                     uint32_t state);
int tl_window_management_destroy(struct tl_window_management_adapter *adapter);

#ifdef __cplusplus
}
#endif
