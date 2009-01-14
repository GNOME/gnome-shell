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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __CLUTTER_STAGE_X11_H__
#define __CLUTTER_STAGE_X11_H__

#include <clutter/clutter-group.h>
#include <clutter/clutter-stage.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "clutter-backend-x11.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_STAGE_X11                  (clutter_stage_x11_get_type ())
#define CLUTTER_STAGE_X11(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_X11, ClutterStageX11))
#define CLUTTER_IS_STAGE_X11(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_X11))
#define CLUTTER_STAGE_X11_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_STAGE_X11, ClutterStageX11Class))
#define CLUTTER_IS_STAGE_X11_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_STAGE_X11))
#define CLUTTER_STAGE_X11_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_STAGE_X11, ClutterStageX11Class))

typedef struct _ClutterStageX11         ClutterStageX11;
typedef struct _ClutterStageX11Class    ClutterStageX11Class;

struct _ClutterStageX11
{
  ClutterGroup parent_instance;

  guint        is_foreign_xwin    : 1;
  guint        fullscreen_on_map  : 1;
  guint        is_cursor_visible  : 1;

  Display     *xdpy;
  Window       xwin_root;
  int          xscreen;
  XVisualInfo *xvisinfo;
  Window       xwin;  
  gint         xwin_width;
  gint         xwin_height; /* FIXME target_width / height */
  Pixmap       xpixmap;
  gchar       *title;

  ClutterBackendX11 *backend;
  ClutterStageState  state;

#ifdef USE_XINPUT
  int          event_types[CLUTTER_X11_XINPUT_LAST_EVENT];
  GList       *devices;
#endif

  ClutterStage *wrapper;
};

struct _ClutterStageX11Class
{
  ClutterGroupClass parent_class;
};

GType clutter_stage_x11_get_type (void) G_GNUC_CONST;

/* Private to subclasses */
void clutter_stage_x11_fix_window_size  (ClutterStageX11 *stage_x11);
void clutter_stage_x11_set_wm_protocols (ClutterStageX11 *stage_x11);
void clutter_stage_x11_map              (ClutterStageX11 *stage_x11);
void clutter_stage_x11_unmap            (ClutterStageX11 *stage_x11);

GList *clutter_stage_x11_get_input_devices (ClutterStageX11 *stage_x11);

G_END_DECLS

#endif /* __CLUTTER_STAGE_H__ */
