/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (c) 2008 Intel Corp.
 *
 * Author: Tomas Frydrych <tf@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <meta/meta-plugin.h>
#include "meta-module.h"

#include <gmodule.h>

enum
{
  PROP_0,
  PROP_PATH,
};

struct _MetaModulePrivate
{
  GModule      *lib;
  gchar        *path;
  GType         plugin_type;
};

#define META_MODULE_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), META_TYPE_MODULE, MetaModulePrivate))

G_DEFINE_TYPE (MetaModule, meta_module, G_TYPE_TYPE_MODULE);

static gboolean
meta_module_load (GTypeModule *gmodule)
{
  MetaModulePrivate  *priv = META_MODULE (gmodule)->priv;
  MetaPluginVersion  *info = NULL;
  GType                (*register_type) (GTypeModule *) = NULL;

  if (priv->lib && priv->plugin_type)
    return TRUE;

  g_assert (priv->path);

  if (!priv->lib &&
      !(priv->lib = g_module_open (priv->path, 0)))
    {
      g_warning ("Could not load library [%s (%s)]",
                 priv->path, g_module_error ());
      return FALSE;
    }

  if (g_module_symbol (priv->lib, "meta_plugin_version",
                       (gpointer *)(void *)&info) &&
      g_module_symbol (priv->lib, "meta_plugin_register_type",
		       (gpointer *)(void *)&register_type) &&
      info && register_type)
    {
      if (info->version_api != MUTTER_PLUGIN_API_VERSION)
	g_warning ("Plugin API mismatch for [%s]", priv->path);
      else
        {
          GType plugin_type;

          if (!(plugin_type = register_type (gmodule)))
            {
              g_warning ("Could not register type for plugin %s",
                         priv->path);
              return FALSE;
            }
          else
            {
              priv->plugin_type =  plugin_type;
            }

          return TRUE;
        }
    }
  else
    g_warning ("Broken plugin module [%s]", priv->path);

  return FALSE;
}

static void
meta_module_unload (GTypeModule *gmodule)
{
  MetaModulePrivate *priv = META_MODULE (gmodule)->priv;

  g_module_close (priv->lib);

  priv->lib = NULL;
  priv->plugin_type = 0;
}

static void
meta_module_dispose (GObject *object)
{
  G_OBJECT_CLASS (meta_module_parent_class)->dispose (object);
}

static void
meta_module_finalize (GObject *object)
{
  MetaModulePrivate *priv = META_MODULE (object)->priv;

  g_free (priv->path);
  priv->path = NULL;

  G_OBJECT_CLASS (meta_module_parent_class)->finalize (object);
}

static void
meta_module_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  MetaModulePrivate *priv = META_MODULE (object)->priv;

  switch (prop_id)
    {
    case PROP_PATH:
      g_free (priv->path);
      priv->path = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_module_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MetaModulePrivate *priv = META_MODULE (object)->priv;

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_string (value, priv->path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_module_class_init (MetaModuleClass *klass)
{
  GObjectClass     *gobject_class = G_OBJECT_CLASS (klass);
  GTypeModuleClass *gmodule_class = G_TYPE_MODULE_CLASS (klass);

  gobject_class->finalize     = meta_module_finalize;
  gobject_class->dispose      = meta_module_dispose;
  gobject_class->set_property = meta_module_set_property;
  gobject_class->get_property = meta_module_get_property;

  gmodule_class->load         = meta_module_load;
  gmodule_class->unload       = meta_module_unload;

  g_object_class_install_property (gobject_class,
				   PROP_PATH,
				   g_param_spec_string ("path",
							"Path",
							"Load path",
							NULL,
							G_PARAM_READWRITE |
						      G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (gobject_class, sizeof (MetaModulePrivate));
}

static void
meta_module_init (MetaModule *self)
{
  self->priv = META_MODULE_GET_PRIVATE (self);
}

GType
meta_module_get_plugin_type (MetaModule *module)
{
  MetaModulePrivate *priv = META_MODULE (module)->priv;

  return priv->plugin_type;
}

