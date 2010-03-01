/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
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
 *
 *
 */

#ifndef __CLUTTER_STAGE_GLX_H__
#define __CLUTTER_STAGE_GLX_H__

#include <glib-object.h>
#include <clutter/clutter-stage.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <GL/glx.h>
#include <GL/gl.h>

#include "clutter-backend-glx.h"
#include "../x11/clutter-stage-x11.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_STAGE_GLX                  (clutter_stage_glx_get_type ())
#define CLUTTER_STAGE_GLX(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_GLX, ClutterStageGLX))
#define CLUTTER_IS_STAGE_GLX(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_GLX))
#define CLUTTER_STAGE_GLX_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_STAGE_GLX, ClutterStageGLXClass))
#define CLUTTER_IS_STAGE_GLX_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_STAGE_GLX))
#define CLUTTER_STAGE_GLX_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_STAGE_GLX, ClutterStageGLXClass))

typedef struct _ClutterStageGLX         ClutterStageGLX;
typedef struct _ClutterStageGLXClass    ClutterStageGLXClass;

struct _ClutterStageGLX
{
  ClutterStageX11 parent_instance;

  int pending_swaps;

  GLXPixmap glxpixmap;
  GLXWindow glxwin;
};

struct _ClutterStageGLXClass
{
  ClutterStageX11Class parent_class;
};

GType clutter_stage_glx_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_STAGE_H__ */
