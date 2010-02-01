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

#ifndef __CLUTTER_STAGE_WIN32_H__
#define __CLUTTER_STAGE_WIN32_H__

#include <clutter/clutter-group.h>
#include <clutter/clutter-stage.h>
#include <windows.h>

#include "clutter-backend-win32.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_STAGE_WIN32                  (clutter_stage_win32_get_type ())
#define CLUTTER_STAGE_WIN32(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_WIN32, ClutterStageWin32))
#define CLUTTER_IS_STAGE_WIN32(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_WIN32))
#define CLUTTER_STAGE_WIN32_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_STAGE_WIN32, ClutterStageWin32Class))
#define CLUTTER_IS_STAGE_WIN32_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_STAGE_WIN32))
#define CLUTTER_STAGE_WIN32_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_STAGE_WIN32, ClutterStageWin32Class))

typedef struct _ClutterStageWin32         ClutterStageWin32;
typedef struct _ClutterStageWin32Class    ClutterStageWin32Class;

struct _ClutterStageWin32
{
  ClutterGroup parent_instance;

  HWND         hwnd;
  HDC          client_dc;
  gint         win_width;
  gint         win_height;
  gint         scroll_pos;
  RECT         fullscreen_rect;
  gboolean     is_foreign_win;
  gboolean     tracking_mouse;
  wchar_t     *wtitle;
  gboolean        is_cursor_visible;

  ClutterBackendWin32 *backend;
  ClutterStageState   state;

  ClutterStage *wrapper;
};

struct _ClutterStageWin32Class
{
  ClutterGroupClass parent_class;
};

GType clutter_stage_win32_get_type (void) G_GNUC_CONST;

void clutter_stage_win32_map   (ClutterStageWin32 *stage_win32);
void clutter_stage_win32_unmap (ClutterStageWin32 *stage_win32);

/* Defined in clutter-event-win32.c */
LRESULT CALLBACK _clutter_stage_win32_window_proc (HWND hwnd,
						   UINT msg,
						   WPARAM wparam,
						   LPARAM lparam);

void _clutter_stage_win32_get_min_max_info (ClutterStageWin32 *stage_win32,
					    MINMAXINFO *min_max_info);

void _clutter_stage_win32_update_cursor (ClutterStageWin32 *stage_win32);

G_END_DECLS

#endif /* __CLUTTER_STAGE_H__ */
