/* shell-workspace-dot.c
 *
 * Copyright 2023 Zander Brown <zbrown@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <graphene.h>
#include <st/st.h>

#include "shell-workspace-dot.h"


#define INACTIVE_WORKSPACE_DOT_SCALE 0.75


typedef struct _ShellWorkspaceDotPrivate ShellWorkspaceDotPrivate;
struct _ShellWorkspaceDotPrivate {
  ClutterActor *dot;

  float width_multiplier;
  float expansion;

  gboolean destroying;
};

enum {
  PROP_0,

  PROP_WIDTH_MULTIPLIER,
  PROP_EXPANSION,
  PROP_DESTROYING,

  PROP_LAST
};
static GParamSpec *obj_props[PROP_LAST] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ShellWorkspaceDot, shell_workspace_dot, CLUTTER_TYPE_ACTOR)


static void
shell_workspace_dot_dispose (GObject *object)
{
  ShellWorkspaceDotPrivate *priv =
    shell_workspace_dot_get_instance_private (SHELL_WORKSPACE_DOT (object));

  g_clear_object (&priv->dot);

  G_OBJECT_CLASS (shell_workspace_dot_parent_class)->dispose (object);
}


static inline gboolean
value_changed (float old_value, float new_value)
{
  return !G_APPROX_VALUE (old_value, new_value, FLT_EPSILON);
}


static gboolean
set_if_changed (GObject      *object,
                GParamSpec   *pspec,
                float        *target,
                const GValue *value)
{
  float new_value = g_value_get_double (value);
 
  if (G_LIKELY (value_changed (*target, new_value))) {
    *target = new_value;
    g_object_notify_by_pspec (object, pspec);
    return TRUE;
  }

  return FALSE;
}


static inline float
lerp (float start, float end, float progress)
{
  return start + progress * (end - start);
}


static void
update_visuals (ShellWorkspaceDot *self)
{
  ShellWorkspaceDotPrivate *priv =
    shell_workspace_dot_get_instance_private (self);

  clutter_actor_set_opacity (priv->dot,
                             lerp (0.50, 1.0, priv->expansion) * 255);
  clutter_actor_set_scale (priv->dot,
                           lerp (INACTIVE_WORKSPACE_DOT_SCALE, 1.0, priv->expansion),
                           lerp (INACTIVE_WORKSPACE_DOT_SCALE, 1.0, priv->expansion));
}


static void
shell_workspace_dot_set_property (GObject      *object,
                                  unsigned int  property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ShellWorkspaceDot *self = SHELL_WORKSPACE_DOT (object);
  ShellWorkspaceDotPrivate *priv =
    shell_workspace_dot_get_instance_private (self);

  switch (property_id) {
    case PROP_WIDTH_MULTIPLIER:
      if (set_if_changed (object, pspec, &priv->width_multiplier, value)) {
        clutter_actor_queue_relayout (CLUTTER_ACTOR (object));
      }
      break;
    case PROP_EXPANSION:
      if (set_if_changed (object, pspec, &priv->expansion, value)) {
        update_visuals (self);
        clutter_actor_queue_relayout (CLUTTER_ACTOR (object));
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}


static void
shell_workspace_dot_get_property (GObject      *object,
                                  unsigned int  property_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
  ShellWorkspaceDotPrivate *priv =
    shell_workspace_dot_get_instance_private (SHELL_WORKSPACE_DOT (object));

  switch (property_id) {
    case PROP_WIDTH_MULTIPLIER:
      g_value_set_double (value, priv->width_multiplier);
      break;
    case PROP_EXPANSION:
      g_value_set_double (value, priv->expansion);
      break;
    case PROP_DESTROYING:
      g_value_set_boolean (value, priv->destroying);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}


static void
shell_workspace_dot_get_preferred_width (ClutterActor *actor,
                                         float         for_height,
                                         float        *min_width_p,
                                         float        *natural_width_p)
{
  ShellWorkspaceDotPrivate *priv =
    shell_workspace_dot_get_instance_private (SHELL_WORKSPACE_DOT (actor));
  float factor = lerp (1.0, priv->width_multiplier, priv->expansion);
  float min, nat;

  clutter_actor_get_preferred_width (priv->dot, for_height, &min, &nat);

  if (min_width_p) {
    *min_width_p = min * factor;
  }

  if (natural_width_p) {
    *natural_width_p = nat * factor;
  }
}


static void
shell_workspace_dot_get_preferred_height (ClutterActor *actor,
                                          float         for_width,
                                          float        *min_height_p,
                                          float        *natural_height_p)
{
  ShellWorkspaceDotPrivate *priv =
    shell_workspace_dot_get_instance_private (SHELL_WORKSPACE_DOT (actor));

  clutter_actor_get_preferred_height (priv->dot,
                                      for_width,
                                      min_height_p,
                                      natural_height_p);
}


static void
shell_workspace_dot_allocate (ClutterActor          *actor,
                              const ClutterActorBox *box)
{
  ShellWorkspaceDotPrivate *priv =
    shell_workspace_dot_get_instance_private (SHELL_WORKSPACE_DOT (actor));
  ClutterActorBox dot_box = *box;

  clutter_actor_set_allocation (actor, box);

  clutter_actor_box_set_origin (&dot_box, 0, 0);
  clutter_actor_allocate (priv->dot, &dot_box);
}


static void
shell_workspace_dot_class_init (ShellWorkspaceDotClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  object_class->dispose = shell_workspace_dot_dispose;
  object_class->set_property = shell_workspace_dot_set_property;
  object_class->get_property = shell_workspace_dot_get_property;

  actor_class->get_preferred_width = shell_workspace_dot_get_preferred_width;
  actor_class->get_preferred_height = shell_workspace_dot_get_preferred_height;
  actor_class->allocate = shell_workspace_dot_allocate;

  obj_props[PROP_WIDTH_MULTIPLIER] =
    g_param_spec_double ("width-multiplier", NULL, NULL,
                         1.0, 10.0, 1.0,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  obj_props[PROP_EXPANSION] =
    g_param_spec_double ("expansion", NULL, NULL,
                         0.0, 1.0, 0.0,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ShellWorkspaceDot:destroying: (attributes org.gtk.Property.get=shell_workspace_dot_is_destroying)
   */
  obj_props[PROP_DESTROYING] =
    g_param_spec_boolean ("destroying", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}


static void
shell_workspace_dot_init (ShellWorkspaceDot *self)
{
  ShellWorkspaceDotPrivate *priv =
    shell_workspace_dot_get_instance_private (self);

  clutter_actor_set_pivot_point (CLUTTER_ACTOR (self), 0.5, 0.5);

  priv->dot = g_object_new (ST_TYPE_WIDGET,
                            "style-class", "workspace-dot",
                            "y-align", CLUTTER_ACTOR_ALIGN_CENTER,
                            "pivot-point", &GRAPHENE_POINT_INIT (0.5, 0.5),
                            "request-mode", CLUTTER_REQUEST_WIDTH_FOR_HEIGHT,
                            NULL);
  g_object_ref_sink (priv->dot);
  clutter_actor_add_child (CLUTTER_ACTOR (self), priv->dot);

  priv->destroying = FALSE;

  update_visuals (self);
}


gboolean
shell_workspace_dot_is_destroying (ShellWorkspaceDot *self)
{
  ShellWorkspaceDotPrivate *priv;

  g_return_val_if_fail (SHELL_IS_WORKSPACE_DOT (self), FALSE);

  priv = shell_workspace_dot_get_instance_private (self);

  return priv->destroying;
}


void
shell_workspace_dot_scale_in (ShellWorkspaceDot *self)
{
  ShellWorkspaceDotClass *klass;

  g_return_if_fail (SHELL_IS_WORKSPACE_DOT (self));

  klass = SHELL_WORKSPACE_DOT_GET_CLASS (self);

  if (G_LIKELY (klass->scale_in)) {
    klass->scale_in (self);
  } else {
    g_warning ("%s didn't override scale_in", G_OBJECT_TYPE_NAME (self));
  }
}


void
shell_workspace_dot_scale_out_and_destroy (ShellWorkspaceDot *self)
{
  ShellWorkspaceDotPrivate *priv;
  ShellWorkspaceDotClass *klass;

  g_return_if_fail (SHELL_IS_WORKSPACE_DOT (self));

  priv = shell_workspace_dot_get_instance_private (self);
  priv->destroying = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_DESTROYING]);

  klass = SHELL_WORKSPACE_DOT_GET_CLASS (self);

  if (G_LIKELY (klass->scale_out_and_destroy)) {
    klass->scale_out_and_destroy (self);
  } else {
    g_warning ("%s didn't override scale_out_and_destroy",
               G_OBJECT_TYPE_NAME (self));
  }
}
