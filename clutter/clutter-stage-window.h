#ifndef __CLUTTER_STAGE_WINDOW_H__
#define __CLUTTER_STAGE_WINDOW_H__

#include <clutter/clutter-actor.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_STAGE_WINDOW               (clutter_stage_window_get_type ())
#define CLUTTER_STAGE_WINDOW(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_WINDOW, ClutterStageWindow))
#define CLUTTER_IS_STAGE_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_WINDOW))
#define CLUTTER_STAGE_WINDOW_GET_IFACE(obj)     (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CLUTTER_TYPE_STAGE_WINDOW, ClutterStageWindowIface))

typedef struct _ClutterStageWindow      ClutterStageWindow; /* dummy */
typedef struct _ClutterStageWindowIface ClutterStageWindowIface;

struct _ClutterStageWindowIface
{
  GTypeInterface parent_iface;

  ClutterActor *(* get_wrapper)        (ClutterStageWindow *stage_window);

  void          (* set_title)          (ClutterStageWindow *stage_window,
                                        const gchar        *title);
  void          (* set_fullscreen)     (ClutterStageWindow *stage_window,
                                        gboolean            is_fullscreen);
  void          (* set_cursor_visible) (ClutterStageWindow *stage_window,
                                        gboolean            cursor_visible);
  void          (* set_user_resizable) (ClutterStageWindow *stage_window,
                                        gboolean            is_resizable);

  gboolean      (* realize)            (ClutterStageWindow *stage_window);
  void          (* unrealize)          (ClutterStageWindow *stage_window);

  void          (* show)               (ClutterStageWindow *stage_window,
                                        gboolean            do_raise);
  void          (* hide)               (ClutterStageWindow *stage_window);

  void          (* resize)             (ClutterStageWindow *stage_window,
                                        gint                width,
                                        gint                height);
  void          (* get_geometry)       (ClutterStageWindow *stage_window,
                                        ClutterGeometry    *geometry);

  int           (* get_pending_swaps)  (ClutterStageWindow *stage_window);
};

GType clutter_stage_window_get_type (void) G_GNUC_CONST;

ClutterActor *_clutter_stage_window_get_wrapper        (ClutterStageWindow *window);

void          _clutter_stage_window_set_title          (ClutterStageWindow *window,
                                                        const gchar        *title);
void          _clutter_stage_window_set_fullscreen     (ClutterStageWindow *window,
                                                        gboolean            is_fullscreen);
void          _clutter_stage_window_set_cursor_visible (ClutterStageWindow *window,
                                                        gboolean            is_visible);
void          _clutter_stage_window_set_user_resizable (ClutterStageWindow *window,
                                                        gboolean            is_resizable);

gboolean      _clutter_stage_window_realize            (ClutterStageWindow *window);
void          _clutter_stage_window_unrealize          (ClutterStageWindow *window);

void          _clutter_stage_window_show               (ClutterStageWindow *window,
                                                        gboolean            do_raise);
void          _clutter_stage_window_hide               (ClutterStageWindow *window);

void          _clutter_stage_window_resize             (ClutterStageWindow *window,
                                                        gint                width,
                                                        gint                height);
void          _clutter_stage_window_get_geometry       (ClutterStageWindow *window,
                                                        ClutterGeometry    *geometry);
int           _clutter_stage_window_get_pending_swaps  (ClutterStageWindow *window);

G_END_DECLS

#endif /* __CLUTTER_STAGE_WINDOW_H__ */
