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
 * SECTION:clutter-stage
 * @short_description: Top level visual element to which actors are placed.
 *
 * #ClutterStage is a top level 'window' on which child actors are placed
 * and manipulated.
 *
 * Clutter creates a default stage upon initialization, which can be retrieved
 * using clutter_stage_get_default(). Clutter always provides the default
 * stage, unless the backend is unable to create one. The stage returned
 * by clutter_stage_get_default() is guaranteed to always be the same.
 *
 * Backends might provide support for multiple stages. The support for this
 * feature can be checked at run-time using the clutter_feature_available()
 * function and the %CLUTTER_FEATURE_STAGE_MULTIPLE flag. If the backend used
 * supports multiple stages, new #ClutterStage instances can be created
 * using clutter_stage_new(). These stages must be managed by the developer
 * using clutter_actor_destroy(), which will take care of destroying all the
 * actors contained inside them.
 *
 * #ClutterStage is a proxy actor, wrapping the backend-specific
 * implementation of the windowing system. It is possible to subclass
 * #ClutterStage, as long as every overridden virtual function chains up to
 * the parent class corresponding function.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend.h"
#include "clutter-stage.h"
#include "clutter-main.h"
#include "clutter-color.h"
#include "clutter-util.h"
#include "clutter-marshal.h"
#include "clutter-master-clock.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-stage-manager.h"
#include "clutter-stage-window.h"
#include "clutter-version.h" 	/* For flavour */
#include "clutter-id-pool.h"
#include "clutter-container.h"
#include "clutter-profile.h"
#include "clutter-input-device.h"

#include "cogl/cogl.h"

G_DEFINE_TYPE (ClutterStage, clutter_stage, CLUTTER_TYPE_GROUP);

#define CLUTTER_STAGE_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_STAGE, ClutterStagePrivate))

struct _ClutterStagePrivate
{
  /* the stage implementation */
  ClutterStageWindow *impl;

  ClutterColor        color;
  ClutterPerspective  perspective;
  ClutterFog          fog;

  gchar              *title;
  ClutterActor       *key_focused_actor;

  GQueue             *event_queue;

  guint redraw_pending         : 1;
  guint is_fullscreen          : 1;
  guint is_cursor_visible      : 1;
  guint is_user_resizable      : 1;
  guint use_fog                : 1;
  guint throttle_motion_events : 1;
  guint use_alpha              : 1;
};

enum
{
  PROP_0,

  PROP_COLOR,
  PROP_FULLSCREEN_SET,
  PROP_OFFSCREEN,
  PROP_CURSOR_VISIBLE,
  PROP_PERSPECTIVE,
  PROP_TITLE,
  PROP_USER_RESIZE,
  PROP_USE_FOG,
  PROP_FOG,
  PROP_USE_ALPHA,
  PROP_KEY_FOCUS
};

enum
{
  FULLSCREEN,
  UNFULLSCREEN,
  ACTIVATE,
  DEACTIVATE,
  DELETE_EVENT,

  LAST_SIGNAL
};

static guint stage_signals[LAST_SIGNAL] = { 0, };

static const ClutterColor default_stage_color = { 255, 255, 255, 255 };

static void
clutter_stage_get_preferred_width (ClutterActor *self,
                                   gfloat        for_height,
                                   gfloat       *min_width_p,
                                   gfloat       *natural_width_p)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  ClutterGeometry geom = { 0, };

  if (priv->impl == NULL)
    return;

  _clutter_stage_window_get_geometry (priv->impl, &geom);

  if (min_width_p)
    *min_width_p = geom.width;

  if (natural_width_p)
    *natural_width_p = geom.width;
}

static void
clutter_stage_get_preferred_height (ClutterActor *self,
                                    gfloat        for_width,
                                    gfloat       *min_height_p,
                                    gfloat       *natural_height_p)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  ClutterGeometry geom = { 0, };

  if (priv->impl == NULL)
    return;

  _clutter_stage_window_get_geometry (priv->impl, &geom);

  if (min_height_p)
    *min_height_p = geom.height;

  if (natural_height_p)
    *natural_height_p = geom.height;
}
static void
clutter_stage_allocate (ClutterActor           *self,
                        const ClutterActorBox  *box,
                        ClutterAllocationFlags  flags)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  gboolean origin_changed;
  gint width, height;

  origin_changed = (flags & CLUTTER_ABSOLUTE_ORIGIN_CHANGED) ? TRUE : FALSE;

  if (priv->impl == NULL)
    return;

  width = clutter_actor_box_get_width (box);
  height = clutter_actor_box_get_height (box);

  /* if the stage is fixed size (for instance, it's using a frame-buffer)
   * then we simply ignore any allocation request and override the
   * allocation chain.
   */
  if ((!clutter_feature_available (CLUTTER_FEATURE_STAGE_STATIC)))
    {
      ClutterActorClass *klass;

      /* XXX: Until Cogl becomes fully responsible for backend windows Clutter
       * need to manually keep it informed of the current window size */
      _cogl_onscreen_clutter_backend_set_size (width, height);

      CLUTTER_NOTE (LAYOUT,
                    "Following allocation to %dx%d (origin %s)",
                    width, height,
                    origin_changed ? "changed" : "not changed");

      klass = CLUTTER_ACTOR_CLASS (clutter_stage_parent_class);
      klass->allocate (self, box, flags);

      _clutter_stage_window_resize (priv->impl, width, height);
    }
  else
    {
      ClutterActorBox override = { 0, };
      ClutterGeometry geom = { 0, };
      ClutterActorClass *klass;

      _clutter_stage_window_get_geometry (priv->impl, &geom);

      /* XXX: Until Cogl becomes fully responsible for backend windows Clutter
       * need to manually keep it informed of the current window size */
      _cogl_onscreen_clutter_backend_set_size (geom.width, geom.height);

      override.x1 = 0;
      override.y1 = 0;
      override.x2 = geom.width;
      override.y2 = geom.height;

      CLUTTER_NOTE (LAYOUT,
                    "Overrigin original allocation of %dx%d "
                    "with %dx%d (origin %s)",
                    width, height,
                    (int) (override.x2),
                    (int) (override.y2),
                    origin_changed ? "changed" : "not changed");

      /* and store the overridden allocation */
      klass = CLUTTER_ACTOR_CLASS (clutter_stage_parent_class);
      klass->allocate (self, &override, flags);
    }
}

static void
clutter_stage_paint (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  CoglColor stage_color;
  guint8 real_alpha;
  CLUTTER_STATIC_TIMER (stage_clear_timer,
                        "Painting actors", /* parent */
                        "Stage clear",
                        "The time spent clearing the stage",
                        0 /* no application private data */);

  CLUTTER_NOTE (PAINT, "Initializing stage paint");

  /* composite the opacity to the stage color */
  real_alpha = clutter_actor_get_opacity (self)
             * priv->color.alpha
             / 255;

  /* we use the real alpha to clear the stage if :use-alpha is
   * set; the effect depends entirely on how the Clutter backend
   */
  cogl_color_set_from_4ub (&stage_color,
                           priv->color.red,
                           priv->color.green,
                           priv->color.blue,
                           priv->use_alpha ? real_alpha
                                           : 255);
  cogl_color_premultiply (&stage_color);

  CLUTTER_TIMER_START (_clutter_uprof_context, stage_clear_timer);
  cogl_clear (&stage_color,
	      COGL_BUFFER_BIT_COLOR |
	      COGL_BUFFER_BIT_DEPTH);
  CLUTTER_TIMER_STOP (_clutter_uprof_context, stage_clear_timer);

  if (priv->use_fog)
    {
      /* we only expose the linear progression of the fog in
       * the ClutterStage API, and that ignores the fog density.
       * thus, we pass 1.0 as the density parameter
       */
      cogl_set_fog (&stage_color,
                    COGL_FOG_MODE_LINEAR,
                    1.0,
                    priv->fog.z_near,
                    priv->fog.z_far);
    }
  else
    cogl_disable_fog ();

  /* this will take care of painting every child */
  CLUTTER_ACTOR_CLASS (clutter_stage_parent_class)->paint (self);
}

static void
clutter_stage_pick (ClutterActor       *self,
		    const ClutterColor *color)
{
  /* Note: we don't chain up to our parent as we don't want any geometry
   * emitted for the stage itself. The stage's pick id is effectively handled
   * by the call to cogl_clear done in clutter-main.c:_clutter_do_pick_async()
   */
  clutter_container_foreach (CLUTTER_CONTAINER (self),
                             CLUTTER_CALLBACK (clutter_actor_paint),
                             NULL);
}

static void
clutter_stage_realize (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  gboolean is_realized;

  /* Make sure the viewport and projection matrix are valid for the
   * first paint (which will likely occur before the ConfigureNotify
   * is received)
   */
  CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_SYNC_MATRICES);

  g_assert (priv->impl != NULL);
  is_realized = _clutter_stage_window_realize (priv->impl);

  /* ensure that the stage is using the context if the
   * realization sequence was successful
   */
  if (is_realized)
    {
      GError *error = NULL;
      ClutterBackend *backend = clutter_get_default_backend ();

      /* We want to select the context without calling
         clutter_backend_ensure_context so that it doesn't call any
         Cogl functions. Otherwise it would create the Cogl context
         before we get a chance to check whether the GL version is
         valid */
      _clutter_backend_ensure_context_internal (backend, CLUTTER_STAGE (self));

      /* Make sure Cogl can support the driver */
      if (!_cogl_check_driver_valid (&error))
        {
          g_warning ("The GL driver is not supported: %s",
                     error->message);
          g_clear_error (&error);
          CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_REALIZED);
        }
      else
        CLUTTER_ACTOR_SET_FLAGS (self, CLUTTER_ACTOR_REALIZED);
    }
  else
    CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_REALIZED);
}

static void
clutter_stage_unrealize (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  /* and then unrealize the implementation */
  g_assert (priv->impl != NULL);
  _clutter_stage_window_unrealize (priv->impl);

  CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_REALIZED);

  clutter_stage_ensure_current (CLUTTER_STAGE (self));
}

static void
clutter_stage_show (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  CLUTTER_ACTOR_CLASS (clutter_stage_parent_class)->show (self);

  /* Possibly do an allocation run so that the stage will have the
     right size before we map it */
  _clutter_stage_maybe_relayout (self);

  g_assert (priv->impl != NULL);
  _clutter_stage_window_show (priv->impl, TRUE);
}

static void
clutter_stage_hide (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  g_assert (priv->impl != NULL);
  _clutter_stage_window_hide (priv->impl);

  CLUTTER_ACTOR_CLASS (clutter_stage_parent_class)->hide (self);
}

static void
clutter_stage_emit_key_focus_event (ClutterStage *stage,
                                    gboolean      focus_in)
{
  ClutterStagePrivate *priv = stage->priv;

  if (priv->key_focused_actor == NULL)
    return;

  if (focus_in)
    g_signal_emit_by_name (priv->key_focused_actor, "key-focus-in");
  else
    g_signal_emit_by_name (priv->key_focused_actor, "key-focus-out");
}

static void
clutter_stage_real_activate (ClutterStage *stage)
{
  clutter_stage_emit_key_focus_event (stage, TRUE);
}

static void
clutter_stage_real_deactivate (ClutterStage *stage)
{
  clutter_stage_emit_key_focus_event (stage, FALSE);
}

static void
clutter_stage_real_fullscreen (ClutterStage *stage)
{
  ClutterStagePrivate *priv = stage->priv;
  ClutterGeometry geom;
  ClutterActorBox box;

  /* we need to force an allocation here because the size
   * of the stage might have been changed by the backend
   *
   * this is a really bad solution to the issues caused by
   * the fact that fullscreening the stage on the X11 backends
   * is really an asynchronous operation
   */
  _clutter_stage_window_get_geometry (priv->impl, &geom);

  box.x1 = 0;
  box.y1 = 0;
  box.x2 = geom.width;
  box.y2 = geom.height;

  clutter_actor_allocate (CLUTTER_ACTOR (stage),
                          &box,
                          CLUTTER_ALLOCATION_NONE);
}

void
_clutter_stage_queue_event (ClutterStage *stage,
			    ClutterEvent *event)
{
  ClutterStagePrivate *priv;
  gboolean first_event;
  ClutterInputDevice *device;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  first_event = priv->event_queue->length == 0;

  g_queue_push_tail (priv->event_queue, clutter_event_copy (event));

  if (first_event)
    {
      ClutterMasterClock *master_clock = _clutter_master_clock_get_default ();
      _clutter_master_clock_start_running (master_clock);
    }

  /* if needed, update the state of the input device of the event.
   * we do it here to avoid calling the same code from every backend
   * event processing function
   */
  device = clutter_event_get_device (event);
  if (device != NULL)
    {
      ClutterModifierType event_state = clutter_event_get_state (event);
      guint32 event_time = clutter_event_get_time (event);
      gfloat event_x, event_y;

      clutter_event_get_coords (event, &event_x, &event_y);

      _clutter_input_device_set_coords (device, event_x, event_y);
      _clutter_input_device_set_state (device, event_state);
      _clutter_input_device_set_time (device, event_time);
    }
}

gboolean
_clutter_stage_has_queued_events (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  priv = stage->priv;

  return priv->event_queue->length > 0;
}

void
_clutter_stage_process_queued_events (ClutterStage *stage)
{
  ClutterStagePrivate *priv;
  GList *events, *l;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (priv->event_queue->length == 0)
    return;

  /* In case the stage gets destroyed during event processing */
  g_object_ref (stage);

  /* Steal events before starting processing to avoid reentrancy
   * issues */
  events = priv->event_queue->head;
  priv->event_queue->head =  NULL;
  priv->event_queue->tail = NULL;
  priv->event_queue->length = 0;

  for (l = events; l != NULL; l = l->next)
    {
      ClutterEvent *event;
      ClutterEvent *next_event;

      event = l->data;
      next_event = l->next ? l->next->data : NULL;

      /* Skip consecutive motion events */
      if (priv->throttle_motion_events &&
          next_event &&
	  event->type == CLUTTER_MOTION &&
	  (next_event->type == CLUTTER_MOTION ||
	   next_event->type == CLUTTER_LEAVE))
	{
	  CLUTTER_NOTE (EVENT,
			"Omitting motion event at %.2f, %.2f",
			event->motion.x, event->motion.y);
	  goto next_event;
	}

      _clutter_process_event (event);

    next_event:
      clutter_event_free (event);
    }

  g_list_free (events);

  g_object_unref (stage);
}

/**
 * _clutter_stage_needs_update:
 * @stage: A #ClutterStage
 *
 * Determines if _clutter_stage_do_update() needs to be called.
 *
 * Return value: %TRUE if the stage need layout or painting
 */
gboolean
_clutter_stage_needs_update (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  priv = stage->priv;

  return priv->redraw_pending;
}

/**
 * _clutter_stage_do_update:
 * @stage: A #ClutterStage
 *
 * Handles per-frame layout and repaint for the stage.
 *
 * Return value: %TRUE if the stage was updated
 */
gboolean
_clutter_stage_do_update (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  priv = stage->priv;

  if (!priv->redraw_pending)
    return FALSE;

  /* clutter_do_redraw() will also call maybe_relayout(), but since a relayout
   * can queue a redraw, we want to do the relayout before we clear the
   * update_idle to avoid painting the stage twice. Calling maybe_relayout()
   * twice in a row is cheap because of caching of requested and allocated
   * size.
   */
  _clutter_stage_maybe_relayout (CLUTTER_ACTOR (stage));

  CLUTTER_NOTE (PAINT, "redrawing via idle for stage[%p]", stage);
  _clutter_do_redraw (stage);

  /* reset the guard, so that new redraws are possible */
  priv->redraw_pending = FALSE;

  if (CLUTTER_CONTEXT ()->redraw_count > 0)
    {
      CLUTTER_NOTE (SCHEDULER, "Queued %lu redraws during the last cycle",
                    CLUTTER_CONTEXT ()->redraw_count);

      CLUTTER_CONTEXT ()->redraw_count = 0;
    }

  return TRUE;
}

static void
clutter_stage_real_queue_redraw (ClutterActor *actor,
                                 ClutterActor *leaf)
{
  ClutterStage *stage = CLUTTER_STAGE (actor);
  ClutterStagePrivate *priv = stage->priv;

  CLUTTER_NOTE (PAINT, "Redraw request number %lu",
                CLUTTER_CONTEXT ()->redraw_count + 1);

  if (!priv->redraw_pending)
    {
      ClutterMasterClock *master_clock;

      priv->redraw_pending = TRUE;

      master_clock = _clutter_master_clock_get_default ();
      _clutter_master_clock_start_running (master_clock);
    }
  else
    CLUTTER_CONTEXT ()->redraw_count += 1;
}

static gboolean
clutter_stage_real_delete_event (ClutterStage *stage,
                                 ClutterEvent *event)
{
  if (clutter_stage_is_default (stage))
    clutter_main_quit ();
  else
    clutter_actor_destroy (CLUTTER_ACTOR (stage));

  return TRUE;
}

static void
clutter_stage_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  ClutterStage        *stage;
  ClutterStagePrivate *priv;
  ClutterActor        *actor;

  stage = CLUTTER_STAGE (object);
  actor = CLUTTER_ACTOR (stage);
  priv = stage->priv;

  switch (prop_id)
    {
    case PROP_COLOR:
      clutter_stage_set_color (stage, clutter_value_get_color (value));
      break;

    case PROP_OFFSCREEN:
      if (g_value_get_boolean (value))
        g_warning ("Offscreen stages are currently not supported\n");
      break;

    case PROP_CURSOR_VISIBLE:
      if (g_value_get_boolean (value))
        clutter_stage_show_cursor (stage);
      else
        clutter_stage_hide_cursor (stage);
      break;

    case PROP_PERSPECTIVE:
      clutter_stage_set_perspective (stage, g_value_get_boxed (value));
      break;

    case PROP_TITLE:
      clutter_stage_set_title (stage, g_value_get_string (value));
      break;

    case PROP_USER_RESIZE:
      clutter_stage_set_user_resizable (stage, g_value_get_boolean (value));
      break;

    case PROP_USE_FOG:
      clutter_stage_set_use_fog (stage, g_value_get_boolean (value));
      break;

    case PROP_FOG:
      clutter_stage_set_fog (stage, g_value_get_boxed (value));
      break;

    case PROP_USE_ALPHA:
      clutter_stage_set_use_alpha (stage, g_value_get_boolean (value));
      break;

    case PROP_KEY_FOCUS:
      clutter_stage_set_key_focus (stage, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_stage_get_property (GObject    *gobject,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (gobject)->priv;

  switch (prop_id)
    {
    case PROP_COLOR:
      clutter_value_set_color (value, &priv->color);
      break;

    case PROP_OFFSCREEN:
      g_value_set_boolean (value, FALSE);
      break;

    case PROP_FULLSCREEN_SET:
      g_value_set_boolean (value, priv->is_fullscreen);
      break;

    case PROP_CURSOR_VISIBLE:
      g_value_set_boolean (value, priv->is_cursor_visible);
      break;

    case PROP_PERSPECTIVE:
      g_value_set_boxed (value, &priv->perspective);
      break;

    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;

    case PROP_USER_RESIZE:
      g_value_set_boolean (value, priv->is_user_resizable);
      break;

    case PROP_USE_FOG:
      g_value_set_boolean (value, priv->use_fog);
      break;

    case PROP_FOG:
      g_value_set_boxed (value, &priv->fog);
      break;

    case PROP_USE_ALPHA:
      g_value_set_boolean (value, priv->use_alpha);
      break;

    case PROP_KEY_FOCUS:
      g_value_set_object (value, priv->key_focused_actor);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_stage_dispose (GObject *object)
{
  ClutterStage        *stage = CLUTTER_STAGE (object);
  ClutterStagePrivate *priv = stage->priv;
  ClutterStageManager *stage_manager = clutter_stage_manager_get_default ();
  ClutterMainContext  *context;
  GList               *l, *next;

  clutter_actor_hide (CLUTTER_ACTOR (object));

  _clutter_stage_manager_remove_stage (stage_manager, stage);

  context = _clutter_context_get_default ();

  /* Remove any pending events for this stage from the event queue */
  for (l = context->events_queue->head; l; l = next)
    {
      ClutterEvent *event = l->data;

      next = l->next;

      if (event->any.stage == stage)
        {
          g_queue_delete_link (context->events_queue, l);
          clutter_event_free (event);
        }
    }

  if (priv->impl != NULL)
    {
      CLUTTER_NOTE (BACKEND, "Disposing of the stage implementation");

      g_object_unref (priv->impl);
      priv->impl = NULL;
    }

  G_OBJECT_CLASS (clutter_stage_parent_class)->dispose (object);
}

static void
clutter_stage_finalize (GObject *object)
{
  ClutterStage *stage = CLUTTER_STAGE (object);
  ClutterStagePrivate *priv = stage->priv;

  g_queue_foreach (priv->event_queue, (GFunc)clutter_event_free, NULL);
  g_queue_free (priv->event_queue);

  g_free (stage->priv->title);

  G_OBJECT_CLASS (clutter_stage_parent_class)->finalize (object);
}

static void
clutter_stage_class_init (ClutterStageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = clutter_stage_set_property;
  gobject_class->get_property = clutter_stage_get_property;
  gobject_class->dispose = clutter_stage_dispose;
  gobject_class->finalize = clutter_stage_finalize;

  actor_class->allocate = clutter_stage_allocate;
  actor_class->get_preferred_width = clutter_stage_get_preferred_width;
  actor_class->get_preferred_height = clutter_stage_get_preferred_height;
  actor_class->paint = clutter_stage_paint;
  actor_class->pick = clutter_stage_pick;
  actor_class->realize = clutter_stage_realize;
  actor_class->unrealize = clutter_stage_unrealize;
  actor_class->show = clutter_stage_show;
  actor_class->hide = clutter_stage_hide;
  actor_class->queue_redraw = clutter_stage_real_queue_redraw;

  /**
   * ClutterStage:fullscreen:
   *
   * Whether the stage should be fullscreen or not.
   *
   * This property is set by calling clutter_stage_set_fullscreen()
   * but since the actual implementation is delegated to the backend
   * you should connect to the notify::fullscreen-set signal in order
   * to get notification if the fullscreen state has been successfully
   * achieved.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_boolean ("fullscreen-set",
                                "Fullscreen Set",
                                "Whether the main stage is fullscreen",
                                FALSE,
                                CLUTTER_PARAM_READABLE);
  g_object_class_install_property (gobject_class,
                                   PROP_FULLSCREEN_SET,
                                   pspec);
  /**
   * ClutterStage:offscreen:
   *
   * Whether the stage should be rendered in an offscreen buffer.
   *
   * <warning><para>Not every backend supports redirecting the
   * stage to an offscreen buffer. This property might not work
   * and it might be deprecated at any later date.</para></warning>
   */
  pspec = g_param_spec_boolean ("offscreen",
                                "Offscreen",
                                "Whether the main stage should be "
                                "rendered offscreen",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_OFFSCREEN,
                                   pspec);
  /**
   * ClutterStage:cursor-visible:
   *
   * Whether the mouse pointer should be visible
   */
  pspec = g_param_spec_boolean ("cursor-visible",
                                "Cursor Visible",
                                "Whether the mouse pointer is visible "
                                "on the main stage",
                                TRUE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_CURSOR_VISIBLE,
                                   pspec);
  /**
   * ClutterStage:user-resizable:
   *
   * Whether the stage is resizable via user interaction.
   *
   * Since: 0.4
   */
  pspec = g_param_spec_boolean ("user-resizable",
                                "User Resizable",
                                "Whether the stage is able to be resized "
                                "via user interaction",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_USER_RESIZE,
                                   pspec);
  /**
   * ClutterStage:color:
   *
   * The color of the main stage.
   */
  pspec = clutter_param_spec_color ("color",
                                    "Color",
                                    "The color of the stage",
                                    &default_stage_color,
                                    CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_COLOR, pspec);

  /**
   * ClutterStage:perspective:
   *
   * The parameters used for the perspective projection from 3D
   * coordinates to 2D
   *
   * Since: 0.8.2
   */
  pspec = g_param_spec_boxed ("perspective",
                              "Perspective",
                              "Perspective projection parameters",
                              CLUTTER_TYPE_PERSPECTIVE,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_PERSPECTIVE,
                                   pspec);

  /**
   * ClutterStage:title:
   *
   * The stage's title - usually displayed in stage windows title decorations.
   *
   * Since: 0.4
   */
  pspec = g_param_spec_string ("title",
                               "Title",
                               "Stage Title",
                               NULL,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TITLE, pspec);

  /**
   * ClutterStage:use-fog:
   *
   * Whether the stage should use a linear GL "fog" in creating the
   * depth-cueing effect, to enhance the perception of depth by fading
   * actors farther from the viewpoint.
   *
   * Since: 0.6
   */
  pspec = g_param_spec_boolean ("use-fog",
                                "Use Fog",
                                "Whether to enable depth cueing",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_USE_FOG, pspec);

  /**
   * ClutterStage:fog:
   *
   * The settings for the GL "fog", used only if #ClutterStage:use-fog
   * is set to %TRUE
   *
   * Since: 1.0
   */
  pspec = g_param_spec_boxed ("fog",
                              "Fog",
                              "Settings for the depth cueing",
                              CLUTTER_TYPE_FOG,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_FOG, pspec);

  /**
   * ClutterStage:use-alpha:
   *
   * Whether the #ClutterStage should honour the alpha component of the
   * #ClutterStage:color property when painting. If Clutter is run under
   * a compositing manager this will result in the stage being blended
   * with the underlying window(s)
   *
   * Since: 1.2
   */
  pspec = g_param_spec_boolean ("use-alpha",
                                "Use Alpha",
                                "Whether to honour the alpha component of "
                                "the stage color",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_USE_ALPHA, pspec);

  /**
   * ClutterStage:key-focus:
   *
   * The #ClutterActor that will receive key events from the underlying
   * windowing system.
   *
   * If %NULL, the #ClutterStage will receive the events.
   *
   * Since: 1.2
   */
  pspec = g_param_spec_object ("key-focus",
                               "Key Focus",
                               "The currently key focused actor",
                               CLUTTER_TYPE_ACTOR,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_KEY_FOCUS, pspec);

  /**
   * ClutterStage::fullscreen
   * @stage: the stage which was fullscreened
   *
   * The ::fullscreen signal is emitted when the stage is made fullscreen.
   *
   * Since: 0.6
   */
  stage_signals[FULLSCREEN] =
    g_signal_new ("fullscreen",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ClutterStageClass, fullscreen),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterStage::unfullscreen
   * @stage: the stage which has left a fullscreen state.
   *
   * The ::unfullscreen signal is emitted when the stage leaves a fullscreen
   * state.
   *
   * Since: 0.6
   */
  stage_signals[UNFULLSCREEN] =
    g_signal_new ("unfullscreen",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, unfullscreen),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterStage::activate
   * @stage: the stage which was activated
   *
   * The ::activate signal is emitted when the stage receives key focus
   * from the underlying window system.
   *
   * Since: 0.6
   */
  stage_signals[ACTIVATE] =
    g_signal_new ("activate",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, activate),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterStage::deactivate
   * @stage: the stage which was deactivated
   *
   * The ::activate signal is emitted when the stage loses key focus
   * from the underlying window system.
   *
   * Since: 0.6
   */
  stage_signals[DEACTIVATE] =
    g_signal_new ("deactivate",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, deactivate),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * ClutterStage::delete-event:
   * @stage: the stage that received the event
   * @event: a #ClutterEvent of type %CLUTTER_DELETE
   *
   * The ::delete-event signal is emitted when the user closes a
   * #ClutterStage window using the window controls.
   *
   * Clutter by default will call clutter_main_quit() if @stage is
   * the default stage, and clutter_actor_destroy() for any other
   * stage.
   *
   * It is possible to override the default behaviour by connecting
   * a new handler and returning %TRUE there.
   *
   * <note>This signal is emitted only on Clutter backends that
   * embed #ClutterStage in native windows. It is not emitted for
   * backends that use a static frame buffer.</note>
   *
   * Since: 1.2
   */
  stage_signals[DELETE_EVENT] =
    g_signal_new (I_("delete-event"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterStageClass, delete_event),
                  _clutter_boolean_handled_accumulator, NULL,
                  clutter_marshal_BOOLEAN__BOXED,
                  G_TYPE_BOOLEAN, 1,
                  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  klass->fullscreen = clutter_stage_real_fullscreen;
  klass->activate = clutter_stage_real_activate;
  klass->deactivate = clutter_stage_real_deactivate;
  klass->delete_event = clutter_stage_real_delete_event;

  g_type_class_add_private (gobject_class, sizeof (ClutterStagePrivate));
}

static void
clutter_stage_init (ClutterStage *self)
{
  ClutterStagePrivate *priv;
  ClutterBackend *backend;

  /* a stage is a top-level object */
  CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IS_TOPLEVEL);

  self->priv = priv = CLUTTER_STAGE_GET_PRIVATE (self);

  CLUTTER_NOTE (BACKEND, "Creating stage from the default backend");
  backend = clutter_get_default_backend ();
  priv->impl = _clutter_backend_create_stage (backend, self, NULL);
  if (!priv->impl)
    {
      g_warning ("Unable to create a new stage, falling back to the "
                 "default stage.");
      priv->impl = _clutter_stage_get_default_window ();

      /* at this point we must have a default stage, or we're screwed */
      g_assert (priv->impl != NULL);
    }

  priv->event_queue = g_queue_new ();

  priv->is_fullscreen          = FALSE;
  priv->is_user_resizable      = FALSE;
  priv->is_cursor_visible      = TRUE;
  priv->use_fog                = FALSE;
  priv->throttle_motion_events = TRUE;

  priv->color = default_stage_color;

  priv->perspective.fovy   = 60.0; /* 60 Degrees */
  priv->perspective.aspect = 1.0;
  priv->perspective.z_near = 0.1;
  priv->perspective.z_far  = 100.0;

  /* depth cueing */
  priv->fog.z_near = 1.0;
  priv->fog.z_far  = 2.0;

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);
  clutter_stage_set_key_focus (self, NULL);
}

/**
 * clutter_stage_get_default:
 *
 * Returns the main stage. The default #ClutterStage is a singleton,
 * so the stage will be created the first time this function is
 * called (typically, inside clutter_init()); all the subsequent
 * calls to clutter_stage_get_default() will return the same instance.
 *
 * Clutter guarantess the existence of the default stage.
 *
 * Return value: (transfer none): the main #ClutterStage.  You should never
 *   destroy or unref the returned actor.
 */
ClutterActor *
clutter_stage_get_default (void)
{
  ClutterStageManager *stage_manager = clutter_stage_manager_get_default ();
  ClutterStage *stage;

  stage = clutter_stage_manager_get_default_stage (stage_manager);
  if (G_UNLIKELY (stage == NULL))
    /* This will take care of automatically adding the stage to the
     * stage manager and setting it as the default. Its floating
     * reference will be claimed by the stage manager.
     */
    stage = g_object_new (CLUTTER_TYPE_STAGE, NULL);

  return CLUTTER_ACTOR (stage);
}

/**
 * clutter_stage_set_color:
 * @stage: A #ClutterStage
 * @color: A #ClutterColor
 *
 * Sets the stage color.
 */
void
clutter_stage_set_color (ClutterStage       *stage,
			 const ClutterColor *color)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (color != NULL);

  priv = stage->priv;

  priv->color = *color;

  if (CLUTTER_ACTOR_IS_VISIBLE (stage))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  g_object_notify (G_OBJECT (stage), "color");
}

/**
 * clutter_stage_get_color:
 * @stage: A #ClutterStage
 * @color: return location for a #ClutterColor
 *
 * Retrieves the stage color.
 */
void
clutter_stage_get_color (ClutterStage *stage,
			 ClutterColor *color)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (color != NULL);

  priv = stage->priv;

  *color = priv->color;
}

/**
 * clutter_stage_set_perspective:
 * @stage: A #ClutterStage
 * @perspective: A #ClutterPerspective
 *
 * Sets the stage perspective.
 */
void
clutter_stage_set_perspective (ClutterStage       *stage,
                               ClutterPerspective *perspective)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (perspective != NULL);
  g_return_if_fail (perspective->z_far - perspective->z_near != 0);

  priv = stage->priv;

  priv->perspective = *perspective;

  /* this will cause the viewport to be reset; see
   * clutter_maybe_setup_viewport() inside clutter-main.c
   */
  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_SYNC_MATRICES);
}

/**
 * clutter_stage_get_perspective:
 * @stage: A #ClutterStage
 * @perspective: return location for a #ClutterPerspective
 *
 * Retrieves the stage perspective.
 */
void
clutter_stage_get_perspective (ClutterStage       *stage,
                               ClutterPerspective *perspective)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (perspective != NULL);

  *perspective = stage->priv->perspective;
}

/**
 * clutter_stage_set_fullscreen:
 * @stage: a #ClutterStage
 * @fullscreen: %TRUE to to set the stage fullscreen
 *
 * Asks to place the stage window in the fullscreen or unfullscreen
 * states.
 *
 ( Note that you shouldn't assume the window is definitely full screen
 * afterward, because other entities (e.g. the user or window manager)
 * could unfullscreen it again, and not all window managers honor
 * requests to fullscreen windows.
 *
 * If you want to receive notification of the fullscreen state you
 * should either use the #ClutterStage::fullscreen and
 * #ClutterStage::unfullscreen signals, or use the notify signal
 * for the #ClutterStage:fullscreen-set property
 *
 * Since: 1.0
 */
void
clutter_stage_set_fullscreen (ClutterStage *stage,
                              gboolean      fullscreen)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (priv->is_fullscreen != fullscreen)
    {
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);

      /* Only set if backend implements.
       *
       * Also see clutter_stage_event() for setting priv->is_fullscreen
       * on state change event.
       */
      if (iface->set_fullscreen)
	iface->set_fullscreen (impl, fullscreen);
    }
}

/**
 * clutter_stage_get_fullscreen:
 * @stage: a #ClutterStage
 *
 * Retrieves whether the stage is full screen or not
 *
 * Return value: %TRUE if the stage is full screen
 *
 * Since: 1.0
 */
gboolean
clutter_stage_get_fullscreen (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->is_fullscreen;
}

/**
 * clutter_stage_set_user_resizable:
 * @stage: a #ClutterStage
 * @resizable: whether the stage should be user resizable.
 *
 * Sets if the stage is resizable by user interaction (e.g. via
 * window manager controls)
 *
 * Since: 0.4
 */
void
clutter_stage_set_user_resizable (ClutterStage *stage,
                                  gboolean      resizable)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (clutter_feature_available (CLUTTER_FEATURE_STAGE_USER_RESIZE)
      && priv->is_user_resizable != resizable)
    {
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);
      if (iface->set_user_resizable)
        {
          priv->is_user_resizable = resizable;

          iface->set_user_resizable (impl, resizable);

          g_object_notify (G_OBJECT (stage), "user-resizable");
        }
    }
}

/**
 * clutter_stage_get_user_resizable:
 * @stage: a #ClutterStage
 *
 * Retrieves the value set with clutter_stage_set_user_resizable().
 *
 * Return value: %TRUE if the stage is resizable by the user.
 *
 * Since: 0.4
 */
gboolean
clutter_stage_get_user_resizable (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->is_user_resizable;
}

/**
 * clutter_stage_show_cursor:
 * @stage: a #ClutterStage
 *
 * Shows the cursor on the stage window
 */
void
clutter_stage_show_cursor (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;
  if (!priv->is_cursor_visible)
    {
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);
      if (iface->set_cursor_visible)
        {
          priv->is_cursor_visible = TRUE;

          iface->set_cursor_visible (impl, TRUE);

          g_object_notify (G_OBJECT (stage), "cursor-visible");
        }
    }
}

/**
 * clutter_stage_hide_cursor:
 * @stage: a #ClutterStage
 *
 * Makes the cursor invisible on the stage window
 *
 * Since: 0.4
 */
void
clutter_stage_hide_cursor (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;
  if (priv->is_cursor_visible)
    {
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);
      if (iface->set_cursor_visible)
        {
          priv->is_cursor_visible = FALSE;

          iface->set_cursor_visible (impl, FALSE);

          g_object_notify (G_OBJECT (stage), "cursor-visible");
        }
    }
}

/**
 * clutter_stage_read_pixels:
 * @stage: A #ClutterStage
 * @x: x coordinate of the first pixel that is read from stage
 * @y: y coordinate of the first pixel that is read from stage
 * @width: Width dimention of pixels to be read, or -1 for the
 *   entire stage width
 * @height: Height dimention of pixels to be read, or -1 for the
 *   entire stage height
 *
 * Makes a screenshot of the stage in RGBA 8bit data, returns a
 * linear buffer with @width * 4 as rowstride.
 *
 * The alpha data contained in the returned buffer is driver-dependent,
 * and not guaranteed to hold any sensible value.
 *
 * Return value: a pointer to newly allocated memory with the buffer
 *   or %NULL if the read failed. Use g_free() on the returned data
 *   to release the resources it has allocated.
 */
guchar *
clutter_stage_read_pixels (ClutterStage *stage,
                           gint          x,
                           gint          y,
                           gint          width,
                           gint          height)
{
  guchar *pixels;
  GLint   viewport[4];
  gint    rowstride;
  gint    stage_width, stage_height;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  /* according to glReadPixels documentation pixels outside the viewport are
   * undefined, but no error should be provoked, thus this is probably unnneed.
   */
  g_return_val_if_fail (x >= 0 && y >= 0, NULL);

  /* Force a redraw of the stage before reading back pixels */
  clutter_stage_ensure_current (stage);
  clutter_actor_paint (CLUTTER_ACTOR (stage));

  glGetIntegerv (GL_VIEWPORT, viewport);
  stage_width  = viewport[2];
  stage_height = viewport[3];

  if (width < 0 || width > stage_width)
    width = stage_width;

  if (height < 0 || height > stage_height)
    height = stage_height;

  rowstride = width * 4;

  pixels  = g_malloc (height * rowstride);

  cogl_read_pixels (x, y, width, height,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    pixels);

  return pixels;
}

/**
 * clutter_stage_get_actor_at_pos:
 * @stage: a #ClutterStage
 * @pick_mode: how the scene graph should be painted
 * @x: X coordinate to check
 * @y: Y coordinate to check
 *
 * Checks the scene at the coordinates @x and @y and returns a pointer
 * to the #ClutterActor at those coordinates.
 *
 * By using @pick_mode it is possible to control which actors will be
 * painted and thus available.
 *
 * Return value: (transfer none): the actor at the specified coordinates,
 *   if any
 */
ClutterActor *
clutter_stage_get_actor_at_pos (ClutterStage    *stage,
                                ClutterPickMode  pick_mode,
                                gint             x,
                                gint             y)
{
  return _clutter_do_pick (stage, x, y, pick_mode);
}

/**
 * clutter_stage_event:
 * @stage: a #ClutterStage
 * @event: a #ClutterEvent
 *
 * This function is used to emit an event on the main stage.
 *
 * You should rarely need to use this function, except for
 * synthetised events.
 *
 * Return value: the return value from the signal emission
 *
 * Since: 0.4
 */
gboolean
clutter_stage_event (ClutterStage *stage,
                     ClutterEvent *event)
{
  ClutterStagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  priv = stage->priv;

  if (event->type == CLUTTER_DELETE)
    {
      gboolean retval = FALSE;

      g_signal_emit_by_name (stage, "event", event, &retval);

      if (!retval)
        g_signal_emit_by_name (stage, "delete-event", event, &retval);

      return retval;
    }

  if (event->type != CLUTTER_STAGE_STATE)
    return FALSE;

  /* emit raw event */
  if (clutter_actor_event (CLUTTER_ACTOR (stage), event, FALSE))
    return TRUE;

  if (event->stage_state.changed_mask & CLUTTER_STAGE_STATE_FULLSCREEN)
    {
      if (event->stage_state.new_state & CLUTTER_STAGE_STATE_FULLSCREEN)
	{
	  priv->is_fullscreen = TRUE;
	  g_signal_emit (stage, stage_signals[FULLSCREEN], 0);

          g_object_notify (G_OBJECT (stage), "fullscreen-set");
	}
      else
	{
	  priv->is_fullscreen = FALSE;
	  g_signal_emit (stage, stage_signals[UNFULLSCREEN], 0);

          g_object_notify (G_OBJECT (stage), "fullscreen-set");
	}
    }

  if (event->stage_state.changed_mask & CLUTTER_STAGE_STATE_ACTIVATED)
    {
      if (event->stage_state.new_state & CLUTTER_STAGE_STATE_ACTIVATED)
	g_signal_emit (stage, stage_signals[ACTIVATE], 0);
      else
	g_signal_emit (stage, stage_signals[DEACTIVATE], 0);
    }

  return TRUE;
}

/**
 * clutter_stage_set_title
 * @stage: A #ClutterStage
 * @title: A utf8 string for the stage windows title.
 *
 * Sets the stage title.
 *
 * Since: 0.4
 **/
void
clutter_stage_set_title (ClutterStage       *stage,
			 const gchar        *title)
{
  ClutterStagePrivate *priv;
  ClutterStageWindow *impl;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  g_free (priv->title);
  priv->title = g_strdup (title);

  impl = CLUTTER_STAGE_WINDOW (priv->impl);
  if (CLUTTER_STAGE_WINDOW_GET_IFACE(impl)->set_title != NULL)
    CLUTTER_STAGE_WINDOW_GET_IFACE (impl)->set_title (impl, priv->title);

  g_object_notify (G_OBJECT (stage), "title");
}

/**
 * clutter_stage_get_title
 * @stage: A #ClutterStage
 *
 * Gets the stage title.
 *
 * Return value: pointer to the title string for the stage. The
 * returned string is owned by the actor and should not
 * be modified or freed.
 *
 * Since: 0.4
 **/
G_CONST_RETURN gchar *
clutter_stage_get_title (ClutterStage       *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  return stage->priv->title;
}

static void
on_key_focused_weak_notify (gpointer data,
			    GObject *where_the_object_was)
{
  ClutterStagePrivate *priv;
  ClutterStage        *stage = CLUTTER_STAGE (data);

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;
  priv->key_focused_actor = NULL;

  /* focused actor has dissapeared - fall back to stage
   * FIXME: need some kind of signal dance/block here.
  */
  clutter_stage_set_key_focus (stage, NULL);
}

/**
 * clutter_stage_set_key_focus:
 * @stage: the #ClutterStage
 * @actor: (allow-none): the actor to set key focus to, or %NULL
 *
 * Sets the key focus on @actor. An actor with key focus will receive
 * all the key events. If @actor is %NULL, the stage will receive
 * focus.
 *
 * Since: 0.6
 */
void
clutter_stage_set_key_focus (ClutterStage *stage,
			     ClutterActor *actor)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (actor == NULL || CLUTTER_IS_ACTOR (actor));

  priv = stage->priv;

  if (priv->key_focused_actor == actor)
    return;

  if (priv->key_focused_actor)
    {
      ClutterActor *old_focused_actor;

      old_focused_actor = priv->key_focused_actor;

      /* set key_focused_actor to NULL before emitting the signal or someone
       * might hide the previously focused actor in the signal handler and we'd
       * get re-entrant call and get glib critical from g_object_weak_unref
       */

      g_object_weak_unref (G_OBJECT (priv->key_focused_actor),
			   on_key_focused_weak_notify,
			   stage);

      priv->key_focused_actor = NULL;

      g_signal_emit_by_name (old_focused_actor, "key-focus-out");
    }
  else
    g_signal_emit_by_name (stage, "key-focus-out");

  /* Note, if someone changes key focus in focus-out signal handler we'd be
   * overriding the latter call below moving the focus where it was originally
   * intended. The order of events would be:
   *   1st focus-out, 2nd focus-out (on stage), 2nd focus-in, 1st focus-in
   */

  if (actor)
    {
      priv->key_focused_actor = actor;

      g_object_weak_ref (G_OBJECT (actor),
			 on_key_focused_weak_notify,
			 stage);
      g_signal_emit_by_name (priv->key_focused_actor, "key-focus-in");
    }
  else
    g_signal_emit_by_name (stage, "key-focus-in");

  g_object_notify (G_OBJECT (stage), "key-focus");
}

/**
 * clutter_stage_get_key_focus:
 * @stage: the #ClutterStage
 *
 * Retrieves the actor that is currently under key focus.
 *
 * Return value: (transfer none): the actor with key focus, or the stage
 *
 * Since: 0.6
 */
ClutterActor *
clutter_stage_get_key_focus (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  if (stage->priv->key_focused_actor)
    return stage->priv->key_focused_actor;

  return CLUTTER_ACTOR (stage);
}

/**
 * clutter_stage_get_use_fog:
 * @stage: the #ClutterStage
 *
 * Gets whether the depth cueing effect is enabled on @stage.
 *
 * Return value: %TRUE if the the depth cueing effect is enabled
 *
 * Since: 0.6
 */
gboolean
clutter_stage_get_use_fog (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->use_fog;
}

/**
 * clutter_stage_set_use_fog:
 * @stage: the #ClutterStage
 * @fog: %TRUE for enabling the depth cueing effect
 *
 * Sets whether the depth cueing effect on the stage should be enabled
 * or not.
 *
 * Depth cueing is a 3D effect that makes actors farther away from the
 * viewing point less opaque, by fading them with the stage color.

 * The parameters of the GL fog used can be changed using the
 * clutter_stage_set_fog() function.
 *
 * Since: 0.6
 */
void
clutter_stage_set_use_fog (ClutterStage *stage,
                           gboolean      fog)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (priv->use_fog != fog)
    {
      priv->use_fog = fog;

      CLUTTER_NOTE (MISC, "%s depth-cueing inside stage",
                    priv->use_fog ? "enabling" : "disabling");

      if (CLUTTER_ACTOR_IS_VISIBLE (stage))
        clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

      g_object_notify (G_OBJECT (stage), "use-fog");
    }
}

/**
 * clutter_stage_set_fog:
 * @stage: the #ClutterStage
 * @fog: a #ClutterFog structure
 *
 * Sets the fog (also known as "depth cueing") settings for the @stage.
 *
 * A #ClutterStage will only use a linear fog progression, which
 * depends solely on the distance from the viewer. The cogl_set_fog()
 * function in COGL exposes more of the underlying implementation,
 * and allows changing the for progression function. It can be directly
 * used by disabling the #ClutterStage:use-fog property and connecting
 * a signal handler to the #ClutterActor::paint signal on the @stage,
 * like:
 *
 * |[
 *   clutter_stage_set_use_fog (stage, FALSE);
 *   g_signal_connect (stage, "paint", G_CALLBACK (on_stage_paint), NULL);
 * ]|
 *
 * The paint signal handler will call cogl_set_fog() with the
 * desired settings:
 *
 * |[
 *   static void
 *   on_stage_paint (ClutterActor *actor)
 *   {
 *     ClutterColor stage_color = { 0, };
 *     CoglColor fog_color = { 0, };
 *
 *     /&ast; set the fog color to the stage background color &ast;/
 *     clutter_stage_get_color (CLUTTER_STAGE (actor), &amp;stage_color);
 *     cogl_color_set_from_4ub (&amp;fog_color,
 *                              stage_color.red,
 *                              stage_color.green,
 *                              stage_color.blue,
 *                              stage_color.alpha);
 *
 *     /&ast; enable fog &ast;/
 *     cogl_set_fog (&amp;fog_color,
 *                   COGL_FOG_MODE_EXPONENTIAL, /&ast; mode &ast;/
 *                   0.5,                       /&ast; density &ast;/
 *                   5.0, 30.0);                /&ast; z_near and z_far &ast;/
 *   }
 * ]|
 *
 * Note: The fogging functions only work correctly when the visible actors use
 *       unmultiplied alpha colors. By default Cogl will premultiply textures
 *       and cogl_set_source_color will premultiply colors, so unless you
 *       explicitly load your textures requesting an unmultiplied
 *       internal_format and use cogl_material_set_color you can only use
 *       fogging with fully opaque actors.
 *
 *       We can look to improve this in the future when we can depend on
 *       fragment shaders.
 *
 * Since: 0.6
 */
void
clutter_stage_set_fog (ClutterStage *stage,
                       ClutterFog   *fog)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (fog != NULL);

  priv = stage->priv;

  priv->fog = *fog;

  if (priv->use_fog && CLUTTER_ACTOR_IS_VISIBLE (stage))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

/**
 * clutter_stage_get_fog:
 * @stage: the #ClutterStage
 * @fog: return location for a #ClutterFog structure
 *
 * Retrieves the current depth cueing settings from the stage.
 *
 * Since: 0.6
 */
void
clutter_stage_get_fog (ClutterStage *stage,
                        ClutterFog   *fog)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (fog != NULL);

  *fog = stage->priv->fog;
}

/*** Perspective boxed type ******/

static gpointer
clutter_perspective_copy (gpointer data)
{
  if (G_LIKELY (data))
    return g_slice_dup (ClutterPerspective, data);

  return NULL;
}

static void
clutter_perspective_free (gpointer data)
{
  if (G_LIKELY (data))
    g_slice_free (ClutterPerspective, data);
}

GType
clutter_perspective_get_type (void)
{
  static GType our_type = 0;

  if (!our_type)
    our_type = g_boxed_type_register_static (I_("ClutterPerspective"),
                                             clutter_perspective_copy,
                                             clutter_perspective_free);
  return our_type;
}

static gpointer
clutter_fog_copy (gpointer data)
{
  if (G_LIKELY (data))
    return g_slice_dup (ClutterFog, data);

  return NULL;
}

static void
clutter_fog_free (gpointer data)
{
  if (G_LIKELY (data))
    g_slice_free (ClutterFog, data);
}

GType
clutter_fog_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type = g_boxed_type_register_static (I_("ClutterFog"),
                                             clutter_fog_copy,
                                             clutter_fog_free);

  return our_type;
}

/**
 * clutter_stage_new:
 *
 * Creates a new, non-default stage. A non-default stage is a new
 * top-level actor which can be used as another container. It works
 * exactly like the default stage, but while clutter_stage_get_default()
 * will always return the same instance, you will have to keep a pointer
 * to any #ClutterStage returned by clutter_stage_create().
 *
 * The ability to support multiple stages depends on the current
 * backend. Use clutter_feature_available() and
 * %CLUTTER_FEATURE_STAGE_MULTIPLE to check at runtime whether a
 * backend supports multiple stages.
 *
 * Return value: a new stage, or %NULL if the default backend does
 *   not support multiple stages. Use clutter_actor_destroy() to
 *   programmatically close the returned stage.
 *
 * Since: 0.8
 */
ClutterActor *
clutter_stage_new (void)
{
  if (!clutter_feature_available (CLUTTER_FEATURE_STAGE_MULTIPLE))
    {
      g_warning ("Unable to create a new stage: the %s backend does not "
                 "support multiple stages.",
                 CLUTTER_FLAVOUR);
      return NULL;
    }

  /* The stage manager will grab the floating reference when the stage
     is added to it in the constructor */
  return g_object_new (CLUTTER_TYPE_STAGE, NULL);
}

/**
 * clutter_stage_ensure_current:
 * @stage: the #ClutterStage
 *
 * This function essentially makes sure the right GL context is
 * current for the passed stage. It is not intended to
 * be used by applications.
 *
 * Since: 0.8
 */
void
clutter_stage_ensure_current (ClutterStage *stage)
{
  ClutterBackend *backend;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  backend = clutter_get_default_backend ();
  _clutter_backend_ensure_context (backend, stage);
}

/**
 * clutter_stage_ensure_viewport:
 * @stage: a #ClutterStage
 *
 * Ensures that the GL viewport is updated with the current
 * stage window size.
 *
 * This function will queue a redraw of @stage.
 *
 * This function should not be called by applications; it is used
 * when embedding a #ClutterStage into a toolkit with another
 * windowing system, like GTK+.
 *
 * Since: 1.0
 */
void
clutter_stage_ensure_viewport (ClutterStage *stage)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_SYNC_MATRICES);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

/**
 * clutter_stage_ensure_redraw:
 * @stage: a #ClutterStage
 *
 * Ensures that @stage is redrawn
 *
 * This function should not be called by applications: it is
 * used when embedding a #ClutterStage into a toolkit with
 * another windowing system, like GTK+.
 *
 * Since: 1.0
 */
void
clutter_stage_ensure_redraw (ClutterStage *stage)
{
  ClutterMasterClock *master_clock;
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;
  priv->redraw_pending = TRUE;

  master_clock = _clutter_master_clock_get_default ();
  _clutter_master_clock_start_running (master_clock);
}

/**
 * clutter_stage_queue_redraw:
 * @stage: the #ClutterStage
 *
 * Queues a redraw for the passed stage.
 *
 * <note>Applications should call clutter_actor_queue_redraw() and not
 * this function.</note>
 *
 * <note>This function is just a wrapper for clutter_actor_queue_redraw()
 * and should probably go away.</note>
 *
 * Since: 0.8
 */
void
clutter_stage_queue_redraw (ClutterStage *stage)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

/**
 * clutter_stage_is_default:
 * @stage: a #ClutterStage
 *
 * Checks if @stage is the default stage, or an instance created using
 * clutter_stage_new() but internally using the same implementation.
 *
 * Return value: %TRUE if the passed stage is the default one
 *
 * Since: 0.8
 */
gboolean
clutter_stage_is_default (ClutterStage *stage)
{
  ClutterStageWindow *impl;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  if (CLUTTER_ACTOR (stage) == clutter_stage_get_default ())
    return TRUE;

  impl = _clutter_stage_get_window (stage);
  if (impl == _clutter_stage_get_default_window ())
    return TRUE;

  return FALSE;
}

void
_clutter_stage_set_window (ClutterStage       *stage,
                           ClutterStageWindow *stage_window)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (stage_window));

  if (stage->priv->impl)
    g_object_unref (stage->priv->impl);

  stage->priv->impl = stage_window;
}

ClutterStageWindow *
_clutter_stage_get_window (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  return CLUTTER_STAGE_WINDOW (stage->priv->impl);
}

ClutterStageWindow *
_clutter_stage_get_default_window (void)
{
  ClutterActor *stage = clutter_stage_get_default ();

  return _clutter_stage_get_window (CLUTTER_STAGE (stage));
}

/**
 * clutter_stage_set_throttle_motion_events:
 * @stage: a #ClutterStage
 * @throttle: %TRUE to throttle motion events
 *
 * Sets whether motion events received between redraws should
 * be throttled or not. If motion events are throttled, those
 * events received by the windowing system between redraws will
 * be compressed so that only the last event will be propagated
 * to the @stage and its actors.
 *
 * This function should only be used if you want to have all
 * the motion events delivered to your application code.
 *
 * Since: 1.0
 */
void
clutter_stage_set_throttle_motion_events (ClutterStage *stage,
                                          gboolean      throttle)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (priv->throttle_motion_events != throttle)
    priv->throttle_motion_events = throttle;
}

/**
 * clutter_stage_get_throttle_motion_events:
 * @stage: a #ClutterStage
 *
 * Retrieves the value set with clutter_stage_set_throttle_motion_events()
 *
 * Return value: %TRUE if the motion events are being throttled,
 *   and %FALSE otherwise
 *
 * Since: 1.0
 */
gboolean
clutter_stage_get_throttle_motion_events (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->throttle_motion_events;
}

/**
 * clutter_stage_set_use_alpha:
 * @stage: a #ClutterStage
 * @use_alpha: whether the stage should honour the opacity or the
 *   alpha channel of the stage color
 *
 * Sets whether the @stage should honour the #ClutterActor:opacity and
 * the alpha channel of the #ClutterStage:color
 *
 * Since: 1.2
 */
void
clutter_stage_set_use_alpha (ClutterStage *stage,
                             gboolean      use_alpha)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (priv->use_alpha != use_alpha)
    {
      priv->use_alpha = use_alpha;

      clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

      g_object_notify (G_OBJECT (stage), "use-alpha");
    }
}

/**
 * clutter_stage_get_use_alpha:
 * @stage: a #ClutterStage
 *
 * Retrieves the value set using clutter_stage_set_use_alpha()
 *
 * Return value: %TRUE if the stage should honour the opacity and the
 *   alpha channel of the stage color
 *
 * Since: 1.2
 */
gboolean
clutter_stage_get_use_alpha (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->use_alpha;
}
