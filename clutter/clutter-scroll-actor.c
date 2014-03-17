/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012  Intel Corporation
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
 */

/**
 * SECTION:clutter-scroll-actor
 * @Title: ClutterScrollActor
 * @Short_Description: An actor for displaying a portion of its children
 *
 * #ClutterScrollActor is an actor that can be used to display a portion
 * of the contents of its children.
 *
 * The extent of the area of a #ClutterScrollActor is defined by the size
 * of its children; the visible region of the children of a #ClutterScrollActor
 * is set by using clutter_scroll_actor_scroll_to_point() or by using
 * clutter_scroll_actor_scroll_to_rect() to define a point or a rectangle
 * acting as the origin, respectively.
 *
 * #ClutterScrollActor does not provide pointer or keyboard event handling,
 * nor does it provide visible scroll handles.
 *
 * See [scroll-actor.c](https://git.gnome.org/browse/clutter/tree/examples/scroll-actor.c?h=clutter-1.18)
 * for an example of how to use #ClutterScrollActor.
 *
 * #ClutterScrollActor is available since Clutter 1.12.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-scroll-actor.h"

#include "clutter-actor-private.h"
#include "clutter-animatable.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"
#include "clutter-property-transition.h"
#include "clutter-transition.h"

struct _ClutterScrollActorPrivate
{
  ClutterPoint scroll_to;

  ClutterScrollMode scroll_mode;

  ClutterTransition *transition;
};

enum
{
  PROP_0,

  PROP_SCROLL_MODE,

  PROP_LAST
};

enum
{
  ANIM_PROP_0,

  ANIM_PROP_SCROLL_TO,

  ANIM_PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };
static GParamSpec *animatable_props[ANIM_PROP_LAST] = { NULL, };

static ClutterAnimatableIface *parent_animatable_iface = NULL;

static void     clutter_animatable_iface_init   (ClutterAnimatableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterScrollActor, clutter_scroll_actor, CLUTTER_TYPE_ACTOR,
                         G_ADD_PRIVATE (ClutterScrollActor)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_ANIMATABLE,
                                                clutter_animatable_iface_init))

static void
clutter_scroll_actor_set_scroll_to_internal (ClutterScrollActor *self,
                                             const ClutterPoint *point)
{
  ClutterScrollActorPrivate *priv = self->priv;
  ClutterActor *actor = CLUTTER_ACTOR (self);
  ClutterMatrix m = CLUTTER_MATRIX_INIT_IDENTITY;
  float dx, dy;

  if (clutter_point_equals (&priv->scroll_to, point))
    return;

  if (point == NULL)
    clutter_point_init (&priv->scroll_to, 0.f, 0.f);
  else
    priv->scroll_to = *point;

  if (priv->scroll_mode & CLUTTER_SCROLL_HORIZONTALLY)
    dx = -priv->scroll_to.x;
  else
    dx = 0.f;

  if (priv->scroll_mode & CLUTTER_SCROLL_VERTICALLY)
    dy = -priv->scroll_to.y;
  else
    dy = 0.f;

  cogl_matrix_translate (&m, dx, dy, 0.f);
  clutter_actor_set_child_transform (actor, &m);
}

static void
clutter_scroll_actor_set_property (GObject      *gobject,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ClutterScrollActor *actor = CLUTTER_SCROLL_ACTOR (gobject);

  switch (prop_id)
    {
    case PROP_SCROLL_MODE:
      clutter_scroll_actor_set_scroll_mode (actor, g_value_get_flags (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_scroll_actor_get_property (GObject    *gobject,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ClutterScrollActor *actor = CLUTTER_SCROLL_ACTOR (gobject);

  switch (prop_id)
    {
    case PROP_SCROLL_MODE:
      g_value_set_flags (value, actor->priv->scroll_mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_scroll_actor_class_init (ClutterScrollActorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = clutter_scroll_actor_set_property;
  gobject_class->get_property = clutter_scroll_actor_get_property;

  /**
   * ClutterScrollActor:scroll-mode:
   *
   * The scrollin direction.
   *
   * Since: 1.12
   */
  obj_props[PROP_SCROLL_MODE] =
    g_param_spec_flags ("scroll-mode",
                        P_("Scroll Mode"),
                        P_("The scrolling direction"),
                        CLUTTER_TYPE_SCROLL_MODE,
                        CLUTTER_SCROLL_BOTH,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
clutter_scroll_actor_init (ClutterScrollActor *self)
{
  self->priv = clutter_scroll_actor_get_instance_private (self);
  self->priv->scroll_mode = CLUTTER_SCROLL_BOTH;

  clutter_actor_set_clip_to_allocation (CLUTTER_ACTOR (self), TRUE);
}

static GParamSpec *
clutter_scroll_actor_find_property (ClutterAnimatable *animatable,
                                    const char        *property_name)
{
  if (strcmp (property_name, "scroll-to") == 0)
    return animatable_props[ANIM_PROP_SCROLL_TO];

  return parent_animatable_iface->find_property (animatable, property_name);
}

static void
clutter_scroll_actor_set_final_state (ClutterAnimatable *animatable,
                                      const char        *property_name,
                                      const GValue      *value)
{
  if (strcmp (property_name, "scroll-to") == 0)
    {
      ClutterScrollActor *self = CLUTTER_SCROLL_ACTOR (animatable);
      const ClutterPoint *point = g_value_get_boxed (value);

      clutter_scroll_actor_set_scroll_to_internal (self, point);
    }
  else
    parent_animatable_iface->set_final_state (animatable, property_name, value);
}

static void
clutter_scroll_actor_get_initial_state (ClutterAnimatable *animatable,
                                        const char        *property_name,
                                        GValue            *value)
{
  if (strcmp (property_name, "scroll-to") == 0)
    {
      ClutterScrollActor *self = CLUTTER_SCROLL_ACTOR (animatable);

      g_value_set_boxed (value, &self->priv->scroll_to);
    }
  else
    parent_animatable_iface->get_initial_state (animatable, property_name, value);
}

static void
clutter_animatable_iface_init (ClutterAnimatableIface *iface)
{
  parent_animatable_iface = g_type_interface_peek_parent (iface);

  animatable_props[ANIM_PROP_SCROLL_TO] =
    g_param_spec_boxed ("scroll-to",
                        "Scroll To",
                        "The point to scroll the actor to",
                        CLUTTER_TYPE_POINT,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        CLUTTER_PARAM_ANIMATABLE);

  iface->find_property = clutter_scroll_actor_find_property;
  iface->get_initial_state = clutter_scroll_actor_get_initial_state;
  iface->set_final_state = clutter_scroll_actor_set_final_state;
}

/**
 * clutter_scroll_actor_new:
 *
 * Creates a new #ClutterScrollActor.
 *
 * Return value: The newly created #ClutterScrollActor
 *   instance.
 *
 * Since: 1.12
 */
ClutterActor *
clutter_scroll_actor_new (void)
{
  return g_object_new (CLUTTER_TYPE_SCROLL_ACTOR, NULL);
}

/**
 * clutter_scroll_actor_set_scroll_mode:
 * @actor: a #ClutterScrollActor
 * @mode: a #ClutterScrollMode
 *
 * Sets the #ClutterScrollActor:scroll-mode property.
 *
 * Since: 1.12
 */
void
clutter_scroll_actor_set_scroll_mode (ClutterScrollActor *actor,
                                      ClutterScrollMode   mode)
{
  ClutterScrollActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_SCROLL_ACTOR (actor));

  priv = actor->priv;

  if (priv->scroll_mode == mode)
    return;

  priv->scroll_mode = mode;

  g_object_notify_by_pspec (G_OBJECT (actor), obj_props[PROP_SCROLL_MODE]);
}

/**
 * clutter_scroll_actor_get_scroll_mode:
 * @actor: a #ClutterScrollActor
 *
 * Retrieves the #ClutterScrollActor:scroll-mode property
 *
 * Return value: the scrolling mode
 *
 * Since: 1.12
 */
ClutterScrollMode
clutter_scroll_actor_get_scroll_mode (ClutterScrollActor *actor)
{
  g_return_val_if_fail (CLUTTER_IS_SCROLL_ACTOR (actor), CLUTTER_SCROLL_NONE);

  return actor->priv->scroll_mode;
}

/**
 * clutter_scroll_actor_scroll_to_point:
 * @actor: a #ClutterScrollActor
 * @point: a #ClutterPoint
 *
 * Scrolls the contents of @actor so that @point is the new origin
 * of the visible area.
 *
 * The coordinates of @point must be relative to the @actor.
 *
 * This function will use the currently set easing state of the @actor
 * to transition from the current scroll origin to the new one.
 *
 * Since: 1.12
 */
void
clutter_scroll_actor_scroll_to_point (ClutterScrollActor *actor,
                                      const ClutterPoint *point)
{
  ClutterScrollActorPrivate *priv;
  const ClutterAnimationInfo *info;

  g_return_if_fail (CLUTTER_IS_SCROLL_ACTOR (actor));
  g_return_if_fail (point != NULL);

  priv = actor->priv;

  info = _clutter_actor_get_animation_info (CLUTTER_ACTOR (actor));

  /* jump to the end if there is no easing state, or if the easing
   * state has a duration of 0 msecs
   */
  if (info->cur_state == NULL ||
      info->cur_state->easing_duration == 0)
    {
      /* ensure that we remove any currently running transition */
      if (priv->transition != NULL)
        {
          clutter_actor_remove_transition (CLUTTER_ACTOR (actor),
                                           "scroll-to");
          priv->transition = NULL;
        }

      clutter_scroll_actor_set_scroll_to_internal (actor, point);

      return;
    }

  if (priv->transition == NULL)
    {
      priv->transition = clutter_property_transition_new ("scroll-to");
      clutter_transition_set_animatable (priv->transition,
                                         CLUTTER_ANIMATABLE (actor));
      clutter_transition_set_remove_on_complete (priv->transition, TRUE);

      /* delay only makes sense if the transition has just been created */
      clutter_timeline_set_delay (CLUTTER_TIMELINE (priv->transition),
                                  info->cur_state->easing_delay);
      /* we need this to clear the priv->transition pointer */
      g_object_add_weak_pointer (G_OBJECT (priv->transition), (gpointer *) &priv->transition);

      clutter_actor_add_transition (CLUTTER_ACTOR (actor),
                                    "scroll-to",
                                    priv->transition);

      /* the actor now owns the transition */
      g_object_unref (priv->transition);
    }

  /* if a transition already exist, update its bounds */
  clutter_transition_set_from (priv->transition,
                               CLUTTER_TYPE_POINT,
                               &priv->scroll_to);
  clutter_transition_set_to (priv->transition,
                             CLUTTER_TYPE_POINT,
                             point);

  /* always use the current easing state */
  clutter_timeline_set_duration (CLUTTER_TIMELINE (priv->transition),
                                 info->cur_state->easing_duration);
  clutter_timeline_set_progress_mode (CLUTTER_TIMELINE (priv->transition),
                                      info->cur_state->easing_mode);

  /* ensure that we start from the beginning */
  clutter_timeline_rewind (CLUTTER_TIMELINE (priv->transition));
  clutter_timeline_start (CLUTTER_TIMELINE (priv->transition));
}

/**
 * clutter_scroll_actor_scroll_to_rect:
 * @actor: a #ClutterScrollActor
 * @rect: a #ClutterRect
 *
 * Scrolls @actor so that @rect is in the visible portion.
 *
 * Since: 1.12
 */
void
clutter_scroll_actor_scroll_to_rect (ClutterScrollActor *actor,
                                     const ClutterRect  *rect)
{
  ClutterRect n_rect;

  g_return_if_fail (CLUTTER_IS_SCROLL_ACTOR (actor));
  g_return_if_fail (rect != NULL);

  n_rect = *rect;

  /* normalize, so that we have a valid origin */
  clutter_rect_normalize (&n_rect);

  clutter_scroll_actor_scroll_to_point (actor, &n_rect.origin);
}
