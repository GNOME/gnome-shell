/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011  Intel Corporation.
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
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

/* This is an internal-only effect used to implement the
   'flatness' property of ClutterActor */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-flatten-effect.h"
#include "clutter-private.h"
#include "clutter-actor-private.h"

static void
_clutter_flatten_effect_set_actor (ClutterActorMeta *meta,
                                   ClutterActor *actor);

static void
_clutter_flatten_effect_run (ClutterEffect *effect,
                             ClutterEffectRunFlags flags);

G_DEFINE_TYPE (ClutterFlattenEffect,
               _clutter_flatten_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

#define CLUTTER_FLATTEN_EFFECT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_FLATTEN_EFFECT, \
                                ClutterFlattenEffectPrivate))

struct _ClutterFlattenEffectPrivate
{
  ClutterActor *actor;

  /* This records whether the last paint went through the FBO or if it
     was painted directly. We need to know this so we can force the
     offscreen effect to clear its image when we switch from rendering
     directly to rendering through the FBO */
  gboolean last_paint_used_fbo;
};

static void
_clutter_flatten_effect_class_init (ClutterFlattenEffectClass *klass)
{
  ClutterActorMetaClass *actor_meta_class = (ClutterActorMetaClass *) klass;
  ClutterEffectClass *effect_class = (ClutterEffectClass *) klass;

  actor_meta_class->set_actor = _clutter_flatten_effect_set_actor;

  effect_class->run = _clutter_flatten_effect_run;

  g_type_class_add_private (klass, sizeof (ClutterFlattenEffectPrivate));
}

static void
_clutter_flatten_effect_init (ClutterFlattenEffect *self)
{
  self->priv = CLUTTER_FLATTEN_EFFECT_GET_PRIVATE (self);
}

ClutterEffect *
_clutter_flatten_effect_new (void)
{
  return g_object_new (CLUTTER_TYPE_FLATTEN_EFFECT, NULL);
}

static gboolean
_clutter_flatten_effect_is_using_fbo (ClutterFlattenEffect *opacity_effect)
{
  ClutterFlattenEffectPrivate *priv = opacity_effect->priv;

  switch (clutter_actor_get_offscreen_redirect (priv->actor))
    {
    case CLUTTER_OFFSCREEN_REDIRECT_NEVER:
      return FALSE;

    case CLUTTER_OFFSCREEN_REDIRECT_ALWAYS:
      return TRUE;

    case CLUTTER_OFFSCREEN_REDIRECT_OPACITY_ONLY:
      return clutter_actor_get_paint_opacity (priv->actor) < 255;
    }

  g_assert_not_reached ();
}


static void
_clutter_flatten_effect_set_actor (ClutterActorMeta *meta,
                                   ClutterActor *actor)
{
  ClutterFlattenEffect *opacity_effect = CLUTTER_FLATTEN_EFFECT (meta);
  ClutterFlattenEffectPrivate *priv = opacity_effect->priv;

  CLUTTER_ACTOR_META_CLASS (_clutter_flatten_effect_parent_class)->
    set_actor (meta, actor);

  /* we keep a back pointer here, to avoid going through the ActorMeta */
  priv->actor = clutter_actor_meta_get_actor (meta);
}

static void
_clutter_flatten_effect_run (ClutterEffect *effect,
                             ClutterEffectRunFlags flags)
{
  ClutterFlattenEffect *opacity_effect = CLUTTER_FLATTEN_EFFECT (effect);
  ClutterFlattenEffectPrivate *priv = opacity_effect->priv;

  if (_clutter_flatten_effect_is_using_fbo (opacity_effect))
    {
      /* If the last paint bypassed the FBO then we'll pretend the
         actor is dirty so that the offscreen will clear its image */
      if (!priv->last_paint_used_fbo)
        {
          flags |= CLUTTER_EFFECT_RUN_ACTOR_DIRTY;
          priv->last_paint_used_fbo = TRUE;
        }

      /* Let the offscreen effect paint the actor through the FBO */
      CLUTTER_EFFECT_CLASS (_clutter_flatten_effect_parent_class)->
        run (effect, flags);
    }
  else
    {
      /* Just let the actor paint directly to the stage */
      clutter_actor_continue_paint (priv->actor);

      priv->last_paint_used_fbo = FALSE;
    }
}
