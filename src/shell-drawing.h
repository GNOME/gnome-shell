/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __SHELL_DRAWING_H__
#define __SHELL_DRAWING_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

ClutterCairoTexture *shell_create_vertical_gradient (ClutterColor *top,
                                                     ClutterColor *bottom);

ClutterCairoTexture *shell_create_horizontal_gradient (ClutterColor *left,
                                                       ClutterColor *right);

void shell_draw_clock (ClutterCairoTexture *texture,
	               int                  hour,
	               int                  minute);

void shell_draw_app_highlight (ClutterCairoTexture *texture,
                               gboolean             multi,
                               double               red,
                               double               blue,
                               double               green,
                               double               alpha);

guint shell_add_hook_paint_red_border (ClutterActor *actor);

G_END_DECLS

#endif /* __SHELL_GLOBAL_H__ */
