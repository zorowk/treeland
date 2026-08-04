// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server-core.h>
#include <wglobal.h>
#include <wserver.h>

#include <QObject>

#include <memory>

class WineWindowManagerPrivate;
class WineWindowControl;

class WineWindowManager
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit WineWindowManager(QObject *parent = nullptr);
    ~WineWindowManager() override;
    static constexpr int InterfaceVersion = 1;

    [[nodiscard]] qsizetype activeControlResourceCount() const;
    [[nodiscard]] quint64 destroyedControlResourceCount() const;

Q_SIGNALS:
    void controlResourceCountChanged(qsizetype active, quint64 destroyed);

protected:
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    friend class WineWindowControl;
    void handleControlResourceCreated();
    void handleControlResourceDestroyed();

    std::unique_ptr<WineWindowManagerPrivate> d;
    qsizetype m_activeControlResourceCount = 0;
    quint64 m_destroyedControlResourceCount = 0;
};
