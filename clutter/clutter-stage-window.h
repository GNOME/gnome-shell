#ifndef __CLUTTER_STAGE_WINDOW_H__
#define __CLUTTER_STAGE_WINDOW_H__

#include <cogl/cogl.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_STAGE_WINDOW               (_clutter_stage_window_get_type ())
#define CLUTTER_STAGE_WINDOW(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_WINDOW, ClutterStageWindow))
#define CLUTTER_IS_STAGE_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_WINDOW))
#define CLUTTER_STAGE_WINDOW_GET_IFACE(obj)     (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CLUTTER_TYPE_STAGE_WINDOW, ClutterStageWindowIface))

/*
 * ClutterStageWindow: (skip)
 *
 * <structname>ClutterStageWindow</structname> is an opaque structure
 * whose members should not be accessed directly
 *
 * Since: 0.8
 */
typedef struct _ClutterStageWindow      ClutterStageWindow; /* dummy */
typedef struct _ClutterStageWindowIface ClutterStageWindowIface;

/*
 * ClutterStageWindowIface: (skip)
 *
 * The interface implemented by backends for stage windows
 *
 * Since: 0.8
 */
struct _ClutterStageWindowIface
{
  /*< private >*/
  GTypeInterface parent_iface;

  ClutterActor     *(* get_wrapper)             (ClutterStageWindow *stage_window);

  void              (* set_title)               (ClutterStageWindow *stage_window,
                                                 const gchar        *title);
  void              (* set_fullscreen)          (ClutterStageWindow *stage_window,
                                                 gboolean            is_fullscreen);
  void              (* set_cursor_visible)      (ClutterStageWindow *stage_window,
                                                 gboolean            cursor_visible);
  void              (* set_user_resizable)      (ClutterStageWindow *stage_window,
                                                 gboolean            is_resizable);

  gboolean          (* realize)                 (ClutterStageWindow *stage_window);
  void              (* unrealize)               (ClutterStageWindow *stage_window);

  void              (* show)                    (ClutterStageWindow *stage_window,
                                                 gboolean            do_raise);
  void              (* hide)                    (ClutterStageWindow *stage_window);

  void              (* resize)                  (ClutterStageWindow *stage_window,
                                                 gint                width,
                                                 gint                height);
  void              (* get_geometry)            (ClutterStageWindow *stage_window,
                                                 cairo_rectangle_int_t *geometry);

  void              (* schedule_update)         (ClutterStageWindow *stage_window,
                                                 int                 sync_delay);
  gint64            (* get_update_time)         (ClutterStageWindow *stage_window);
  void              (* clear_update_time)       (ClutterStageWindow *stage_window);

  void              (* add_redraw_clip)         (ClutterStageWindow    *stage_window,
                                                 cairo_rectangle_int_t *stage_rectangle);
  gboolean          (* has_redraw_clips)        (ClutterStageWindow    *stage_window);
  gboolean          (* ignoring_redraw_clips)   (ClutterStageWindow    *stage_window);
  gboolean          (* get_redraw_clip_bounds)  (ClutterStageWindow    *stage_window,
                                                 cairo_rectangle_int_t *clip);


  void              (* set_accept_focus)        (ClutterStageWindow *stage_window,
                                                 gboolean            accept_focus);

  void              (* redraw)                  (ClutterStageWindow *stage_window);

  void              (* dirty_back_buffer)       (ClutterStageWindow *stage_window);

  void              (* get_dirty_pixel)         (ClutterStageWindow *stage_window,
                                                 int *x, int *y);

  CoglFramebuffer  *(* get_active_framebuffer)  (ClutterStageWindow *stage_window);

  gboolean          (* can_clip_redraws)        (ClutterStageWindow *stage_window);

  void              (* set_scale_factor)        (ClutterStageWindow *stage_window,
                                                 int                 factor);
  int               (* get_scale_factor)        (ClutterStageWindow *stage_window);
};

GType _clutter_stage_window_get_type (void) G_GNUC_CONST;

ClutterActor *    _clutter_stage_window_get_wrapper        (ClutterStageWindow *window);

void              _clutter_stage_window_set_title          (ClutterStageWindow *window,
                                                            const gchar        *title);
void              _clutter_stage_window_set_fullscreen     (ClutterStageWindow *window,
                                                            gboolean            is_fullscreen);
void              _clutter_stage_window_set_cursor_visible (ClutterStageWindow *window,
                                                            gboolean            is_visible);
void              _clutter_stage_window_set_user_resizable (ClutterStageWindow *window,
                                                            gboolean            is_resizable);

gboolean          _clutter_stage_window_realize                 (ClutterStageWindow *window);
void              _clutter_stage_window_unrealize               (ClutterStageWindow *window);

void              _clutter_stage_window_show                    (ClutterStageWindow *window,
                                                                 gboolean            do_raise);
void              _clutter_stage_window_hide                    (ClutterStageWindow *window);

void              _clutter_stage_window_resize                  (ClutterStageWindow *window,
                                                                 gint                width,
                                                                 gint                height);
void              _clutter_stage_window_get_geometry            (ClutterStageWindow *window,
                                                                 cairo_rectangle_int_t *geometry);
void              _clutter_stage_window_schedule_update         (ClutterStageWindow *window,
                                                                 int                 sync_delay);
gint64            _clutter_stage_window_get_update_time         (ClutterStageWindow *window);
void              _clutter_stage_window_clear_update_time       (ClutterStageWindow *window);

void              _clutter_stage_window_add_redraw_clip         (ClutterStageWindow    *window,
                                                                 cairo_rectangle_int_t *stage_clip);
gboolean          _clutter_stage_window_has_redraw_clips        (ClutterStageWindow    *window);
gboolean          _clutter_stage_window_ignoring_redraw_clips   (ClutterStageWindow    *window);
gboolean          _clutter_stage_window_get_redraw_clip_bounds  (ClutterStageWindow    *window,
                                                                 cairo_rectangle_int_t *clip);

void              _clutter_stage_window_set_accept_focus        (ClutterStageWindow *window,
                                                                 gboolean            accept_focus);

void              _clutter_stage_window_redraw                  (ClutterStageWindow *window);

void              _clutter_stage_window_dirty_back_buffer       (ClutterStageWindow *window);

void              _clutter_stage_window_get_dirty_pixel         (ClutterStageWindow *window,
                                                                 int *x, int *y);

CoglFramebuffer  *_clutter_stage_window_get_active_framebuffer  (ClutterStageWindow *window);

gboolean          _clutter_stage_window_can_clip_redraws        (ClutterStageWindow *window);

void              _clutter_stage_window_set_scale_factor        (ClutterStageWindow *window,
                                                                 int                 factor);
int               _clutter_stage_window_get_scale_factor        (ClutterStageWindow *window);

G_END_DECLS

#endif /* __CLUTTER_STAGE_WINDOW_H__ */
