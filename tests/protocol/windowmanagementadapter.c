// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "windowmanagementadapter.h"

#include "wayland-treeland-window-management-v1-client-protocol.h"

#include <string.h>
#include <wayland-client.h>

static void handle_show_desktop(void *data,
                                struct treeland_window_management_v1 *proxy,
                                uint32_t state)
{
    struct tl_window_management_adapter *adapter = data;
    (void)proxy;

    if (adapter->event_count < TL_WINDOW_MANAGEMENT_MAX_EVENTS)
        adapter->events[adapter->event_count++] = state;
}

static const struct treeland_window_management_v1_listener window_management_listener = {
    .show_desktop = handle_show_desktop,
};

static void handle_global(void *data,
                          struct wl_registry *registry,
                          uint32_t name,
                          const char *interface,
                          uint32_t version)
{
    struct tl_window_management_adapter *adapter = data;
    (void)registry;

    if (strcmp(interface, treeland_window_management_v1_interface.name) != 0)
        return;

    adapter->global_name = name;
    adapter->advertised_version = version;
}

static void handle_global_remove(void *data,
                                 struct wl_registry *registry,
                                 uint32_t name)
{
    struct tl_window_management_adapter *adapter = data;
    (void)registry;

    if (adapter->global_name == name)
        adapter->global_name = 0;
}

static const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

void tl_window_management_adapter_init(struct tl_window_management_adapter *adapter)
{
    memset(adapter, 0, sizeof(*adapter));
}

const struct wl_registry_listener *tl_window_management_registry_listener(void)
{
    return &registry_listener;
}

int tl_window_management_bind(struct tl_window_management_adapter *adapter,
                              struct wl_registry *registry,
                              uint32_t requested_version)
{
    if (!adapter->global_name || !adapter->advertised_version || adapter->proxy)
        return -1;

    adapter->bound_version = requested_version < adapter->advertised_version
        ? requested_version
        : adapter->advertised_version;
    adapter->proxy = wl_registry_bind(registry,
                                      adapter->global_name,
                                      &treeland_window_management_v1_interface,
                                      adapter->bound_version);
    if (!adapter->proxy)
        return -1;

    if (treeland_window_management_v1_add_listener(adapter->proxy,
                                                    &window_management_listener,
                                                    adapter) != 0) {
        wl_proxy_destroy((struct wl_proxy *)adapter->proxy);
        adapter->proxy = NULL;
        return -1;
    }

    adapter->local_proxy_alive = true;
    return 0;
}

void tl_window_management_clear_events(struct tl_window_management_adapter *adapter)
{
    adapter->event_count = 0;
}

int tl_window_management_set_desktop(struct tl_window_management_adapter *adapter,
                                     uint32_t state)
{
    if (!adapter->proxy || !adapter->local_proxy_alive)
        return -1;

    treeland_window_management_v1_set_desktop(adapter->proxy, state);
    return 0;
}

int tl_window_management_destroy(struct tl_window_management_adapter *adapter)
{
    if (!adapter->proxy || !adapter->local_proxy_alive)
        return -1;

    treeland_window_management_v1_destroy(adapter->proxy);
    adapter->proxy = NULL;
    adapter->local_proxy_alive = false;
    adapter->protocol_destructor_sent = true;
    return 0;
}
