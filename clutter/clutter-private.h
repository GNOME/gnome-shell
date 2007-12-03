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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <math.h>

#include <glib.h>

#include <pango/pangoft2.h>

#include "clutter-event.h"
#include "clutter-backend.h"
#include "clutter-stage.h"
#include "clutter-feature.h"

G_BEGIN_DECLS

typedef enum {
  CLUTTER_ACTOR_UNUSED_FLAG = 0,

  CLUTTER_ACTOR_IN_DESTRUCTION = 1 << 0,
  CLUTTER_ACTOR_IS_TOPLEVEL    = 1 << 1,
  CLUTTER_ACTOR_IN_REPARENT    = 1 << 2,
  CLUTTER_ACTOR_SYNC_MATRICES  = 1 << 3 /* Used by stage to indicate GL
					 * viewport / perspective etc
					 * needs (re)setting. 
					*/
} ClutterPrivateFlags;

typedef enum {
  CLUTTER_PICK_NONE = 0,
  CLUTTER_PICK_REACTIVE,
  CLUTTER_PICK_ALL
} ClutterPickMode;

typedef struct _ClutterMainContext ClutterMainContext;

struct _ClutterMainContext
{
  ClutterBackend  *backend;            /* holds a pointer to the windowing 
                                          system backend */
  GQueue          *events_queue;       /* the main event queue */
  PangoFT2FontMap *font_map;
  guint            update_idle;	       /* repaint idler id */
  
  guint            is_initialized : 1;  
  GTimer          *timer;	       /* Used for debugging scheduler */

  ClutterPickMode  pick_mode;          /* Indicates pick render mode   */
  guint            motion_events_per_actor : 1;/* set for enter/leave events */
  guint            motion_frequency;   /* Motion events per second */
  gint             num_reactives;      /* Num of reactive actors */

  GHashTable      *actor_hash;	       /* Hash of all actors mapped to id */

  guint            frame_rate;         /* Default FPS */

  ClutterActor    *pointer_grab_actor; /* The actor having the pointer grab
                                          (or NULL if there is no pointer grab) 
                                        */
  ClutterActor    *keyboard_grab_actor; /* The actor having the pointer grab
                                          (or NULL if there is no pointer grab) 
                                        */
  GSList          *shaders;            /* stack of overridden shaders */
};

#define CLUTTER_CONTEXT()	(clutter_context_get_default ())
ClutterMainContext *clutter_context_get_default (void);

#define CLUTTER_PRIVATE_FLAGS(a)	 (((ClutterActor *) (a))->private_flags)
#define CLUTTER_SET_PRIVATE_FLAGS(a,f)	 (CLUTTER_PRIVATE_FLAGS (a) |= (f))
#define CLUTTER_UNSET_PRIVATE_FLAGS(a,f) (CLUTTER_PRIVATE_FLAGS (a) &= ~(f))

#define CLUTTER_PARAM_READABLE  \
        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB
#define CLUTTER_PARAM_WRITABLE  \
        G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB
#define CLUTTER_PARAM_READWRITE \
        G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |G_PARAM_STATIC_BLURB

/* vfuncs implemnted by backend */

GType _clutter_backend_impl_get_type (void);

ClutterActor *_clutter_backend_get_stage     (ClutterBackend  *backend);

void          _clutter_backend_redraw        (ClutterBackend *backend);

void          _clutter_backend_add_options   (ClutterBackend  *backend,
                                              GOptionGroup    *group);
gboolean      _clutter_backend_pre_parse     (ClutterBackend  *backend,
                                              GError         **error);
gboolean      _clutter_backend_post_parse    (ClutterBackend  *backend,
                                              GError         **error);
gboolean      _clutter_backend_init_stage    (ClutterBackend  *backend,
                                              GError         **error);
void          _clutter_backend_init_events   (ClutterBackend  *backend);

ClutterFeatureFlags _clutter_backend_get_features (ClutterBackend *backend);

void          _clutter_feature_init (void);

ClutterActor *_clutter_do_pick (ClutterStage   *stage,
				 gint            x,
				 gint            y,
				 ClutterPickMode mode);

/* use this function as the accumulator if you have a signal with
 * a G_TYPE_BOOLEAN return value; this will stop the emission as
 * soon as one handler returns TRUE
 */
gboolean      _clutter_boolean_handled_accumulator (GSignalInvocationHint *ihint,
                                                    GValue                *return_accu,
                                                    const GValue          *handler_return,
                                                    gpointer               dummy);

/* Does this need to be private ? */
void clutter_do_event (ClutterEvent *event);

G_END_DECLS

#endif /* _HAVE_CLUTTER_PRIVATE_H */
