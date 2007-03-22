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

#ifndef _HAVE_CLUTTER_PRIVATE_H
#define _HAVE_CLUTTER_PRIVATE_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <glib.h>

#include <pango/pangoft2.h>

#include "clutter-event.h"
#include "clutter-backend.h"
#include "clutter-stage.h"

G_BEGIN_DECLS

typedef struct _ClutterMainContext ClutterMainContext;

struct _ClutterMainContext
{
  /* holds a pointer to the stage */
  ClutterBackend *backend;

  PangoFT2FontMap *font_map;
  
  GMutex *gl_lock;
  guint update_idle;
  
  guint main_loop_level;
  GSList *main_loops;
  
  guint is_initialized : 1;
};

#define CLUTTER_CONTEXT()	(clutter_context_get_default ())
ClutterMainContext *clutter_context_get_default (void);

const gchar *clutter_vblank_method (void);

typedef enum {
  CLUTTER_ACTOR_UNUSED_FLAG = 0,

  CLUTTER_ACTOR_IN_DESTRUCTION = 1 << 0,
  CLUTTER_ACTOR_IS_TOPLEVEL    = 1 << 1,
  CLUTTER_ACTOR_IN_REPARENT    = 1 << 2
} ClutterPrivateFlags;

#define CLUTTER_PRIVATE_FLAGS(a)	 (CLUTTER_ACTOR ((a))->private_flags)
#define CLUTTER_SET_PRIVATE_FLAGS(a,f)	 G_STMT_START{ (CLUTTER_PRIVATE_FLAGS (a) |= (f)); }G_STMT_END
#define CLUTTER_UNSET_PRIVATE_FLAGS(a,f) G_STMT_START{ (CLUTTER_PRIVATE_FLAGS (a) &= ~(f)); }G_STMT_END

#define CLUTTER_PARAM_READABLE  \
        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB
#define CLUTTER_PARAM_WRITABLE  \
        G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB
#define CLUTTER_PARAM_READWRITE \
        G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |G_PARAM_STATIC_BLURB

GType _clutter_backend_impl_get_type (void);

/* backend-specific private functions */
void          _clutter_events_init               (ClutterBackend *backend);
void          _clutter_events_uninit             (ClutterBackend *backend);
void          _clutter_events_queue              (ClutterBackend *backend);
void          _clutter_event_queue_push          (ClutterBackend *backend,
                                                  ClutterEvent   *event);
ClutterEvent *_clutter_event_queue_pop           (ClutterBackend *backend);
ClutterEvent *_clutter_event_queue_peek          (ClutterBackend *backend);
gboolean      _clutter_event_queue_check_pending (ClutterBackend *backend);

typedef void (* ClutterEventFunc) (ClutterEvent *event,
                                   gpointer      data);

/* the event dispatcher function */
extern ClutterEventFunc _clutter_event_func;
extern gpointer _clutter_event_data;
extern GDestroyNotify _clutter_event_destroy;

void          _clutter_set_events_handler     (ClutterEventFunc   func,
                                               gpointer           data,
                                               GDestroyNotify     destroy);

void          _clutter_event_button_generate  (ClutterBackend    *backend,
                                               ClutterEvent      *event);
void          _clutter_synthetise_click       (ClutterBackend    *backend,
                                               ClutterEvent      *event,
                                               gint               n_clicks);
void          _clutter_synthetise_stage_state (ClutterBackend    *backend,
                                               ClutterEvent      *event,
                                               ClutterStageState  set_flags,
                                               ClutterStageState  unset_flags);

G_END_DECLS

#endif /* _HAVE_CLUTTER_PRIVATE_H */
