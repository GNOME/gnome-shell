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
 * Copyright (C) 2006, 2007 OpenedHand
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
 * #ClutterAlpha is a class for calculating an integer value between
 * 0 and %CLUTTER_ALPHA_MAX_ALPHA as a function of time.  You should
 * provide a #ClutterTimeline and bind it to the #ClutterAlpha object;
 * you should also provide a function returning the alpha value depending
 * on the position inside the timeline; this function will be executed
 * each time a new frame in the #ClutterTimeline is reached.  Since the
 * alpha function is controlled by the timeline instance, you can pause
 * or stop the #ClutterAlpha from calling the alpha function by controlling
 * the #ClutterTimeline object.
 *
 * #ClutterAlpha is used to "drive" a #ClutterBehaviour instance.
 *
 * <figure id="alpha-functions">
 *   <title>Graphic representation of some alpha functions</title>
 *   <graphic fileref="alpha-func.png" format="PNG"/>
 * </figure>
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

  guint32 alpha;

  GClosure *closure;

  ClutterAnimationMode mode;
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
      clutter_alpha_set_mode (alpha, g_value_get_enum (value));
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
      g_value_set_uint (value, priv->alpha);
      break;
    case PROP_MODE:
      g_value_set_enum (value, priv->mode);
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
  g_object_class_install_property (object_class,
                                   PROP_TIMELINE,
                                   g_param_spec_object ("timeline",
                                                        "Timeline",
                                                        "Timeline",
                                                        CLUTTER_TYPE_TIMELINE,
                                                        CLUTTER_PARAM_READWRITE));
  /**
   * ClutterAlpha:alpha:
   *
   * The alpha value as computed by the alpha function.
   *
   * Since: 0.2
   */
  g_object_class_install_property (object_class,
                                   PROP_ALPHA,
                                   g_param_spec_uint ("alpha",
                                                      "Alpha value",
                                                      "Alpha value",
                                                      0, 
                                                      CLUTTER_ALPHA_MAX_ALPHA,
                                                      0,
                                                      CLUTTER_PARAM_READABLE));
  /**
   * ClutterAlpha:mode:
   *
   * The progress function as a #ClutterAnimationMode enumeration
   * value. If %CLUTTER_CUSTOM_MODE is used then the function set
   * using clutter_alpha_set_closure() or clutter_alpha_set_func()
   * will be used.
   *
   * Since: 1.0
   */
  g_object_class_install_property (object_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode",
                                                      "Mode",
                                                      "Progress mode",
                                                      CLUTTER_TYPE_ANIMATION_MODE,
                                                      CLUTTER_CUSTOM_MODE,
                                                      G_PARAM_CONSTRUCT |
                                                      CLUTTER_PARAM_READWRITE));
}

static void
clutter_alpha_init (ClutterAlpha *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
					    CLUTTER_TYPE_ALPHA,
					    ClutterAlphaPrivate);

  self->priv->mode = CLUTTER_CUSTOM_MODE;
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
guint32
clutter_alpha_get_alpha (ClutterAlpha *alpha)
{
  ClutterAlphaPrivate *priv;
  guint32 retval = 0;

  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), 0);

  priv = alpha->priv;

  if (G_LIKELY (priv->closure))
    {
      GValue params = { 0, };
      GValue result_value = { 0, };

      g_object_ref (alpha);

      g_value_init (&result_value, G_TYPE_UINT);

      g_value_init (&params, CLUTTER_TYPE_ALPHA);
      g_value_set_object (&params, alpha);
		     
      g_closure_invoke (priv->closure,
                        &result_value,
                        1,
                        &params,
                        NULL);

      retval = g_value_get_uint (&result_value);

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
 * Sets the #GClosure used to compute
 * the alpha value at each frame of the #ClutterTimeline
 * bound to @alpha.
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
      GClosureMarshal marshal = clutter_marshal_UINT__VOID;

      g_closure_set_marshal (closure, marshal);
    }

  priv->mode = CLUTTER_CUSTOM_MODE;
  g_object_notify (G_OBJECT (alpha), "mode");
}

/**
 * clutter_alpha_set_func:
 * @alpha: A #ClutterAlpha
 * @func: A #ClutterAlphaAlphaFunc
 * @data: user data to be passed to the alpha function, or %NULL
 * @destroy: notify function used when disposing the alpha function
 *
 * Sets the #ClutterAlphaFunc function used to compute
 * the alpha value at each frame of the #ClutterTimeline
 * bound to @alpha.
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
 * @func: #ClutterAlphaFunc alpha function
 * @data: data to be passed to the alpha function
 * @destroy: notify to be called when removing the alpha function
 *
 * Creates a new #ClutterAlpha instance and sets the timeline
 * and alpha function.
 *
 * Return Value: the newly created #ClutterAlpha
 *
 * Since: 0.2
 */
ClutterAlpha *
clutter_alpha_new_full (ClutterTimeline  *timeline,
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
 * clutter_alpha_new_for_mode:
 * @mode: a #ClutterAnimationMode value
 *
 * Creates a new #ClutterAlpha using @mode to set the
 * progress function using its symbolic name.
 *
 * Return value: the newly created #ClutterAlpha
 *
 * Since: 1.0
 */
ClutterAlpha *
clutter_alpha_new_for_mode (ClutterAnimationMode mode)
{
  return g_object_new (CLUTTER_TYPE_ALPHA,
                       "mode", mode,
                       NULL);
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
ClutterAnimationMode
clutter_alpha_get_mode (ClutterAlpha *alpha)
{
  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), CLUTTER_CUSTOM_MODE);

  return alpha->priv->mode;
}

/**
 * clutter_alpha_set_mode:
 * @alpha: a #ClutterAlpha
 * @mode: a #ClutterAnimationMode
 *
 * Sets the progress function of @alpha using the symbolic value
 * of @mode, as taken by the #ClutterAnimationMode enumeration
 *
 * Since: 1.0
 */
void
clutter_alpha_set_mode (ClutterAlpha      *alpha,
                        ClutterAnimationMode  mode)
{
  ClutterAlphaPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ALPHA (alpha));

  priv = alpha->priv;

  priv->mode = mode;

  switch (priv->mode)
    {
    case CLUTTER_LINEAR:
      clutter_alpha_set_func (alpha, clutter_ramp_inc_func, NULL, NULL);
      break;

    case CLUTTER_SINE_IN:
      clutter_alpha_set_func (alpha, clutter_sine_in_func, NULL, NULL);
      break;

    case CLUTTER_SINE_OUT:
      clutter_alpha_set_func (alpha, clutter_sine_out_func, NULL, NULL);
      break;

    case CLUTTER_SINE_IN_OUT:
      clutter_alpha_set_func (alpha, clutter_sine_inc_func, NULL, NULL);
      break;

    case CLUTTER_EASE_IN:
      clutter_alpha_set_func (alpha, clutter_ease_in_func, NULL, NULL);
      break;

    case CLUTTER_EASE_OUT:
      clutter_alpha_set_func (alpha, clutter_ease_out_func, NULL, NULL);
      break;

    case CLUTTER_EASE_IN_OUT:
      clutter_alpha_set_func (alpha, clutter_ease_in_out_func, NULL, NULL);
      break;

    case CLUTTER_CUSTOM_MODE:
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  g_object_notify (G_OBJECT (alpha), "mode");
}

/**
 * CLUTTER_ALPHA_RAMP_INC:
 *
 * Convenience symbol for clutter_ramp_inc_func().
 *
 * Since: 0.2
 */

/**
 * clutter_ramp_inc_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a monotonic increasing ramp. You
 * can use this function as the alpha function for clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 0.2
 */
guint32
clutter_ramp_inc_func (ClutterAlpha *alpha,
                       gpointer      dummy)
{
  ClutterTimeline *timeline;
  gint current_frame_num, n_frames;

  timeline = clutter_alpha_get_timeline (alpha);

  current_frame_num = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  return (current_frame_num * CLUTTER_ALPHA_MAX_ALPHA) / n_frames;
}

/**
 * CLUTTER_ALPHA_RAMP_DEC:
 *
 * Convenience symbol for clutter_ramp_dec_func().
 *
 * Since: 0.2
 */

/**
 * clutter_ramp_dec_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a monotonic decreasing ramp. You
 * can use this function as the alpha function for clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 0.2
 */
guint32
clutter_ramp_dec_func (ClutterAlpha *alpha,
                       gpointer      dummy)
{
  ClutterTimeline *timeline;
  gint current_frame_num, n_frames;

  timeline = clutter_alpha_get_timeline (alpha);

  current_frame_num = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  return (n_frames - current_frame_num)
         * CLUTTER_ALPHA_MAX_ALPHA
         / n_frames;
}

/**
 * CLUTTER_ALPHA_RAMP:
 *
 * Convenience symbol for clutter_ramp_func().
 *
 * Since: 0.2
 */

/**
 * clutter_ramp_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a full ramp function (increase for
 * half the time, decrease for the remaining half). You can use this
 * function as the alpha function for clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 0.2
 */
guint32
clutter_ramp_func (ClutterAlpha *alpha,
                   gpointer      dummy)
{
  ClutterTimeline *timeline;
  gint current_frame_num, n_frames;

  timeline = clutter_alpha_get_timeline (alpha);

  current_frame_num = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  if (current_frame_num > (n_frames / 2))
    {
      return (n_frames - current_frame_num)
             * CLUTTER_ALPHA_MAX_ALPHA
             / (n_frames / 2);
    }
  else
    {
      return current_frame_num
             * CLUTTER_ALPHA_MAX_ALPHA
             / (n_frames / 2);
    }
}

static guint32
sincx1024_func (ClutterAlpha *alpha, 
		ClutterAngle  angle,
		ClutterFixed  offset)
{
  ClutterTimeline *timeline;
  gint current_frame_num, n_frames;
  ClutterAngle x;
  ClutterFixed sine;
  
  timeline = clutter_alpha_get_timeline (alpha);

  current_frame_num = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  x = angle * current_frame_num / n_frames;

  x -= (512 * 512 / angle);
  
  sine = ((cogl_angle_sin (x) + offset) / 2)
       * CLUTTER_ALPHA_MAX_ALPHA;

  sine = sine >> COGL_FIXED_Q;

  return sine;
}

#if 0
/*
 * The following two functions are left in place for reference
 * purposes.
 */
static guint32
sincx_func (ClutterAlpha *alpha, 
	    ClutterFixed  angle,
	    ClutterFixed  offset)
{
  ClutterTimeline *timeline;
  gint current_frame_num, n_frames;
  ClutterFixed x, sine;
  
  timeline = clutter_alpha_get_timeline (alpha);

  current_frame_num = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  x = angle * current_frame_num / n_frames;
  x = COGL_FIXED_FAST_MUL (x, COGL_FIXED_PI)
    - COGL_FIXED_FAST_DIV (COGL_FIXED_PI, angle);

  sine = (cogl_fixed_sin (x) + offset) / 2;

  CLUTTER_NOTE (ALPHA, "sine: %2f\n", COGL_FIXED_TO_DOUBLE (sine));

  return COGL_FIXED_TO_INT (sine * CLUTTER_ALPHA_MAX_ALPHA);
}

/* NB: angle is not in radians but in muliples of PI, i.e., 2.0
 * represents full circle.
 */
static guint32
sinc_func (ClutterAlpha *alpha, 
	   float         angle,
	   float         offset)
{
  ClutterTimeline *timeline;
  gint current_frame_num, n_frames;
  gdouble x, sine;
  
  timeline = clutter_alpha_get_timeline (alpha);

  current_frame_num = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  /* FIXME: fixed point, and fixed point sine() */

  x = (gdouble) (current_frame_num * angle * G_PI) / n_frames ;
  sine = (sin (x - (G_PI / angle)) + offset) * 0.5f;

  CLUTTER_NOTE (ALPHA, "sine: %2f\n",sine);

  return COGL_FLOAT_TO_INT ((sine * (gdouble) CLUTTER_ALPHA_MAX_ALPHA));
}
#endif

/**
 * CLUTTER_ALPHA_SINE:
 *
 * Convenience symbol for clutter_sine_func().
 *
 * Since: 0.2
 */

/**
 * clutter_sine_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a sine wave. You can use this
 * function as the alpha function for clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 0.2
 */
guint32 
clutter_sine_func (ClutterAlpha *alpha,
                   gpointer      dummy)
{
#if 0
    return sinc_func (alpha, 2.0, 1.0);
#else
    /* 2.0 above represents full circle */
    return sincx1024_func (alpha, 1024, COGL_FIXED_1);
#endif
}

/**
 * CLUTTER_ALPHA_SINE_INC:
 *
 * Convenience symbol for clutter_sine_inc_func().
 *
 * Since: 0.2
 */

/**
 * clutter_sine_inc_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a sine wave over interval [0, pi / 2].
 * You can use this function as the alpha function for
 * clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 0.2
 */
guint32 
clutter_sine_inc_func (ClutterAlpha *alpha,
		       gpointer      dummy)
{
  ClutterTimeline * timeline;
  gint              frame;
  gint              n_frames;
  ClutterAngle      x;
  ClutterFixed      sine;
  
  timeline = clutter_alpha_get_timeline (alpha);
  frame    = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  x = 256 * frame / n_frames;

  sine = cogl_angle_sin (x) * CLUTTER_ALPHA_MAX_ALPHA;

  return ((guint32) sine) >> COGL_FIXED_Q;
}

/**
 * CLUTTER_ALPHA_SINE_DEC:
 *
 * Convenience symbol for clutter_sine_dec_func().
 *
 * Since: 0.2
 */

/**
 * clutter_sine_dec_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a sine wave over interval [pi / 2, pi].
 * You can use this function as the alpha function for
 * clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 0.4
 */
guint32 
clutter_sine_dec_func (ClutterAlpha *alpha,
		       gpointer      dummy)
{
  ClutterTimeline * timeline;
  gint              frame;
  gint              n_frames;
  ClutterAngle      x;
  ClutterFixed      sine;
  
  timeline = clutter_alpha_get_timeline (alpha);
  frame    = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  x = 256 * frame / n_frames + 256;

  sine = cogl_angle_sin (x) * CLUTTER_ALPHA_MAX_ALPHA;

  return ((guint32) sine) >> COGL_FIXED_Q;
}

/**
 * CLUTTER_ALPHA_SINE_HALF:
 *
 * Convenience symbol for clutter_sine_half_func().
 *
 * Since: 0.4
 */

/**
 * clutter_sine_half_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a sine wave over interval [0, pi].
 * You can use this function as the alpha function for
 * clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 0.4
 */
guint32 
clutter_sine_half_func (ClutterAlpha *alpha,
			gpointer      dummy)
{
  ClutterTimeline *timeline;
  gint             frame;
  gint             n_frames;
  ClutterAngle     x;
  ClutterFixed     sine;
  
  timeline = clutter_alpha_get_timeline (alpha);
  frame    = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  x = 512 * frame / n_frames;

  sine = cogl_angle_sin (x) * CLUTTER_ALPHA_MAX_ALPHA;

  return ((guint32) sine) >> COGL_FIXED_Q;
}

/**
 * clutter_sine_in_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for (sin(x) + 1) over the
 * interval [-pi/2, 0].
 *
 * You can use this function as the alpha function for
 * clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 1.0
 */
guint32
clutter_sine_in_func (ClutterAlpha *alpha,
                      gpointer      dummy)
{
  ClutterTimeline *timeline;
  gint             frame;
  gint             n_frames;
  ClutterAngle     x;
  ClutterFixed     sine;

  timeline = clutter_alpha_get_timeline (alpha);
  frame    = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  /* XXX- if we use 768 we overflow */
  x = 256 * frame / n_frames + 767;

  sine = (cogl_angle_sin (x) + 1) * CLUTTER_ALPHA_MAX_ALPHA;

  return ((guint32) sine) >> COGL_FIXED_Q;
}

/**
 * clutter_sine_in_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for sin(x) over the interval [0, pi/2].
 *
 * You can use this function as the alpha function for
 * clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 1.0
 */
guint32
clutter_sine_out_func (ClutterAlpha *alpha,
                       gpointer      dummy)
{
  ClutterTimeline *timeline;
  gint             frame;
  gint             n_frames;
  ClutterAngle     x;
  ClutterFixed     sine;

  timeline = clutter_alpha_get_timeline (alpha);
  frame    = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  x = 256 * frame / n_frames;

  sine = cogl_angle_sin (x) * CLUTTER_ALPHA_MAX_ALPHA;

  return ((guint32) sine) >> COGL_FIXED_Q;
}

/**
 * clutter_sine_in_out_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for (sin(x) + 1) / 2 over the
 * interval [-pi/2, pi/2].
 *
 * You can use this function as the alpha function for
 * clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 1.0
 */
guint32
clutter_sine_in_out_func (ClutterAlpha *alpha,
                          gpointer      dummy)
{
  ClutterTimeline *timeline;
  gint             frame;
  gint             n_frames;
  ClutterAngle     x;
  ClutterFixed     sine;

  timeline = clutter_alpha_get_timeline (alpha);
  frame    = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  x = -256 * frame / n_frames + 256;

  sine = (cogl_angle_sin (x) + 1) / 2 * CLUTTER_ALPHA_MAX_ALPHA;

  return ((guint32) sine) >> COGL_FIXED_Q;
}

/**
 * CLUTTER_ALPHA_SQUARE:
 *
 * Convenience symbol for clutter_square_func().
 *
 * Since: 0.4
 *
 * Deprecated: 1.0: Use clutter_square_func() instead
 */

/**
 * clutter_square_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a square wave. You can use this
 * function as the alpha function for clutter_alpha_set_func().
 *
 * Return value: an alpha value
 *
 * Since: 0.4
 */
guint32
clutter_square_func (ClutterAlpha *alpha,
                     gpointer      dummy)
{
  ClutterTimeline *timeline;
  gint current_frame_num, n_frames;

  timeline = clutter_alpha_get_timeline (alpha);

  current_frame_num = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  return (current_frame_num > (n_frames / 2)) ? CLUTTER_ALPHA_MAX_ALPHA
                                              : 0;
}

/**
 * CLUTTER_ALPHA_SMOOTHSTEP_INC:
 *
 * Convenience symbol for clutter_smoothstep_inc_func().
 *
 * Since: 0.4
 */

/**
 * clutter_smoothstep_inc_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused
 *
 * Convenience alpha function for a smoothstep curve. You can use this
 * function as the alpha function for clutter_alpha_set_func().
 *
 * Return value: an alpha value
 *
 * Since: 0.4
 */
guint32
clutter_smoothstep_inc_func (ClutterAlpha  *alpha,
			     gpointer       dummy)
{
  ClutterTimeline    *timeline;
  gint                frame;
  gint                n_frames;
  guint32             r; 
  guint32             x; 

  /*
   * The smoothstep function uses f(x) = -2x^3 + 3x^2 where x is from <0,1>,
   * and precission is critical -- we use 8.24 fixed format for this operation.
   * The earlier operations involve division, which we cannot do in 8.24 for
   * numbers in <0,1> we use ClutterFixed.
   */
  timeline = clutter_alpha_get_timeline (alpha);
  frame    = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  /*
   * Convert x to 8.24 for next step.
   */
  x = COGL_FIXED_FAST_DIV (frame, n_frames) << 8;

  /*
   * f(x) = -2x^3 + 3x^2
   * 
   * Convert result to ClutterFixed to avoid overflow in next step.
   */
  r = ((x >> 12) * (x >> 12) * 3 - (x >> 15) * (x >> 16) * (x >> 16)) >> 8;

  return COGL_FIXED_TO_INT (r * CLUTTER_ALPHA_MAX_ALPHA);
}

/**
 * CLUTTER_ALPHA_SMOOTHSTEP_DEC:
 *
 * Convenience symbol for clutter_smoothstep_dec_func().
 *
 * Since: 0.4
 */

/**
 * clutter_smoothstep_dec_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused
 *
 * Convenience alpha function for a downward smoothstep curve. You can use
 * this function as the alpha function for clutter_alpha_set_func().
 *
 * Return value: an alpha value
 *
 * Since: 0.4
 */
guint32
clutter_smoothstep_dec_func (ClutterAlpha  *alpha,
			     gpointer       dummy)
{
  return CLUTTER_ALPHA_MAX_ALPHA - clutter_smoothstep_inc_func (alpha, dummy);
}

/**
 * CLUTTER_ALPHA_EXP_INC:
 *
 * Convenience symbol for clutter_exp_inc_func()
 *
 * Since: 0.4
 */

/**
 * clutter_exp_inc_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a 2^x curve. You can use this function as the
 * alpha function for clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 0.4
 */
guint32 
clutter_exp_inc_func (ClutterAlpha *alpha,
		      gpointer      dummy)
{
  ClutterTimeline * timeline;
  gint              frame;
  gint              n_frames;
  ClutterFixed      x;
  ClutterFixed      x_alpha_max = 0x100000;
  guint32           result;
  
  /*
   * Choose x_alpha_max such that
   * 
   *   (2^x_alpha_max) - 1 == CLUTTER_ALPHA_MAX_ALPHA
   */
#if CLUTTER_ALPHA_MAX_ALPHA != 0xffff
#error Adjust x_alpha_max to match CLUTTER_ALPHA_MAX_ALPHA
#endif
  
  timeline = clutter_alpha_get_timeline (alpha);
  frame    = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  x =  x_alpha_max * frame / n_frames;

  result = CLAMP (cogl_fixed_pow2 (x) - 1, 0, CLUTTER_ALPHA_MAX_ALPHA);

  return result;
}

/**
 * CLUTTER_ALPHA_EXP_DEC:
 *
 * Convenience symbold for clutter_exp_dec_func().
 *
 * Since: 0.4
 */

/**
 * clutter_exp_dec_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a decreasing 2^x curve. You can use this
 * function as the alpha function for clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 0.4
 */
guint32 
clutter_exp_dec_func (ClutterAlpha *alpha,
		      gpointer      dummy)
{
  ClutterTimeline * timeline;
  gint              frame;
  gint              n_frames;
  ClutterFixed      x;
  ClutterFixed      x_alpha_max = 0x100000;
  guint32           result;
  
  /*
   * Choose x_alpha_max such that
   * 
   *   (2^x_alpha_max) - 1 == CLUTTER_ALPHA_MAX_ALPHA
   */
#if CLUTTER_ALPHA_MAX_ALPHA != 0xffff
#error Adjust x_alpha_max to match CLUTTER_ALPHA_MAX_ALPHA
#endif
  
  timeline = clutter_alpha_get_timeline (alpha);
  frame    = clutter_timeline_get_current_frame (timeline);
  n_frames = clutter_timeline_get_n_frames (timeline);

  x =  (x_alpha_max * (n_frames - frame)) / n_frames;

  result = CLAMP (cogl_fixed_pow2 (x) - 1, 0, CLUTTER_ALPHA_MAX_ALPHA);

  return result;
}

static inline gdouble
clutter_cubic_bezier (ClutterAlpha *alpha,
                      gdouble       x_1,
                      gdouble       y_1,
                      gdouble       x_2,
                      gdouble       y_2)
{
  ClutterTimeline *timeline;
  gdouble t, b_t, res;

  /* the cubic bezier has a parametric form of:
   *
   * B(t) =        (1 - t)^3 * P_0
   *      + 3t   * (1 - t)^2 * P_1
   *      + 3t^2 * (1 - t)   * P_2
   *      + 3t^3             * P_3      (with t included in [0, 1])
   *
   * the P_0 and P_3 points are set to (0, 0) and (1, 1) respectively,
   * and the curve never passes through P_1 and P_2 - with these two
   * points merely acting as control points for the curve starting
   * from P_0 and ending at P_3.
   *
   * since the starting point is (0, 0) we can simplify the previous
   * parametric form to:
   *
   * B(t) = 3t   * (1 - t)^2 * P_1
   *      + 3t^2 * (1 - t)   * P_2
   *      + 3t^3             * P_3      (with t included in [0, 1])
   *
   * and, similarly, since the final point is (1, 1) we can simplify
   * it further to:
   *
   * B(t) = 3t   * (1 - t)^2 * P_1
   *      + 3t^2 * (1 - t)   * P_2
   *      + 3t^3                        (with t included in [0, 1])
   *
   * since an alpha function has only a time parameter and we have two
   * coordinates for each point, we pass the time as the first
   * coordinate for the point and then we solve the cubic beziér curve
   * for the second coordinate at the same point.
   */

  timeline  = clutter_alpha_get_timeline (alpha);
  t = clutter_timeline_get_progress (timeline);

  b_t = 3 * t          * pow (1 - t, 2) * x_1
      + 3 * pow (t, 2) * (1 - t)        * x_2
      + 3 * pow (t, 3);

  res = 3 * b_t          * pow (1 - b_t, 2) * y_1
      + 3 * pow (b_t, 2) * (1 - b_t)        * y_2
      + 3 * pow (b_t, 3);

  return res;
}

/**
 * clutter_ease_in_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a cubic Beziér curve with control
 * points at (0.42, 0) and (1, 0). You can use this function as the
 * alpha function for clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 1.0
 */
guint32
clutter_ease_in_func (ClutterAlpha *alpha,
                      gpointer      dummy)
{
  gdouble res;

  res = clutter_cubic_bezier (alpha, 0.42, 0, 1, 0);

  return CLAMP (res * CLUTTER_ALPHA_MAX_ALPHA, 0, CLUTTER_ALPHA_MAX_ALPHA);
}

/**
 * clutter_ease_out_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a cubic Beziér curve with control
 * points at (0, 0) and (0.58, 1). You can use this function as the
 * alpha function for clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 1.0
 */
guint32
clutter_ease_out_func (ClutterAlpha *alpha,
                       gpointer      dummy)
{
  gdouble res;

  res = clutter_cubic_bezier (alpha, 0, 0, 0.58, 1);

  return CLAMP (res * CLUTTER_ALPHA_MAX_ALPHA, 0, CLUTTER_ALPHA_MAX_ALPHA);
}

/**
 * clutter_ease_in_out_func:
 * @alpha: a #ClutterAlpha
 * @dummy: unused argument
 *
 * Convenience alpha function for a cubic Beziér curve with control
 * points at (0.42, 0) and (0.58, 1). You can use this function as
 * the alpha function for clutter_alpha_set_func().
 *
 * Return value: an alpha value.
 *
 * Since: 1.0
 */
guint32
clutter_ease_in_out_func (ClutterAlpha *alpha,
                          gpointer      dummy)
{
  gdouble res;

  res = clutter_cubic_bezier (alpha, 0.42, 0, 0.58, 1);

  return CLAMP (res * CLUTTER_ALPHA_MAX_ALPHA, 0, CLUTTER_ALPHA_MAX_ALPHA);
}
