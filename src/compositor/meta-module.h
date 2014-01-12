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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_MODULE_H_
#define META_MODULE_H_

#include <glib-object.h>

#define META_TYPE_MODULE            (meta_module_get_type ())
#define META_MODULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_MODULE, MetaModule))
#define META_MODULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_MODULE, MetaModuleClass))
#define META_IS_MODULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_MODULE_TYPE))
#define META_IS_MODULE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_MODULE))
#define META_MODULE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_MODULE, MetaModuleClass))

typedef struct _MetaModule        MetaModule;
typedef struct _MetaModuleClass   MetaModuleClass;
typedef struct _MetaModulePrivate MetaModulePrivate;

struct _MetaModule
{
  GTypeModule parent;

  MetaModulePrivate *priv;
};

struct _MetaModuleClass
{
  GTypeModuleClass parent_class;
};


GType meta_module_get_type (void);

GType meta_module_get_plugin_type (MetaModule *module);

#endif
