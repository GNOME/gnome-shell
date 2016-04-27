/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010,2011  Intel Corporation.
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

#ifndef __CLUTTER_STAGE_EGL_NATIVE_H__
#define __CLUTTER_STAGE_EGL_NATIVE_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <clutter/clutter-stage.h>

#include "cogl/clutter-stage-cogl.h"

#define CLUTTER_TYPE_STAGE_EGL_NATIVE                  (_clutter_stage_eglnative_get_type ())
#define CLUTTER_STAGE_EGL_NATIVE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_EGL_NATIVE, ClutterStageEglNative))
#define CLUTTER_IS_STAGE_EGL_NATIVE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_EGL_NATIVE))
#define CLUTTER_STAGE_EGL_NATIVE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_STAGE_EGL_NATIVE, ClutterStageEglNativeClass))
#define CLUTTER_IS_STAGE_EGL_NATIVE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_STAGE_EGL_NATIVE))
#define CLUTTER_STAGE_EGL_NATIVE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_STAGE_EGL_NATIVE, ClutterStageEglNativeClass))

typedef struct _ClutterStageEglNative         ClutterStageEglNative;
typedef struct _ClutterStageEglNativeClass    ClutterStageEglNativeClass;

struct _ClutterStageEglNative
{
  ClutterStageCogl parent_instance;
};

struct _ClutterStageEglNativeClass
{
  ClutterStageCoglClass parent_class;
};

GType _clutter_stage_eglnative_get_type (void) G_GNUC_CONST;

#endif /* __CLUTTER_STAGE_EGL_NATIVE_H__ */
