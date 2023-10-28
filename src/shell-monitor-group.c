/* shell-monitor-group.c
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

#include "shell-monitor-group.h"
#include "shell-workspace-group.h"
#include "shell-global.h"

#define WORKSPACE_SPACING 100

typedef struct _ShellMonitorGroupPrivate ShellMonitorGroupPrivate;
struct _ShellMonitorGroupPrivate
{
  ClutterActor *container;
  GPtrArray *workspace_groups;
  
  int index, width, height;
};

enum
{
  PROP_0,

  PROP_INDEX,
  PROP_MONITOR_WIDTH,
  PROP_MONITOR_HEIGHT,
  PROP_PROGRESS,
  PROP_BASE_DISTANCE,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ShellMonitorGroup, shell_monitor_group, ST_TYPE_WIDGET)

static void
shell_monitor_group_dispose (GObject *object)
{
  ShellMonitorGroupPrivate *priv =
    shell_monitor_group_get_instance_private (SHELL_MONITOR_GROUP (object));

  g_clear_weak_pointer (&priv->container);
  g_clear_pointer (&priv->workspace_groups, g_ptr_array_unref);

  G_OBJECT_CLASS (shell_monitor_group_parent_class)->dispose (object);
}

static void
shell_monitor_group_set_property (GObject      *object,
                                  unsigned int  property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ShellMonitorGroupPrivate *priv =
    shell_monitor_group_get_instance_private (SHELL_MONITOR_GROUP (object));

  switch (property_id)
    {
    case PROP_INDEX:
      priv->index = g_value_get_int (value);
      break;
    case PROP_MONITOR_WIDTH:
      priv->width = g_value_get_int (value);
      clutter_actor_set_size (CLUTTER_ACTOR (object), g_value_get_int (value), priv->height);
      break;
    case PROP_MONITOR_HEIGHT:
      priv->height = g_value_get_int (value);
      clutter_actor_set_size (CLUTTER_ACTOR (object), priv->width, g_value_get_int (value));
      break;
    case PROP_PROGRESS:
      shell_monitor_group_set_progress (SHELL_MONITOR_GROUP (object),
                                        g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
shell_monitor_group_get_property (GObject      *object,
                                  unsigned int  property_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
  ShellMonitorGroup *self = SHELL_MONITOR_GROUP (object);
  ShellMonitorGroupPrivate *priv =
    shell_monitor_group_get_instance_private (self);

  switch (property_id)
    {
    case PROP_INDEX:
      g_value_set_int (value, priv->index);
      break;
    case PROP_MONITOR_WIDTH:
      g_value_set_int (value, priv->width);
      break;
    case PROP_MONITOR_HEIGHT:
      g_value_set_int (value, priv->height);
      break;
    case PROP_PROGRESS:
      g_value_set_double (value,
                          shell_monitor_group_get_progress (self));
      break;
    case PROP_BASE_DISTANCE:
      g_value_set_double (value,
                          shell_monitor_group_get_base_distance (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
shell_monitor_group_class_init (ShellMonitorGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = shell_monitor_group_dispose;
  object_class->set_property = shell_monitor_group_set_property;
  object_class->get_property = shell_monitor_group_get_property;

  obj_props[PROP_INDEX] =
    g_param_spec_int ("index", NULL, NULL,
                      0, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  obj_props[PROP_MONITOR_WIDTH] =
    g_param_spec_int ("monitor-width", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  obj_props[PROP_MONITOR_HEIGHT] =
    g_param_spec_int ("monitor-height", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  obj_props[PROP_PROGRESS] =
    g_param_spec_double ("progress", NULL, NULL,
                         -INFINITY, INFINITY, 0.0,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  obj_props[PROP_BASE_DISTANCE] =
    g_param_spec_double ("base-distance", NULL, NULL,
                         -INFINITY, INFINITY, 0.0,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static void
shell_monitor_group_init (ShellMonitorGroup *self)
{
  ShellMonitorGroupPrivate *priv =
    shell_monitor_group_get_instance_private (self);
  ClutterActor *container = clutter_actor_new ();

  g_set_weak_pointer (&priv->container, container);
  clutter_actor_add_child (CLUTTER_ACTOR (self), container);

  priv->workspace_groups = g_ptr_array_new_with_free_func (g_object_unref);
}

float
shell_monitor_group_get_base_distance (ShellMonitorGroup *self)
{
  ShellMonitorGroupPrivate *priv;
  ShellGlobal *global = shell_global_get ();
  StThemeContext *context;
  int scale, spacing, rows;

  g_return_val_if_fail (SHELL_IS_MONITOR_GROUP (self), 0.0);

  priv = shell_monitor_group_get_instance_private (self);

  context = st_theme_context_get_for_stage (shell_global_get_stage (global));
  scale = st_theme_context_get_scale_factor (context);
  /* TODO: Add mutter API? */
  g_object_get (shell_global_get_workspace_manager (global),
                "layout-rows", &rows,
                NULL);

  spacing = WORKSPACE_SPACING * scale;

  if (rows == -1)
    return priv->height + spacing;
  
  return priv->width + spacing;
}

float
shell_monitor_group_get_progress (ShellMonitorGroup *self)
{
  ShellMonitorGroupPrivate *priv;
  ShellGlobal *global = shell_global_get ();
  int rows;
  float base_distance;

  g_return_val_if_fail (SHELL_IS_MONITOR_GROUP (self), 0.0);

  priv = shell_monitor_group_get_instance_private (self);

  /* TODO: Add mutter API? */
  g_object_get (shell_global_get_workspace_manager (global),
                "layout-rows", &rows,
                NULL);

  base_distance = shell_monitor_group_get_base_distance (self);

  if (rows == -1)
    return -clutter_actor_get_y (priv->container) / base_distance;
  else if (clutter_actor_get_text_direction (CLUTTER_ACTOR (self)) == CLUTTER_TEXT_DIRECTION_RTL)
    return clutter_actor_get_x (priv->container) / base_distance;
  else
    return -clutter_actor_get_x (priv->container) / base_distance;
}

void
shell_monitor_group_set_progress (ShellMonitorGroup *self,
                                  float              progress)
{
  ShellMonitorGroupPrivate *priv;
  ShellGlobal *global = shell_global_get ();
  int rows;
  float base_distance;

  g_return_if_fail (SHELL_IS_MONITOR_GROUP (self));

  priv = shell_monitor_group_get_instance_private (self);

  /* TODO: Add mutter API? */
  g_object_get (shell_global_get_workspace_manager (global),
                "layout-rows", &rows,
                NULL);

  base_distance = shell_monitor_group_get_base_distance (self);

  if (rows == -1)
    clutter_actor_set_y (priv->container, -roundf (progress * base_distance));
  else if (clutter_actor_get_text_direction (CLUTTER_ACTOR (self)) == CLUTTER_TEXT_DIRECTION_RTL)
    clutter_actor_set_x (priv->container, roundf (progress * base_distance));
  else
    clutter_actor_set_x (priv->container, -roundf (progress * base_distance));

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_PROGRESS]);
}

static float
get_workspace_group_progress (ShellMonitorGroup   *self,
                              ShellWorkspaceGroup *group)
{
  ShellGlobal *global = shell_global_get ();
  int rows;
  float base_distance;

  /* TODO: Add mutter API? */
  g_object_get (shell_global_get_workspace_manager (global),
                "layout-rows", &rows,
                NULL);

  base_distance = shell_monitor_group_get_base_distance (self);

  if (rows == -1)
    return clutter_actor_get_y (CLUTTER_ACTOR (group)) / base_distance;
  else if (clutter_actor_get_text_direction (CLUTTER_ACTOR (self)) == CLUTTER_TEXT_DIRECTION_RTL)
    return -clutter_actor_get_x (CLUTTER_ACTOR (group)) / base_distance;
  else
    return clutter_actor_get_x (CLUTTER_ACTOR (group)) / base_distance;
}

float
shell_monitor_group_get_workspace_progress (ShellMonitorGroup *self,
                                            MetaWorkspace     *workspace)
{
  ShellMonitorGroupPrivate *priv;
  ShellWorkspaceGroup *workspace_group;
  int target_index;
  
  g_return_val_if_fail (SHELL_IS_MONITOR_GROUP (self), 0.0);
  g_return_val_if_fail (META_IS_WORKSPACE (workspace), 0.0);

  priv = shell_monitor_group_get_instance_private (self);

  target_index = meta_workspace_index (workspace);

  for (size_t i = 0; i < priv->workspace_groups->len; i++)
    {
      workspace_group = g_ptr_array_index (priv->workspace_groups, i);

      if (meta_workspace_index (shell_workspace_group_get_workspace (workspace_group)) == target_index)
        return get_workspace_group_progress (self, workspace_group);
    }

  g_return_val_if_reached (0.0);
}

/**
 * shell_monitor_group_get_snap_points:
 * @self: the #ShellMonitorGroup
 * @n_points: length of @points
 * @points: (array length=n_points) (out): array of snap points
 */
void
shell_monitor_group_get_snap_points (ShellMonitorGroup *self,
                                     size_t            *n_points,
                                     float             *points[])
{
  ShellMonitorGroupPrivate *priv;
  float *data;
  
  g_return_if_fail (SHELL_IS_MONITOR_GROUP (self));
  g_return_if_fail (n_points != NULL);
  g_return_if_fail (points != NULL);

  priv = shell_monitor_group_get_instance_private (self);

  *n_points = priv->workspace_groups->len;
  data = g_new0 (float, priv->workspace_groups->len + 1);

  for (size_t i = 0; i < priv->workspace_groups->len; i++)
    data[i] = get_workspace_group_progress (self, g_ptr_array_index (priv->workspace_groups, i));

  *points = data;
}

void
shell_monitor_group_add_group (ShellMonitorGroup   *self,
                               ShellWorkspaceGroup *group,
                               float                x,
                               float                y)
{
  ShellMonitorGroupPrivate *priv;
  
  g_return_if_fail (SHELL_IS_MONITOR_GROUP (self));
  g_return_if_fail (SHELL_IS_WORKSPACE_GROUP (group));

  priv = shell_monitor_group_get_instance_private (self);

  g_ptr_array_add (priv->workspace_groups, g_object_ref (group));
  clutter_actor_add_child (priv->container, CLUTTER_ACTOR (group));
  clutter_actor_set_position (CLUTTER_ACTOR (group), x, y);
}

static float
interpolate_progress (ShellMonitorGroup *self,
                      float              progress,
                      ShellMonitorGroup *monitor_group)
{
  ShellMonitorGroupPrivate *priv =
    shell_monitor_group_get_instance_private (self);
  ShellMonitorGroupPrivate *monitor_group_priv =
    shell_monitor_group_get_instance_private (monitor_group);
  size_t points1_len, points2_len;
  g_autofree float *points1 = NULL, *points2 = NULL;
  size_t upper = 0, lower = 0;
  float t;

  if (priv->index == monitor_group_priv->index)
    return progress;

  shell_monitor_group_get_snap_points (monitor_group, &points1_len, &points1);
  shell_monitor_group_get_snap_points (self, &points2_len, &points2);

  for (size_t i = 0; i < points1_len; i++)
    if (points1[i] >= progress)
      {
        upper = i;
        break;
      }

  for (size_t i = points2_len - 1; i >= 0; i--)
    if (points2[i] <= progress)
      {
        lower = i;
        break;
      }

  if (G_APPROX_VALUE (points1[upper], points1[lower], FLT_EPSILON))
    return points2[upper];

  t = (progress - points1[lower]) / (points1[upper] - points1[lower]);

  return points2[lower] + (points2[upper] - points2[lower]) * t;
}

void
shell_monitor_group_update_swipe_for_monitor (ShellMonitorGroup *self,
                                              float              progress,
                                              ShellMonitorGroup *monitor_group)
{
  g_return_if_fail (SHELL_IS_MONITOR_GROUP (self));
  g_return_if_fail (SHELL_IS_MONITOR_GROUP (monitor_group));

  shell_monitor_group_set_progress (self,
                                    interpolate_progress (self, progress, monitor_group));
}

/**
 * shell_monitor_group_find_closest_workspace:
 * @self: the #ShellMonitorGroup
 * @progress: current progress
 * 
 * Returns: (transfer none): the #MetaWorkspace
 */
MetaWorkspace *
shell_monitor_group_find_closest_workspace (ShellMonitorGroup *self,
                                            float              progress)
{
  ShellMonitorGroupPrivate *priv;
  ShellWorkspaceGroup *group, *min_group = NULL;
  float min_distance = -1.0, distance, value;

  g_return_val_if_fail (SHELL_IS_MONITOR_GROUP (self), NULL);

  priv = shell_monitor_group_get_instance_private (self);

  for (size_t i = 0; i < priv->workspace_groups->len; i++)
    {
      group = g_ptr_array_index (priv->workspace_groups, i);
      distance = get_workspace_group_progress (self, group);
      value = fabsf (distance - progress);

      if (G_UNLIKELY (min_distance < 0))
        {
          min_distance = value;
          min_group = group;
        }
      else
        {
          min_distance = MIN (min_distance, value);
          if (G_APPROX_VALUE (min_distance, value, FLT_EPSILON))
            min_group = group;
        }
    }

  return shell_workspace_group_get_workspace (min_group);
}
