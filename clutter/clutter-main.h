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

#ifndef _HAVE_CLUTTER_MAIN_H
#define _HAVE_CLUTTER_MAIN_H

#include <clutter/clutter-actor.h>
#include <clutter/clutter-stage.h>

G_BEGIN_DECLS

#define CLUTTER_INIT_ERROR      (clutter_init_error_quark ())

/**
 * ClutterInitError:
 * @CLUTTER_INIT_SUCCESS: Initialisation successful
 * @CLUTTER_INIT_ERROR_UNKNOWN: Unknown error
 * @CLUTTER_INIT_ERROR_THREADS: Thread initialisation failed
 * @CLUTTER_INIT_ERROR_BACKEND: Backend initialisation failed
 * @CLUTTER_INIT_ERROR_INTERNAL: Internal error
 *
 * Error conditions returned by clutter_init() and clutter_init_with_args().
 *
 * Since: 0.2
 */
typedef enum {
  CLUTTER_INIT_SUCCESS        =  1,
  CLUTTER_INIT_ERROR_UNKNOWN  =  0,
  CLUTTER_INIT_ERROR_THREADS  = -1,
  CLUTTER_INIT_ERROR_BACKEND  = -2,
  CLUTTER_INIT_ERROR_INTERNAL = -3
} ClutterInitError;

GQuark clutter_init_error_quark (void);

ClutterInitError clutter_init             (int          *argc,
                                           char       ***argv);
ClutterInitError clutter_init_with_args   (int          *argc,
                                           char       ***argv,
                                           char         *parameter_string,
                                           GOptionEntry *entries,
                                           char         *translation_domain,
                                           GError      **error);

GOptionGroup *   clutter_get_option_group (void);

void             clutter_main             (void);
void             clutter_main_quit        (void);
gint             clutter_main_level       (void);

void             clutter_redraw           (void);

gboolean         clutter_get_debug_enabled (void);
gboolean         clutter_get_show_fps      (void);

void             clutter_base_init        (void);

gulong           clutter_get_timestamp    (void);

G_END_DECLS

#endif
