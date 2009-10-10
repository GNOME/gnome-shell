/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2008 OpenedHand
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
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_STAGE_MANAGER_H__
#define __CLUTTER_STAGE_MANAGER_H__

#include <glib-object.h>
#include <clutter/clutter-stage.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_STAGE_MANAGER              (clutter_stage_manager_get_type ())
#define CLUTTER_STAGE_MANAGER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_MANAGER, ClutterStageManager))
#define CLUTTER_IS_STAGE_MANAGER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_MANAGER))
#define CLUTTER_STAGE_MANAGER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_STAGE_MANAGER, ClutterStageManagerClass))
#define CLUTTER_IS_STAGE_MANAGER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_STAGE_MANAGER))
#define CLUTTER_STAGE_MANAGER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_STAGE_MANAGER, ClutterStageManagerClass))

typedef struct _ClutterStageManager             ClutterStageManager;
typedef struct _ClutterStageManagerClass        ClutterStageManagerClass;

/**
 * ClutterStageManagerClass;
 * @stage_added: class handler for the #ClutterStageManager::stage-added
 *   signal
 * @stage_removed: class handler for the #ClutterStageManager::stage-removed
 *   signal
 *
 * The #ClutterStageManagerClass structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.0
 */
struct _ClutterStageManagerClass
{
  /*< private >*/
  GObjectClass parent_class;

  void (* stage_added)   (ClutterStageManager *stage_manager,
                          ClutterStage        *stage);
  void (* stage_removed) (ClutterStageManager *stage_manager,
                          ClutterStage        *stage);
};

GType clutter_stage_manager_get_type (void) G_GNUC_CONST;

ClutterStageManager *clutter_stage_manager_get_default       (void);
ClutterStage *       clutter_stage_manager_get_default_stage (ClutterStageManager *stage_manager);
GSList *             clutter_stage_manager_list_stages       (ClutterStageManager *stage_manager);
const GSList *       clutter_stage_manager_peek_stages       (ClutterStageManager *stage_manager);

#ifndef CLUTTER_DISABLE_DEPRECATED
void                 clutter_stage_manager_set_default_stage (ClutterStageManager *stage_manager,
                                                              ClutterStage        *stage);
#endif

G_END_DECLS

#endif /* __CLUTTER_STAGE_MANAGER_H__ */
