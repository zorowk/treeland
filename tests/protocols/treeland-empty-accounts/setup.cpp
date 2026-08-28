// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "greeter/usermodel.h"
#include "seat/helper.h"
#include "treeland-empty-accounts.h"

void protocol_test_setup(Helper *helper)
{
    Q_ASSERT(helper);
    Q_ASSERT(helper->userModel());
}

extern "C" void empty_accounts_read_state(void *data)
{
    auto *state = static_cast<empty_accounts_state *>(data);
    if (!state)
        return;

    auto *model = Helper::instance()->userModel();
    state->user_model_present = model ? 1 : 0;
    state->user_count = model ? model->rowCount() : -1;
    state->current_user_name_empty = model && model->currentUserName().isEmpty() ? 1 : 0;
}
