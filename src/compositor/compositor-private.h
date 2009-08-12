/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef META_COMPOSITOR_PRIVATE_H
#define META_COMPOSITOR_PRIVATE_H

#include <X11/extensions/Xfixes.h>

#include "compositor.h"
#include "display.h"
#include "mutter-plugin-manager.h"
#include <clutter/clutter.h>

typedef struct _MetaCompScreen MetaCompScreen;

struct _MetaCompositor
{
  MetaDisplay    *display;

  Atom            atom_x_root_pixmap;
  Atom            atom_x_set_root;
  Atom            atom_net_wm_window_opacity;
  guint           repaint_func_id;

  ClutterActor   *shadow_src;

  MutterPlugin   *modal_plugin;

  gboolean        show_redraw : 1;
  gboolean        debug       : 1;
  gboolean        no_mipmaps  : 1;
};

struct _MetaCompScreen
{
  MetaScreen            *screen;

  ClutterActor          *stage, *window_group, *overlay_group;
  ClutterActor		*hidden_group;
  GList                 *windows;
  GHashTable            *windows_by_xid;
  Window                 output;

  /* Before we create the output window */
  XserverRegion     pending_input_region;

  gint                   switch_workspace_in_progress;

  MutterPluginManager *plugin_mgr;
};

void mutter_switch_workspace_completed (MetaScreen    *screen);
void mutter_set_stage_input_region     (MetaScreen    *screen,
                                        XserverRegion  region);
void mutter_empty_stage_input_region   (MetaScreen    *screen);

gboolean mutter_begin_modal_for_plugin (MetaScreen       *screen,
                                        MutterPlugin     *plugin,
                                        Window            grab_window,
                                        Cursor            cursor,
                                        MetaModalOptions  options,
                                        guint32           timestamp);
void     mutter_end_modal_for_plugin   (MetaScreen       *screen,
                                        MutterPlugin     *plugin,
                                        guint32           timestamp);

void mutter_check_end_modal (MetaScreen *screen);

#endif /* META_COMPOSITOR_PRIVATE_H */
