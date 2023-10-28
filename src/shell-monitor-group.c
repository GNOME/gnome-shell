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
  ShellGlobal *global;
  MetaWorkspaceManager *workspace_manager;
  StThemeContext *theme_context;

  ClutterActor *container;
  GPtrArray *workspace_groups;

  int index, width, height;
  float base_distance;

  gboolean is_vertical;
  gboolean is_rtl;
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

  g_clear_weak_pointer (&priv->global);

  if (priv->workspace_manager)
    g_signal_handlers_disconnect_by_data (priv->workspace_manager, object);
  g_clear_weak_pointer (&priv->workspace_manager);

  if (priv->theme_context)
    g_signal_handlers_disconnect_by_data (priv->theme_context, object);
  g_clear_weak_pointer (&priv->theme_context);

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
      break;
    case PROP_MONITOR_HEIGHT:
      priv->height = g_value_get_int (value);
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
update_base_distance (ShellMonitorGroup *self)
{
  ShellMonitorGroupPrivate *priv =
    shell_monitor_group_get_instance_private (self);
  int spacing =
    WORKSPACE_SPACING * st_theme_context_get_scale_factor (priv->theme_context);

  if (priv->is_vertical)
    priv->base_distance = priv->height + spacing;
  else
    priv->base_distance = priv->width + spacing;

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_BASE_DISTANCE]);
}

static void
shell_monitor_group_constructed (GObject *object)
{
  ShellMonitorGroup *self = SHELL_MONITOR_GROUP (object);
  ShellMonitorGroupPrivate *priv =
    shell_monitor_group_get_instance_private (self);

  G_OBJECT_CLASS (shell_monitor_group_parent_class)->constructed (object);

  clutter_actor_set_size (CLUTTER_ACTOR (object), priv->width, priv->height);
  update_base_distance (self);
}

static void
shell_monitor_group_class_init (ShellMonitorGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = shell_monitor_group_dispose;
  object_class->set_property = shell_monitor_group_set_property;
  object_class->get_property = shell_monitor_group_get_property;
  object_class->constructed = shell_monitor_group_constructed;

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
layout_rows_changed (ShellMonitorGroup *self)
{
  ShellMonitorGroupPrivate *priv =
    shell_monitor_group_get_instance_private (self);
  int rows;

  /*
   * TODO: https://gitlab.gnome.org/GNOME/mutter/-/merge_requests/3351
   * priv->is_vertical =
   *   meta_workspace_manager_get_layout_rows (priv->workspace_manager) == -1;
   */
  g_object_get (priv->workspace_manager, "layout-rows", &rows, NULL);

  priv->is_vertical = rows == -1;
  update_base_distance (self);

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_PROGRESS]);
}

static void
scale_factor_changed (ShellMonitorGroup *self)
{
  update_base_distance (self);

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_PROGRESS]);
}

static void
text_direction_changed (ShellMonitorGroup *self)
{
  ShellMonitorGroupPrivate *priv =
    shell_monitor_group_get_instance_private (self);

  priv->is_rtl =
    clutter_actor_get_text_direction (CLUTTER_ACTOR (self)) == CLUTTER_TEXT_DIRECTION_RTL;

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_PROGRESS]);
}

static void
shell_monitor_group_init (ShellMonitorGroup *self)
{
  ShellMonitorGroupPrivate *priv =
    shell_monitor_group_get_instance_private (self);
  ShellGlobal *global = shell_global_get ();
  MetaWorkspaceManager *workspace_manager =
    shell_global_get_workspace_manager (global);
  StThemeContext *theme_context =
    st_theme_context_get_for_stage (shell_global_get_stage (global));
  ClutterActor *container = clutter_actor_new ();

  g_set_weak_pointer (&priv->global, global);
  g_set_weak_pointer (&priv->workspace_manager, workspace_manager);
  g_set_weak_pointer (&priv->theme_context, theme_context);

  g_set_weak_pointer (&priv->container, container);
  clutter_actor_add_child (CLUTTER_ACTOR (self), container);

  priv->workspace_groups = g_ptr_array_new_with_free_func (g_object_unref);

  g_object_freeze_notify (G_OBJECT (self));
  g_signal_connect_swapped (priv->workspace_manager,
                            "notify::layout-rows", G_CALLBACK (layout_rows_changed),
                            self);
  layout_rows_changed (self);

  g_signal_connect_swapped (priv->theme_context,
                            "notify::scale-factor", G_CALLBACK (scale_factor_changed),
                            self);
  scale_factor_changed (self);

  g_signal_connect_swapped (self,
                            "notify::text-direction", G_CALLBACK (text_direction_changed),
                            self);
  text_direction_changed (self);
  g_object_thaw_notify (G_OBJECT (self));
}

float
shell_monitor_group_get_base_distance (ShellMonitorGroup *self)
{
  ShellMonitorGroupPrivate *priv;

  g_return_val_if_fail (SHELL_IS_MONITOR_GROUP (self), 0.0);

  priv = shell_monitor_group_get_instance_private (self);
  
  return priv->base_distance;
}

static float
get_progress_from_actor (ShellMonitorGroup *self,
                         ClutterActor      *actor)
{
  ShellMonitorGroupPrivate *priv =
    shell_monitor_group_get_instance_private (self);

  if (priv->is_vertical)
    return clutter_actor_get_y (actor) / priv->base_distance;
  else if (priv->is_rtl)
    return -clutter_actor_get_x (actor) / priv->base_distance;
  else
    return clutter_actor_get_x (actor) / priv->base_distance;
}

float
shell_monitor_group_get_progress (ShellMonitorGroup *self)
{
  ShellMonitorGroupPrivate *priv;

  g_return_val_if_fail (SHELL_IS_MONITOR_GROUP (self), 0.0);

  priv = shell_monitor_group_get_instance_private (self);

  return -get_progress_from_actor (self, priv->container);
}

void
shell_monitor_group_set_progress (ShellMonitorGroup *self,
                                  float              progress)
{
  ShellMonitorGroupPrivate *priv;

  g_return_if_fail (SHELL_IS_MONITOR_GROUP (self));

  priv = shell_monitor_group_get_instance_private (self);

  if (priv->is_vertical)
    clutter_actor_set_y (priv->container, -roundf (progress * priv->base_distance));
  else if (priv->is_rtl)
    clutter_actor_set_x (priv->container, roundf (progress * priv->base_distance));
  else
    clutter_actor_set_x (priv->container, -roundf (progress * priv->base_distance));

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_PROGRESS]);
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
        return get_progress_from_actor (self,
                                        CLUTTER_ACTOR (workspace_group));
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
    data[i] = get_progress_from_actor (self,
                                       g_ptr_array_index (priv->workspace_groups, i));

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
      distance = get_progress_from_actor (self, CLUTTER_ACTOR (group));
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
