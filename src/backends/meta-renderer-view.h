/*
 * Copyright (C) 2016 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_RENDERER_VIEW_H
#define META_RENDERER_VIEW_H

#include "backends/meta-monitor-manager-private.h"
#include "clutter/clutter-mutter.h"

#define META_TYPE_RENDERER_VIEW (meta_renderer_view_get_type ())
G_DECLARE_FINAL_TYPE (MetaRendererView, meta_renderer_view,
                      META, RENDERER_VIEW,
                      ClutterStageViewCogl)

MetaMonitorInfo *meta_renderer_view_get_monitor_info (MetaRendererView *view);

#endif /* META_RENDERER_VIEW_H */
