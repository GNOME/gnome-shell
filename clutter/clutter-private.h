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
#include "clutter-feature.h"

G_BEGIN_DECLS

typedef struct _ClutterMainContext ClutterMainContext;

struct _ClutterMainContext
{
  /* holds a pointer to the stage */
  ClutterBackend  *backend;
  GQueue          *events_queue;
  PangoFT2FontMap *font_map;
  GMutex          *gl_lock;
  guint            update_idle;
  guint            main_loop_level;
  GSList          *main_loops;
  guint            is_initialized : 1;
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

/* signal accumulators */
gboolean _clutter_boolean_accumulator (GSignalInvocationHint *ihint,
                                       GValue                *return_accu,
                                       const GValue          *handler_return,
                                       gpointer               dummy);

/* vfuncs implemnted by backend */

GType _clutter_backend_impl_get_type (void);

ClutterActor *_clutter_backend_get_stage     (ClutterBackend  *backend);

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

/* backend helpers */
void          _clutter_event_button_generate  (ClutterBackend    *backend,
                                               ClutterEvent      *event);

void          _clutter_feature_init (void);

/* FIXME: move elsewhere via ClutterAudience */
void _clutter_stage_sync_viewport (ClutterStage *stage);

/* Does this need to be private ? */
void clutter_do_event (ClutterEvent *event);

G_END_DECLS

#endif /* _HAVE_CLUTTER_PRIVATE_H */
