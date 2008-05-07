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

#ifndef __CLUTTER_BACKEND_SDL_H__
#define __CLUTTER_BACKEND_SDL_H__

#include <SDL.h>
#include <glib-object.h>
#include <clutter/clutter-backend.h>
#include "clutter-stage-sdl.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_SDL                (clutter_backend_sdl_get_type ())
#define CLUTTER_BACKEND_SDL(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND_SDL, ClutterBackendSDL))
#define CLUTTER_IS_BACKEND_SDL(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND_SDL))
#define CLUTTER_BACKEND_SDL_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND_SDL, ClutterBackendSDLClass))
#define CLUTTER_IS_BACKEND_SDL_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND_SDL))
#define CLUTTER_BACKEND_SDL_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND_SDL, ClutterBackendSDLClass))

typedef struct _ClutterBackendSDL       ClutterBackendSDL;
typedef struct _ClutterBackendSDLClass  ClutterBackendSDLClass;

struct _ClutterBackendSDL
{
  ClutterBackend parent_instance;

  /* main stage singleton */
  ClutterStageSDL *stage;

  /* event source */
  GSource *event_source;

  /* our own timer for events */
  GTimer *timer;

  /*< private >*/
};

struct _ClutterBackendSDLClass
{
  ClutterBackendClass parent_class;
};

GType clutter_backend_sdl_get_type (void) G_GNUC_CONST;

void _clutter_events_init (ClutterBackend *backend);
void _clutter_events_uninit (ClutterBackend *backend);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_SDL_H__ */
