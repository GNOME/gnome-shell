/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
 *
 * Copyright (C) 2015  Intel Corporation.
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

#ifndef __CLUTTER_MASTER_CLOCK_GDK_H__
#define __CLUTTER_MASTER_CLOCK_GDK_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_MASTER_CLOCK_GDK            (clutter_master_clock_gdk_get_type ())
#define CLUTTER_MASTER_CLOCK_GDK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_MASTER_CLOCK_GDK, ClutterMasterClockGdk))
#define CLUTTER_IS_MASTER_CLOCK_GDK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_MASTER_CLOCK_GDK))
#define CLUTTER_MASTER_CLOCK_GDK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_MASTER_CLOCK_GDK, ClutterMasterClockGdkClass))

typedef struct _ClutterMasterClockGdk      ClutterMasterClockGdk;
typedef struct _ClutterMasterClockGdkClass ClutterMasterClockGdkClass;

struct _ClutterMasterClockGdkClass
{
  GObjectClass parent_class;
};

GType clutter_master_clock_gdk_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_MASTER_CLOCK_GDK_H__ */
