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

void shell_draw_glow (ClutterCairoTexture *texture,
                      double red,
                      double blue,
                      double green,
                      double alpha);

G_END_DECLS

#endif /* __SHELL_GLOBAL_H__ */
