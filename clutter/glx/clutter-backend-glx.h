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

#ifndef __CLUTTER_BACKEND_GLX_H__
#define __CLUTTER_BACKEND_GLX_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>
#include <clutter/clutter-backend.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <GL/glx.h>
#include <GL/gl.h>

#include "../x11/clutter-backend-x11.h"
#include "clutter-glx.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_GLX                (_clutter_backend_glx_get_type ())
#define CLUTTER_BACKEND_GLX(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND_GLX, ClutterBackendGLX))
#define CLUTTER_IS_BACKEND_GLX(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND_GLX))
#define CLUTTER_BACKEND_GLX_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND_GLX, ClutterBackendGLXClass))
#define CLUTTER_IS_BACKEND_GLX_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND_GLX))
#define CLUTTER_BACKEND_GLX_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND_GLX, ClutterBackendGLXClass))

typedef struct _ClutterBackendGLX       ClutterBackendGLX;
typedef struct _ClutterBackendGLXClass  ClutterBackendGLXClass;

typedef enum ClutterGLXVBlankType {
  CLUTTER_VBLANK_NONE = 0,
  CLUTTER_VBLANK_AUTOMATIC_THROTTLE,
  CLUTTER_VBLANK_VBLANK_COUNTER,
  CLUTTER_VBLANK_MANUAL_WAIT
} ClutterGLXVBlankType;

struct _ClutterBackendGLX
{
  ClutterBackendX11 parent_instance;

  int                    error_base;
  int                    event_base;

  CoglContext           *cogl_context;

  /* Vblank stuff */
  ClutterGLXVBlankType   vblank_type;
  unsigned int           last_video_sync_count;

  gboolean               can_blit_sub_buffer;

  /* props */
  Atom atom_WM_STATE;
  Atom atom_WM_STATE_FULLSCREEN;
};

struct _ClutterBackendGLXClass
{
  ClutterBackendX11Class parent_class;
};

GType _clutter_backend_glx_get_type (void) G_GNUC_CONST;

G_CONST_RETURN gchar*
_clutter_backend_glx_get_vblank (void);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_GLX_H__ */
