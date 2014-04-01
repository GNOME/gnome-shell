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

typedef struct _MetaLauncher MetaLauncher;

MetaLauncher     *meta_launcher_new                     (void);
void              meta_launcher_free                    (MetaLauncher  *self);

gboolean          meta_launcher_activate_vt             (MetaLauncher  *self,
							 signed char    vt,
							 GError       **error);
#endif
