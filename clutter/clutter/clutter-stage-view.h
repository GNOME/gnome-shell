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

#ifndef __CLUTTER_STAGE_VIEW_H__
#define __CLUTTER_STAGE_VIEW_H__

#include <cairo.h>
#include <glib-object.h>
#include <cogl/cogl.h>

#include "clutter-macros.h"

#define CLUTTER_TYPE_STAGE_VIEW (clutter_stage_view_get_type ())
CLUTTER_AVAILABLE_IN_MUTTER
G_DECLARE_DERIVABLE_TYPE (ClutterStageView, clutter_stage_view,
                          CLUTTER, STAGE_VIEW,
                          GObject)

struct _ClutterStageViewClass
{
  GObjectClass parent_class;

  void (* setup_offscreen_blit_pipeline) (ClutterStageView *view,
                                          CoglPipeline     *pipeline);

  void (* get_offscreen_transformation_matrix) (ClutterStageView *view,
                                                CoglMatrix       *matrix);
};

CLUTTER_AVAILABLE_IN_MUTTER
void clutter_stage_view_get_layout (ClutterStageView      *view,
                                    cairo_rectangle_int_t *rect);

CLUTTER_AVAILABLE_IN_MUTTER
CoglFramebuffer *clutter_stage_view_get_framebuffer (ClutterStageView *view);
CLUTTER_AVAILABLE_IN_MUTTER
CoglFramebuffer *clutter_stage_view_get_onscreen (ClutterStageView *view);
CLUTTER_AVAILABLE_IN_MUTTER
void             clutter_stage_view_invalidate_offscreen_blit_pipeline (ClutterStageView *view);

CLUTTER_AVAILABLE_IN_MUTTER
void             clutter_stage_view_transform_to_onscreen (ClutterStageView *view,
                                                           gfloat           *x,
                                                           gfloat           *y);

void clutter_stage_view_blit_offscreen (ClutterStageView            *view,
					const cairo_rectangle_int_t *clip);

CLUTTER_AVAILABLE_IN_MUTTER
float clutter_stage_view_get_scale (ClutterStageView *view);

gboolean clutter_stage_view_is_dirty_viewport (ClutterStageView *view);

void clutter_stage_view_set_dirty_viewport (ClutterStageView *view,
                                            gboolean          dirty);

gboolean clutter_stage_view_is_dirty_projection (ClutterStageView *view);

void clutter_stage_view_set_dirty_projection (ClutterStageView *view,
                                              gboolean          dirty);

CLUTTER_AVAILABLE_IN_MUTTER
void clutter_stage_view_get_offscreen_transformation_matrix (ClutterStageView *view,
                                                             CoglMatrix       *matrix);

#endif /* __CLUTTER_STAGE_VIEW_H__ */
