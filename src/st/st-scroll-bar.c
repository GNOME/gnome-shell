/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-scroll-bar.c: Scroll bar actor
 *
 * Copyright 2008 OpenedHand
 * Copyright 2008, 2009 Intel Corporation.
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2010 Maxim Ermilov
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * StScrollBar:
 *
 * A user interface element to control scrollable areas.
 *
 * The #StScrollBar allows users to scroll scrollable actors, either by
 * the step or page amount, or by manually dragging the handle.
 */

#include "config.h"

#include <math.h>
#include <clutter/clutter.h>

#include "st-scroll-bar.h"
#include "st-bin.h"
#include "st-enum-types.h"
#include "st-private.h"
#include "st-button.h"
#include "st-settings.h"

#define PAGING_INITIAL_REPEAT_TIMEOUT 500
#define PAGING_SUBSEQUENT_REPEAT_TIMEOUT 200

typedef struct _StScrollBarPrivate   StScrollBarPrivate;
struct _StScrollBarPrivate
{
  StAdjustment *adjustment;

  gfloat        x_origin;
  gfloat        y_origin;

  ClutterActor *trough;
  ClutterActor *handle;

  gfloat        move_x;
  gfloat        move_y;

  ClutterPanGesture *trough_pan_gesture;
  ClutterPanGesture *handle_pan_gesture;

  /* Trough-click handling. */
  enum { NONE, UP, DOWN }  paging_direction;
  guint             paging_source_id;
  guint             paging_event_no;

  ClutterOrientation orientation;
};

G_DEFINE_TYPE_WITH_PRIVATE (StScrollBar, st_scroll_bar, ST_TYPE_WIDGET)

#define ST_SCROLL_BAR_PRIVATE(sb) st_scroll_bar_get_instance_private (ST_SCROLL_BAR (sb))

enum
{
  PROP_0,

  PROP_ADJUSTMENT,
  PROP_ORIENTATION,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

enum
{
  SCROLL_START,
  SCROLL_STOP,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

ClutterOrientation
st_scroll_bar_get_orientation (StScrollBar *bar)
{
  StScrollBarPrivate *priv;

  g_return_val_if_fail (ST_IS_SCROLL_BAR (bar), CLUTTER_ORIENTATION_HORIZONTAL);

  priv = ST_SCROLL_BAR_PRIVATE (bar);
  return priv->orientation;
}

void
st_scroll_bar_set_orientation (StScrollBar        *bar,
                               ClutterOrientation  orientation)
{
  StScrollBarPrivate *priv;

  g_return_if_fail (ST_IS_SCROLL_BAR (bar));

  priv = ST_SCROLL_BAR_PRIVATE (bar);

  if (priv->orientation == orientation)
    return;

  priv->orientation = orientation;

  if (priv->orientation == CLUTTER_ORIENTATION_VERTICAL)
    st_widget_set_style_class_name (ST_WIDGET (priv->handle), "vhandle");
  else
    st_widget_set_style_class_name (ST_WIDGET (priv->handle), "hhandle");

  clutter_actor_queue_relayout (CLUTTER_ACTOR (bar));
  g_object_notify_by_pspec (G_OBJECT (bar), props[PROP_ORIENTATION]);
}

static void
st_scroll_bar_get_property (GObject    *gobject,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  StScrollBarPrivate *priv = ST_SCROLL_BAR_PRIVATE (gobject);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, priv->adjustment);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_scroll_bar_set_property (GObject      *gobject,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  StScrollBar *bar = ST_SCROLL_BAR (gobject);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      st_scroll_bar_set_adjustment (bar, g_value_get_object (value));
      break;

    case PROP_ORIENTATION:
      st_scroll_bar_set_orientation (bar, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_scroll_bar_dispose (GObject *gobject)
{
  StScrollBar *bar = ST_SCROLL_BAR (gobject);
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (bar);

  if (priv->adjustment)
    st_scroll_bar_set_adjustment (bar, NULL);

  if (priv->handle)
    {
      clutter_actor_destroy (priv->handle);
      priv->handle = NULL;
    }

  if (priv->trough)
    {
      clutter_actor_destroy (priv->trough);
      priv->trough = NULL;
    }

  G_OBJECT_CLASS (st_scroll_bar_parent_class)->dispose (gobject);
}

static void
st_scroll_bar_unmap (ClutterActor *actor)
{
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (ST_SCROLL_BAR (actor));

  if (priv->handle)
    clutter_gesture_cancel (CLUTTER_GESTURE (priv->handle_pan_gesture));

  CLUTTER_ACTOR_CLASS (st_scroll_bar_parent_class)->unmap (actor);
}

static void
scroll_bar_allocate_children (StScrollBar           *bar,
                              const ClutterActorBox *box)
{
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (bar);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (bar));
  ClutterActorBox content_box, trough_box;

  st_theme_node_get_content_box (theme_node, box, &content_box);

  trough_box.x1 = content_box.x1;
  trough_box.y1 = content_box.y1;
  trough_box.x2 = content_box.x2;
  trough_box.y2 = content_box.y2;
  clutter_actor_allocate (priv->trough, &trough_box);

  if (priv->adjustment)
    {
      float handle_size, position, avail_size;
      gdouble value, lower, upper, page_size, increment, min_size, max_size;
      ClutterActorBox handle_box = { 0, };

      st_adjustment_get_values (priv->adjustment,
                                &value,
                                &lower,
                                &upper,
                                NULL,
                                NULL,
                                &page_size);

      if ((upper == lower)
          || (page_size >= (upper - lower)))
        increment = 1.0;
      else
        increment = page_size / (upper - lower);

      min_size = 32.;
      st_theme_node_lookup_length (theme_node, "min-size", FALSE, &min_size);
      max_size = G_MAXINT16;
      st_theme_node_lookup_length (theme_node, "max-size", FALSE, &max_size);

      if (upper - lower - page_size <= 0)
        position = 0;
      else
        position = (value - lower) / (upper - lower - page_size);

      if (priv->orientation == CLUTTER_ORIENTATION_VERTICAL)
        {
          avail_size = content_box.y2 - content_box.y1;
          handle_size = increment * avail_size;
          handle_size = CLAMP (handle_size, min_size, max_size);

          handle_box.x1 = content_box.x1;
          handle_box.y1 = content_box.y1 + position * (avail_size - handle_size);

          handle_box.x2 = content_box.x2;
          handle_box.y2 = handle_box.y1 + handle_size;
        }
      else
        {
          ClutterTextDirection direction;

          avail_size = content_box.x2 - content_box.x1;
          handle_size = increment * avail_size;
          handle_size = CLAMP (handle_size, min_size, max_size);

          direction = clutter_actor_get_text_direction (CLUTTER_ACTOR (bar));
          if (direction == CLUTTER_TEXT_DIRECTION_RTL)
            {
              handle_box.x2 = content_box.x2 - position * (avail_size - handle_size);
              handle_box.x1 = handle_box.x2 - handle_size;
            }
          else
            {
              handle_box.x1 = content_box.x1 + position * (avail_size - handle_size);
              handle_box.x2 = handle_box.x1 + handle_size;
            }

          handle_box.y1 = content_box.y1;
          handle_box.y2 = content_box.y2;
        }

      clutter_actor_allocate (priv->handle, &handle_box);
    }
}

static void
st_scroll_bar_get_preferred_width (ClutterActor *self,
                                   gfloat        for_height,
                                   gfloat       *min_width_p,
                                   gfloat       *natural_width_p)
{
  StScrollBar *bar = ST_SCROLL_BAR (self);
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (bar);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (self));
  gfloat trough_min_width, trough_natural_width;
  gfloat handle_min_width, handle_natural_width;

  st_theme_node_adjust_for_height (theme_node, &for_height);

  _st_actor_get_preferred_width (priv->trough, for_height, TRUE,
                                 &trough_min_width, &trough_natural_width);

  _st_actor_get_preferred_width (priv->handle, for_height, TRUE,
                                 &handle_min_width, &handle_natural_width);

  if (priv->orientation == CLUTTER_ORIENTATION_VERTICAL)
    {
      if (min_width_p)
        *min_width_p = MAX (trough_min_width, handle_min_width);

      if (natural_width_p)
        *natural_width_p = MAX (trough_natural_width, handle_natural_width);
    }
  else
    {
      if (min_width_p)
        *min_width_p = trough_min_width + handle_min_width;

      if (natural_width_p)
        *natural_width_p = trough_natural_width + handle_natural_width;
    }

  st_theme_node_adjust_preferred_width (theme_node, min_width_p, natural_width_p);
}

static void
st_scroll_bar_get_preferred_height (ClutterActor *self,
                                    gfloat        for_width,
                                    gfloat       *min_height_p,
                                    gfloat       *natural_height_p)
{
  StScrollBar *bar = ST_SCROLL_BAR (self);
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (bar);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (self));
  gfloat trough_min_height, trough_natural_height;
  gfloat handle_min_height, handle_natural_height;

  st_theme_node_adjust_for_width (theme_node, &for_width);

  _st_actor_get_preferred_height (priv->trough, for_width, TRUE,
                                  &trough_min_height, &trough_natural_height);

  _st_actor_get_preferred_height (priv->handle, for_width, TRUE,
                                  &handle_min_height, &handle_natural_height);

  if (priv->orientation == CLUTTER_ORIENTATION_VERTICAL)
    {
      if (min_height_p)
        *min_height_p = trough_min_height + handle_min_height;

      if (natural_height_p)
        *natural_height_p = trough_natural_height + handle_natural_height;
    }
  else
    {
      if (min_height_p)
        *min_height_p = MAX (trough_min_height, handle_min_height);

      if (natural_height_p)
        *natural_height_p = MAX (trough_natural_height, handle_natural_height);
    }

  st_theme_node_adjust_preferred_height (theme_node, min_height_p, natural_height_p);
}

static void
st_scroll_bar_allocate (ClutterActor          *actor,
                        const ClutterActorBox *box)
{
  StScrollBar *bar = ST_SCROLL_BAR (actor);

  clutter_actor_set_allocation (actor, box);

  scroll_bar_allocate_children (bar, box);
}

static void
scroll_bar_update_positions (StScrollBar *bar)
{
  ClutterActorBox box;

  /* Due to a change in the adjustments, we need to reposition our
   * children; since adjustments changes can come from allocation
   * changes in the scrolled area, we can't just queue a new relayout -
   * we may already be in a relayout cycle. On the other hand, if
   * a relayout is already queued, we can't just go ahead and allocate
   * our children, since we don't have a valid allocation, and calling
   * clutter_actor_get_allocation_box() will trigger an immediate
   * stage relayout. So what we do is go ahead and immediately
   * allocate our children if we already have a valid allocation, and
   * otherwise just wait for the queued relayout.
   */
  if (!clutter_actor_has_allocation (CLUTTER_ACTOR (bar)))
    return;

  clutter_actor_get_allocation_box (CLUTTER_ACTOR (bar), &box);
  scroll_bar_allocate_children (bar, &box);
}

static void
bar_reactive_notify_cb (GObject    *gobject,
                        GParamSpec *arg1,
                        gpointer    user_data)
{
  StScrollBar *bar = ST_SCROLL_BAR (gobject);
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (bar);

  clutter_actor_set_reactive (priv->handle,
                              clutter_actor_get_reactive (CLUTTER_ACTOR (bar)));
}

static GObject*
st_scroll_bar_constructor (GType                  type,
                           guint                  n_properties,
                           GObjectConstructParam *properties)
{
  GObjectClass *gobject_class;
  GObject *obj;
  StScrollBar *bar;

  gobject_class = G_OBJECT_CLASS (st_scroll_bar_parent_class);
  obj = gobject_class->constructor (type, n_properties, properties);

  bar  = ST_SCROLL_BAR (obj);

  g_signal_connect (bar, "notify::reactive",
                    G_CALLBACK (bar_reactive_notify_cb), NULL);

  return obj;
}

static void
adjust_with_direction (StAdjustment           *adj,
                       ClutterScrollDirection  direction)
{
  gdouble delta;

  switch (direction)
    {
    case CLUTTER_SCROLL_UP:
    case CLUTTER_SCROLL_LEFT:
      delta = -1.0;
      break;
    case CLUTTER_SCROLL_RIGHT:
    case CLUTTER_SCROLL_DOWN:
      delta = 1.0;
      break;
    case CLUTTER_SCROLL_SMOOTH:
    default:
      g_assert_not_reached ();
      break;
    }

  st_adjustment_adjust_for_scroll_event (adj, delta);
}

static gboolean
st_scroll_bar_scroll_event (ClutterActor *actor,
                            ClutterEvent *event)
{
  StScrollBarPrivate *priv = ST_SCROLL_BAR_PRIVATE (actor);
  ClutterTextDirection direction;
  ClutterScrollDirection scroll_dir;

  if (!!(clutter_event_get_flags (event) &
         CLUTTER_EVENT_FLAG_POINTER_EMULATED))
    return TRUE;

  direction = clutter_actor_get_text_direction (actor);
  scroll_dir = clutter_event_get_scroll_direction (event);

  switch (scroll_dir)
    {
    case CLUTTER_SCROLL_SMOOTH:
      {
        gdouble delta_x, delta_y;
        clutter_event_get_scroll_delta (event, &delta_x, &delta_y);

        if (direction == CLUTTER_TEXT_DIRECTION_RTL)
          delta_x *= -1;

        if (priv->orientation == CLUTTER_ORIENTATION_VERTICAL)
          st_adjustment_adjust_for_scroll_event (priv->adjustment, delta_y);
        else
          st_adjustment_adjust_for_scroll_event (priv->adjustment, delta_x);
      }
      break;
    case CLUTTER_SCROLL_LEFT:
    case CLUTTER_SCROLL_RIGHT:
      if (direction == CLUTTER_TEXT_DIRECTION_RTL)
          scroll_dir = scroll_dir == CLUTTER_SCROLL_LEFT ? CLUTTER_SCROLL_RIGHT
                                                         : CLUTTER_SCROLL_LEFT;
    /* Fall through */
    case CLUTTER_SCROLL_UP:
    case CLUTTER_SCROLL_DOWN:
      adjust_with_direction (priv->adjustment, scroll_dir);
      break;
    default:
      g_return_val_if_reached (FALSE);
      break;
    }

  return TRUE;
}

static void
st_scroll_bar_class_init (StScrollBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  object_class->get_property = st_scroll_bar_get_property;
  object_class->set_property = st_scroll_bar_set_property;
  object_class->dispose      = st_scroll_bar_dispose;
  object_class->constructor  = st_scroll_bar_constructor;

  actor_class->get_preferred_width  = st_scroll_bar_get_preferred_width;
  actor_class->get_preferred_height = st_scroll_bar_get_preferred_height;
  actor_class->allocate       = st_scroll_bar_allocate;
  actor_class->scroll_event   = st_scroll_bar_scroll_event;
  actor_class->unmap          = st_scroll_bar_unmap;

  /**
   * StScrollBar:adjustment:
   *
   * The #StAdjustment controlling the #StScrollBar.
   */
  props[PROP_ADJUSTMENT] =
    g_param_spec_object ("adjustment", NULL, NULL,
                         ST_TYPE_ADJUSTMENT,
                         ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StScrollBar:orientation:
   *
   *  The orientation of the #StScrollBar, horizontal or vertical.
   */
  props[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation", NULL, NULL,
                       CLUTTER_TYPE_ORIENTATION,
                       CLUTTER_ORIENTATION_HORIZONTAL,
                       ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, props);


  /**
   * StScrollBar::scroll-start:
   * @bar: a #StScrollBar
   *
   * Emitted when the #StScrollBar begins scrolling.
   */
  signals[SCROLL_START] =
    g_signal_new ("scroll-start",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (StScrollBarClass, scroll_start),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * StScrollBar::scroll-stop:
   * @bar: a #StScrollBar
   *
   * Emitted when the #StScrollBar finishes scrolling.
   */
  signals[SCROLL_STOP] =
    g_signal_new ("scroll-stop",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (StScrollBarClass, scroll_stop),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
move_slider (StScrollBar *bar,
             gfloat       x,
             gfloat       y)
{
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (bar);
  ClutterTextDirection direction;
  gboolean vertical;
  gdouble position, lower, upper, page_size;
  gfloat ux, uy, pos, size;

  if (!priv->adjustment)
    return;

  if (!clutter_actor_transform_stage_point (priv->trough, x, y, &ux, &uy))
    return;

  vertical = priv->orientation == CLUTTER_ORIENTATION_VERTICAL;

  if (vertical)
    size = clutter_actor_get_height (priv->trough)
           - clutter_actor_get_height (priv->handle);
  else
    size = clutter_actor_get_width (priv->trough)
           - clutter_actor_get_width (priv->handle);

  if (size == 0)
    return;

  if (vertical)
    pos = uy - priv->y_origin;
  else
    pos = ux - priv->x_origin;
  pos = CLAMP (pos, 0, size);

  st_adjustment_get_values (priv->adjustment,
                            NULL,
                            &lower,
                            &upper,
                            NULL,
                            NULL,
                            &page_size);

  direction = clutter_actor_get_text_direction (CLUTTER_ACTOR (bar));
  if (!vertical && direction == CLUTTER_TEXT_DIRECTION_RTL)
    pos = size - pos;

  position = ((pos / size)
              * (upper - lower - page_size))
             + lower;

  st_adjustment_set_value (priv->adjustment, position);
}

static gboolean
trough_paging_cb (StScrollBar *self)
{
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (self);
  ClutterTextDirection direction;
  gboolean vertical;
  g_autoptr (ClutterTransition) transition = NULL;
  StSettings *settings;
  gfloat handle_pos, event_pos, tx, ty;
  gdouble value, new_value;
  gdouble page_increment;
  gdouble slow_down_factor;
  gboolean ret;

  gulong mode;

  vertical = priv->orientation == CLUTTER_ORIENTATION_VERTICAL;

  if (priv->paging_event_no == 0)
    {
      /* Scroll on after initial timeout. */
      mode = CLUTTER_EASE_OUT_CUBIC;
      ret = FALSE;
      priv->paging_event_no = 1;
      priv->paging_source_id = g_timeout_add (
        PAGING_INITIAL_REPEAT_TIMEOUT,
        (GSourceFunc) trough_paging_cb,
        self);
      g_source_set_name_by_id (priv->paging_source_id, "[gnome-shell] trough_paging_cb");
    }
  else if (priv->paging_event_no == 1)
    {
      /* Scroll on after subsequent timeout. */
      ret = FALSE;
      mode = CLUTTER_EASE_IN_CUBIC;
      priv->paging_event_no = 2;
      priv->paging_source_id = g_timeout_add (
        PAGING_SUBSEQUENT_REPEAT_TIMEOUT,
        (GSourceFunc) trough_paging_cb,
        self);
      g_source_set_name_by_id (priv->paging_source_id, "[gnome-shell] trough_paging_cb");
    }
  else
    {
      /* Keep scrolling. */
      ret = TRUE;
      mode = CLUTTER_LINEAR;
      priv->paging_event_no++;
    }

  /* Do the scrolling */
  st_adjustment_get_values (priv->adjustment,
                            &value, NULL, NULL,
                            NULL, &page_increment, NULL);

  if (vertical)
    handle_pos = clutter_actor_get_y (priv->handle);
  else
    handle_pos = clutter_actor_get_x (priv->handle);

  clutter_actor_transform_stage_point (CLUTTER_ACTOR (priv->trough),
                                       priv->move_x,
                                       priv->move_y,
                                       &tx, &ty);

  direction = clutter_actor_get_text_direction (CLUTTER_ACTOR (self));
  if (!vertical && direction == CLUTTER_TEXT_DIRECTION_RTL)
    page_increment *= -1;

  if (vertical)
    event_pos = ty;
  else
    event_pos = tx;

  if (event_pos > handle_pos)
    {
      if (priv->paging_direction == NONE)
        {
          /* Remember direction. */
          priv->paging_direction = DOWN;
        }
      if (priv->paging_direction == UP)
        {
          /* Scrolled far enough. */
          return FALSE;
        }
      new_value = value + page_increment;
    }
  else
    {
      if (priv->paging_direction == NONE)
        {
          /* Remember direction. */
          priv->paging_direction = UP;
        }
      if (priv->paging_direction == DOWN)
        {
          /* Scrolled far enough. */
          return FALSE;
        }
      new_value = value - page_increment;
    }

  /* Stop existing transition, if one exists */
  st_adjustment_remove_transition (priv->adjustment, "value");

  settings = st_settings_get ();
  g_object_get (settings, "slow-down-factor", &slow_down_factor, NULL);

  /* FIXME: Creating a new transition for each scroll is probably not the best
  * idea, but it's a lot less involved than extending the current animation */
  transition = g_object_new (CLUTTER_TYPE_PROPERTY_TRANSITION,
                             "property-name", "value",
                             "interval", clutter_interval_new (G_TYPE_DOUBLE, value, new_value),
                             "duration", (guint)(PAGING_SUBSEQUENT_REPEAT_TIMEOUT * slow_down_factor),
                             "progress-mode", mode,
                             "remove-on-complete", TRUE,
                             NULL);
  st_adjustment_add_transition (priv->adjustment, "value", transition);

  return ret;
}

static void
trough_pan_recognize_cb (ClutterPanGesture *pan_gesture,
                         StScrollBar       *self)
{
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (self);
  graphene_point_t centroid;

  if (!priv->adjustment)
    return;

  clutter_pan_gesture_get_centroid_abs (pan_gesture, &centroid);

  priv->move_x = centroid.x;
  priv->move_y = centroid.y;
  priv->paging_direction = NONE;
  priv->paging_event_no = 0;
  trough_paging_cb (self);
}

static void
trough_pan_end_cb (ClutterPanGesture *pan_gesture,
                   StScrollBar       *self)
{
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (self);

  g_clear_handle_id (&priv->paging_source_id, g_source_remove);
}

static void
trough_pan_cancel_cb (ClutterPanGesture *pan_gesture,
                      StScrollBar       *self)
{
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (self);

  g_clear_handle_id (&priv->paging_source_id, g_source_remove);
}

static void
handle_pan_recognize_cb (ClutterPanGesture *pan_gesture,
                         StScrollBar       *self)
{
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (self);
  graphene_point_t centroid;

  clutter_pan_gesture_get_centroid_abs (pan_gesture, &centroid);
  if (!clutter_actor_transform_stage_point (priv->handle, centroid.x, centroid.y, &centroid.x, &centroid.y))
    return;

  priv->x_origin = centroid.x;
  priv->y_origin = centroid.y;

  st_widget_add_style_pseudo_class (ST_WIDGET (priv->handle), "active");

  /* Account for the scrollbar-trough-handle nesting. */
  priv->x_origin += clutter_actor_get_x (priv->trough);
  priv->y_origin += clutter_actor_get_y (priv->trough);

  g_signal_emit (self, signals[SCROLL_START], 0);
}

static void
handle_pan_update_cb (ClutterPanGesture *pan_gesture,
                      StScrollBar       *self)
{
  graphene_point_t centroid;

  clutter_pan_gesture_get_centroid_abs (pan_gesture, &centroid);
  move_slider (self, centroid.x, centroid.y);
}

static void
handle_pan_end_cb (ClutterPanGesture *pan_gesture,
                   StScrollBar       *self)
{
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (self);

  st_widget_remove_style_pseudo_class (ST_WIDGET (priv->handle), "active");
  g_signal_emit (self, signals[SCROLL_STOP], 0);
}

static void
handle_pan_cancel_cb (ClutterPanGesture *pan_gesture,
                      StScrollBar       *self)
{
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (self);

  st_widget_remove_style_pseudo_class (ST_WIDGET (priv->handle), "active");
  g_signal_emit (self, signals[SCROLL_STOP], 0);
}

static void
st_scroll_bar_notify_reactive (StScrollBar *self)
{
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (self);

  gboolean reactive = clutter_actor_get_reactive (CLUTTER_ACTOR (self));

  clutter_actor_set_reactive (CLUTTER_ACTOR (priv->trough), reactive);
  clutter_actor_set_reactive (CLUTTER_ACTOR (priv->handle), reactive);
}

static void
st_scroll_bar_init (StScrollBar *self)
{
  StScrollBarPrivate *priv = st_scroll_bar_get_instance_private (self);

  priv->trough = (ClutterActor *) st_bin_new ();
  clutter_actor_set_reactive ((ClutterActor *) priv->trough, TRUE);
  clutter_actor_set_name (CLUTTER_ACTOR (priv->trough), "trough");
  clutter_actor_add_child (CLUTTER_ACTOR (self),
                           CLUTTER_ACTOR (priv->trough));

  priv->trough_pan_gesture = CLUTTER_PAN_GESTURE (clutter_pan_gesture_new ());
  clutter_pan_gesture_set_begin_threshold (priv->trough_pan_gesture, 0);
  clutter_actor_meta_set_name (CLUTTER_ACTOR_META (priv->trough_pan_gesture),
                               "StScrollBar trough pan");

  g_signal_connect (priv->trough_pan_gesture, "recognize",
                    G_CALLBACK (trough_pan_recognize_cb), self);
  g_signal_connect (priv->trough_pan_gesture, "end",
                    G_CALLBACK (trough_pan_end_cb), self);
  g_signal_connect (priv->trough_pan_gesture, "cancel",
                    G_CALLBACK (trough_pan_cancel_cb), self);

  clutter_actor_add_action (CLUTTER_ACTOR (priv->trough), CLUTTER_ACTION (priv->trough_pan_gesture));

  priv->handle = (ClutterActor *) g_object_new (ST_TYPE_WIDGET, NULL);
  st_widget_set_track_hover (ST_WIDGET (priv->handle), TRUE);

  st_widget_set_style_class_name (ST_WIDGET (priv->handle), "hhandle");
  clutter_actor_add_child (CLUTTER_ACTOR (self),
                           CLUTTER_ACTOR (priv->handle));

  priv->handle_pan_gesture = CLUTTER_PAN_GESTURE (clutter_pan_gesture_new ());
  clutter_pan_gesture_set_begin_threshold (priv->handle_pan_gesture, 0);
  clutter_actor_meta_set_name (CLUTTER_ACTOR_META (priv->handle_pan_gesture),
                               "StScrollBar handle pan");

  g_signal_connect (priv->handle_pan_gesture, "recognize",
                    G_CALLBACK (handle_pan_recognize_cb), self);
  g_signal_connect (priv->handle_pan_gesture, "pan-update",
                    G_CALLBACK (handle_pan_update_cb), self);
  g_signal_connect (priv->handle_pan_gesture, "end",
                    G_CALLBACK (handle_pan_end_cb), self);
  g_signal_connect (priv->handle_pan_gesture, "cancel",
                    G_CALLBACK (handle_pan_cancel_cb), self);

  clutter_actor_add_action (CLUTTER_ACTOR (priv->handle), CLUTTER_ACTION (priv->handle_pan_gesture));

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);

  g_signal_connect (self, "notify::reactive",
                    G_CALLBACK (st_scroll_bar_notify_reactive), NULL);
}

StWidget *
st_scroll_bar_new (StAdjustment *adjustment)
{
  return g_object_new (ST_TYPE_SCROLL_BAR,
                       "adjustment", adjustment,
                       NULL);
}

static void
on_notify_value (GObject     *object,
                 GParamSpec  *pspec,
                 StScrollBar *bar)
{
  scroll_bar_update_positions (bar);
}

static void
on_changed (StAdjustment *adjustment,
            StScrollBar  *bar)
{
  scroll_bar_update_positions (bar);
}

void
st_scroll_bar_set_adjustment (StScrollBar  *bar,
                              StAdjustment *adjustment)
{
  StScrollBarPrivate *priv;

  g_return_if_fail (ST_IS_SCROLL_BAR (bar));

  priv = st_scroll_bar_get_instance_private (bar);

  if (adjustment == priv->adjustment)
    return;

  if (priv->adjustment)
    {
      g_signal_handlers_disconnect_by_func (priv->adjustment,
                                            on_notify_value,
                                            bar);
      g_signal_handlers_disconnect_by_func (priv->adjustment,
                                            on_changed,
                                            bar);
      g_object_unref (priv->adjustment);
      priv->adjustment = NULL;
    }

  if (adjustment)
    {
      priv->adjustment = g_object_ref (adjustment);

      g_signal_connect (priv->adjustment, "notify::value",
                        G_CALLBACK (on_notify_value),
                        bar);
      g_signal_connect (priv->adjustment, "changed",
                        G_CALLBACK (on_changed),
                        bar);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (bar));
    }

  g_object_notify_by_pspec (G_OBJECT (bar), props[PROP_ADJUSTMENT]);
}

/**
 * st_scroll_bar_get_adjustment:
 * @bar: a #StScrollbar
 *
 * Gets the #StAdjustment that controls the current position of @bar.
 *
 * Returns: (transfer none): an #StAdjustment
 */
StAdjustment *
st_scroll_bar_get_adjustment (StScrollBar *bar)
{
  g_return_val_if_fail (ST_IS_SCROLL_BAR (bar), NULL);

  return ((StScrollBarPrivate *)ST_SCROLL_BAR_PRIVATE (bar))->adjustment;
}

