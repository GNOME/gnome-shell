/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen  <jorn@openedhand.com>
 *             Emmanuele Bassi  <ebassi@openedhand.com>
 *             Tomas Frydrych <tf@openedhand.com>
 *
 * Copyright (C) 2006, 2007, 2008 OpenedHand
 * Copyright (C) 2009 Intel Corp.
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
 * SECTION:clutter-alpha
 * @short_description: A class for calculating an alpha value as a function
 * of time.
 *
 * #ClutterAlpha is a class for calculating an floating point value
 * dependent only on the position of a #ClutterTimeline.
 *
 * A #ClutterAlpha binds a #ClutterTimeline to a progress function which
 * translates the time T into an adimensional factor alpha. The factor can
 * then be used to drive a #ClutterBehaviour, which will translate the
 * alpha value into something meaningful for a #ClutterActor.
 *
 * You should provide a #ClutterTimeline and bind it to the #ClutterAlpha
 * instance using clutter_alpha_set_timeline(). You should also set an
 * "animation mode", either by using the #ClutterAnimatioMode values that
 * Clutter itself provides or by registering custom functions using
 * clutter_alpha_register_func().
 *
 * Instead of a #ClutterAnimationMode you may provide a function returning
 * the alpha value depending on the progress of the timeline, using
 * clutter_alpha_set_func() or clutter_alpha_set_closure(). The alpha
 * function will be executed each time a new frame in the #ClutterTimeline
 * is reached.
 *
 * Since the alpha function is controlled by the timeline instance, you can
 * pause, stop or resume the #ClutterAlpha from calling the alpha function by
 * using the appropriate functions of the #ClutterTimeline object.
 *
 * #ClutterAlpha is used to "drive" a #ClutterBehaviour instance, and it
 * is internally used by the #ClutterAnimation API.
 *
 * Since: 0.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "clutter-alpha.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

G_DEFINE_TYPE (ClutterAlpha, clutter_alpha, G_TYPE_INITIALLY_UNOWNED);


struct _ClutterAlphaPrivate
{
  ClutterTimeline *timeline;
  guint timeline_new_frame_id;

  gdouble alpha;

  GClosure *closure;

  gulong mode;
};

enum
{
  PROP_0,
  
  PROP_TIMELINE,
  PROP_ALPHA,
  PROP_MODE
};

static void
timeline_new_frame_cb (ClutterTimeline *timeline,
                       guint            current_frame_num,
                       ClutterAlpha    *alpha)
{
  ClutterAlphaPrivate *priv = alpha->priv;

  /* Update alpha value and notify */
  priv->alpha = clutter_alpha_get_alpha (alpha);
  g_object_notify (G_OBJECT (alpha), "alpha");
}

static void 
clutter_alpha_set_property (GObject      *object, 
			    guint         prop_id,
			    const GValue *value, 
			    GParamSpec   *pspec)
{
  ClutterAlpha *alpha;
  ClutterAlphaPrivate *priv;

  alpha = CLUTTER_ALPHA (object);
  priv = alpha->priv;

  switch (prop_id) 
    {
    case PROP_TIMELINE:
      clutter_alpha_set_timeline (alpha, g_value_get_object (value));
      break;

    case PROP_MODE:
      clutter_alpha_set_mode (alpha, g_value_get_ulong (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void 
clutter_alpha_get_property (GObject    *object, 
			    guint       prop_id,
			    GValue     *value, 
			    GParamSpec *pspec)
{
  ClutterAlpha        *alpha;
  ClutterAlphaPrivate *priv;

  alpha = CLUTTER_ALPHA (object);
  priv = alpha->priv;

  switch (prop_id) 
    {
    case PROP_TIMELINE:
      g_value_set_object (value, priv->timeline);
      break;

    case PROP_ALPHA:
      g_value_set_double (value, priv->alpha);
      break;

    case PROP_MODE:
      g_value_set_ulong (value, priv->mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
clutter_alpha_finalize (GObject *object)
{
  ClutterAlphaPrivate *priv = CLUTTER_ALPHA (object)->priv;

  if (priv->closure)
    g_closure_unref (priv->closure);

  G_OBJECT_CLASS (clutter_alpha_parent_class)->finalize (object);
}

static void 
clutter_alpha_dispose (GObject *object)
{
  ClutterAlpha *self = CLUTTER_ALPHA(object);

  clutter_alpha_set_timeline (self, NULL);

  G_OBJECT_CLASS (clutter_alpha_parent_class)->dispose (object);
}


static void
clutter_alpha_class_init (ClutterAlphaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  object_class->set_property = clutter_alpha_set_property;
  object_class->get_property = clutter_alpha_get_property;
  object_class->finalize     = clutter_alpha_finalize;
  object_class->dispose      = clutter_alpha_dispose;

  g_type_class_add_private (klass, sizeof (ClutterAlphaPrivate));

  /**
   * ClutterAlpha:timeline:
   *
   * A #ClutterTimeline instance used to drive the alpha function.
   *
   * Since: 0.2
   */
  pspec = g_param_spec_object ("timeline",
                               "Timeline",
                               "Timeline used by the alpha",
                               CLUTTER_TYPE_TIMELINE,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_TIMELINE, pspec);

  /**
   * ClutterAlpha:alpha:
   *
   * The alpha value as computed by the alpha function. The linear
   * interval is 0.0 to 1.0, but the Alpha allows overshooting by
   * one unit in each direction, so the valid interval is -1.0 to 2.0.
   *
   * Since: 0.2
   */
  pspec = g_param_spec_double ("alpha",
                               "Alpha value",
                               "Alpha value",
                               -1.0, 2.0,
                               0.0,
                               CLUTTER_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_ALPHA, pspec);

  /**
   * ClutterAlpha:mode:
   *
   * The progress function logical id - either a value from the
   * #ClutterAnimationMode enumeration or a value returned by
   * clutter_alpha_register_func().
   *
   * If %CLUTTER_CUSTOM_MODE is used then the function set using
   * clutter_alpha_set_closure() or clutter_alpha_set_func()
   * will be used.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_ulong ("mode",
                              "Mode",
                              "Progress mode",
                              0, G_MAXULONG,
                              CLUTTER_CUSTOM_MODE,
                              G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_MODE, pspec);
}

static void
clutter_alpha_init (ClutterAlpha *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
					    CLUTTER_TYPE_ALPHA,
					    ClutterAlphaPrivate);

  self->priv->mode = CLUTTER_CUSTOM_MODE;
  self->priv->alpha = 0.0;
}

/**
 * clutter_alpha_get_alpha:
 * @alpha: A #ClutterAlpha
 *
 * Query the current alpha value.
 *
 * Return Value: The current alpha value for the alpha
 *
 * Since: 0.2
 */
gdouble
clutter_alpha_get_alpha (ClutterAlpha *alpha)
{
  ClutterAlphaPrivate *priv;
  gdouble retval = 0;

  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), 0);

  priv = alpha->priv;

  if (G_LIKELY (priv->closure))
    {
      GValue params = { 0, };
      GValue result_value = { 0, };

      g_object_ref (alpha);

      g_value_init (&result_value, G_TYPE_DOUBLE);

      g_value_init (&params, CLUTTER_TYPE_ALPHA);
      g_value_set_object (&params, alpha);
		     
      g_closure_invoke (priv->closure, &result_value, 1, &params, NULL);

      retval = g_value_get_double (&result_value);

      g_value_unset (&result_value);
      g_value_unset (&params);

      g_object_unref (alpha);
    }

  return retval;
}

/**
 * clutter_alpha_set_closure:
 * @alpha: A #ClutterAlpha
 * @closure: A #GClosure
 *
 * Sets the #GClosure used to compute the alpha value at each
 * frame of the #ClutterTimeline bound to @alpha.
 *
 * Since: 0.8
 */
void
clutter_alpha_set_closure (ClutterAlpha *alpha,
                           GClosure     *closure)
{
  ClutterAlphaPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ALPHA (alpha));
  g_return_if_fail (closure != NULL);

  priv = alpha->priv;

  if (priv->closure)
    g_closure_unref (priv->closure);

  priv->closure = g_closure_ref (closure);
  g_closure_sink (closure);

  if (G_CLOSURE_NEEDS_MARSHAL (closure))
    {
      GClosureMarshal marshal = clutter_marshal_DOUBLE__VOID;

      g_closure_set_marshal (closure, marshal);
    }

  priv->mode = CLUTTER_CUSTOM_MODE;
  g_object_notify (G_OBJECT (alpha), "mode");
}

/**
 * clutter_alpha_set_func:
 * @alpha: A #ClutterAlpha
 * @func: A #ClutterAlphaFunc
 * @data: user data to be passed to the alpha function, or %NULL
 * @destroy: notify function used when disposing the alpha function
 *
 * Sets the #ClutterAlphaFunc function used to compute
 * the alpha value at each frame of the #ClutterTimeline
 * bound to @alpha.
 *
 * This function will not register @func as a global alpha function.
 *
 * Since: 0.2
 */
void
clutter_alpha_set_func (ClutterAlpha    *alpha,
		        ClutterAlphaFunc func,
                        gpointer         data,
                        GDestroyNotify   destroy)
{
  GClosure *closure;

  g_return_if_fail (CLUTTER_IS_ALPHA (alpha));
  g_return_if_fail (func != NULL);
  
  closure = g_cclosure_new (G_CALLBACK (func), data, (GClosureNotify) destroy);
  clutter_alpha_set_closure (alpha, closure);
}

/**
 * clutter_alpha_set_timeline:
 * @alpha: A #ClutterAlpha
 * @timeline: A #ClutterTimeline
 *
 * Binds @alpha to @timeline.
 *
 * Since: 0.2
 */
void
clutter_alpha_set_timeline (ClutterAlpha    *alpha,
                            ClutterTimeline *timeline)
{
  ClutterAlphaPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ALPHA (alpha));
  g_return_if_fail (timeline == NULL || CLUTTER_IS_TIMELINE (timeline));
  
  priv = alpha->priv;

  if (priv->timeline == timeline)
    return;

  if (priv->timeline)
    {
      g_signal_handlers_disconnect_by_func (priv->timeline,
                                            timeline_new_frame_cb,
                                            alpha);

      g_object_unref (priv->timeline);
      priv->timeline = NULL;
    }

  if (timeline)
    {
      priv->timeline = g_object_ref (timeline);

      g_signal_connect (priv->timeline, "new-frame",
                        G_CALLBACK (timeline_new_frame_cb),
                        alpha);
    }

  g_object_notify (G_OBJECT (alpha), "timeline");
}

/**
 * clutter_alpha_get_timeline:
 * @alpha: A #ClutterAlpha
 *
 * Gets the #ClutterTimeline bound to @alpha.
 *
 * Return value: a #ClutterTimeline instance
 *
 * Since: 0.2
 */
ClutterTimeline *
clutter_alpha_get_timeline (ClutterAlpha *alpha)
{
  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), NULL);

  return alpha->priv->timeline;
}

/**
 * clutter_alpha_new:
 * 
 * Creates a new #ClutterAlpha instance.  You must set a function
 * to compute the alpha value using clutter_alpha_set_func() and
 * bind a #ClutterTimeline object to the #ClutterAlpha instance
 * using clutter_alpha_set_timeline().
 *
 * You should use the newly created #ClutterAlpha instance inside
 * a #ClutterBehaviour object.
 *
 * Return value: the newly created empty #ClutterAlpha instance.
 *
 * Since: 0.2
 */
ClutterAlpha *
clutter_alpha_new (void)
{
  return g_object_new (CLUTTER_TYPE_ALPHA, NULL);
}

/**
 * clutter_alpha_new_full:
 * @timeline: #ClutterTimeline timeline
 * @mode: animation mode
 *
 * Creates a new #ClutterAlpha instance and sets the timeline
 * and animation mode.
 *
 * See also clutter_alpha_set_timeline() and clutter_alpha_set_mode().
 *
 * Return Value: the newly created #ClutterAlpha
 *
 * Since: 1.0
 */
ClutterAlpha *
clutter_alpha_new_full (ClutterTimeline *timeline,
                        gulong           mode)
{
  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), NULL);
  g_return_val_if_fail (mode != CLUTTER_ANIMATION_LAST, NULL);

  return g_object_new (CLUTTER_TYPE_ALPHA,
                       "timeline", timeline,
                       "mode", mode,
                       NULL);
}

/**
 * clutter_alpha_new_with_func:
 * @timeline: a #ClutterTimeline
 * @func: a #ClutterAlphaFunc
 * @data: data to pass to the function, or %NULL
 * @destroy: function to call when removing the alpha function, or %NULL
 *
 * Creates a new #ClutterAlpha instances and sets the timeline
 * and the alpha function.
 *
 * This function will not register @func as a global alpha function.
 *
 * See also clutter_alpha_set_timeline() and clutter_alpha_set_func().
 *
 * Return value: the newly created #ClutterAlpha
 *
 * Since: 1.0
 */
ClutterAlpha *
clutter_alpha_new_with_func (ClutterTimeline  *timeline,
                             ClutterAlphaFunc  func,
                             gpointer          data,
                             GDestroyNotify    destroy)
{
  ClutterAlpha *retval;

  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), NULL);
  g_return_val_if_fail (func != NULL, NULL);

  retval = clutter_alpha_new ();
  clutter_alpha_set_timeline (retval, timeline);
  clutter_alpha_set_func (retval, func, data, destroy);

  return retval;
}

/**
 * clutter_alpha_get_mode:
 * @alpha: a #ClutterAlpha
 *
 * Retrieves the #ClutterAnimatioMode used by @alpha.
 *
 * Return value: the animation mode
 *
 * Since: 1.0
 */
gulong
clutter_alpha_get_mode (ClutterAlpha *alpha)
{
  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), CLUTTER_CUSTOM_MODE);

  return alpha->priv->mode;
}

/* static enum/function mapping table for the animation modes
 * we provide internally
 *
 * XXX - keep in sync with ClutterAnimationMode
 */
static const struct {
  gulong mode;
  ClutterAlphaFunc func;
} animation_modes[] = {
  { CLUTTER_CUSTOM_MODE, NULL },
};

typedef struct _AlphaData {
  guint closure_set : 1;

  ClutterAlphaFunc func;
  gpointer data;

  GClosure *closure;
} AlphaData;

static GPtrArray *clutter_alphas = NULL;

/**
 * clutter_alpha_set_mode:
 * @alpha: a #ClutterAlpha
 * @mode: a #ClutterAnimationMode
 *
 * Sets the progress function of @alpha using the symbolic value
 * of @mode, as taken by the #ClutterAnimationMode enumeration or
 * using the value returned by clutter_alpha_register_func().
 *
 * Since: 1.0
 */
void
clutter_alpha_set_mode (ClutterAlpha *alpha,
                        gulong        mode)
{
  ClutterAlphaPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ALPHA (alpha));
  g_return_if_fail (mode != CLUTTER_ANIMATION_LAST);

  priv = alpha->priv;

  if (mode < CLUTTER_ANIMATION_LAST)
    {
      /* sanity check to avoid getting an out of sync
       * enum/function mapping
       */
      g_assert (animation_modes[mode].mode == mode);

      if (G_LIKELY (animation_modes[mode].func != NULL))
        clutter_alpha_set_func (alpha, animation_modes[mode].func, NULL, NULL);

      priv->mode = mode;
    }
  else if (mode > CLUTTER_ANIMATION_LAST)
    {
      AlphaData *alpha_data = NULL;
      gulong real_index = 0;

      if (G_UNLIKELY (clutter_alphas == NULL))
        {
          g_warning ("No alpha functions defined for ClutterAlpha to use. "
                     "Use clutter_alpha_register_func() to register an "
                     "alpha function.");
          return;
        }

      real_index = mode - CLUTTER_ANIMATION_LAST - 1;

      alpha_data = g_ptr_array_index (clutter_alphas, real_index);
      if (G_UNLIKELY (alpha_data == NULL))
        {
          g_warning ("No alpha function registered for mode %lu.",
                     mode);
          return;
        }

      if (alpha_data->closure_set)
        clutter_alpha_set_closure (alpha, alpha_data->closure);
      else
        clutter_alpha_set_func (alpha, alpha_data->func,
                                alpha_data->data,
                                NULL);

      priv->mode = mode;
    }
  else
    g_assert_not_reached ();

  g_object_notify (G_OBJECT (alpha), "mode");
}

/**
 * clutter_alpha_register_func:
 * @func: a #ClutterAlphaFunc
 * @data: user data to pass to @func, or %NULL
 *
 * Registers a global alpha function and returns its logical id
 * to be used by clutter_alpha_set_mode() or by #ClutterAnimation.
 *
 * The logical id is always greater than %CLUTTER_ANIMATION_LAST.
 *
 * Return value: the logical id of the alpha function
 *
 * Since: 1.0
 */
gulong
clutter_alpha_register_func (ClutterAlphaFunc func,
                             gpointer         data)
{
  AlphaData *alpha_data;

  g_return_val_if_fail (func != NULL, 0);

  alpha_data = g_slice_new (AlphaData);
  alpha_data->closure_set = FALSE;
  alpha_data->func = func;
  alpha_data->data = data;

  if (G_UNLIKELY (clutter_alphas == NULL))
    clutter_alphas = g_ptr_array_new ();

  g_ptr_array_add (clutter_alphas, alpha_data);

  return clutter_alphas->len + CLUTTER_ANIMATION_LAST;
}

/**
 * clutter_alpha_register_closure:
 * @closure: a #GClosure
 *
 * #GClosure variant of clutter_alpha_register_func().
 *
 * Registers a global alpha function and returns its logical id
 * to be used by clutter_alpha_set_mode() or by #ClutterAnimation.
 *
 * The logical id is always greater than %CLUTTER_ANIMATION_LAST.
 *
 * Return value: the logical id of the alpha function
 *
 * Since: 1.0
 */
gulong
clutter_alpha_register_closure (GClosure *closure)
{
  AlphaData *data;

  g_return_val_if_fail (closure != NULL, 0);

  data = g_slice_new (AlphaData);
  data->closure_set = TRUE;
  data->closure = closure;

  if (G_UNLIKELY (clutter_alphas == NULL))
    clutter_alphas = g_ptr_array_new ();

  g_ptr_array_add (clutter_alphas, data);

  return clutter_alphas->len + CLUTTER_ANIMATION_LAST;
}
