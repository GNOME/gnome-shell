/*
 * st-adjustment.c: Adjustment object
 *
 * Copyright (C) 2008 OpenedHand
 * Copyright (c) 2009 Intel Corporation.
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Written by: Chris Lord <chris@openedhand.com>, inspired by GtkAdjustment
 * Port to St by: Robert Staudinger <robsta@openedhand.com>
 *
 */

/**
 * SECTION:st-adjustment
 * @short_description: A GObject representing an adjustable bounded value
 *
 * The #StAdjustment object represents a range of values bounded between a
 * minimum and maximum, together with step and page increments and a page size.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <clutter/clutter.h>

#include "st-adjustment.h"
#include "st-marshal.h"
#include "st-private.h"

G_DEFINE_TYPE (StAdjustment, st_adjustment, G_TYPE_OBJECT)

#define ADJUSTMENT_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ST_TYPE_ADJUSTMENT, StAdjustmentPrivate))

struct _StAdjustmentPrivate
{
  /* Do not sanity-check values while constructing,
   * not all properties may be set yet. */
  gboolean is_constructing : 1;

  gdouble  lower;
  gdouble  upper;
  gdouble  value;
  gdouble  step_increment;
  gdouble  page_increment;
  gdouble  page_size;

  /* For interpolation */
  ClutterTimeline *interpolation;
  gdouble          old_position;
  gdouble          new_position;

  /* For elasticity */
  gboolean      elastic;
  guint         bounce_source;
  ClutterAlpha *bounce_alpha;
};

enum
{
  PROP_0,

  PROP_LOWER,
  PROP_UPPER,
  PROP_VALUE,
  PROP_STEP_INC,
  PROP_PAGE_INC,
  PROP_PAGE_SIZE,

  PROP_ELASTIC,
};

enum
{
  CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static gboolean st_adjustment_set_lower          (StAdjustment *adjustment,
                                                  gdouble       lower);
static gboolean st_adjustment_set_upper          (StAdjustment *adjustment,
                                                  gdouble       upper);
static gboolean st_adjustment_set_step_increment (StAdjustment *adjustment,
                                                  gdouble       step);
static gboolean st_adjustment_set_page_increment (StAdjustment *adjustment,
                                                  gdouble       page);
static gboolean st_adjustment_set_page_size      (StAdjustment *adjustment,
                                                  gdouble       size);

static void
st_adjustment_constructed (GObject *object)
{
  GObjectClass *g_class;
  StAdjustment *self = ST_ADJUSTMENT (object);

  g_class = G_OBJECT_CLASS (st_adjustment_parent_class);
  /* The docs say we're suppose to chain up, but would crash without
   * some extra care. */
  if (g_class && g_class->constructed &&
      g_class->constructed != st_adjustment_constructed)
    {
      g_class->constructed (object);
    }

  ST_ADJUSTMENT (self)->priv->is_constructing = FALSE;
  st_adjustment_clamp_page (self, self->priv->lower, self->priv->upper);
}

static void
st_adjustment_get_property (GObject    *gobject,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  StAdjustmentPrivate *priv = ST_ADJUSTMENT (gobject)->priv;

  switch (prop_id)
    {
    case PROP_LOWER:
      g_value_set_double (value, priv->lower);
      break;

    case PROP_UPPER:
      g_value_set_double (value, priv->upper);
      break;

    case PROP_VALUE:
      g_value_set_double (value, priv->value);
      break;

    case PROP_STEP_INC:
      g_value_set_double (value, priv->step_increment);
      break;

    case PROP_PAGE_INC:
      g_value_set_double (value, priv->page_increment);
      break;

    case PROP_PAGE_SIZE:
      g_value_set_double (value, priv->page_size);
      break;

    case PROP_ELASTIC:
      g_value_set_boolean (value, priv->elastic);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_adjustment_set_property (GObject      *gobject,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  StAdjustment *adj = ST_ADJUSTMENT (gobject);

  switch (prop_id)
    {
    case PROP_LOWER:
      st_adjustment_set_lower (adj, g_value_get_double (value));
      break;

    case PROP_UPPER:
      st_adjustment_set_upper (adj, g_value_get_double (value));
      break;

    case PROP_VALUE:
      st_adjustment_set_value (adj, g_value_get_double (value));
      break;

    case PROP_STEP_INC:
      st_adjustment_set_step_increment (adj, g_value_get_double (value));
      break;

    case PROP_PAGE_INC:
      st_adjustment_set_page_increment (adj, g_value_get_double (value));
      break;

    case PROP_PAGE_SIZE:
      st_adjustment_set_page_size (adj, g_value_get_double (value));
      break;

    case PROP_ELASTIC:
      st_adjustment_set_elastic (adj, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
stop_interpolation (StAdjustment *adjustment)
{
  StAdjustmentPrivate *priv = adjustment->priv;

  if (priv->interpolation)
    {
      clutter_timeline_stop (priv->interpolation);
      g_object_unref (priv->interpolation);
      priv->interpolation = NULL;

      if (priv->bounce_alpha)
        {
          g_object_unref (priv->bounce_alpha);
          priv->bounce_alpha = NULL;
        }
    }

  if (priv->bounce_source)
    {
      g_source_remove (priv->bounce_source);
      priv->bounce_source = 0;
    }
}

static void
st_adjustment_dispose (GObject *object)
{
  stop_interpolation (ST_ADJUSTMENT (object));

  G_OBJECT_CLASS (st_adjustment_parent_class)->dispose (object);
}

static void
st_adjustment_class_init (StAdjustmentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (StAdjustmentPrivate));

  object_class->constructed = st_adjustment_constructed;
  object_class->get_property = st_adjustment_get_property;
  object_class->set_property = st_adjustment_set_property;
  object_class->dispose = st_adjustment_dispose;

  g_object_class_install_property (object_class,
                                   PROP_LOWER,
                                   g_param_spec_double ("lower",
                                                        "Lower",
                                                        "Lower bound",
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        ST_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_UPPER,
                                   g_param_spec_double ("upper",
                                                        "Upper",
                                                        "Upper bound",
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        ST_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_VALUE,
                                   g_param_spec_double ("value",
                                                        "Value",
                                                        "Current value",
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        ST_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_STEP_INC,
                                   g_param_spec_double ("step-increment",
                                                        "Step Increment",
                                                        "Step increment",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        ST_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_PAGE_INC,
                                   g_param_spec_double ("page-increment",
                                                        "Page Increment",
                                                        "Page increment",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        ST_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_PAGE_SIZE,
                                   g_param_spec_double ("page-size",
                                                        "Page Size",
                                                        "Page size",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        ST_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_ELASTIC,
                                   g_param_spec_boolean ("elastic",
                                                         "Elastic",
                                                         "Make interpolation "
                                                         "behave in an "
                                                         "'elastic' way and "
                                                         "stop clamping value.",
                                                         FALSE,
                                                         ST_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT));

  /**
   * StAdjustment::changed:
   *
   * Emitted when any of the adjustment values have changed
   */
  signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (StAdjustmentClass, changed),
                  NULL, NULL,
                  _st_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
st_adjustment_init (StAdjustment *self)
{
  self->priv = ADJUSTMENT_PRIVATE (self);

  self->priv->is_constructing = TRUE;
}

StAdjustment *
st_adjustment_new (gdouble value,
                   gdouble lower,
                   gdouble upper,
                   gdouble step_increment,
                   gdouble page_increment,
                   gdouble page_size)
{
  return g_object_new (ST_TYPE_ADJUSTMENT,
                       "value", value,
                       "lower", lower,
                       "upper", upper,
                       "step-increment", step_increment,
                       "page-increment", page_increment,
                       "page-size", page_size,
                       NULL);
}

gdouble
st_adjustment_get_value (StAdjustment *adjustment)
{
  StAdjustmentPrivate *priv;

  g_return_val_if_fail (ST_IS_ADJUSTMENT (adjustment), 0);

  priv = adjustment->priv;

  if (priv->interpolation)
    {
      return MAX (priv->lower,
                  MIN (priv->upper - priv->page_size,
                       priv->new_position));
    }
  else
    return priv->value;
}

void
st_adjustment_set_value (StAdjustment *adjustment,
                         gdouble       value)
{
  StAdjustmentPrivate *priv;

  g_return_if_fail (ST_IS_ADJUSTMENT (adjustment));

  priv = adjustment->priv;

  stop_interpolation (adjustment);

  /* Defer clamp until after construction. */
  if (!priv->is_constructing)
    {
      if (!priv->elastic)
        value = CLAMP (value,
                       priv->lower,
                       MAX (priv->lower, priv->upper - priv->page_size));
    }

  if (priv->value != value)
    {
      priv->value = value;

      g_object_notify (G_OBJECT (adjustment), "value");
    }
}

void
st_adjustment_clamp_page (StAdjustment *adjustment,
                          gdouble       lower,
                          gdouble       upper)
{
  StAdjustmentPrivate *priv;
  gboolean changed;

  g_return_if_fail (ST_IS_ADJUSTMENT (adjustment));

  priv = adjustment->priv;

  stop_interpolation (adjustment);

  lower = CLAMP (lower, priv->lower, priv->upper - priv->page_size);
  upper = CLAMP (upper, priv->lower + priv->page_size, priv->upper);

  changed = FALSE;

  if (priv->value + priv->page_size > upper)
    {
      priv->value = upper - priv->page_size;
      changed = TRUE;
    }

  if (priv->value < lower)
    {
      priv->value = lower;
      changed = TRUE;
    }

  if (changed)
    g_object_notify (G_OBJECT (adjustment), "value");
}

static gboolean
st_adjustment_set_lower (StAdjustment *adjustment,
                         gdouble       lower)
{
  StAdjustmentPrivate *priv = adjustment->priv;

  if (priv->lower != lower)
    {
      priv->lower = lower;

      g_signal_emit (adjustment, signals[CHANGED], 0);

      g_object_notify (G_OBJECT (adjustment), "lower");

      /* Defer clamp until after construction. */
      if (!priv->is_constructing)
        st_adjustment_clamp_page (adjustment, priv->lower, priv->upper);

      return TRUE;
    }

  return FALSE;
}

static gboolean
st_adjustment_set_upper (StAdjustment *adjustment,
                         gdouble       upper)
{
  StAdjustmentPrivate *priv = adjustment->priv;

  if (priv->upper != upper)
    {
      priv->upper = upper;

      g_signal_emit (adjustment, signals[CHANGED], 0);

      g_object_notify (G_OBJECT (adjustment), "upper");

      /* Defer clamp until after construction. */
      if (!priv->is_constructing)
        st_adjustment_clamp_page (adjustment, priv->lower, priv->upper);

      return TRUE;
    }

  return FALSE;
}

static gboolean
st_adjustment_set_step_increment (StAdjustment *adjustment,
                                  gdouble       step)
{
  StAdjustmentPrivate *priv = adjustment->priv;

  if (priv->step_increment != step)
    {
      priv->step_increment = step;

      g_signal_emit (adjustment, signals[CHANGED], 0);

      g_object_notify (G_OBJECT (adjustment), "step-increment");

      return TRUE;
    }

  return FALSE;
}

static gboolean
st_adjustment_set_page_increment (StAdjustment *adjustment,
                                  gdouble       page)
{
  StAdjustmentPrivate *priv = adjustment->priv;

  if (priv->page_increment != page)
    {
      priv->page_increment = page;

      g_signal_emit (adjustment, signals[CHANGED], 0);

      g_object_notify (G_OBJECT (adjustment), "page-increment");

      return TRUE;
    }

  return FALSE;
}

static gboolean
st_adjustment_set_page_size (StAdjustment *adjustment,
                             gdouble       size)
{
  StAdjustmentPrivate *priv = adjustment->priv;

  if (priv->page_size != size)
    {
      priv->page_size = size;

      g_signal_emit (adjustment, signals[CHANGED], 0);

      g_object_notify (G_OBJECT (adjustment), "page_size");

      /* Well explicitely clamp after construction. */
      if (!priv->is_constructing)
        st_adjustment_clamp_page (adjustment, priv->lower, priv->upper);

      return TRUE;
    }

  return FALSE;
}

void
st_adjustment_set_values (StAdjustment *adjustment,
                          gdouble       value,
                          gdouble       lower,
                          gdouble       upper,
                          gdouble       step_increment,
                          gdouble       page_increment,
                          gdouble       page_size)
{
  StAdjustmentPrivate *priv;
  gboolean emit_changed = FALSE;

  g_return_if_fail (ST_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (page_size >= 0 && page_size <= G_MAXDOUBLE);
  g_return_if_fail (step_increment >= 0 && step_increment <= G_MAXDOUBLE);
  g_return_if_fail (page_increment >= 0 && page_increment <= G_MAXDOUBLE);

  priv = adjustment->priv;

  stop_interpolation (adjustment);

  emit_changed = FALSE;

  g_object_freeze_notify (G_OBJECT (adjustment));

  emit_changed |= st_adjustment_set_lower (adjustment, lower);
  emit_changed |= st_adjustment_set_upper (adjustment, upper);
  emit_changed |= st_adjustment_set_step_increment (adjustment, step_increment);
  emit_changed |= st_adjustment_set_page_increment (adjustment, page_increment);
  emit_changed |= st_adjustment_set_page_size (adjustment, page_size);

  if (value != priv->value)
    {
      st_adjustment_set_value (adjustment, value);
      emit_changed = TRUE;
    }

  if (emit_changed)
    g_signal_emit (G_OBJECT (adjustment), signals[CHANGED], 0);

  g_object_thaw_notify (G_OBJECT (adjustment));
}

void
st_adjustment_get_values (StAdjustment *adjustment,
                          gdouble      *value,
                          gdouble      *lower,
                          gdouble      *upper,
                          gdouble      *step_increment,
                          gdouble      *page_increment,
                          gdouble      *page_size)
{
  StAdjustmentPrivate *priv;

  g_return_if_fail (ST_IS_ADJUSTMENT (adjustment));

  priv = adjustment->priv;

  if (lower)
    *lower = priv->lower;

  if (upper)
    *upper = priv->upper;

  if (value)
    *value = st_adjustment_get_value (adjustment);

  if (step_increment)
    *step_increment = priv->step_increment;

  if (page_increment)
    *page_increment = priv->page_increment;

  if (page_size)
    *page_size = priv->page_size;
}

static void
interpolation_new_frame_cb (ClutterTimeline *timeline,
                            guint            msecs,
                            StAdjustment    *adjustment)
{
  StAdjustmentPrivate *priv = adjustment->priv;

  priv->interpolation = NULL;

  if (priv->elastic)
    {
      gdouble progress = clutter_alpha_get_alpha (priv->bounce_alpha) / 1.0;
      gdouble dx = priv->old_position
                   + (priv->new_position - priv->old_position)
                   * progress;

      st_adjustment_set_value (adjustment, dx);
    }
  else
    st_adjustment_set_value (adjustment,
                             priv->old_position +
                             (priv->new_position - priv->old_position) *
                             clutter_timeline_get_progress (timeline));

  priv->interpolation = timeline;
}

static void
interpolation_completed_cb (ClutterTimeline *timeline,
                            StAdjustment    *adjustment)
{
  StAdjustmentPrivate *priv = adjustment->priv;

  stop_interpolation (adjustment);
  st_adjustment_set_value (adjustment, priv->new_position);
}

/* Note, there's super-optimal code that does a similar thing in
 * clutter-alpha.c
 *
 * Tried this instead of CLUTTER_ALPHA_SINE_INC, but I think SINE_INC looks
 * better. Leaving code here in case this is revisited.
 */
/*
   static guint32
   bounce_alpha_func (ClutterAlpha *alpha,
                   gpointer      user_data)
   {
   ClutterFixed progress, angle;
   ClutterTimeline *timeline = clutter_alpha_get_timeline (alpha);

   progress = clutter_timeline_get_progressx (timeline);
   angle = clutter_qmulx (CFX_PI_2 + CFX_PI_4/2, progress);

   return clutter_sinx (angle) +
    (CFX_ONE - clutter_sinx (CFX_PI_2 + CFX_PI_4/2));
   }
 */

void
st_adjustment_interpolate (StAdjustment *adjustment,
                           gdouble       value,
                           guint         duration)
{
  StAdjustmentPrivate *priv = adjustment->priv;

  stop_interpolation (adjustment);

  if (duration <= 1)
    {
      st_adjustment_set_value (adjustment, value);
      return;
    }

  priv->old_position = priv->value;
  priv->new_position = value;

  priv->interpolation = clutter_timeline_new (duration);

  if (priv->elastic)
    priv->bounce_alpha = clutter_alpha_new_full (priv->interpolation,
                                                 CLUTTER_LINEAR);

  g_signal_connect (priv->interpolation,
                    "new-frame",
                    G_CALLBACK (interpolation_new_frame_cb),
                    adjustment);
  g_signal_connect (priv->interpolation,
                    "completed",
                    G_CALLBACK (interpolation_completed_cb),
                    adjustment);

  clutter_timeline_start (priv->interpolation);
}

gboolean
st_adjustment_get_elastic (StAdjustment *adjustment)
{
  return adjustment->priv->elastic;
}

void
st_adjustment_set_elastic (StAdjustment *adjustment,
                           gboolean      elastic)
{
  adjustment->priv->elastic = elastic;
}

gboolean
st_adjustment_clamp (StAdjustment *adjustment,
                     gboolean      interpolate,
                     guint         duration)
{
  StAdjustmentPrivate *priv = adjustment->priv;
  gdouble dest = priv->value;

  if (priv->value < priv->lower)
    dest = priv->lower;

  if (priv->value > priv->upper - priv->page_size)
    dest = priv->upper - priv->page_size;

  if (dest != priv->value)
    {
      if (interpolate)
        st_adjustment_interpolate (adjustment, dest, duration);
      else
        st_adjustment_set_value (adjustment, dest);

      return TRUE;
    }

  return FALSE;
}
