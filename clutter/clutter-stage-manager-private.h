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
