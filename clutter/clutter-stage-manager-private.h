/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

#ifndef __CLUTTER_STAGE_MANAGER_PRIVATE_H__
#define __CLUTTER_STAGE_MANAGER_PRIVATE_H__

#include <clutter/clutter-stage-manager.h>

G_BEGIN_DECLS

struct _ClutterStageManager
{
  GObject parent_instance;

  GSList *stages;
};

/* stage manager */
void _clutter_stage_manager_add_stage         (ClutterStageManager *stage_manager,
                                               ClutterStage        *stage);
void _clutter_stage_manager_remove_stage      (ClutterStageManager *stage_manager,
                                               ClutterStage        *stage);
void _clutter_stage_manager_set_default_stage (ClutterStageManager *stage_manager,
                                               ClutterStage        *stage);

G_END_DECLS

#endif /* __CLUTTER_STAGE_MANAGER_PRIVATE_H__ */
