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
};

GType clutter_stage_window_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_STAGE_WINDOW_H__ */
