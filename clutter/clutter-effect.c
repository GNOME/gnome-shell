/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 * SECTION:clutter-effect
 * @short_description: Base class for actor effects
 *
 * The #ClutterEffect class provides a default type and API for creating
 * effects for generic actors.
 *
 * Effects are a #ClutterActorMeta sub-class that modify the way an actor
 * is painted in a way that is not part of the actor's implementation.
 *
 * Effects should be the preferred way to affect the paint sequence of an
 * actor without sub-classing the actor itself and overriding the
 * #ClutterActor::paint virtual function.
 *
 * <refsect2 id="ClutterEffect-implementation">
 *   <title>Implementing a ClutterEffect</title>
 *   <para>Creating a sub-class of #ClutterEffect requires the implementation
 *   of two virtual functions:</para>
 *   <itemizedlist>
 *     <listitem><simpara><function>pre_paint()</function>, which is called
 *     before painting the #ClutterActor.</simpara></listitem>
 *     <listitem><simpara><function>post_paint()</function>, which is called
 *     after painting the #ClutterActor.</simpara></listitem>
 *   </itemizedlist>
 *   <para>The <function>pre_paint()</function> function should be used to set
 *   up the #ClutterEffect right before the #ClutterActor's paint
 *   sequence. This function can fail, and return %FALSE; in that case, no
 *   <function>post_paint()</function> invocation will follow.</para>
 *   <para>The <function>post_paint()</function> function is called after the
 *   #ClutterActor's paint sequence.</para>
 *   <para>The <function>pre_paint()</function> phase could be seen as a custom
 *   handler to the #ClutterActor::paint signal, while the
 *   <function>post_paint()</function> phase could be seen as a custom handler
 *   to the #ClutterActor::paint signal connected using
 *   g_signal_connect_after().</para>
 *   <example id="ClutterEffect-example">
 *     <title>A simple ClutterEffect implementation</title>
 *     <para>The example below creates two rectangles: one will be painted
 *     "behind" the actor, while another will be painted "on top" of the actor.
 *     The <function>set_actor()</function> implementation will create the two
 *     materials used for the two different rectangles; the
 *     <function>pre_paint()</function> function will paint the first material
 *     using cogl_rectangle(), while the <function>post_paint()</function>
 *     phase will paint the second material.</para>
 *     <programlisting>
 *  typedef struct {
 *    ClutterEffect parent_instance;
 *
 *    CoglHandle rect_1;
 *    CoglHandle rect_2;
 *  } MyEffect;
 *
 *  typedef struct _ClutterEffectClass MyEffectClass;
 *
 *  G_DEFINE_TYPE (MyEffect, my_effect, CLUTTER_TYPE_EFFECT);
 *
 *  static void
 *  my_effect_set_actor (ClutterActorMeta *meta,
 *                       ClutterActor     *actor)
 *  {
 *    MyEffect *self = MY_EFFECT (meta);
 *
 *    /&ast; Clear the previous state &ast;/
 *    if (self-&gt;rect_1)
 *      {
 *        cogl_handle_unref (self-&gt;rect_1);
 *        self-&gt;rect_1 = NULL;
 *      }
 *
 *    if (self-&gt;rect_2)
 *      {
 *        cogl_handle_unref (self-&gt;rect_2);
 *        self-&gt;rect_2 = NULL;
 *      }
 *
 *    /&ast; Maintain a pointer to the actor &ast;
 *    self-&gt;actor = actor;
 *
 *    /&ast; If we've been detached by the actor then we should
 *     &ast; just bail out here
 *     &ast;/
 *    if (self-&gt;actor == NULL)
 *      return;
 *
 *    /&ast; Create a red material &ast;/
 *    self-&gt;rect_1 = cogl_material_new ();
 *    cogl_material_set_color4f (self-&gt;rect_1, 1.0, 0.0, 0.0, 1.0);
 *
 *    /&ast; Create a green material &ast;/
 *    self-&gt;rect_2 = cogl_material_new ();
 *    cogl_material_set_color4f (self-&gt;rect_2, 0.0, 1.0, 0.0, 1.0);
 *  }
 *
 *  static gboolean
 *  my_effect_pre_paint (ClutterEffect *effect)
 *  {
 *    MyEffect *self = MY_EFFECT (effect);
 *    gfloat width, height;
 *
 *    /&ast; If we were disabled we don't need to paint anything &ast;/
 *    if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
 *      return FALSE;
 *
 *    clutter_actor_get_size (self-&gt;actor, &amp;width, &amp;height);
 *
 *    /&ast; Paint the first rectangle in the upper left quadrant &ast;/
 *    cogl_set_source (self-&gt;rect_1);
 *    cogl_rectangle (0, 0, width / 2, height / 2);
 *
 *    return TRUE;
 *  }
 *
 *  static void
 *  my_effect_post_paint (ClutterEffect *effect)
 *  {
 *    MyEffect *self = MY_EFFECT (effect);
 *    gfloat width, height;
 *
 *    clutter_actor_get_size (self-&gt;actor, &amp;width, &amp;height);
 *
 *    /&ast; Paint the second rectangle in the lower right quadrant &ast;/
 *    cogl_set_source (self-&gt;rect_2);
 *    cogl_rectangle (width / 2, height / 2, width, height);
 *  }
 *
 *  static void
 *  my_effect_class_init (MyEffectClass *klass)
 *  {
 *    ClutterActorMetaClas *meta_class = CLUTTER_ACTOR_META_CLASS (klass);
 *
 *    meta_class-&gt;set_actor = my_effect_set_actor;
 *
 *    klass-&gt;pre_paint = my_effect_pre_paint;
 *    klass-&gt;post_paint = my_effect_post_paint;
 *  }
 *     </programlisting>
 *   </example>
 * </refsect2>
 *
 * #ClutterEffect is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-effect.h"

#include "clutter-actor-meta-private.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

G_DEFINE_ABSTRACT_TYPE (ClutterEffect,
                        clutter_effect,
                        CLUTTER_TYPE_ACTOR_META);

static gboolean
clutter_effect_real_pre_paint (ClutterEffect *effect)
{
  return TRUE;
}

static void
clutter_effect_real_post_paint (ClutterEffect *effect)
{
}

static gboolean
clutter_effect_real_get_paint_volume (ClutterEffect      *effect,
                                      ClutterPaintVolume *volume)
{
  return TRUE;
}

static void
clutter_effect_class_init (ClutterEffectClass *klass)
{
  klass->pre_paint = clutter_effect_real_pre_paint;
  klass->post_paint = clutter_effect_real_post_paint;
  klass->get_paint_volume = clutter_effect_real_get_paint_volume;
}

static void
clutter_effect_init (ClutterEffect *self)
{
}

gboolean
_clutter_effect_pre_paint (ClutterEffect *effect)
{
  g_return_val_if_fail (CLUTTER_IS_EFFECT (effect), FALSE);

  return CLUTTER_EFFECT_GET_CLASS (effect)->pre_paint (effect);
}

void
_clutter_effect_post_paint (ClutterEffect *effect)
{
  g_return_if_fail (CLUTTER_IS_EFFECT (effect));

  CLUTTER_EFFECT_GET_CLASS (effect)->post_paint (effect);
}

gboolean
_clutter_effect_get_paint_volume (ClutterEffect      *effect,
                                  ClutterPaintVolume *volume)
{
  g_return_val_if_fail (CLUTTER_IS_EFFECT (effect), FALSE);
  g_return_val_if_fail (volume != NULL, FALSE);

  return CLUTTER_EFFECT_GET_CLASS (effect)->get_paint_volume (effect, volume);
}
