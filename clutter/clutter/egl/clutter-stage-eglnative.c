/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 *  Adel Gadllah
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include "clutter-stage-window.h"
#include "clutter-stage-private.h"
#include "clutter-stage-eglnative.h"
#include <cogl/cogl.h>

static ClutterStageWindowIface *clutter_stage_window_parent_iface = NULL;

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

#define clutter_stage_eglnative_get_type _clutter_stage_eglnative_get_type

G_DEFINE_TYPE_WITH_CODE (ClutterStageEglNative,
                         clutter_stage_eglnative,
                         CLUTTER_TYPE_STAGE_COGL,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static gboolean
clutter_stage_eglnative_can_clip_redraws (ClutterStageWindow *stage_window)
{
  return TRUE;
}

static void
clutter_stage_eglnative_class_init (ClutterStageEglNativeClass *klass)
{
}

static void
clutter_stage_eglnative_init (ClutterStageEglNative *stage_eglnative)
{
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  clutter_stage_window_parent_iface = g_type_interface_peek_parent (iface);

  iface->can_clip_redraws = clutter_stage_eglnative_can_clip_redraws;
}
