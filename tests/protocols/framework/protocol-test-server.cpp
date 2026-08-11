// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocol-test-server.h"

#include <wbackend.h>
#include <woutput.h>
#include <wserver.h>

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwshm.h>

#include <drm/drm_fourcc.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_output.h>

#include <iterator>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

bool protocol_test_create_headless_output(WServer *server, int width, int height)
{
    auto *backend = server->findInterface<WBackend>();
    if (!backend)
        return false;

    if (!backend->handle()->start())
        return false;

    auto *multi = qw_multi_backend::from(backend->handle()->handle());
    if (!multi)
        return false;

    wlr_backend *headlessHandle = nullptr;
    multi->for_each_backend([](wlr_backend *backend, void *data) {
        if (wlr_backend_is_headless(backend))
            *static_cast<wlr_backend **>(data) = backend;
    }, &headlessHandle);
    if (!headlessHandle)
        return false;

    auto *headless = qw_headless_backend::from(headlessHandle);
    auto *output = headless->add_output(width, height);
    if (!output)
        return false;

    wlr_output_create_global(output, server->handle()->handle());
    return WOutput::fromHandle(qw_output::from(output)) != nullptr;
}

bool protocol_test_enable_shm(WServer *server)
{
    static constexpr uint32_t formats[] = {
        DRM_FORMAT_ARGB8888,
        DRM_FORMAT_XRGB8888,
    };
    return qw_shm::create(server->handle()->handle(), 1, formats, std::size(formats)) != nullptr;
}
