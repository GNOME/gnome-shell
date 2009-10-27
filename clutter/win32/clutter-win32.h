/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

/**
 * SECTION:clutter-win32
 * @short_description: Win32 specific API
 *
 * The Win32 backend for Clutter provides some specific API, allowing
 * integration with the Win32 API for embedding and manipulating the
 * stage window.
 *
 * The ClutterWin32 API is available since Clutter 0.8
 */

#ifndef __CLUTTER_WIN32_H__
#define __CLUTTER_WIN32_H__

#include <glib.h>
#include <clutter/clutter.h>
#include <windows.h>

G_BEGIN_DECLS

HWND          clutter_win32_get_stage_window        (ClutterStage *stage);
ClutterStage *clutter_win32_get_stage_from_window   (HWND          hwnd);

gboolean      clutter_win32_set_stage_foreign       (ClutterStage *stage,
                                                     HWND          hwnd);

void          clutter_win32_disable_event_retrieval (void);

G_END_DECLS

#endif /* __CLUTTER_WIN32_H__ */
