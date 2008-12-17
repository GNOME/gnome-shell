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

#ifndef MUTTER_MODULE_H_
#define MUTTER_MODULE_H_

#include <glib-object.h>

#define MUTTER_TYPE_MODULE            (mutter_module_get_type ())
#define MUTTER_MODULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MUTTER_TYPE_MODULE, MutterModule))
#define MUTTER_MODULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MUTTER_TYPE_MODULE, MutterModuleClass))
#define MUTTER_IS_MODULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MUTTER_MODULE_TYPE))
#define MUTTER_IS_MODULE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MUTTER_TYPE_MODULE))
#define MUTTER_MODULE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MUTTER_TYPE_MODULE, MutterModuleClass))

typedef struct _MutterModule        MutterModule;
typedef struct _MutterModuleClass   MutterModuleClass;
typedef struct _MutterModulePrivate MutterModulePrivate;

struct _MutterModule
{
  GTypeModule parent;

  MutterModulePrivate *priv;
};

struct _MutterModuleClass
{
  GTypeModuleClass parent_class;
};


GType mutter_module_get_type (void);

GType mutter_module_get_plugin_type (MutterModule *module);

#endif
