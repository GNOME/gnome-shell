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
#include "clutter-interval.h"
#include "clutter-private.h"
#include "clutter-units.h"

#include "deprecated/clutter-fixed.h"

enum
{
  PROP_0,

  PROP_VALUE_TYPE,
  PROP_INITIAL,
  PROP_FINAL,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

enum
{
  INITIAL = 0,
  FINAL,
  RESULT,

  N_VALUES
};

struct _ClutterIntervalPrivate
{
  GType value_type;

  GValue *values;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClutterInterval, clutter_interval, G_TYPE_INITIALLY_UNOWNED)

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

    case G_TYPE_INT64:
      {
        GParamSpecInt64 *pspec_int = G_PARAM_SPEC_INT64 (pspec);
        gint64 a, b;

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

    case G_TYPE_UINT64:
      {
        GParamSpecUInt64 *pspec_int = G_PARAM_SPEC_UINT64 (pspec);
        guint64 a, b;

        a = b = 0;
        clutter_interval_get_interval (interval, &a, &b);
        if ((a >= pspec_int->minimum && a <= pspec_int->maximum) &&
            (b >= pspec_int->minimum && b <= pspec_int->maximum))
          return TRUE;
        else
          return FALSE;
      }
      break;

    case G_TYPE_CHAR:
      {
        GParamSpecChar *pspec_char = G_PARAM_SPEC_CHAR (pspec);
        guchar a, b;

        a = b = 0;
        clutter_interval_get_interval (interval, &a, &b);
        if ((a >= pspec_char->minimum && a <= pspec_char->maximum) &&
            (b >= pspec_char->minimum && b <= pspec_char->maximum))
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

    case G_TYPE_FLOAT:
      {
        GParamSpecFloat *pspec_flt = G_PARAM_SPEC_FLOAT (pspec);
        float a, b;

        a = b = 0.f;
        clutter_interval_get_interval (interval, &a, &b);
        if ((a >= pspec_flt->minimum && a <= pspec_flt->maximum) &&
            (b >= pspec_flt->minimum && b <= pspec_flt->maximum))
          return TRUE;
        else
          return FALSE;
      }
      break;

    case G_TYPE_DOUBLE:
      {
        GParamSpecDouble *pspec_flt = G_PARAM_SPEC_DOUBLE (pspec);
        double a, b;

        a = b = 0;
        clutter_interval_get_interval (interval, &a, &b);
        if ((a >= pspec_flt->minimum && a <= pspec_flt->maximum) &&
            (b >= pspec_flt->minimum && b <= pspec_flt->maximum))
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

  if (_clutter_has_progress_function (value_type))
    {
      retval = _clutter_run_progress_function (value_type,
                                               initial,
                                               final,
                                               factor,
                                               value);
      if (retval)
        return TRUE;
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

    case G_TYPE_CHAR:
      {
        gchar ia, ib, res;

        ia = g_value_get_schar (initial);
        ib = g_value_get_schar (final);

        res = (factor * (ib - (gdouble) ia)) + ia;

        g_value_set_schar (value, res);

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
      break;

    default:
      break;
    }

  /* We're trying to animate a property without knowing how to do that. Issue
   * a warning with a hint to what could be done to fix that */
  if (G_UNLIKELY (retval == FALSE))
    {
      g_warning ("%s: Could not compute progress between two %s. You can "
                 "register a progress function to instruct ClutterInterval "
                 "how to deal with this GType",
                 G_STRLOC,
                 g_type_name (value_type));
    }

  return retval;
}

static void
clutter_interval_finalize (GObject *gobject)
{
  ClutterIntervalPrivate *priv = CLUTTER_INTERVAL (gobject)->priv;

  if (G_IS_VALUE (&priv->values[INITIAL]))
    g_value_unset (&priv->values[INITIAL]);

  if (G_IS_VALUE (&priv->values[FINAL]))
    g_value_unset (&priv->values[FINAL]);

  if (G_IS_VALUE (&priv->values[RESULT]))
    g_value_unset (&priv->values[RESULT]);

  g_free (priv->values);

  G_OBJECT_CLASS (clutter_interval_parent_class)->finalize (gobject);
}

static void
clutter_interval_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ClutterInterval *self = CLUTTER_INTERVAL (gobject);
  ClutterIntervalPrivate *priv = clutter_interval_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_VALUE_TYPE:
      priv->value_type = g_value_get_gtype (value);
      break;

    case PROP_INITIAL:
      if (g_value_get_boxed (value) != NULL)
        clutter_interval_set_initial_value (self, g_value_get_boxed (value));
      else if (G_IS_VALUE (&priv->values[INITIAL]))
        g_value_unset (&priv->values[INITIAL]);
      break;

    case PROP_FINAL:
      if (g_value_get_boxed (value) != NULL)
        clutter_interval_set_final_value (self, g_value_get_boxed (value));
      else if (G_IS_VALUE (&priv->values[FINAL]))
        g_value_unset (&priv->values[FINAL]);
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
  ClutterIntervalPrivate *priv;
  
  priv = clutter_interval_get_instance_private (CLUTTER_INTERVAL (gobject));

  switch (prop_id)
    {
    case PROP_VALUE_TYPE:
      g_value_set_gtype (value, priv->value_type);
      break;

    case PROP_INITIAL:
      if (G_IS_VALUE (&priv->values[INITIAL]))
        g_value_set_boxed (value, &priv->values[INITIAL]);
      break;

    case PROP_FINAL:
      if (G_IS_VALUE (&priv->values[FINAL]))
        g_value_set_boxed (value, &priv->values[FINAL]);
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
  obj_props[PROP_VALUE_TYPE] =
    g_param_spec_gtype ("value-type",
                        P_("Value Type"),
                        P_("The type of the values in the interval"),
                        G_TYPE_NONE,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);

  /**
   * ClutterInterval:initial:
   *
   * The initial value of the interval.
   *
   * Since: 1.12
   */
  obj_props[PROP_INITIAL] =
    g_param_spec_boxed ("initial",
                        P_("Initial Value"),
                        P_("Initial value of the interval"),
                        G_TYPE_VALUE,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS);

  /**
   * ClutterInterval:final:
   *
   * The final value of the interval.
   *
   * Since: 1.12
   */
  obj_props[PROP_FINAL] =
    g_param_spec_boxed ("final",
                        P_("Final Value"),
                        P_("Final value of the interval"),
                        G_TYPE_VALUE,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
clutter_interval_init (ClutterInterval *self)
{
  self->priv = clutter_interval_get_instance_private (self);

  self->priv->value_type = G_TYPE_INVALID;
  self->priv->values = g_malloc0 (sizeof (GValue) * N_VALUES);
}

static inline void
clutter_interval_set_value_internal (ClutterInterval *interval,
                                     gint             index_,
                                     const GValue    *value)
{
  ClutterIntervalPrivate *priv = interval->priv;
  GType value_type;

  g_assert (index_ >= INITIAL && index_ <= RESULT);

  if (G_IS_VALUE (&priv->values[index_]))
    g_value_unset (&priv->values[index_]);

  g_value_init (&priv->values[index_], priv->value_type);

  value_type = G_VALUE_TYPE (value);
  if (value_type != priv->value_type ||
      !g_type_is_a (value_type, priv->value_type))
    {
      if (g_value_type_compatible (value_type, priv->value_type))
        {
          g_value_copy (value, &priv->values[index_]);
          return;
        }

      if (g_value_type_transformable (value_type, priv->value_type))
        {
          GValue transform = G_VALUE_INIT;

          g_value_init (&transform, priv->value_type);

          if (g_value_transform (value, &transform))
            g_value_copy (&transform, &priv->values[index_]);
          else
            {
              g_warning ("%s: Unable to convert a value of type '%s' into "
                         "the value type '%s' of the interval.",
                         G_STRLOC,
                         g_type_name (value_type),
                         g_type_name (priv->value_type));
            }

          g_value_unset (&transform);
        }
    }
  else
    g_value_copy (value, &priv->values[index_]);
}

static inline void
clutter_interval_get_value_internal (ClutterInterval *interval,
                                     gint             index_,
                                     GValue          *value)
{
  ClutterIntervalPrivate *priv = interval->priv;

  g_assert (index_ >= INITIAL && index_ <= RESULT);

  g_value_copy (&priv->values[index_], value);
}

static gboolean
clutter_interval_set_initial_internal (ClutterInterval *interval,
                                       va_list         *args)
{
  GType gtype = interval->priv->value_type;
  GValue value = G_VALUE_INIT;
  gchar *error;

  /* initial value */
  G_VALUE_COLLECT_INIT (&value, gtype, *args, 0, &error);

  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error);

      /* we leak the value here as it might not be in a valid state
       * given the error and calling g_value_unset() might lead to
       * undefined behaviour
       */
      g_free (error);
      return FALSE;
    }

  clutter_interval_set_value_internal (interval, INITIAL, &value);
  g_value_unset (&value);

  return TRUE;
}

static gboolean
clutter_interval_set_final_internal (ClutterInterval *interval,
                                     va_list         *args)
{
  GType gtype = interval->priv->value_type;
  GValue value = G_VALUE_INIT;
  gchar *error;

  /* initial value */
  G_VALUE_COLLECT_INIT (&value, gtype, *args, 0, &error);

  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error);

      /* we leak the value here as it might not be in a valid state
       * given the error and calling g_value_unset() might lead to
       * undefined behaviour
       */
      g_free (error);
      return FALSE;
    }

  clutter_interval_set_value_internal (interval, FINAL, &value);
  g_value_unset (&value);

  return TRUE;
}

static void
clutter_interval_get_interval_valist (ClutterInterval *interval,
                                      va_list          var_args)
{
  GType gtype = interval->priv->value_type;
  GValue value = G_VALUE_INIT;
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
 * @...: the initial value and the final value of the interval
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

  if (!clutter_interval_set_initial_internal (retval, &args))
    goto out;

  clutter_interval_set_final_internal (retval, &args);

out:
  va_end (args);

  return retval;
}

/**
 * clutter_interval_new_with_values:
 * @gtype: the type of the values in the interval
 * @initial: (allow-none): a #GValue holding the initial value of the interval
 * @final: (allow-none): a #GValue holding the final value of the interval
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
  g_return_val_if_fail (gtype != G_TYPE_INVALID, NULL);
  g_return_val_if_fail (initial == NULL || G_VALUE_TYPE (initial) == gtype, NULL);
  g_return_val_if_fail (final == NULL || G_VALUE_TYPE (final) == gtype, NULL);

  return g_object_new (CLUTTER_TYPE_INTERVAL,
                       "value-type", gtype,
                       "initial", initial,
                       "final", final,
                       NULL);
}

/**
 * clutter_interval_clone:
 * @interval: a #ClutterInterval
 *
 * Creates a copy of @interval.
 *
 * Return value: (transfer full): the newly created #ClutterInterval
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

/**
 * clutter_interval_set_initial_value:
 * @interval: a #ClutterInterval
 * @value: a #GValue
 *
 * Sets the initial value of @interval to @value. The value is copied
 * inside the #ClutterInterval.
 *
 * Rename to: clutter_interval_set_initial
 *
 * Since: 1.0
 */
void
clutter_interval_set_initial_value (ClutterInterval *interval,
                                    const GValue    *value)
{
  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));
  g_return_if_fail (value != NULL);

  clutter_interval_set_value_internal (interval, INITIAL, value);
}

/**
 * clutter_interval_set_initial: (skip)
 * @interval: a #ClutterInterval
 * @...: the initial value of the interval.
 *
 * Variadic arguments version of clutter_interval_set_initial_value().
 *
 * This function is meant as a convenience for the C API.
 *
 * Language bindings should use clutter_interval_set_initial_value()
 * instead.
 *
 * Since: 1.10
 */
void
clutter_interval_set_initial (ClutterInterval *interval,
                              ...)
{
  va_list args;

  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));

  va_start (args, interval);
  clutter_interval_set_initial_internal (interval, &args);
  va_end (args);
}

/**
 * clutter_interval_get_initial_value:
 * @interval: a #ClutterInterval
 * @value: (out caller-allocates): a #GValue
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
  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));
  g_return_if_fail (value != NULL);

  clutter_interval_get_value_internal (interval, INITIAL, value);
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

  return interval->priv->values + INITIAL;
}

/**
 * clutter_interval_set_final_value:
 * @interval: a #ClutterInterval
 * @value: a #GValue
 *
 * Sets the final value of @interval to @value. The value is
 * copied inside the #ClutterInterval.
 *
 * Rename to: clutter_interval_set_final
 *
 * Since: 1.0
 */
void
clutter_interval_set_final_value (ClutterInterval *interval,
                                  const GValue    *value)
{
  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));
  g_return_if_fail (value != NULL);

  clutter_interval_set_value_internal (interval, FINAL, value);
}

/**
 * clutter_interval_get_final_value:
 * @interval: a #ClutterInterval
 * @value: (out caller-allocates): a #GValue
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
  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));
  g_return_if_fail (value != NULL);

  clutter_interval_get_value_internal (interval, FINAL, value);
}

/**
 * clutter_interval_set_final: (skip)
 * @interval: a #ClutterInterval
 * @...: the final value of the interval
 *
 * Variadic arguments version of clutter_interval_set_final_value().
 *
 * This function is meant as a convenience for the C API.
 *
 * Language bindings should use clutter_interval_set_final_value() instead.
 *
 * Since: 1.10
 */
void
clutter_interval_set_final (ClutterInterval *interval,
                            ...)
{
  va_list args;

  g_return_if_fail (CLUTTER_IS_INTERVAL (interval));

  va_start (args, interval);
  clutter_interval_set_final_internal (interval, &args);
  va_end (args);
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

  return interval->priv->values + FINAL;
}

/**
 * clutter_interval_set_interval:
 * @interval: a #ClutterInterval
 * @...: the initial and final values of the interval
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

  if (!clutter_interval_set_initial_internal (interval, &args))
    goto out;

  clutter_interval_set_final_internal (interval, &args);

out:
  va_end (args);
}

/**
 * clutter_interval_get_interval:
 * @interval: a #ClutterInterval
 * @...: return locations for the initial and final values of
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
 * @value: (out caller-allocates): return location for an initialized #GValue
 *
 * Computes the value between the @interval boundaries given the
 * progress @factor and copies it into @value.
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
 * clutter_interval_compute:
 * @interval: a #ClutterInterval
 * @factor: the progress factor, between 0 and 1
 *
 * Computes the value between the @interval boundaries given the
 * progress @factor
 *
 * Unlike clutter_interval_compute_value(), this function will
 * return a const pointer to the computed value
 *
 * You should use this function if you immediately pass the computed
 * value to another function that makes a copy of it, like
 * g_object_set_property()
 *
 * Return value: (transfer none): a pointer to the computed value,
 *   or %NULL if the computation was not successfull
 *
 * Since: 1.4
 */
const GValue *
clutter_interval_compute (ClutterInterval *interval,
                          gdouble          factor)
{
  GValue *value;
  gboolean res;

  g_return_val_if_fail (CLUTTER_IS_INTERVAL (interval), NULL);

  value = &(interval->priv->values[RESULT]);

  if (G_VALUE_TYPE (value) == G_TYPE_INVALID)
    g_value_init (value, interval->priv->value_type);

  res = CLUTTER_INTERVAL_GET_CLASS (interval)->compute_value (interval,
                                                              factor,
                                                              value);

  if (res)
    return interval->priv->values + RESULT;

  return NULL;
}

/**
 * clutter_interval_is_valid:
 * @interval: a #ClutterInterval
 *
 * Checks if the @interval has a valid initial and final values.
 *
 * Return value: %TRUE if the #ClutterInterval has an initial and
 *   final values, and %FALSE otherwise
 *
 * Since: 1.12
 */
gboolean
clutter_interval_is_valid (ClutterInterval *interval)
{
  ClutterIntervalPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_INTERVAL (interval), FALSE);

  priv = interval->priv;

  return G_IS_VALUE (&priv->values[INITIAL]) &&
         G_IS_VALUE (&priv->values[FINAL]);
}
