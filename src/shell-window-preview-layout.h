#ifndef __SHELL_WINDOW_PREVIEW_LAYOUT_H__
#define __SHELL_WINDOW_PREVIEW_LAYOUT_H__

G_BEGIN_DECLS

#include <clutter/clutter.h>

#define SHELL_TYPE_WINDOW_PREVIEW_LAYOUT (shell_window_preview_layout_get_type ())
G_DECLARE_FINAL_TYPE (ShellWindowPreviewLayout, shell_window_preview_layout,
                      SHELL, WINDOW_PREVIEW_LAYOUT, ClutterLayoutManager)

typedef struct _ShellWindowPreviewLayout ShellWindowPreviewLayout;
typedef struct _ShellWindowPreviewLayoutPrivate ShellWindowPreviewLayoutPrivate;

struct _ShellWindowPreviewLayout
{
  /*< private >*/
  ClutterLayoutManager parent;

  ShellWindowPreviewLayoutPrivate *priv;
};

ClutterActor * shell_window_preview_layout_add_window (ShellWindowPreviewLayout  *self,
                                                       MetaWindow *window);

void  shell_window_preview_layout_remove_window (ShellWindowPreviewLayout  *self,
                                                 MetaWindow *window);

GList * shell_window_preview_layout_get_windows (ShellWindowPreviewLayout  *self);

G_END_DECLS

#endif /* __SHELL_WINDOW_PREVIEW_LAYOUT_H__ */
