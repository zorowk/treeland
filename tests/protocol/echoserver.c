// Echo server for gen-test-client E2E tests. Takes socket path as argv[1].
#include "wayland-treeland-test-multi-arg-v1-server-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server.h>

static void h_echo(struct wl_client *c, struct wl_resource *r,
                   uint32_t id, int32_t offset, const char *name) {
    treeland_test_multi_arg_v1_send_reply(r, id, offset, name);
}
static void h_destroy(struct wl_client *c, struct wl_resource *r) { wl_resource_destroy(r); }
static const struct treeland_test_multi_arg_v1_interface impl = {
    .echo = h_echo, .destroy = h_destroy
};
static void bind_fn(struct wl_client *c, void *d, uint32_t v, uint32_t id) {
    struct wl_resource *r = wl_resource_create(c, &treeland_test_multi_arg_v1_interface, v, id);
    wl_resource_set_implementation(r, &impl, NULL, NULL);
}
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <socket-path>\n", argv[0]); return 1; }
    const char *path = argv[1];
    unlink(path);
    struct wl_display *d = wl_display_create();
    wl_global_create(d, &treeland_test_multi_arg_v1_interface, 1, NULL, bind_fn);
    if (wl_display_add_socket(d, path) != 0) { fprintf(stderr, "socket fail\n"); return 1; }
    wl_display_run(d);
    return 0;
}
