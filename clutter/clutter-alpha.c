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
 * "animation mode", either by using the #ClutterAnimationMode values that
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
 * <figure id="easing-modes">
 *   <title>Easing modes provided by Clutter</title>
 *   <graphic fileref="easing-modes.png" format="PNG"/>
 * </figure>
 *
 * <refsect2 id="ClutterAlpha-script">
 *   <title>ClutterAlpha custom properties for #ClutterScript</title>
 *   <para>#ClutterAlpha defines a custom "function" property for
 *   #ClutterScript which allows to reference a custom alpha function
 *   available in the source code. Setting the "function" property
 *   is equivalent to calling clutter_alpha_set_func() with the
 *   specified function name. No user data or #GDestroyNotify is
 *   available to be passed.</para>
 *   <example id="ClutterAlpha-script-example">
 *     <title>Defining a ClutterAlpha in ClutterScript</title>
 *     <para>The following JSON fragment defines a #ClutterAlpha
 *     using a #ClutterTimeline with id "sine-timeline" and an alpha
 *     function called my_sine_alpha(). The defined #ClutterAlpha
 *     instance can be reused in multiple #ClutterBehaviour
 *     definitions or for #ClutterAnimation definitions.</para>
 *     <programlisting>
 *  {
 *    "id" : "sine-alpha",
 *    "timeline" : {
 *      "id" : "sine-timeline",
 *      "duration" : 500,
 *      "loop" : true
 *    },
 *    "function" : "my_sine_alpha"
 *  }
 *     </programlisting>
 *   </example>
 *   <para>For the way to define the #ClutterAlpha:mode property
 *   inside a ClutterScript fragment, see <link
 *   linkend="clutter-AnimationMode-Script">the corresponding section</link>
 *   in #ClutterAnimation.</para>
 * </refsect2>
 *
 * Since: 0.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include <gmodule.h>

#include "clutter-alpha.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-scriptable.h"
#include "clutter-script-private.h"

static void clutter_scriptable_iface_init (ClutterScriptableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterAlpha,
                         clutter_alpha,
                         G_TYPE_INITIALLY_UNOWNED,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_SCRIPTABLE,
                                                clutter_scriptable_iface_init));

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
  ClutterAlpha *alpha = CLUTTER_ALPHA (object);

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
  ClutterAlphaPrivate *priv = CLUTTER_ALPHA (object)->priv;

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

static ClutterAlphaFunc
resolve_alpha_func (const gchar *name)
{
  static GModule *module = NULL;
  ClutterAlphaFunc func;

  CLUTTER_NOTE (SCRIPT, "Looking up '%s' alpha function", name);

  if (G_UNLIKELY (module == NULL))
    module = g_module_open (NULL, 0);

  if (g_module_symbol (module, name, (gpointer) &func))
    {
      CLUTTER_NOTE (SCRIPT, "Found '%s' alpha function in the symbols table",
                    name);
      return func;
    }

  return NULL;
}

static void
clutter_alpha_set_custom_property (ClutterScriptable *scriptable,
                                   ClutterScript     *script,
                                   const gchar       *name,
                                   const GValue      *value)
{
  if (strncmp (name, "function", 8) == 0)
    {
      g_assert (G_VALUE_HOLDS (value, G_TYPE_POINTER));
      if (g_value_get_pointer (value) != NULL)
        {
          clutter_alpha_set_func (CLUTTER_ALPHA (scriptable),
                                  g_value_get_pointer (value),
                                  NULL, NULL);
        }
    }
  else
    g_object_set_property (G_OBJECT (scriptable), name, value);
}

static gboolean
clutter_alpha_parse_custom_node (ClutterScriptable *scriptable,
                                 ClutterScript     *script,
                                 GValue            *value,
                                 const gchar       *name,
                                 JsonNode          *node)
{
  if (strncmp (name, "function", 8) == 0)
    {
      const gchar *func_name = json_node_get_string (node);

      g_value_init (value, G_TYPE_POINTER);
      g_value_set_pointer (value, resolve_alpha_func (func_name));

      return TRUE;
    }

  /* we need to do this because we use gulong in place
   * of ClutterAnimationMode for ClutterAlpha:mode
   */
  if (strncmp (name, "mode", 4) == 0)
    {
      gulong mode;

      mode = clutter_script_resolve_animation_mode (node);

      g_value_init (value, G_TYPE_ULONG);
      g_value_set_ulong (value, mode);

      return TRUE;
    }

  return FALSE;
}

static void
clutter_scriptable_iface_init (ClutterScriptableIface *iface)
{
  iface->parse_custom_node = clutter_alpha_parse_custom_node;
  iface->set_custom_property = clutter_alpha_set_custom_property;
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

/*
 * clutter_alpha_set_closure_internal:
 * @alpha: a #ClutterAlpha
 * @closure: a #GClosure
 *
 * Sets the @closure for @alpha. This function does not
 * set the #ClutterAlpha:mode property and does not emit
 * the #GObject::notify signal for it.
 */
static inline void
clutter_alpha_set_closure_internal (ClutterAlpha *alpha,
                                    GClosure     *closure)
{
  ClutterAlphaPrivate *priv = alpha->priv;

  if (priv->closure)
    g_closure_unref (priv->closure);

  /* need to take ownership of the closure before sinking it */
  priv->closure = g_closure_ref (closure);
  g_closure_sink (closure);

  /* set the marshaller */
  if (G_CLOSURE_NEEDS_MARSHAL (closure))
    {
      GClosureMarshal marshal = clutter_marshal_DOUBLE__VOID;

      g_closure_set_marshal (closure, marshal);
    }
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

  clutter_alpha_set_closure_internal (alpha, closure);

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
  ClutterAlphaPrivate *priv;
  GClosure *closure;

  g_return_if_fail (CLUTTER_IS_ALPHA (alpha));
  g_return_if_fail (func != NULL);

  priv = alpha->priv;

  closure = g_cclosure_new (G_CALLBACK (func), data, (GClosureNotify) destroy);
  clutter_alpha_set_closure_internal (alpha, closure);

  priv->mode = CLUTTER_CUSTOM_MODE;
  g_object_notify (G_OBJECT (alpha), "mode");
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
 * Return value: (transfer none): a #ClutterTimeline instance
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
 * Retrieves the #ClutterAnimationMode used by @alpha.
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

static gdouble
clutter_linear (ClutterAlpha *alpha,
                gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;

  return clutter_timeline_get_progress (timeline);
}

static gdouble
clutter_ease_in_quad (ClutterAlpha *alpha,
                      gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble progress = clutter_timeline_get_progress (timeline);

  return progress * progress;
}

static gdouble
clutter_ease_out_quad (ClutterAlpha *alpha,
                       gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble p = clutter_timeline_get_progress (timeline);

  return -1.0 * p * (p - 2);
}

static gdouble
clutter_ease_in_out_quad (ClutterAlpha *alpha,
                          gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = t / (d / 2);

  if (p < 1)
    return 0.5 * p * p;

  p -= 1;

  return -0.5 * (p * (p - 2) - 1);
}

static gdouble
clutter_ease_in_cubic (ClutterAlpha *alpha,
                       gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble p = clutter_timeline_get_progress (timeline);

  return p * p * p;
}

static gdouble
clutter_ease_out_cubic (ClutterAlpha *alpha,
                        gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = t / d - 1;

  return p * p * p + 1;
}

static gdouble
clutter_ease_in_out_cubic (ClutterAlpha *alpha,
                           gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = t / (d / 2);

  if (p < 1)
    return 0.5 * p * p * p;

  p -= 2;

  return 0.5 * (p * p * p + 2);
}

static gdouble
clutter_ease_in_quart (ClutterAlpha *alpha,
                       gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble p = clutter_timeline_get_progress (timeline);

  return p * p * p * p;
}

static gdouble
clutter_ease_out_quart (ClutterAlpha *alpha,
                        gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = t / d - 1;

  return -1.0 * (p * p * p * p - 1);
}

static gdouble
clutter_ease_in_out_quart (ClutterAlpha *alpha,
                           gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = t / (d / 2);

  if (p < 1)
    return 0.5 * p * p * p * p;

  p -= 2;

  return -0.5 * (p * p * p * p - 2);
}

static gdouble
clutter_ease_in_quint (ClutterAlpha *alpha,
                       gpointer      dummy G_GNUC_UNUSED)
 {
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble p = clutter_timeline_get_progress (timeline);

  return p * p * p * p * p;
}

static gdouble
clutter_ease_out_quint (ClutterAlpha *alpha,
                        gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = t / d - 1;

  return p * p * p * p * p + 1;
}

static gdouble
clutter_ease_in_out_quint (ClutterAlpha *alpha,
                           gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = t / (d / 2);

  if (p < 1)
    return 0.5 * p * p * p * p * p;

  p -= 2;

  return 0.5 * (p * p * p * p * p + 2);
}

static gdouble
clutter_ease_in_sine (ClutterAlpha *alpha,
                      gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);

  return -1.0 * cos (t / d * G_PI_2) + 1.0;
}

static gdouble
clutter_ease_out_sine (ClutterAlpha *alpha,
                       gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);

  return sin (t / d * G_PI_2);
}

static gdouble
clutter_ease_in_out_sine (ClutterAlpha *alpha,
                          gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);

  return -0.5 * (cos (G_PI * t / d) - 1);
}

static gdouble
clutter_ease_in_expo (ClutterAlpha *alpha,
                      gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);

  return (t == 0) ? 0.0 : pow (2, 10 * (t / d - 1));
}

static gdouble
clutter_ease_out_expo (ClutterAlpha *alpha,
                       gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);

  return (t == d) ? 1.0 : -pow (2, -10 * t / d) + 1;
}

static gdouble
clutter_ease_in_out_expo (ClutterAlpha *alpha,
                          gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p;

  if (t == 0)
    return 0.0;

  if (t == d)
    return 1.0;

  p = t / (d / 2);

  if (p < 1)
    return 0.5 * pow (2, 10 * (p - 1));

  p -= 1;

  return 0.5 * (-pow (2, -10 * p) + 2);
}

static gdouble
clutter_ease_in_circ (ClutterAlpha *alpha,
                      gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble p = clutter_timeline_get_progress (timeline);

  return -1.0 * (sqrt (1 - p * p) - 1);
}

static gdouble
clutter_ease_out_circ (ClutterAlpha *alpha,
                       gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = t / d - 1;

  return sqrt (1 - p * p);
}

static gdouble
clutter_ease_in_out_circ (ClutterAlpha *alpha,
                          gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = t / (d / 2);

  if (p < 1)
    return -0.5 * (sqrt (1 - p * p) - 1);

  p -= 2;

  return 0.5 * (sqrt (1 - p * p) + 1);
}

static gdouble
clutter_ease_in_elastic (ClutterAlpha *alpha,
                         gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = d * .3;
  gdouble s = p / 4;
  gdouble q = t / d;

  if (q == 1)
    return 1.0;

  q -= 1;

  return -(pow (2, 10 * q) * sin ((q * d - s) * (2 * G_PI) / p));
}

static gdouble
clutter_ease_out_elastic (ClutterAlpha *alpha,
                          gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = d * .3;
  gdouble s = p / 4;
  gdouble q = t / d;

  if (q == 1)
    return 1.0;

  return pow (2, -10 * q) * sin ((q * d - s) * (2 * G_PI) / p) + 1.0;
}

static gdouble
clutter_ease_in_out_elastic (ClutterAlpha *alpha,
                             gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = d * (.3 * 1.5);
  gdouble s = p / 4;
  gdouble q = t / (d / 2);

  if (q == 2)
    return 1.0;

  if (q < 1)
    {
      q -= 1;

      return -.5 * (pow (2, 10 * q) * sin ((q * d - s) * (2 * G_PI) / p));
    }
  else
    {
      q -= 1;

      return pow (2, -10 * q)
           * sin ((q * d - s) * (2 * G_PI) / p)
           * .5 + 1.0;
    }
}

static gdouble
clutter_ease_in_back (ClutterAlpha *alpha,
                      gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble p = clutter_timeline_get_progress (timeline);

  return p * p * ((1.70158 + 1) * p - 1.70158);
}

static gdouble
clutter_ease_out_back (ClutterAlpha *alpha,
                       gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = t / d - 1;

  return p * p * ((1.70158 + 1) * p + 1.70158) + 1;
}

static gdouble
clutter_ease_in_out_back (ClutterAlpha *alpha,
                          gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);
  gdouble p = t / (d / 2);
  gdouble s = 1.70158 * 1.525;

  if (p < 1)
    return 0.5 * (p * p * ((s + 1) * p - s));

  p -= 2;

  return 0.5 * (p * p * ((s + 1) * p + s) + 2);
}

static gdouble
ease_out_bounce_internal (gdouble t,
                          gdouble d)
{
  gdouble p = t / d;

  if (p < (1 / 2.75))
    return 7.5625 * p * p;
  else if (p < (2 / 2.75))
    {
      p -= (1.5 / 2.75);

      return 7.5625 * p * p + .75;
    }
  else if (p < (2.5 / 2.75))
    {
      p -= (2.25 / 2.75);

      return 7.5625 * p * p + .9375;
    }
  else
    {
      p -= (2.625 / 2.75);

      return 7.5625 * p * p + .984375;
    }
}

static gdouble
ease_in_bounce_internal (gdouble t,
                         gdouble d)
{
  return 1.0 - ease_out_bounce_internal (d - t, d);
}

static gdouble
clutter_ease_in_bounce (ClutterAlpha *alpha,
                        gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);

  return ease_in_bounce_internal (t, d);
}

static gdouble
clutter_ease_out_bounce (ClutterAlpha *alpha,
                         gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);

  return ease_out_bounce_internal (t, d);
}

static gdouble
clutter_ease_in_out_bounce (ClutterAlpha *alpha,
                            gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = alpha->priv->timeline;
  gdouble t = clutter_timeline_get_elapsed_time (timeline);
  gdouble d = clutter_timeline_get_duration (timeline);

  if (t < d / 2)
    return ease_in_bounce_internal (t * 2, d) * 0.5;
  else
    return ease_out_bounce_internal (t * 2 - d, d) * 0.5 + 1.0 * 0.5;
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
  { CLUTTER_CUSTOM_MODE,         NULL },

  { CLUTTER_LINEAR,              clutter_linear },
  { CLUTTER_EASE_IN_QUAD,        clutter_ease_in_quad },
  { CLUTTER_EASE_OUT_QUAD,       clutter_ease_out_quad },
  { CLUTTER_EASE_IN_OUT_QUAD,    clutter_ease_in_out_quad },
  { CLUTTER_EASE_IN_CUBIC,       clutter_ease_in_cubic },
  { CLUTTER_EASE_OUT_CUBIC,      clutter_ease_out_cubic },
  { CLUTTER_EASE_IN_OUT_CUBIC,   clutter_ease_in_out_cubic },
  { CLUTTER_EASE_IN_QUART,       clutter_ease_in_quart },
  { CLUTTER_EASE_OUT_QUART,      clutter_ease_out_quart },
  { CLUTTER_EASE_IN_OUT_QUART,   clutter_ease_in_out_quart },
  { CLUTTER_EASE_IN_QUINT,       clutter_ease_in_quint },
  { CLUTTER_EASE_OUT_QUINT,      clutter_ease_out_quint },
  { CLUTTER_EASE_IN_OUT_QUINT,   clutter_ease_in_out_quint },
  { CLUTTER_EASE_IN_SINE,        clutter_ease_in_sine },
  { CLUTTER_EASE_OUT_SINE,       clutter_ease_out_sine },
  { CLUTTER_EASE_IN_OUT_SINE,    clutter_ease_in_out_sine },
  { CLUTTER_EASE_IN_EXPO,        clutter_ease_in_expo },
  { CLUTTER_EASE_OUT_EXPO,       clutter_ease_out_expo },
  { CLUTTER_EASE_IN_OUT_EXPO,    clutter_ease_in_out_expo },
  { CLUTTER_EASE_IN_CIRC,        clutter_ease_in_circ },
  { CLUTTER_EASE_OUT_CIRC,       clutter_ease_out_circ },
  { CLUTTER_EASE_IN_OUT_CIRC,    clutter_ease_in_out_circ },
  { CLUTTER_EASE_IN_ELASTIC,     clutter_ease_in_elastic },
  { CLUTTER_EASE_OUT_ELASTIC,    clutter_ease_out_elastic },
  { CLUTTER_EASE_IN_OUT_ELASTIC, clutter_ease_in_out_elastic },
  { CLUTTER_EASE_IN_BACK,        clutter_ease_in_back },
  { CLUTTER_EASE_OUT_BACK,       clutter_ease_out_back },
  { CLUTTER_EASE_IN_OUT_BACK,    clutter_ease_in_out_back },
  { CLUTTER_EASE_IN_BOUNCE,      clutter_ease_in_bounce },
  { CLUTTER_EASE_OUT_BOUNCE,     clutter_ease_out_bounce },
  { CLUTTER_EASE_IN_OUT_BOUNCE,  clutter_ease_in_out_bounce },

  { CLUTTER_ANIMATION_LAST,      NULL },
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

  if (mode == CLUTTER_CUSTOM_MODE)
    {
      priv->mode = mode;
    }
  else if (mode < CLUTTER_ANIMATION_LAST)
    {
      GClosure *closure;

      /* sanity check to avoid getting an out of sync
       * enum/function mapping
       */
      g_assert (animation_modes[mode].mode == mode);
      g_assert (animation_modes[mode].func != NULL);

      closure = g_cclosure_new (G_CALLBACK (animation_modes[mode].func),
                                NULL,
                                NULL);
      clutter_alpha_set_closure_internal (alpha, closure);

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
        {
          GClosure *closure;

          closure = g_cclosure_new (G_CALLBACK (alpha_data->func),
                                    alpha_data->data,
                                    NULL);
          clutter_alpha_set_closure_internal (alpha, closure);
        }

      priv->mode = mode;
    }
  else
    g_assert_not_reached ();

  g_object_notify (G_OBJECT (alpha), "mode");
}

static gulong
register_alpha_internal (AlphaData *alpha_data)
{
  if (G_UNLIKELY (clutter_alphas == NULL))
    clutter_alphas = g_ptr_array_new ();

  g_ptr_array_add (clutter_alphas, alpha_data);

  return clutter_alphas->len + CLUTTER_ANIMATION_LAST;
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

  return register_alpha_internal (alpha_data);
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
  AlphaData *alpha_data;

  g_return_val_if_fail (closure != NULL, 0);

  alpha_data = g_slice_new (AlphaData);
  alpha_data->closure_set = TRUE;
  alpha_data->closure = closure;

  return register_alpha_internal (alpha_data);
}
