// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "geometryutils.h"

#include <algorithm>

namespace GeometryUtils {

bool isSurfaceSafe(const QRectF &geo, const QRectF &safeArea, qreal margin)
{
    qreal marginX = std::min(margin, geo.width());
    qreal marginY = std::min(margin, geo.height());
    
    QRectF intersect = geo.intersected(safeArea);
    return intersect.width() >= marginX && intersect.height() >= marginY;
}

void applyTitlebarConstraint(QRectF &geo, const QRectF &titlebarGeo, const QRectF &validArea, const QRectF &screenArea, 
                             EdgeConstraint topBottom, EdgeConstraint leftRight)
{
    if (topBottom == EdgeConstraint::Strict) {
        if (titlebarGeo.top() < validArea.top()) {
            geo.moveTop(geo.top() + validArea.top() - titlebarGeo.top());
        } else if (titlebarGeo.bottom() > validArea.bottom()) {
            geo.moveBottom(geo.bottom() - (titlebarGeo.bottom() - validArea.bottom()));
        }
    }

    if (leftRight == EdgeConstraint::Soft) {
        if (titlebarGeo.left() < validArea.left() && titlebarGeo.left() >= screenArea.left()) {
            geo.moveLeft(geo.left() + validArea.left() - titlebarGeo.left());
        } else if (titlebarGeo.right() > validArea.right() && titlebarGeo.right() <= screenArea.right()) {
            geo.moveRight(geo.right() - (titlebarGeo.right() - validArea.right()));
        }
    } else if (leftRight == EdgeConstraint::Strict) {
        if (titlebarGeo.left() < validArea.left()) {
            geo.moveLeft(geo.left() + validArea.left() - titlebarGeo.left());
        } else if (titlebarGeo.right() > validArea.right()) {
            geo.moveRight(geo.right() - (titlebarGeo.right() - validArea.right()));
        }
    }
}

} // namespace GeometryUtils
