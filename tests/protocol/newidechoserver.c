#include "wayland-treeland-test-new-id-v1-server-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wayland-server.h>
static void h_create_child(struct wl_client *c, struct wl_resource *r) {
    struct wl_resource *ch = wl_resource_create(c, &treeland_test_child_v1_interface, 1, 0);
    treeland_test_new_id_v1_send_child_created(r, ch);
    treeland_test_child_v1_send_done(ch, 99);
}
static void h_destroy(struct wl_client *c, struct wl_resource *r) { wl_resource_destroy(r); }
static const struct treeland_test_new_id_v1_interface impl = { .create_child = h_create_child, .destroy = h_destroy };
static void bind_fn(struct wl_client *c, void *d, uint32_t v, uint32_t id) {
    struct wl_resource *r = wl_resource_create(c, &treeland_test_new_id_v1_interface, v, id);
    wl_resource_set_implementation(r, &impl, NULL, NULL);
}
int main(int argc, char **argv) {
    if (argc < 2) return 1; unlink(argv[1]);
    struct wl_display *d = wl_display_create();
    wl_global_create(d, &treeland_test_new_id_v1_interface, 1, NULL, bind_fn);
    wl_display_add_socket(d, argv[1]); wl_display_run(d); return 0;
}
