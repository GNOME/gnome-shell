/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __SHELL_DRAWING_H__
#define __SHELL_DRAWING_H__

#include <clutter/clutter.h>
#include "st.h"

G_BEGIN_DECLS

void shell_draw_clock (StDrawingArea       *area,
	               int                  hour,
	               int                  minute);

G_END_DECLS

#endif /* __SHELL_GLOBAL_H__ */
