/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2014 Canonical Ltd.
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

 * Authors:
 *  Marco Trevisan <marco.trevisan@canonical.com>
 */

#ifndef __CLUTTER_STAGE_MIR_H__
#define __CLUTTER_STAGE_MIR_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <clutter/clutter-stage.h>
#include "cogl/clutter-stage-cogl.h"

#define CLUTTER_TYPE_STAGE_MIR                  (_clutter_stage_mir_get_type ())
#define CLUTTER_STAGE_MIR(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_MIR, ClutterStageMir))
#define CLUTTER_IS_STAGE_MIR(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_MIR))
#define CLUTTER_STAGE_MIR_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_STAGE_MIR, ClutterStageMirClass))
#define CLUTTER_IS_STAGE_MIR_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_STAGE_MIR))
#define CLUTTER_STAGE_MIR_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_STAGE_MIR, ClutterStageMirClass))

typedef struct _ClutterStageMir         ClutterStageMir;
typedef struct _ClutterStageMirClass    ClutterStageMirClass;

struct _ClutterStageMir
{
  ClutterStageCogl parent_instance;

  MirSurfaceState surface_state;
  MirMotionButton button_state;

  gboolean foreign_mir_surface;
  gboolean cursor_visible;
};

struct _ClutterStageMirClass
{
  ClutterStageCoglClass parent_class;
};

GType _clutter_stage_mir_get_type (void) G_GNUC_CONST;

#endif /* __CLUTTER_STAGE_MIR_H__ */
