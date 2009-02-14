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
 * SECTION:clutter-x11
 * @short_description: X11 specific API
 *
 * The X11 backend for Clutter provides some specific API, allowing
 * integration with the Xlibs API for embedding and manipulating the
 * stage window, or for trapping X errors.
 *
 * The ClutterX11 API is available since Clutter 0.6
 */

#ifndef __CLUTTER_X11_H__
#define __CLUTTER_X11_H__

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11-enum-types.h>
#include <clutter/x11/clutter-x11-texture-pixmap.h>

G_BEGIN_DECLS

/**
 * ClutterX11FilterReturn:
 * @CLUTTER_X11_FILTER_CONTINUE: The event was not handled, continues the
 *   processing
 * @CLUTTER_X11_FILTER_TRANSLATE: Native event translated into a Clutter
 *   event, stops the processing
 * @CLUTTER_X11_FILTER_REMOVE: Remove the event, stops the processing
 *
 * Return values for the #ClutterX11FilterFunc function.
 *
 * Since: 0.6
 */
typedef enum {
  CLUTTER_X11_FILTER_CONTINUE,
  CLUTTER_X11_FILTER_TRANSLATE,
  CLUTTER_X11_FILTER_REMOVE
} ClutterX11FilterReturn;

typedef enum {
  CLUTTER_X11_XINPUT_KEY_PRESS_EVENT = 0,
  CLUTTER_X11_XINPUT_KEY_RELEASE_EVENT,
  CLUTTER_X11_XINPUT_BUTTON_PRESS_EVENT,
  CLUTTER_X11_XINPUT_BUTTON_RELEASE_EVENT,
  CLUTTER_X11_XINPUT_MOTION_NOTIFY_EVENT,
  CLUTTER_X11_XINPUT_LAST_EVENT
} ClutterX11XInputEventTypes;

typedef enum {
  CLUTTER_X11_XINPUT_POINTER_DEVICE,
  CLUTTER_X11_XINPUT_KEYBOARD_DEVICE,
  CLUTTER_X11_XINPUT_EXTENSION_DEVICE
} ClutterX11InputDeviceType;

typedef struct _ClutterX11XInputDevice ClutterX11XInputDevice;

/**
 * ClutterX11FilterFunc:
 * @xev: Native X11 event structure
 * @cev: Clutter event structure
 * @data: user data passed to the filter function
 *
 * Filter function for X11 native events.
 *
 * Return value: the result of the filtering
 *
 * Since: 0.6
 */
typedef ClutterX11FilterReturn (*ClutterX11FilterFunc) (XEvent        *xev,
                                                        ClutterEvent  *cev,
                                                        gpointer       data);

void     clutter_x11_trap_x_errors       (void);
gint     clutter_x11_untrap_x_errors     (void);

Display *clutter_x11_get_default_display (void);
int      clutter_x11_get_default_screen  (void);
Window   clutter_x11_get_root_window     (void);
void     clutter_x11_set_display         (Display * xdpy);

Window       clutter_x11_get_stage_window  (ClutterStage *stage);
XVisualInfo *clutter_x11_get_stage_visual  (ClutterStage *stage);
gboolean     clutter_x11_set_stage_foreign (ClutterStage *stage,
                                            Window        xwindow);

void         clutter_x11_add_filter    (ClutterX11FilterFunc func,
                                        gpointer             data);
void         clutter_x11_remove_filter (ClutterX11FilterFunc func,
                                        gpointer             data);

ClutterX11FilterReturn clutter_x11_handle_event (XEvent *xevent);

void     clutter_x11_disable_event_retrieval (void);
gboolean clutter_x11_has_event_retrieval (void);

ClutterStage *clutter_x11_get_stage_from_window (Window win);

GSList* clutter_x11_get_input_devices (void);

ClutterX11InputDeviceType clutter_x11_get_input_device_type (ClutterX11XInputDevice *device);

void     clutter_x11_enable_xinput (void);
gboolean clutter_x11_has_xinput (void);

gboolean clutter_x11_has_composite_extension (void);

G_END_DECLS

#endif /* __CLUTTER_X11_H__ */
