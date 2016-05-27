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

#include "config.h"

#include "backends/meta-renderer-view.h"

#include "backends/meta-renderer.h"
#include "clutter/clutter-mutter.h"

struct _MetaRendererView
{
  ClutterStageView parent;

  MetaMonitorInfo *monitor_info;
};

G_DEFINE_TYPE (MetaRendererView, meta_renderer_view,
               CLUTTER_TYPE_STAGE_VIEW_COGL)

static void
meta_renderer_view_init (MetaRendererView *view)
{
}

static void
meta_renderer_view_class_init (MetaRendererViewClass *klass)
{
}
