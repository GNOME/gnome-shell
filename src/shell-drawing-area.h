/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_DRAWING_AREA_H__
#define __SHELL_DRAWING_AREA_H__

#include <clutter/clutter.h>
#include <gtk/gtk.h>

#define SHELL_TYPE_DRAWING_AREA                 (shell_drawing_area_get_type ())
#define SHELL_DRAWING_AREA(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_DRAWING_AREA, ShellDrawingArea))
#define SHELL_DRAWING_AREA_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_DRAWING_AREA, ShellDrawingAreaClass))
#define SHELL_IS_DRAWING_AREA(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_DRAWING_AREA))
#define SHELL_IS_DRAWING_AREA_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_DRAWING_AREA))
#define SHELL_DRAWING_AREA_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_DRAWING_AREA, ShellDrawingAreaClass))

typedef struct _ShellDrawingArea        ShellDrawingArea;
typedef struct _ShellDrawingAreaClass   ShellDrawingAreaClass;

typedef struct _ShellDrawingAreaPrivate ShellDrawingAreaPrivate;

struct _ShellDrawingArea
{
    ClutterGroup parent;

    ShellDrawingAreaPrivate *priv;
};

struct _ShellDrawingAreaClass
{
    ClutterGroupClass parent_class;

    void (*redraw) (ShellDrawingArea *area, ClutterCairoTexture *texture);
};

GType shell_drawing_area_get_type (void) G_GNUC_CONST;

ClutterCairoTexture *shell_drawing_area_get_texture (ShellDrawingArea *area);

#endif /* __SHELL_DRAWING_AREA_H__ */
