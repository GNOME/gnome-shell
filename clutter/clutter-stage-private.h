#ifndef __CLUTTER_STAGE_PRIVATE_H__
#define __CLUTTER_STAGE_PRIVATE_H__

#include <clutter/clutter-stage-window.h>
#include <clutter/clutter-stage.h>

G_BEGIN_DECLS

typedef struct _ClutterStageQueueRedrawEntry ClutterStageQueueRedrawEntry;

/* stage */
ClutterStageWindow *_clutter_stage_get_default_window    (void);
void                _clutter_stage_do_paint              (ClutterStage          *stage,
                                                          const ClutterGeometry *clip);
void                _clutter_stage_set_window            (ClutterStage          *stage,
                                                          ClutterStageWindow    *stage_window);
ClutterStageWindow *_clutter_stage_get_window            (ClutterStage          *stage);
void                _clutter_stage_get_projection_matrix (ClutterStage          *stage,
                                                          CoglMatrix            *projection);
void                _clutter_stage_dirty_projection      (ClutterStage          *stage);
void                _clutter_stage_set_viewport          (ClutterStage          *stage,
                                                          int                    x,
                                                          int                    y,
                                                          int                    width,
                                                          int                    height);
void                _clutter_stage_get_viewport          (ClutterStage          *stage,
                                                          int                   *x,
                                                          int                   *y,
                                                          int                   *width,
                                                          int                   *height);
void                _clutter_stage_dirty_viewport        (ClutterStage          *stage);
void                _clutter_stage_maybe_setup_viewport  (ClutterStage          *stage);
void                _clutter_stage_maybe_relayout        (ClutterActor          *stage);
gboolean            _clutter_stage_needs_update          (ClutterStage          *stage);
gboolean            _clutter_stage_do_update             (ClutterStage          *stage);

void     _clutter_stage_queue_event                       (ClutterStage *stage,
					                   ClutterEvent *event);
gboolean _clutter_stage_has_queued_events                 (ClutterStage *stage);
void     _clutter_stage_process_queued_events             (ClutterStage *stage);
void     _clutter_stage_update_input_devices              (ClutterStage *stage);
int      _clutter_stage_get_pending_swaps                 (ClutterStage *stage);
gboolean _clutter_stage_has_full_redraw_queued            (ClutterStage *stage);
void     _clutter_stage_set_pick_buffer_valid             (ClutterStage *stage,
                                                           gboolean      valid);
gboolean _clutter_stage_get_pick_buffer_valid             (ClutterStage *stage);
void     _clutter_stage_increment_picks_per_frame_counter (ClutterStage *stage);
void     _clutter_stage_reset_picks_per_frame_counter     (ClutterStage *stage);
guint    _clutter_stage_get_picks_per_frame_counter       (ClutterStage *stage);

ClutterPaintVolume *_clutter_stage_paint_volume_stack_allocate (ClutterStage *stage);
void                _clutter_stage_paint_volume_stack_free_all (ClutterStage *stage);

const ClutterGeometry *_clutter_stage_get_clip (ClutterStage *stage);

ClutterStageQueueRedrawEntry *_clutter_stage_queue_actor_redraw            (ClutterStage                 *stage,
                                                                            ClutterStageQueueRedrawEntry *entry,
                                                                            ClutterActor                 *actor,
                                                                            ClutterPaintVolume           *clip);
void                          _clutter_stage_queue_redraw_entry_invalidate (ClutterStageQueueRedrawEntry *entry);

G_END_DECLS

#endif /* __CLUTTER_STAGE_PRIVATE_H__ */
