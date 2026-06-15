// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QRectF>

namespace GeometryUtils {

enum class EdgeConstraint {
    Strict,
    Soft,
    None
};

bool isSurfaceSafe(const QRectF &geo, const QRectF &safeArea, qreal margin = 20.0);

void applyTitlebarConstraint(QRectF &geo, const QRectF &titlebarGeo, const QRectF &validArea, const QRectF &screenArea, 
                             EdgeConstraint topBottom = EdgeConstraint::Strict, 
                             EdgeConstraint leftRight = EdgeConstraint::Soft);

} // namespace GeometryUtils
