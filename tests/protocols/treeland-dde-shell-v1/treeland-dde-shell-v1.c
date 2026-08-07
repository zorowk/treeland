/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Pure-C wayland client — called from C++ event loop via QSocketNotifier.
 * Each function returns immediately; no blocking, no threads.
 */
#define _GNU_SOURCE
#include "treeland-dde-shell-v1.h"
#include "treeland-dde-shell-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>

/* ================================================================ */
/* result bookkeeping                                                */
/* ================================================================ */

void test_init(struct test_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->result_cap = 32;
    ctx->results = calloc(ctx->result_cap, sizeof(struct test_result));
}

void test_destroy(struct test_ctx *ctx)
{
    free(ctx->results);
    memset(ctx, 0, sizeof(*ctx));
}

int test_add(struct test_ctx *ctx, const char *name)
{
    if (ctx->result_count >= ctx->result_cap) {
        ctx->result_cap *= 2;
        ctx->results = realloc(ctx->results,
                               (size_t)ctx->result_cap * sizeof(struct test_result));
    }
    int i = ctx->result_count++;
    ctx->results[i].name = name;
    ctx->results[i].failed = 0;
    ctx->results[i].message[0] = 0;
    return i;
}

void test_fail(struct test_ctx *ctx, int idx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ctx->results[idx].message, TEST_MSG_MAX, fmt, ap);
    va_end(ap);
    ctx->results[idx].failed = 1;
}

void test_pass(struct test_ctx *ctx, int idx)
{
    ctx->results[idx].failed = 0;
}

int test_print_results(struct test_ctx *ctx)
{
    int failed = 0;
    printf("\n=== results ===\n");
    for (int i = 0; i < ctx->result_count; i++) {
        printf("  [%s] %s", ctx->results[i].failed ? "FAIL" : "PASS",
               ctx->results[i].name);
        if (ctx->results[i].failed)
            printf(" -- %s", ctx->results[i].message);
        printf("\n");
        if (ctx->results[i].failed) failed++;
    }
    printf("%d/%d passed\n", ctx->result_count - failed, ctx->result_count);
    return failed ? 0 : 1;
}

/* ================================================================ */
/* registry glue                                                     */
/* ================================================================ */

static void registry_global(void *data, struct wl_registry *r,
                            uint32_t name, const char *iface, uint32_t ver)
{
    struct test_ctx *ctx = data;
    (void)ver;

    if (!strcmp(iface, "wl_compositor"))
        ctx->compositor = wl_registry_bind(r, name, &wl_compositor_interface, 1);
    else if (!strcmp(iface, "wl_seat"))
        ctx->seat = wl_registry_bind(r, name, &wl_seat_interface, 1);
    else if (!strcmp(iface, "wl_output"))
        ctx->output = wl_registry_bind(r, name, &wl_output_interface, 1);
    else if (!strcmp(iface, "treeland_dde_shell_manager_v1"))
        ctx->manager = wl_registry_bind(r, name,
                                        &treeland_dde_shell_manager_v1_interface, 1);
}

static void registry_global_remove(void *data, struct wl_registry *r, uint32_t name)
{
    (void)data; (void)r; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

/* ================================================================ */
/* event listeners                                                   */
/* ================================================================ */

static void checker_enter(void *data, struct treeland_window_overlap_checker *c)
{
    (void)c;
    ((struct test_ctx *)data)->checker_enter_received = 1;
}
static void checker_leave(void *data, struct treeland_window_overlap_checker *c)
{
    (void)c;
    ((struct test_ctx *)data)->checker_leave_received = 1;
}
static const struct treeland_window_overlap_checker_listener checker_listener = {
    .enter = checker_enter, .leave = checker_leave,
};

static void active_in(void *data, struct treeland_dde_active_v1 *a, uint32_t reason)
{
    (void)a;
    ((struct test_ctx *)data)->active_in_received = (int)reason;
}
static void active_out(void *data, struct treeland_dde_active_v1 *a, uint32_t reason)
{
    (void)a;
    ((struct test_ctx *)data)->active_out_received = (int)reason;
}
static void start_drag_cb(void *data, struct treeland_dde_active_v1 *a)
{
    (void)a;
    ((struct test_ctx *)data)->start_drag_received = 1;
}
static void drop_cb(void *data, struct treeland_dde_active_v1 *a)
{
    (void)a;
    ((struct test_ctx *)data)->drop_received = 1;
}
static const struct treeland_dde_active_v1_listener active_listener = {
    .active_in = active_in, .active_out = active_out,
    .start_drag = start_drag_cb, .drop = drop_cb,
};

static void picker_window(void *data, struct treeland_window_picker_v1 *p, int32_t pid)
{
    (void)p;
    struct test_ctx *ctx = data;
    ctx->picker_window_received = 1;
    ctx->picker_pid = pid;
}
static const struct treeland_window_picker_v1_listener picker_listener = {
    .window = picker_window,
};

/* ================================================================ */
/* phases                                                            */
/* ================================================================ */

int test_phase_bind(struct test_ctx *ctx)
{
    if (ctx->registry_done) return 1; /* already done */

    if (!ctx->registry) {
        /* first call */
        ctx->registry = wl_display_get_registry(ctx->display);
        wl_registry_add_listener(ctx->registry, &registry_listener, ctx);
        return 0; /* need dispatch to receive globals */
    }

    /* after dispatch: check if manager is bound */
    if (ctx->manager) {
        ctx->registry_done = 1;
        return 1;
    }
    return 0; /* keep dispatching */
}

/* connect + bind globals, returns 0 on failure */
int test_connect(struct test_ctx *ctx, const char *socket_name)
{
    ctx->display = wl_display_connect(socket_name);
    if (!ctx->display) return 0;
    ctx->registry = wl_display_get_registry(ctx->display);
    wl_registry_add_listener(ctx->registry, &registry_listener, ctx);
    wl_display_roundtrip(ctx->display);
    wl_display_roundtrip(ctx->display);
    return ctx->manager != NULL;
}
int test_phase_factories(struct test_ctx *ctx)
{
    static int step = 0;
    struct test_ctx *c = ctx;

    switch (step) {
    case 0:
        test_add(ctx, "manager.get_window_overlap_checker");
        c->checker = treeland_dde_shell_manager_v1_get_window_overlap_checker(c->manager);
        if (!c->checker) test_fail(ctx, ctx->result_count - 1, "returned NULL");
        step++; return 0;

    case 1:
        test_add(ctx, "manager.get_shell_surface");
        if (!ctx->compositor) { step = 0; return 1; }
        c->test_surface = wl_compositor_create_surface(c->compositor);
        c->shell_surface = treeland_dde_shell_manager_v1_get_shell_surface(c->manager, c->test_surface);
        if (!c->shell_surface) test_fail(ctx, ctx->result_count - 1, "returned NULL");
        step++; return 0;

    case 2:
        test_add(ctx, "manager.get_treeland_dde_active");
        c->active = treeland_dde_shell_manager_v1_get_treeland_dde_active(c->manager, c->seat);
        if (!c->active) test_fail(ctx, ctx->result_count - 1, "returned NULL");
        step++; return 0;

    case 3:
        test_add(ctx, "manager.get_treeland_multitaskview");
        c->multitaskview = treeland_dde_shell_manager_v1_get_treeland_multitaskview(c->manager);
        if (!c->multitaskview) test_fail(ctx, ctx->result_count - 1, "returned NULL");
        step++; return 0;

    case 4:
        test_add(ctx, "manager.get_treeland_window_picker");
        c->picker = treeland_dde_shell_manager_v1_get_treeland_window_picker(c->manager);
        treeland_window_picker_v1_add_listener(c->picker, &picker_listener, c);
        if (!c->picker) test_fail(ctx, ctx->result_count - 1, "returned NULL");
        step++; return 0;

    case 5:
        test_add(ctx, "manager.get_treeland_lockscreen");
        c->lockscreen = treeland_dde_shell_manager_v1_get_treeland_lockscreen(c->manager);
        if (!c->lockscreen) test_fail(ctx, ctx->result_count - 1, "returned NULL");
        step = 0; return 1;
    }
    return 0;
}

int test_phase_setters(struct test_ctx *ctx)
{
    static int step = 0;
    struct test_ctx *c = ctx;

    switch (step) {
    case 0:
        test_add(ctx, "checker.update");
        treeland_window_overlap_checker_add_listener(c->checker, &checker_listener, c);
        if (c->output) treeland_window_overlap_checker_update(c->checker, 100, 100,
                                               TREELAND_WINDOW_OVERLAP_CHECKER_ANCHOR_TOP,
                                               c->output);
        step++; return 0;

    case 1:
        test_add(ctx, "shell_surface.set_surface_position");
        treeland_dde_shell_surface_v1_set_surface_position(c->shell_surface, 42, 24);
        step++; return 0;

    case 2:
        test_add(ctx, "shell_surface.set_role");
        treeland_dde_shell_surface_v1_set_role(c->shell_surface,
                                               TREELAND_DDE_SHELL_SURFACE_V1_ROLE_OVERLAY);
        step++; return 0;

    case 3:
        test_add(ctx, "shell_surface.set_skip_switcher");
        treeland_dde_shell_surface_v1_set_skip_switcher(c->shell_surface, 1);
        step++; return 0;

    case 4:
        test_add(ctx, "shell_surface.set_accept_keyboard_focus");
        treeland_dde_shell_surface_v1_set_accept_keyboard_focus(c->shell_surface, 0);
        step++; return 0;

    case 5:
        test_add(ctx, "multitaskview.toggle");
        treeland_multitaskview_v1_toggle(c->multitaskview);
        step++; return 0;

    case 6:
        test_add(ctx, "lockscreen.lock");
        treeland_lockscreen_v1_lock(c->lockscreen);
        step++; return 0;

    case 7:
        test_add(ctx, "lockscreen.shutdown");
        treeland_lockscreen_v1_shutdown(c->lockscreen);
        step++; return 0;

    case 8:
        test_add(ctx, "lockscreen.switch_user");
        treeland_lockscreen_v1_switch_user(c->lockscreen);
        step++; return 0;

    case 9:
        test_add(ctx, "picker.pick");
        treeland_window_picker_v1_pick(c->picker, "test-hint");
        step = 0; return 1;
    }
    return 0;
}

int test_phase_verify_events(struct test_ctx *ctx)
{
    static int step = 0;

    switch (step) {
    case 0:
        test_add(ctx, "checker.event.enter");
        if (!ctx->checker_enter_received)
            test_fail(ctx, ctx->result_count - 1, "not received");
        step++; return 0;

    case 1:
        test_add(ctx, "checker.event.leave");
        if (!ctx->checker_leave_received)
            test_fail(ctx, ctx->result_count - 1, "not received");
        step++; return 0;

    case 2:
        test_add(ctx, "active.event.active_in");
        if (!ctx->active_in_received)
            test_fail(ctx, ctx->result_count - 1, "not received");
        step++; return 0;

    case 3:
        test_add(ctx, "active.event.active_out");
        if (!ctx->active_out_received)
            test_fail(ctx, ctx->result_count - 1, "not received");
        step++; return 0;

    case 4:
        test_add(ctx, "active.event.start_drag");
        if (!ctx->start_drag_received)
            test_fail(ctx, ctx->result_count - 1, "not received");
        step++; return 0;

    case 5:
        test_add(ctx, "active.event.drop");
        if (!ctx->drop_received)
            test_fail(ctx, ctx->result_count - 1, "not received");
        step++; return 0;

    case 6:
        test_add(ctx, "picker.event.window");
        if (!ctx->picker_window_received)
            test_fail(ctx, ctx->result_count - 1, "not received");
        else if (ctx->picker_pid < 0)
            test_fail(ctx, ctx->result_count - 1, "invalid pid %d", ctx->picker_pid);
        step = 0; return 1;
    }
    return 0;
}


void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->shell_surface) treeland_dde_shell_surface_v1_destroy(ctx->shell_surface);
    if (ctx->checker) treeland_window_overlap_checker_destroy(ctx->checker);
    if (ctx->multitaskview) treeland_multitaskview_v1_destroy(ctx->multitaskview);
    if (ctx->picker) treeland_window_picker_v1_destroy(ctx->picker);
    if (ctx->lockscreen) treeland_lockscreen_v1_destroy(ctx->lockscreen);
    if (ctx->active) treeland_dde_active_v1_destroy(ctx->active);
    if (ctx->manager) treeland_dde_shell_manager_v1_destroy(ctx->manager);
    if (ctx->registry) wl_registry_destroy(ctx->registry);
    if (ctx->display) wl_display_disconnect(ctx->display);
}
