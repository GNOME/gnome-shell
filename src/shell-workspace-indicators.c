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

#include <clutter/clutter.h>

#include "shell-workspace-dot.h"
#include "shell-workspace-indicators.h"

typedef struct _ShellWorkspaceIndicatorsPrivate ShellWorkspaceIndicatorsPrivate;
struct _ShellWorkspaceIndicatorsPrivate
{
  GType dot_type;
  StAdjustment *workspaces_adjustment;
};

enum
{
  PROP_0,

  PROP_DOT_TYPE,
  PROP_WORKSPACES_ADJUSTMENT,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (ShellWorkspaceIndicators, shell_workspace_indicators, ST_TYPE_BOX_LAYOUT)

static void
shell_workspace_indicators_dispose (GObject *object)
{
  ShellWorkspaceIndicatorsPrivate *priv =
    shell_workspace_indicators_get_instance_private (SHELL_WORKSPACE_INDICATORS (object));

  g_clear_object (&priv->workspaces_adjustment);

  G_OBJECT_CLASS (shell_workspace_indicators_parent_class)->dispose (object);
}

static GPtrArray *
get_active_dots (ShellWorkspaceIndicators *self)
{
  GPtrArray *active = g_ptr_array_new ();
  ClutterActor *child;

  for (child = clutter_actor_get_first_child (CLUTTER_ACTOR (self));
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
      if (!shell_workspace_dot_is_destroying (SHELL_WORKSPACE_DOT (child)))
        g_ptr_array_add (active, child);
    }

  return active;
}

static void
update_expansion (ShellWorkspaceIndicators *self)
{
  ShellWorkspaceIndicatorsPrivate *priv;
  g_autoptr (GPtrArray) active_dots = NULL;
  size_t n_indicators;
  double active_workspace, width_multiplier;
  size_t index = 0;

  g_return_if_fail (SHELL_IS_WORKSPACE_INDICATORS (self));

  priv = shell_workspace_indicators_get_instance_private (self);

  active_dots = get_active_dots (self);
  n_indicators = active_dots->len;
  active_workspace = st_adjustment_get_value (priv->workspaces_adjustment);

  if (n_indicators <= 2)
    width_multiplier = 3.625;
  else if (n_indicators <= 5)
    width_multiplier = 3.25;
  else
    width_multiplier = 2.75;

  for (ClutterActor *child = clutter_actor_get_first_child (CLUTTER_ACTOR (self));
       child != NULL;
       index++, child = clutter_actor_get_next_sibling (child))
    {
      double distance = fabs (((double) index) - active_workspace);
      
      g_object_set (child,
                    "expansion", CLAMP (1.0 - distance, 0.0, 1.0),
                    "width-multiplier", width_multiplier,
                    NULL);
    }
}

static void
recalculate_dots (ShellWorkspaceIndicators *self)
{
  ShellWorkspaceIndicatorsPrivate *priv =
    shell_workspace_indicators_get_instance_private (self);
  ShellWorkspaceDot *dot;
  g_autoptr (GPtrArray) active_dots = get_active_dots (self);
  size_t remaining;
  double target;
  
  st_adjustment_get_values (priv->workspaces_adjustment,
                            NULL,
                            NULL,
                            &target,
                            NULL,
                            NULL,
                            NULL);

  remaining = abs (active_dots->len - target);

  while (remaining--)
    if (active_dots->len < target)
      {
        dot = g_object_new (priv->dot_type, NULL);
        clutter_actor_add_child (CLUTTER_ACTOR (self), CLUTTER_ACTOR (dot));
        shell_workspace_dot_scale_in (dot);
      }
    else
      {
        dot = g_ptr_array_index (active_dots,
                                 active_dots->len - remaining - 1);
        shell_workspace_dot_scale_out_and_destroy (dot);
      }

  update_expansion (self);
}

static void
shell_workspace_indicators_set_property (GObject      *object,
                                         unsigned int  property_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ShellWorkspaceIndicators *self = SHELL_WORKSPACE_INDICATORS (object);
  ShellWorkspaceIndicatorsPrivate *priv =
    shell_workspace_indicators_get_instance_private (self);
  ShellWorkspaceDot *dot;
  double upper;

  switch (property_id)
    {
    case PROP_DOT_TYPE:
      priv->dot_type = g_value_get_gtype (value);
      g_object_notify_by_pspec (object, pspec);
      break;
    case PROP_WORKSPACES_ADJUSTMENT:
      priv->workspaces_adjustment = g_value_dup_object (value);
      g_signal_connect_object (priv->workspaces_adjustment,
                               "notify::value",
                               G_CALLBACK (update_expansion),
                               object,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (priv->workspaces_adjustment,
                               "notify::upper",
                               G_CALLBACK (recalculate_dots),
                               object,
                               G_CONNECT_SWAPPED);

      st_adjustment_get_values (priv->workspaces_adjustment,
                                NULL,
                                NULL,
                                &upper,
                                NULL,
                                NULL,
                                NULL);

      for (size_t i = 0; i < ((size_t) upper); i++)
        {
          dot = g_object_new (priv->dot_type, NULL);
          clutter_actor_insert_child_at_index (CLUTTER_ACTOR (self),
                                               CLUTTER_ACTOR (dot),
                                               i);
        }
      update_expansion (self);

      g_object_notify_by_pspec (object, pspec);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
shell_workspace_indicators_get_property (GObject      *object,
                                         unsigned int  property_id,
                                         GValue       *value,
                                         GParamSpec   *pspec)
{
  ShellWorkspaceIndicators *self = SHELL_WORKSPACE_INDICATORS (object);
  ShellWorkspaceIndicatorsPrivate *priv =
    shell_workspace_indicators_get_instance_private (self);

  switch (property_id)
    {
    case PROP_DOT_TYPE:
      g_value_set_gtype (value, priv->dot_type);
      break;
    case PROP_WORKSPACES_ADJUSTMENT:
      g_value_set_object (value, priv->workspaces_adjustment);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
shell_workspace_indicators_class_init (ShellWorkspaceIndicatorsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = shell_workspace_indicators_dispose;
  object_class->set_property = shell_workspace_indicators_set_property;
  object_class->get_property = shell_workspace_indicators_get_property;

  obj_props[PROP_DOT_TYPE] =
    g_param_spec_gtype ("dot-type", NULL, NULL,
                        SHELL_TYPE_WORKSPACE_DOT,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  obj_props[PROP_WORKSPACES_ADJUSTMENT] =
    g_param_spec_object ("workspaces-adjustment", NULL, NULL,
                         ST_TYPE_ADJUSTMENT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static void
shell_workspace_indicators_init (ShellWorkspaceIndicators *self)
{
}
