/*
 * Copyright (C) 2013 Red Hat, Inc.
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

#ifndef META_WESTON_LAUNCH_H
#define META_WESTON_LAUNCH_H

#include <glib-object.h>
#include "weston-launch.h"

#define META_TYPE_LAUNCHER              (meta_launcher_get_type())
#define META_LAUNCHER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_LAUNCHER, MetaLauncher))
#define META_LAUNCHER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_LAUNCHER, MetaLauncherClass))
#define META_IS_LAUNCHER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_LAUNCHER))
#define META_IS_LAUNCHER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_LAUNCHER))
#define META_LAUNCHER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), META_LAUNCHER, MetaLauncherClass))

typedef struct _MetaLauncher      MetaLauncher;
typedef struct _MetaLauncherClass MetaLauncherClass;

GType             meta_launcher_get_type                (void) G_GNUC_CONST;

MetaLauncher     *meta_launcher_new                     (void);

gboolean          meta_launcher_set_drm_fd              (MetaLauncher  *self,
							 int            drm_fd,
							 GError       **error);
gboolean          meta_launcher_set_master              (MetaLauncher  *self,
							 gboolean       master,
							 GError       **error);
int               meta_launcher_open_input_device       (MetaLauncher  *self,
							 const char    *name,
							 int            flags,
							 GError       **error);

#endif
