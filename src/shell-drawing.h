/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __SHELL_DRAWING_H__
#define __SHELL_DRAWING_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

typedef enum {
  SHELL_POINTER_UP,
  SHELL_POINTER_DOWN,
  SHELL_POINTER_LEFT,
  SHELL_POINTER_RIGHT
} ShellPointerDirection;

void shell_draw_box_pointer (ClutterCairoTexture   *texture,
                             ShellPointerDirection  direction,
                             ClutterColor          *border_color,
                             ClutterColor          *background_color);

void shell_draw_clock (ClutterCairoTexture *texture,
	               int                  hour,
	               int                  minute);

guint shell_add_hook_paint_red_border (ClutterActor *actor);

G_END_DECLS

#endif /* __SHELL_GLOBAL_H__ */
