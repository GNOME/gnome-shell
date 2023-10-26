/* shell-workspace-group.c
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

#include "shell-workspace-group.h"

typedef struct _ShellWorkspaceGroupPrivate ShellWorkspaceGroupPrivate;
struct _ShellWorkspaceGroupPrivate
{
  MetaWorkspace *workspace;
};

enum
{
  PROP_0,

  PROP_WORKSPACE,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ShellWorkspaceGroup, shell_workspace_group, CLUTTER_TYPE_ACTOR)

static void
shell_workspace_group_dispose (GObject *object)
{
  ShellWorkspaceGroupPrivate *priv =
    shell_workspace_group_get_instance_private (SHELL_WORKSPACE_GROUP (object));

  g_clear_object (&priv->workspace);

  G_OBJECT_CLASS (shell_workspace_group_parent_class)->dispose (object);
}

static void
shell_workspace_group_set_property (GObject      *object,
                                    unsigned int  property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ShellWorkspaceGroupPrivate *priv =
    shell_workspace_group_get_instance_private (SHELL_WORKSPACE_GROUP (object));

  switch (property_id)
  {
    case PROP_WORKSPACE:
      priv->workspace = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
shell_workspace_group_get_property (GObject      *object,
                                    unsigned int  property_id,
                                    GValue       *value,
                                    GParamSpec   *pspec)
{
  ShellWorkspaceGroup *self = SHELL_WORKSPACE_GROUP (object);

  switch (property_id)
  {
    case PROP_WORKSPACE:
      g_value_set_object (value,
                          shell_workspace_group_get_workspace (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
shell_workspace_group_class_init (ShellWorkspaceGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = shell_workspace_group_dispose;
  object_class->set_property = shell_workspace_group_set_property;
  object_class->get_property = shell_workspace_group_get_property;

  /**
   * ShellWorkspaceGroup:workspace: (attributes org.gtk.Property.get=shell_workspace_group_get_workspace)
   */
  obj_props[PROP_WORKSPACE] =
    g_param_spec_object ("workspace", NULL, NULL,
                         META_TYPE_WORKSPACE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static void
shell_workspace_group_init (ShellWorkspaceGroup *self)
{
}

/**
 * shell_workspace_group_get_workspace:
 * @self: the #ShellWorkspaceGroup
 * 
 * Returns: (transfer none): the #MetaWorkspace represented by @self
 */
MetaWorkspace *
shell_workspace_group_get_workspace (ShellWorkspaceGroup *self)
{
  ShellWorkspaceGroupPrivate *priv;

  g_return_val_if_fail (SHELL_IS_WORKSPACE_GROUP (self), NULL);

  priv = shell_workspace_group_get_instance_private (self);

  return priv->workspace;
}
