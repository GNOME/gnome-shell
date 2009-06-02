/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2008  Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-interval
 * @short_description: An object holding an interval of two values
 *
 * #ClutterInterval is a simple object that can hold two values
 * defining an interval. #ClutterInterval can hold any value that
 * can be enclosed inside a #GValue.
 *
 * Once a #ClutterInterval for a specific #GType has been instantiated
 * the #ClutterInterval:value-type property cannot be changed anymore.
 *
 * #ClutterInterval starts with a floating reference; this means that
 * any object taking a reference on a #ClutterInterval instance should
 * also take ownership of the interval by using g_object_ref_sink().
 *
 * #ClutterInterval is used by #ClutterAnimation to define the
 * interval of values that an implicit animation should tween over.
 *
 * #ClutterInterval can be subclassed to override the validation
 * and value computation.
 *
 * #ClutterInterval is available since Clutter 1.0
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-color.h"
#include "clutter-fixed.h"
#include "clutter-interval.h"
#include "clutter-units.h"

typedef struct
{
  GType value_type;
  ClutterProgressFunc func;
} ProgressData;

static GHashTable *progress_funcs = NULL;

enum
{
  PROP_0,

  PROP_VALUE_TYPE
};

#define CLUTTER_INTERVAL_GET_PRIVATE(obj)  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_INTERVAL, ClutterIntervalPrivate))

struct _ClutterIntervalPrivate
{
  GType value_type;

  GValue *values;
};

G_DEFINE_TYPE (ClutterInterval, clutter_interval, G_TYPE_INITIALLY_UNOWNED);

static gboolean
clutter_interval_real_validate (ClutterInterval *interval,
                                GParamSpec      *pspec)
{
  GType pspec_gtype = G_PARAM_SPEC_VALUE_TYPE (pspec);

  /* check the GTypes we provide first */
  if (pspec_gtype == COGL_TYPE_FIXED)
    {
      ClutterParamSpecFixed *pspec_fixed = CLUTTER_PARAM_SPEC_FIXED (pspec);
      CoglFixed a, b;

      a = b = 0;
      clutter_interval_get_interval (interval, &a, &b);
      if ((a >= pspec_fixed->minimum && a <= pspec_fixed->maximum) &&
          (b >= pspec_fixed->minimum && b <= pspec_fixed->maximum))
        return TRUE;
      else
        return FALSE;
    }

  /* then check the fundamental types */
  switch (G_TYPE_FUNDAMENTAL (pspec_gtype))
    {
    case G_TYPE_INT:
      {
        GParamSpecInt *pspec_int = G_PARAM_SPEC_INT (pspec);
        gint a, b;

        a = b = 0;
        clutter_interval_get_interval (interval, &a, &b);
        if ((a >= pspec_int->minimum && a <= pspec_int->maximum) &&
            (b >= pspec_int->minimum && b <= pspec_int->maximum))
          return TRUE;
        else
          return FALSE;
      }
      break;

    case G_TYPE_UINT:
      {
        GParamSpecUInt *pspec_uint = G_PARAM_SPEC_UINT (pspec);
        guint a, b;

        a = b = 0;
        clutter_interval_get_interval (interval, &a, &b);
        if ((a >= pspec_uint->minimum && a <= pspec_uint->maximum) &&
            (b >= pspec_uint->minimum && b <= pspec_uint->maximum))
          return TRUE;
        else
          return FALSE;
      }
      break;

    case G_TYPE_UCHAR:
      {
        GParamSpecUChar *pspec_uchar = G_PARAM_SPEC_UCHAR (pspec);
        guchar a, b;

        a = b = 0;
        clutter_interval_get_interval (interval, &a, &b);
        if ((a >= pspec_uchar->minimum && a <= pspec_uchar->maximum) &&
            (b >= pspec_uchar->minimum && b <= pspec_uchar->maximum))
          return TRUE;
        else
          return FALSE;
      }
      break;

    case G_TYPE_BOOLEAN:
      return TRUE;

    default:
      break;
    }

  return TRUE;
}

static gboolean
clutter_interval_real_compute_value (ClutterInterval *interval,
                                     gdouble          factor,
                                     GValue          *value)
{
  GValue *initial, *final;
  GType value_type;
  gboolean retval = FALSE;

  initial = clutter_interval_peek_initial_value (interval);
  final = clutter_interval_peek_final_value (interval);

  value_type = clutter_interval_get_value_type (interval);

  if (G_UNLIKELY (progress_funcs != NULL))
    {
      ProgressData *p_data;

      p_data =
        g_hash_table_lookup (progress_funcs, GUINT_TO_POINTER (value_type));

      /* if we have a progress function, and that function was
       * successful in computing the progress, then we bail out
       * as fast as we can
       */
      if (p_data != NULL)
        {
          retval = p_data->func (initial, final, factor, value);
          if (retval)
            return retval;
        }
    }

  switch (G_TYPE_FUNDAMENTAL (value_type))
    {
    case G_TYPE_INT:
      {
        gint ia, ib, res;

        ia = g_value_get_int (initial);
        ib = g_value_get_int (final);

        res = (factor * (ib - ia)) + ia;

        g_value_set_int (value, res);

        retval = TRUE;
      }
      break;

    case G_TYPE_UINT:
      {
        guint ia, ib, res;

        ia = g_value_get_uint (initial);
        ib = g_value_get_uint (final);

        res = (factor * (ib - (gdouble) ia)) + ia;

        g_value_set_uint (value, res);

        retval = TRUE;
      }
      break;

    case G_TYPE_UCHAR:
      {
        guchar ia, ib, res;

        ia = g_value_get_uchar (initial);
        ib = g_value_get_uchar (final);

        res = (factor * (ib - (gdouble) ia)) + ia;

        g_value_set_uchar (value, res);

        retval = TRUE;
      }
      break;

    case G_TYPE_FLOAT:
    case G_TYPE_DOUBLE:
      {
        gdouble ia, ib, res;

        if (value_type == G_TYPE_DOUBLE)
          {
            ia = g_value_get_double (initial);
            ib = g_value_get_double (final);
          }
        else
          {
            ia = g_value_get_float (initial);
            ib = g_value_get_float (final);
          }

        res = (factor * (ib - ia)) + ia;

        if (value_type == G_TYPE_DOUBLE)
          g_value_set_double (value, res);
        else
          g_value_set_float (value, res);

        retval = TRUE;
      }
      break;

    case G_TYPE_BOOLEAN:
      if (factor > 0.5)
        g_value_set_boolean (value, TRUE);
      else
        g_value_set_boolean (value, FALSE);

      retval = TRUE;
      break;

    case G_TYPE_BOXED:
      if (value_type == CLUTTER_TYPE_COLOR)
        {
          const ClutterColor *ia, *ib;
          ClutterColor res = { 0, };

          ia = clutter_value_get_color (initial);
          ib = clutter_value_get_color (final);

          res.red   = (factor * (ib->red   - (gdouble) ia->red))   + ia->red;
          res.green = (factor * (ib->green - (gdouble) ia->green)) + ia->green;
          res.blue  = (factor * (ib->blue  - (gdouble) ia->blue))  + ia->blue;
          res.alpha = (factor * (ib->alpha - (gdouble) ia->alpha)) + ia->alpha;

          clutter_value_set_color (value, &res);

          retval = TRUE;
        }
      break;

    default:
      break;
    }

  return retval;
}

static void
clutter_interval_finalize (GObject *gobject)
{
  ClutterIntervalPrivate *priv = CLUTTER_INTERVAL (gobject)->priv;

  g_value_unset (&priv->values[0]);
  g_value_unset (&priv->values[1]);

  g_free (priv->values);
}

static void
clutter_interval_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ClutterIntervalPrivate *priv = CLUTTER_INTERVAL_GET_PRIVATE (gobject);

  switch (prop_id)
    {
    case PROP_VALUE_TYPE:
      priv->value_type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_interval_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ClutterIntervalPrivate *priv = CLUTTER_INTERVAL_GET_PRIVATE (gobject);

  switch (prop_id)
    {
    case PROP_VALUE_TYPE:
      g_value_set_gtype (value, priv->value_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_interval_class_init (ClutterIntervalClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (ClutterIntervalPrivate));

  klass->validate = clutter_interval_real_validate;
  klass->compute_value = clutter_interval_real_compute_value;

  gobject_class->set_property = clutter_interval_set_property,
  gobject_class->get_property = clutter_interval_get_property;
  gobject_class->finalize = clutter_interval_finalize;

  /**
   * ClutterInterval:value-type:
   *
   * The type of the values in the interval.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_gtype ("value-type",
                              "Value Type",
                              "The type of the values in the interval",
                              G_TYPE_NONE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_VALUE_TYPE, pspec);
}

static void
clutter_interval_init (ClutterInterval *self)
{
  ClutterIntervalPrivate *priv;

  self->priv = priv = CLUTTER_INTERVAL_GET_PRIVATE (self);

  priv->value_type = G_TYPE_INVALID;
  priv->values = g_malloc0 (sizeof (GValue) * 2);
}

static void
clutter_interval_set_interval_valist (ClutterInterval *interval,
                                      va_list          var_args)
{
  GType gtype = interval->priv->value_type;
  GValue value = { 0, };
  gchar *error;

  /* initial value */
  g_value_init (&value, gtype);
  G_VALUE_COLLECT (&value, var_args, 0, &error);
  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error);

      /* we leak the value here as it might not be in a valid state
       * given the error and calling g_value_unset() might lead to
       * undefined behaviour
       */
      g_free (error);
      return;
    }

  clutter_interval_set_initial_value (interval, &value);
  g_value_unset (&value);

  /* final value */
  g_value_init (&value, gtype);
  G_VALUE_COLLECT (&value, var_args, 0, &error);
  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error);

      /* see above */
      g_free (error);
      return;
    }

  clutter_interval_set_final_value (interval, &value);
  g_value_unset (&value);
}

static void
clutter_interval_get_interval_valist (ClutterInterval *interval,
                                      va_list          var_args)
{
  GType gtype = interval->priv->value_type;
  GValue value = { 0, };
  gchar *error;

  /* initial value */
  g_value_init (&value, gtype);
  clutter_interval_get_initial_value (interval, &value);
  G_VALUE_LCOPY (&value, var_args, 0, &error);
  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error);
      g_free (error);
      g_value_unset (&value);
      return;
    }

  g_value_unset (&value);

  /* final value */
  g_value_init (&value, gtype);
  clutter_interval_get_final_value (interval, &value);
  G_VALUE_LCOPY (&value, var_args, 0, &error);
  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error);
      g_free (error);
      g_value_unset (&value);
      return;
    }

  g_value_unset (&value);
}

/**
 * clutter_interval_new:
 * @gtype: the type of the values in the interval
 * @Varargs: the initial value and the final value of the interval
 *
 * Creates a new #ClutterInterval holding values of type @gtype.
 *
 * This function avoids using a #GValue for the initial and final values
 * of the interval:
 *
 * |[
 *   interval = clutter_interval_new (G_TYPE_FLOAT, 0.0, 1.0);
 *   interval = clutter_interval_new (G_TYPE_BOOLEAN, FALSE, TRUE);
 *   interval = clutter_interval_new (G_TYPE_INT, 0, 360);
 * ]|
 *
 * Return value: the newly created #ClutterInterval
 *
 * Since: 1.0
 */
ClutterInterval *
clutter_interval_new (GType gtype,
                      ...)
{
  ClutterInterval *retval;
  va_list args;

  g_return_val_if_fail (gtype != G_TYPE_INVALID, NULL);

  retval = g_object_new (CLUTTER_TYPE_INTERVAL, "value-type", gtype, NULL);

  va_start (args, gtype);
  clutter_interval_set_interval_valist (retval, args);
  va_end (args);

  return retval;
}

/**
 * clutter_interval_new_with_values:
 * @gtype: the type of the values in the interval
 * @initial: a #GValue holding the initial value of the interval
 * @final: a #GValue holding the final value of the interval
 *
 * Creates a new #ClutterInterval of type @gtype, between @initial
 * and @final.
 *
 * This function is useful for language bindings.
 *
 * Return value: the newly created #ClutterInterval
 *
 * Since: 1.0
 */
ClutterInterval *
clutter_interval_new_with_values (GType         gtype,
                                  const GValue *initial,
                                  const GValue *final)
{
  ClutterInterval *retval;

  g_return_val_if_fail (gtype != G_TYPE_INVALID, NULL);
  g_return_val_if_fail (initial != NULL, NULL);
  g_return_val_if_fail (final != NULL, NULL);
  g_return_val_if_fail (G_VALUE_TYPE (initial) == gtype, NULL);
  g_return_val_if_fail (G_VALUE_TYPE (final) == gtype, NULL);

  retval = g_object_new (CLUTTER_TYPE_INTERVAL, "value-type", gtype, NULL);

  clutter_interval_set_initial_value (retval, initial);
  clutter_interval_set_final_value (retval, final);

  return retval;
}

/**
 * clutter_interval_clone:
 * @interval: a #ClutterInterval
 *
 * Creates a copy of @interval.
 *
 * Return value: the newly created #ClutterInterval
 *
 * Since: 1.0
 */
ClutterInterval *
clutter_interval_clone (ClutterInterval *interval)
{
  ClutterInterval *retval;
  GType gtype;
  GValue *tmp;

  g_return_val_if_fail (CLUTTER_IS_INTERVAL (interval), NULL);
  g_return_val_if_fail (interval->priv->value_type != G_TYPE_INVALID, NULL);

  gtype = interval->priv->value_type;
  retval = g_object_new (CLUTTER_TYPE_INTERVAL, "value-type", gtype, NULL);

  tmp = clutter_interval_peek_initial_value (interval);
  clutter_interval_set_initial_value (retval, tmp);

  tmp = clutter_interval_peek_final_value (interval);
  clutter_interval_set_final_value (retval, tmp);

  return retval;
}

/**
 * clutter_interval_get_value_type:
 * @interval: a #ClutterInterval
 *
 * Retrieves the #GType of the values inside @interval.
 *
 * Return value: the type of the value, or G_TYPE_INVALID
 *
 * Since: 1.0
 */
GType
clutter_interval_get_value_type (ClutterInterval *interval)
{
  g_return_val_if_fail (CLUTTER_IS_INTERVAL (interval), G_TYPE_INVALID);

  return interval->priv->value_type;
}

static inline void
clutter_interval_set_value_internal (ClutterInterval *interval,
                                     gint             index_,
                                     const GValue    *value)
{
  ClutterIntervalPrivate *priv = interval->priv;

  if (G_IS_VALUE (&priv->values[index_]))
    g_value_unset (&priv->values[index_]);

  g_value_init (&priv->values[index_], priv->value_type);
  g_value_copy (value, &priv->values[index_]);
}

static inline void
clutter_interval_get_value_internal (ClutterInterval *interval,
                                     gint             index_,
                                     GValue          *value)
{
  ClutterIntervalPrivate *priv = interval->priv;

  g_value_copy (&priv->values[index_], value);
}

/**
 * clutter_interval_set_initial_value:
 * @interval: a #ClutterInterval
 * @value: a #GValue
 *
 * Sets the initial value of @interval to @value. The value is copied
 * inside the #ClutterInterval.
 *
 * Since: 1.0
 */
void
clutter_interval_set_initial_value (ClutterInterval *interval,
                                    const GValue    *value)
{
  ClutterIntervalPrivate *priv;

  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));
  g_return_if_fail (value != NULL);

  priv = interval->priv;

  g_return_if_fail (G_VALUE_TYPE (value) == priv->value_type);

  clutter_interval_set_value_internal (interval, 0, value);
}

/**
 * clutter_interval_get_initial_value:
 * @interval: a #ClutterInterval
 * @value: a #GValue
 *
 * Retrieves the initial value of @interval and copies
 * it into @value.
 *
 * The passed #GValue must be initialized to the value held by
 * the #ClutterInterval.
 *
 * Since: 1.0
 */
void
clutter_interval_get_initial_value (ClutterInterval *interval,
                                    GValue          *value)
{
  ClutterIntervalPrivate *priv;

  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));
  g_return_if_fail (value != NULL);

  priv = interval->priv;

  clutter_interval_get_value_internal (interval, 0, value);
}

/**
 * clutter_interval_peek_initial_value:
 * @interval: a #ClutterInterval
 *
 * Gets the pointer to the initial value of @interval
 *
 * Return value: (transfer none): the initial value of the interval.
 *   The value is owned by the #ClutterInterval and it should not be
 *   modified or freed
 *
 * Since: 1.0
 */
GValue *
clutter_interval_peek_initial_value (ClutterInterval *interval)
{
  g_return_val_if_fail (CLUTTER_IS_INTERVAL (interval), NULL);

  return interval->priv->values;
}

/**
 * clutter_interval_set_final_value:
 * @interval: a #ClutterInterval
 * @value: a #GValue
 *
 * Sets the final value of @interval to @value. The value is
 * copied inside the #ClutterInterval.
 *
 * Since: 1.0
 */
void
clutter_interval_set_final_value (ClutterInterval *interval,
                                  const GValue    *value)
{
  ClutterIntervalPrivate *priv;

  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));
  g_return_if_fail (value != NULL);

  priv = interval->priv;

  g_return_if_fail (G_VALUE_TYPE (value) == priv->value_type);

  clutter_interval_set_value_internal (interval, 1, value);
}

/**
 * clutter_interval_get_final_value:
 * @interval: a #ClutterInterval
 * @value: a #GValue
 *
 * Retrieves the final value of @interval and copies
 * it into @value.
 *
 * The passed #GValue must be initialized to the value held by
 * the #ClutterInterval.
 *
 * Since: 1.0
 */
void
clutter_interval_get_final_value (ClutterInterval *interval,
                                  GValue          *value)
{
  ClutterIntervalPrivate *priv;

  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));
  g_return_if_fail (value != NULL);

  priv = interval->priv;

  clutter_interval_get_value_internal (interval, 1, value);
}

/**
 * clutter_interval_peek_final_value:
 * @interval: a #ClutterInterval
 *
 * Gets the pointer to the final value of @interval
 *
 * Return value: (transfer none): the final value of the interval.
 *   The value is owned by the #ClutterInterval and it should not be
 *   modified or freed
 *
 * Since: 1.0
 */
GValue *
clutter_interval_peek_final_value (ClutterInterval *interval)
{
  g_return_val_if_fail (CLUTTER_IS_INTERVAL (interval), NULL);

  return interval->priv->values + 1;
}

/**
 * clutter_interval_set_interval:
 * @interval: a #ClutterInterval
 * @Varargs: the initial and final values of the interval
 *
 * Variable arguments wrapper for clutter_interval_set_initial_value()
 * and clutter_interval_set_final_value() that avoids using the
 * #GValue arguments:
 *
 * |[
 *   clutter_interval_set_interval (interval, 0, 50);
 *   clutter_interval_set_interval (interval, 1.0, 0.0);
 *   clutter_interval_set_interval (interval, FALSE, TRUE);
 * ]|
 *
 * This function is meant for the convenience of the C API; bindings
 * should reimplement this function using the #GValue-based API.
 *
 * Since: 1.0
 */
void
clutter_interval_set_interval (ClutterInterval *interval,
                               ...)
{
  va_list args;

  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));
  g_return_if_fail (interval->priv->value_type != G_TYPE_INVALID);

  va_start (args, interval);
  clutter_interval_set_interval_valist (interval, args);
  va_end (args);
}

/**
 * clutter_interval_get_interval:
 * @interval: a #ClutterInterval
 * @Varargs: return locations for the initial and final values of
 *   the interval
 *
 * Variable arguments wrapper for clutter_interval_get_initial_value()
 * and clutter_interval_get_final_value() that avoids using the
 * #GValue arguments:
 *
 * |[
 *   gint a = 0, b = 0;
 *   clutter_interval_get_interval (interval, &a, &b);
 * ]|
 *
 * This function is meant for the convenience of the C API; bindings
 * should reimplement this function using the #GValue-based API.
 *
 * Since: 1.0
 */
void
clutter_interval_get_interval (ClutterInterval *interval,
                               ...)
{
  va_list args;

  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));
  g_return_if_fail (interval->priv->value_type != G_TYPE_INVALID);

  va_start (args, interval);
  clutter_interval_get_interval_valist (interval, args);
  va_end (args);
}

/**
 * clutter_interval_validate:
 * @interval: a #ClutterInterval
 * @pspec: a #GParamSpec
 *
 * Validates the initial and final values of @interval against
 * a #GParamSpec.
 *
 * Return value: %TRUE if the #ClutterInterval is valid, %FALSE otherwise
 *
 * Since: 1.0
 */
gboolean
clutter_interval_validate (ClutterInterval *interval,
                           GParamSpec      *pspec)
{
  g_return_val_if_fail (CLUTTER_IS_INTERVAL (interval), FALSE);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);

  return CLUTTER_INTERVAL_GET_CLASS (interval)->validate (interval, pspec);
}

/**
 * clutter_interval_compute_value:
 * @interval: a #ClutterInterval
 * @factor: the progress factor, between 0 and 1
 * @value: return location for an initialized #GValue
 *
 * Computes the value between the @interval boundaries given the
 * progress @factor and puts it into @value.
 *
 * Return value: %TRUE if the operation was successful
 *
 * Since: 1.0
 */
gboolean
clutter_interval_compute_value (ClutterInterval *interval,
                                gdouble          factor,
                                GValue          *value)
{
  g_return_val_if_fail (CLUTTER_IS_INTERVAL (interval), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  return CLUTTER_INTERVAL_GET_CLASS (interval)->compute_value (interval,
                                                               factor,
                                                               value);
}

/**
 * clutter_interval_register_progress_func:
 * @value_type: a #GType
 * @func: a #ClutterProgressFunc, or %NULL to unset a previously
 *   set progress function
 *
 * Sets the progress function for a given @value_type, like:
 *
 * |[
 *   clutter_interval_register_progress_func (MY_TYPE_FOO,
 *                                            my_foo_progress);
 * ]|
 *
 * Whenever a #ClutterInterval instance using the default
 * #ClutterInterval::compute_value implementation is set as an
 * interval between two #GValue of type @value_type, it will call
 * @func to establish the value depending on the given progress,
 * for instance:
 *
 * |[
 *   static gboolean
 *   my_int_progress (const GValue *a,
 *                    const GValue *b,
 *                    gdouble       progress,
 *                    GValue       *retval)
 *   {
 *     gint ia = g_value_get_int (a);
 *     gint ib = g_value_get_int (b);
 *     gint res = factor * (ib - ia) + ia;
 *
 *     g_value_set_int (retval, res);
 *
 *     return TRUE;
 *   }
 *
 *   clutter_interval_register_progress_func (G_TYPE_INT, my_int_progress);
 * ]|
 *
 * To unset a previously set progress function of a #GType, pass %NULL
 * for @func.
 *
 * Since: 1.0
 */
void
clutter_interval_register_progress_func (GType               value_type,
                                         ClutterProgressFunc func)
{
  ProgressData *progress_func;

  g_return_if_fail (value_type != G_TYPE_INVALID);

  if (G_UNLIKELY (progress_funcs == NULL))
    progress_funcs = g_hash_table_new (NULL, NULL);

  progress_func =
    g_hash_table_lookup (progress_funcs, GUINT_TO_POINTER (value_type));
  if (G_UNLIKELY (progress_func))
    {
      if (func == NULL)
        {
          g_hash_table_remove (progress_funcs, GUINT_TO_POINTER (value_type));
          g_slice_free (ProgressData, progress_func);
        }
      else
        progress_func->func = func;
    }
  else
    {
      progress_func = g_slice_new (ProgressData);
      progress_func->value_type = value_type;
      progress_func->func = func;

      g_hash_table_replace (progress_funcs,
                            GUINT_TO_POINTER (value_type),
                            progress_func);
    }
}
