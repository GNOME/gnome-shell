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
 * #ClutterActorClass.paint()_ virtual function.
 *
 * ## Implementing a ClutterEffect
 *
 * Creating a sub-class of #ClutterEffect requires overriding the
 * #ClutterEffectClass.paint() method. The implementation of the function should look
 * something like this:
 *
 * |[
 * void effect_paint (ClutterEffect *effect, ClutterEffectPaintFlags flags)
 * {
 *   // Set up initialisation of the paint such as binding a
 *   // CoglOffscreen or other operations
 *
 *   // Chain to the next item in the paint sequence. This will either call
 *   // ‘paint’ on the next effect or just paint the actor if this is
 *   // the last effect.
 *   ClutterActor *actor =
 *     clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
 *
 *   clutter_actor_continue_paint (actor);
 *
 *   // perform any cleanup of state, such as popping the CoglOffscreen
 * }
 * ]|
 *
 * The effect can optionally avoid calling clutter_actor_continue_paint() to skip any
 * further stages of the paint sequence. This is useful for example if the effect
 * contains a cached image of the actor. In that case it can optimise painting by
 * avoiding the actor paint and instead painting the cached image.
 *
 * The %CLUTTER_EFFECT_PAINT_ACTOR_DIRTY flag is useful in this case. Clutter will set
 * this flag when a redraw has been queued on the actor since it was last painted. The
 * effect can use this information to decide if the cached image is still valid.
 *
 * ## A simple ClutterEffect implementation
 *
 * The example below creates two rectangles: one will be painted "behind" the actor,
 * while another will be painted "on top" of the actor.
 *
 * The #ClutterActorMetaClass.set_actor() implementation will create the two materials
 * used for the two different rectangles; the #ClutterEffectClass.paint() implementation
 * will paint the first material using cogl_rectangle(), before continuing and then it
 * will paint paint the second material after.
 *
 *  |[
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
 *    // Clear the previous state //
 *    if (self->rect_1)
 *      {
 *        cogl_handle_unref (self->rect_1);
 *        self->rect_1 = NULL;
 *      }
 *
 *    if (self->rect_2)
 *      {
 *        cogl_handle_unref (self->rect_2);
 *        self->rect_2 = NULL;
 *      }
 *
 *    // Maintain a pointer to the actor
 *    self->actor = actor;
 *
 *    // If we've been detached by the actor then we should just bail out here
 *    if (self->actor == NULL)
 *      return;
 *
 *    // Create a red material
 *    self->rect_1 = cogl_material_new ();
 *    cogl_material_set_color4f (self->rect_1, 1.0, 0.0, 0.0, 1.0);
 *
 *    // Create a green material
 *    self->rect_2 = cogl_material_new ();
 *    cogl_material_set_color4f (self->rect_2, 0.0, 1.0, 0.0, 1.0);
 *  }
 *
 *  static gboolean
 *  my_effect_paint (ClutterEffect *effect)
 *  {
 *    MyEffect *self = MY_EFFECT (effect);
 *    gfloat width, height;
 *
 *    clutter_actor_get_size (self->actor, &width, &height);
 *
 *    // Paint the first rectangle in the upper left quadrant
 *    cogl_set_source (self->rect_1);
 *    cogl_rectangle (0, 0, width / 2, height / 2);
 *
 *    // Continue to the rest of the paint sequence
 *    clutter_actor_continue_paint (self->actor);
 *
 *    // Paint the second rectangle in the lower right quadrant
 *    cogl_set_source (self->rect_2);
 *    cogl_rectangle (width / 2, height / 2, width, height);
 *  }
 *
 *  static void
 *  my_effect_class_init (MyEffectClass *klass)
 *  {
 *    ClutterActorMetaClas *meta_class = CLUTTER_ACTOR_META_CLASS (klass);
 *
 *    meta_class->set_actor = my_effect_set_actor;
 *
 *    klass->paint = my_effect_paint;
 *  }
 * ]|
 *
 * #ClutterEffect is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-effect.h"

#include "clutter-actor-meta-private.h"
#include "clutter-debug.h"
#include "clutter-effect-private.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-actor-private.h"

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
clutter_effect_real_paint (ClutterEffect           *effect,
                           ClutterEffectPaintFlags  flags)
{
  ClutterActorMeta *actor_meta = CLUTTER_ACTOR_META (effect);
  ClutterActor *actor;
  gboolean pre_paint_succeeded;

  /* The default implementation provides a compatibility wrapper for
     effects that haven't migrated to use the 'paint' virtual yet. This
     just calls the old pre and post virtuals before chaining on */

  pre_paint_succeeded = _clutter_effect_pre_paint (effect);

  actor = clutter_actor_meta_get_actor (actor_meta);
  clutter_actor_continue_paint (actor);

  if (pre_paint_succeeded)
    _clutter_effect_post_paint (effect);
}

static void
clutter_effect_real_pick (ClutterEffect           *effect,
                          ClutterEffectPaintFlags  flags)
{
  ClutterActorMeta *actor_meta = CLUTTER_ACTOR_META (effect);
  ClutterActor *actor;

  actor = clutter_actor_meta_get_actor (actor_meta);
  clutter_actor_continue_paint (actor);
}

static void
clutter_effect_notify (GObject    *gobject,
                       GParamSpec *pspec)
{
  if (strcmp (pspec->name, "enabled") == 0)
    {
      ClutterActorMeta *meta = CLUTTER_ACTOR_META (gobject);
      ClutterActor *actor = clutter_actor_meta_get_actor (meta);

      if (actor != NULL)
        clutter_actor_queue_redraw (actor);
    }

  if (G_OBJECT_CLASS (clutter_effect_parent_class)->notify != NULL)
    G_OBJECT_CLASS (clutter_effect_parent_class)->notify (gobject, pspec);
}

static void
clutter_effect_class_init (ClutterEffectClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->notify = clutter_effect_notify;

  klass->pre_paint = clutter_effect_real_pre_paint;
  klass->post_paint = clutter_effect_real_post_paint;
  klass->get_paint_volume = clutter_effect_real_get_paint_volume;
  klass->paint = clutter_effect_real_paint;
  klass->pick = clutter_effect_real_pick;
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

void
_clutter_effect_paint (ClutterEffect           *effect,
                       ClutterEffectPaintFlags  flags)
{
  g_return_if_fail (CLUTTER_IS_EFFECT (effect));

  CLUTTER_EFFECT_GET_CLASS (effect)->paint (effect, flags);
}

void
_clutter_effect_pick (ClutterEffect           *effect,
                      ClutterEffectPaintFlags  flags)
{
  g_return_if_fail (CLUTTER_IS_EFFECT (effect));

  CLUTTER_EFFECT_GET_CLASS (effect)->pick (effect, flags);
}

gboolean
_clutter_effect_get_paint_volume (ClutterEffect      *effect,
                                  ClutterPaintVolume *volume)
{
  g_return_val_if_fail (CLUTTER_IS_EFFECT (effect), FALSE);
  g_return_val_if_fail (volume != NULL, FALSE);

  return CLUTTER_EFFECT_GET_CLASS (effect)->get_paint_volume (effect, volume);
}

/**
 * clutter_effect_queue_repaint:
 * @effect: A #ClutterEffect which needs redrawing
 *
 * Queues a repaint of the effect. The effect can detect when the ‘paint’
 * method is called as a result of this function because it will not
 * have the %CLUTTER_EFFECT_PAINT_ACTOR_DIRTY flag set. In that case the
 * effect is free to assume that the actor has not changed its
 * appearance since the last time it was painted so it doesn't need to
 * call clutter_actor_continue_paint() if it can draw a cached
 * image. This is mostly intended for effects that are using a
 * %CoglOffscreen to redirect the actor (such as
 * %ClutterOffscreenEffect). In that case the effect can save a bit of
 * rendering time by painting the cached texture without causing the
 * entire actor to be painted.
 *
 * This function can be used by effects that have their own animatable
 * parameters. For example, an effect which adds a varying degree of a
 * red tint to an actor by redirecting it through a CoglOffscreen
 * might have a property to specify the level of tint. When this value
 * changes, the underlying actor doesn't need to be redrawn so the
 * effect can call clutter_effect_queue_repaint() to make sure the
 * effect is repainted.
 *
 * Note however that modifying the position of the parent of an actor
 * may change the appearance of the actor because its transformation
 * matrix would change. In this case a redraw wouldn't be queued on
 * the actor itself so the %CLUTTER_EFFECT_PAINT_ACTOR_DIRTY would still
 * not be set. The effect can detect this case by keeping track of the
 * last modelview matrix that was used to render the actor and
 * veryifying that it remains the same in the next paint.
 *
 * Any other effects that are layered on top of the passed in effect
 * will still be passed the %CLUTTER_EFFECT_PAINT_ACTOR_DIRTY flag. If
 * anything queues a redraw on the actor without specifying an effect
 * or with an effect that is lower in the chain of effects than this
 * one then that will override this call. In that case this effect
 * will instead be called with the %CLUTTER_EFFECT_PAINT_ACTOR_DIRTY
 * flag set.
 *
 * Since: 1.8
 */
void
clutter_effect_queue_repaint (ClutterEffect *effect)
{
  ClutterActor *actor;

  g_return_if_fail (CLUTTER_IS_EFFECT (effect));

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));

  /* If the effect has no actor then nothing needs to be done */
  if (actor != NULL)
    _clutter_actor_queue_redraw_full (actor,
                                      0, /* flags */
                                      NULL, /* clip volume */
                                      effect /* effect */);
}
