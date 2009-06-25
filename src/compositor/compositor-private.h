/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef META_COMPOSITOR_PRIVATE_H
#define META_COMPOSITOR_PRIVATE_H

#include "compositor.h"
#include "display.h"
#include <clutter/clutter.h>

struct _MetaCompositor
{
  MetaDisplay    *display;

  Atom            atom_x_root_pixmap;
  Atom            atom_x_set_root;
  Atom            atom_net_wm_window_opacity;

  ClutterActor   *shadow_src;

  gboolean        show_redraw : 1;
  gboolean        debug       : 1;
  gboolean        no_mipmaps  : 1;
};

#endif /* META_COMPOSITOR_PRIVATE_H */
