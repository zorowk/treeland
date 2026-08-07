/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Pure-C wayland client test infrastructure — no threads, no barriers.
 * All functions are called from the C++ event loop; C code never blocks.
 */
#ifndef DDE_SHELL_TEST_H
#define DDE_SHELL_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_MSG_MAX 256

struct test_result {
    const char *name;
    int         failed;
    char        message[TEST_MSG_MAX];
};

struct test_ctx {
    struct wl_display    *display;
    struct wl_registry   *registry;
    int                   registry_done;

    /* bound globals */
    struct wl_compositor *compositor;
    struct wl_seat       *seat;
    struct wl_output     *output;

    /* protocol objects */
    struct treeland_dde_shell_manager_v1  *manager;
    struct treeland_window_overlap_checker *checker;
    struct treeland_dde_shell_surface_v1  *shell_surface;
    struct treeland_dde_active_v1         *active;
    struct treeland_multitaskview_v1      *multitaskview;
    struct treeland_window_picker_v1      *picker;
    struct treeland_lockscreen_v1         *lockscreen;
    struct wl_surface                     *test_surface;

    /* event verification */
    int checker_enter_received;
    int checker_leave_received;
    int active_in_received;
    int active_out_received;
    int start_drag_received;
    int drop_received;
    int picker_window_received;
    int picker_pid;

    /* results */
    struct test_result *results;
    int                 result_count;
    int                 result_cap;
};

void test_init(struct test_ctx *ctx);
void test_destroy(struct test_ctx *ctx);
int  test_add(struct test_ctx *ctx, const char *name);
void test_fail(struct test_ctx *ctx, int idx, const char *fmt, ...);
void test_pass(struct test_ctx *ctx, int idx);

/* ---- phases: called from C++ event loop ---- */

/* phase 1: bind registry, get globals. returns 1 when done. */
int test_connect(struct test_ctx *ctx, const char *socket);

/* phase 2: send factory requests (get_*). returns 1 when done. */
int test_phase_factories(struct test_ctx *ctx);

/* phase 3: send setter requests. returns 1 when done. */
int test_phase_setters(struct test_ctx *ctx);

/* phase 4: compositor fired events; check listeners. returns 1 when done. */
int test_phase_verify_events(struct test_ctx *ctx);

int test_print_results(struct test_ctx *ctx);
void test_cleanup(struct test_ctx *ctx);

#ifdef __cplusplus
}
#endif
#endif
