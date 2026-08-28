// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treeland-empty-accounts.h"

#include "client-connection.h"
#include "server-bridge-api.h"

#include <stdio.h>

extern void empty_accounts_read_state(void *data);

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name)) {
        fprintf(stderr, "empty-accounts: failed to connect to Treeland Wayland socket\n");
        return 1;
    }

    struct empty_accounts_state state = { 0 };
    const int read = invoke_on_server_thread(empty_accounts_read_state, &state);
    client_disconnect(&connection);

    if (!read || !state.user_model_present || state.user_count != 0
        || !state.current_user_name_empty) {
        fprintf(stderr,
                "empty-accounts: expected an empty UserModel "
                "(present=%d, count=%d, currentUserNameEmpty=%d)\n",
                state.user_model_present,
                state.user_count,
                state.current_user_name_empty);
        return 1;
    }

    return 0;
}
