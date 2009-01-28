/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity interface for talking to GTK+ UI module */

/* 
 * Copyright (C) 2002 Havoc Pennington
 * stock icon code Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "prefs.h"
#include "ui.h"
#include "frames.h"
#include "util.h"
#include "menu.h"
#include "core.h"
#include "theme.h"

#include "inlinepixbufs.h"

#include <string.h>
#include <stdlib.h>

static void meta_stock_icons_init (void);
static void meta_ui_accelerator_parse (const char      *accel,
                                       guint           *keysym,
                                       guint           *keycode,
                                       GdkModifierType *keymask);

struct _MetaUI
{
  Display *xdisplay;
  Screen *xscreen;
  MetaFrames *frames;
};

void
meta_ui_init (int *argc, char ***argv)
{
  if (!gtk_init_check (argc, argv))
    meta_fatal ("Unable to open X display %s\n", XDisplayName (NULL));

  meta_stock_icons_init ();
}

Display*
meta_ui_get_display (void)
{
  return gdk_display;
}

typedef struct _EventFunc EventFunc;

struct _EventFunc
{
  MetaEventFunc func;
  gpointer data;
};

static EventFunc *ef = NULL;

static GdkFilterReturn
filter_func (GdkXEvent *xevent,
             GdkEvent *event,
             gpointer data)
{
  g_return_val_if_fail (ef != NULL, GDK_FILTER_CONTINUE);

  if ((* ef->func) (xevent, ef->data))
    return GDK_FILTER_REMOVE;
  else
    return GDK_FILTER_CONTINUE;
}

void
meta_ui_add_event_func (Display       *xdisplay,
                        MetaEventFunc  func,
                        gpointer       data)
{
  g_return_if_fail (ef == NULL);

  ef = g_new (EventFunc, 1);
  ef->func = func;
  ef->data = data;

  gdk_window_add_filter (NULL, filter_func, ef);
}

/* removal is by data due to proxy function */
void
meta_ui_remove_event_func (Display       *xdisplay,
                           MetaEventFunc  func,
                           gpointer       data)
{
  g_return_if_fail (ef != NULL);
  
  gdk_window_remove_filter (NULL, filter_func, ef);

  g_free (ef);
  ef = NULL;
}

MetaUI*
meta_ui_new (Display *xdisplay,
             Screen  *screen)
{
  MetaUI *ui;

  ui = g_new (MetaUI, 1);
  ui->xdisplay = xdisplay;
  ui->xscreen = screen;

  g_assert (xdisplay == gdk_display);
  ui->frames = meta_frames_new (XScreenNumberOfScreen (screen));
  gtk_widget_realize (GTK_WIDGET (ui->frames));
  
  return ui;
}

void
meta_ui_free (MetaUI *ui)
{
  gtk_widget_destroy (GTK_WIDGET (ui->frames));

  g_free (ui);
}

void
meta_ui_get_frame_geometry (MetaUI *ui,
                            Window frame_xwindow,
                            int *top_height, int *bottom_height,
                            int *left_width, int *right_width)
{
  meta_frames_get_geometry (ui->frames, frame_xwindow,
                            top_height, bottom_height,
                            left_width, right_width);
}

Window
meta_ui_create_frame_window (MetaUI *ui,
                             Display *xdisplay,
                             Visual *xvisual,
			     gint x,
			     gint y,
			     gint width,
			     gint height,
			     gint screen_no)
{
  GdkDisplay *display = gdk_x11_lookup_xdisplay (xdisplay);
  GdkScreen *screen = gdk_display_get_screen (display, screen_no);
  GdkWindowAttr attrs;
  gint attributes_mask;
  GdkWindow *window;
  GdkVisual *visual;
  GdkColormap *cmap = gdk_screen_get_default_colormap (screen);
  
  /* Default depth/visual handles clients with weird visuals; they can
   * always be children of the root depth/visual obviously, but
   * e.g. DRI games can't be children of a parent that has the same
   * visual as the client.
   */
  if (!xvisual)
    visual = gdk_screen_get_system_visual (screen);
  else
    {
      visual = gdk_x11_screen_lookup_visual (screen,
                                             XVisualIDFromVisual (xvisual));
      cmap = gdk_colormap_new (visual, FALSE);
    }

  attrs.title = NULL;

  /* frame.c is going to replace the event mask immediately, but
   * we still have to set it here to let GDK know what it is.
   */
  attrs.event_mask =
    GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
    GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
    GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_FOCUS_CHANGE_MASK;
  attrs.x = x;
  attrs.y = y;
  attrs.wclass = GDK_INPUT_OUTPUT;
  attrs.visual = visual;
  attrs.colormap = cmap;
  attrs.window_type = GDK_WINDOW_CHILD;
  attrs.cursor = NULL;
  attrs.wmclass_name = NULL;
  attrs.wmclass_class = NULL;
  attrs.override_redirect = FALSE;

  attrs.width  = width;
  attrs.height = height;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  window =
    gdk_window_new (gdk_screen_get_root_window(screen),
		    &attrs, attributes_mask);

  gdk_window_resize (window, width, height);
  
  meta_frames_manage_window (ui->frames, GDK_WINDOW_XID (window), window);

  return GDK_WINDOW_XID (window);
}

void
meta_ui_destroy_frame_window (MetaUI *ui,
			      Window  xwindow)
{
  meta_frames_unmanage_window (ui->frames, xwindow);
}

void
meta_ui_move_resize_frame (MetaUI *ui,
			   Window frame,
			   int x,
			   int y,
			   int width,
			   int height)
{
  meta_frames_move_resize_frame (ui->frames, frame, x, y, width, height);
}

void
meta_ui_map_frame   (MetaUI *ui,
                     Window  xwindow)
{
  GdkWindow *window;

  window = gdk_xid_table_lookup (xwindow);

  if (window)
    gdk_window_show_unraised (window);
}

void
meta_ui_unmap_frame (MetaUI *ui,
                     Window  xwindow)
{
  GdkWindow *window;

  window = gdk_xid_table_lookup (xwindow);

  if (window)
    gdk_window_hide (window);
}

void
meta_ui_unflicker_frame_bg (MetaUI *ui,
                            Window  xwindow,
                            int     target_width,
                            int     target_height)
{
  meta_frames_unflicker_bg (ui->frames, xwindow,
                            target_width, target_height);
}

void
meta_ui_repaint_frame (MetaUI *ui,
                       Window xwindow)
{
  meta_frames_repaint_frame (ui->frames, xwindow);
}

void
meta_ui_reset_frame_bg (MetaUI *ui,
                        Window xwindow)
{
  meta_frames_reset_bg (ui->frames, xwindow);
}

void
meta_ui_apply_frame_shape  (MetaUI  *ui,
                            Window   xwindow,
                            int      new_window_width,
                            int      new_window_height,
                            gboolean window_has_shape)
{
  meta_frames_apply_shapes (ui->frames, xwindow,
                            new_window_width, new_window_height,
                            window_has_shape);
}

void
meta_ui_queue_frame_draw (MetaUI *ui,
                          Window xwindow)
{
  meta_frames_queue_draw (ui->frames, xwindow);
}


void
meta_ui_set_frame_title (MetaUI     *ui,
                         Window      xwindow,
                         const char *title)
{
  meta_frames_set_title (ui->frames, xwindow, title);
}

MetaWindowMenu*
meta_ui_window_menu_new  (MetaUI             *ui,
                          Window              client_xwindow,
                          MetaMenuOp          ops,
                          MetaMenuOp          insensitive,
                          unsigned long       active_workspace,
                          int                 n_workspaces,
                          MetaWindowMenuFunc  func,
                          gpointer            data)
{
  return meta_window_menu_new (ui->frames,
                               ops, insensitive,
                               client_xwindow,
                               active_workspace,
                               n_workspaces,
                               func, data);
}

void
meta_ui_window_menu_popup (MetaWindowMenu     *menu,
                           int                 root_x,
                           int                 root_y,
                           int                 button,
                           guint32             timestamp)
{
  meta_window_menu_popup (menu, root_x, root_y, button, timestamp);
}

void
meta_ui_window_menu_free (MetaWindowMenu *menu)
{
  meta_window_menu_free (menu);
}

struct _MetaImageWindow
{
  GtkWidget *window;
  GdkPixmap *pixmap;
};

MetaImageWindow*
meta_image_window_new (Display *xdisplay,
                       int      screen_number,
                       int      max_width,
                       int      max_height)
{
  MetaImageWindow *iw;
  GdkDisplay *gdisplay;
  GdkScreen *gscreen;
    
  iw = g_new (MetaImageWindow, 1);
  iw->window = gtk_window_new (GTK_WINDOW_POPUP);
    
  gdisplay = gdk_x11_lookup_xdisplay (xdisplay);
  gscreen = gdk_display_get_screen (gdisplay, screen_number);
  
  gtk_window_set_screen (GTK_WINDOW (iw->window), gscreen);
 
  gtk_widget_realize (iw->window);
  iw->pixmap = gdk_pixmap_new (iw->window->window,
                               max_width, max_height,
                               -1);
  
  gtk_widget_set_size_request (iw->window, 1, 1);
  gtk_widget_set_double_buffered (iw->window, FALSE);
  gtk_widget_set_app_paintable (iw->window, TRUE);
  
  return iw;
}

void
meta_image_window_free (MetaImageWindow *iw)
{
  gtk_widget_destroy (iw->window);
  g_object_unref (G_OBJECT (iw->pixmap));
  g_free (iw);
}

void
meta_image_window_set_showing  (MetaImageWindow *iw,
                                gboolean         showing)
{
  if (showing)
    gtk_widget_show_all (iw->window);
  else
    {
      gtk_widget_hide (iw->window);
      meta_core_increment_event_serial (gdk_display);
    }
}

void
meta_image_window_set (MetaImageWindow *iw,
                       GdkPixbuf       *pixbuf,
                       int              x,
                       int              y)
{
  /* We use a back pixmap to avoid having to handle exposes, because
   * it's really too slow for large clients being minimized, etc.
   * and this way flicker is genuinely zero.
   */

  gdk_draw_pixbuf (iw->pixmap,
                   iw->window->style->black_gc,
		   pixbuf,
                   0, 0,
                   0, 0,
                   gdk_pixbuf_get_width (pixbuf),
                   gdk_pixbuf_get_height (pixbuf),
                   GDK_RGB_DITHER_NORMAL,
                   0, 0);

  gdk_window_set_back_pixmap (iw->window->window,
                              iw->pixmap,
                              FALSE);
  
  gdk_window_move_resize (iw->window->window,
                          x, y,
                          gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf));

  gdk_window_clear (iw->window->window);
}

static GdkColormap*
get_cmap (GdkPixmap *pixmap)
{
  GdkColormap *cmap;

  cmap = gdk_drawable_get_colormap (pixmap);
  if (cmap)
    g_object_ref (G_OBJECT (cmap));

  if (cmap == NULL)
    {
      if (gdk_drawable_get_depth (pixmap) == 1)
        {
          meta_verbose ("Using NULL colormap for snapshotting bitmap\n");
          cmap = NULL;
        }
      else
        {
          meta_verbose ("Using system cmap to snapshot pixmap\n");
          cmap = gdk_screen_get_system_colormap (gdk_drawable_get_screen (pixmap));

          g_object_ref (G_OBJECT (cmap));
        }
    }

  /* Be sure we aren't going to blow up due to visual mismatch */
  if (cmap &&
      (gdk_colormap_get_visual (cmap)->depth !=
       gdk_drawable_get_depth (pixmap)))
    {
      cmap = NULL;
      meta_verbose ("Switching back to NULL cmap because of depth mismatch\n");
    }
  
  return cmap;
}

GdkPixbuf*
meta_gdk_pixbuf_get_from_window (GdkPixbuf   *dest,
                                 Window       xwindow,
                                 int          src_x,
                                 int          src_y,
                                 int          dest_x,
                                 int          dest_y,
                                 int          width,
                                 int          height)
{
  GdkDrawable *drawable;
  GdkPixbuf *retval;
  GdkColormap *cmap;
  
  retval = NULL;
  
  drawable = gdk_xid_table_lookup (xwindow);

  if (drawable)
    g_object_ref (G_OBJECT (drawable));
  else
    drawable = gdk_window_foreign_new (xwindow);

  cmap = get_cmap (drawable);
  
  retval = gdk_pixbuf_get_from_drawable (dest,
                                         drawable,
                                         cmap,
                                         src_x, src_y,
                                         dest_x, dest_y,
                                         width, height);

  if (cmap)
    g_object_unref (G_OBJECT (cmap));
  g_object_unref (G_OBJECT (drawable));

  return retval;
}

GdkPixbuf*
meta_gdk_pixbuf_get_from_pixmap (GdkPixbuf   *dest,
                                 Pixmap       xpixmap,
                                 int          src_x,
                                 int          src_y,
                                 int          dest_x,
                                 int          dest_y,
                                 int          width,
                                 int          height)
{
  GdkDrawable *drawable;
  GdkPixbuf *retval;
  GdkColormap *cmap;
  
  retval = NULL;
  cmap = NULL;
  
  drawable = gdk_xid_table_lookup (xpixmap);

  if (drawable)
    g_object_ref (G_OBJECT (drawable));
  else
    drawable = gdk_pixmap_foreign_new (xpixmap);

  if (drawable)
    {
      cmap = get_cmap (drawable);
  
      retval = gdk_pixbuf_get_from_drawable (dest,
                                             drawable,
                                             cmap,
                                             src_x, src_y,
                                             dest_x, dest_y,
                                             width, height);
    }
  if (cmap)
    g_object_unref (G_OBJECT (cmap));
  if (drawable)
    g_object_unref (G_OBJECT (drawable));

  return retval;
}

void
meta_ui_push_delay_exposes (MetaUI *ui)
{
  meta_frames_push_delay_exposes (ui->frames);
}

void
meta_ui_pop_delay_exposes  (MetaUI *ui)
{
  meta_frames_pop_delay_exposes (ui->frames);
}

GdkPixbuf*
meta_ui_get_default_window_icon (MetaUI *ui)
{
  static GdkPixbuf *default_icon = NULL;

  if (default_icon == NULL)
    {
      GtkIconTheme *theme;
      gboolean icon_exists;

      theme = gtk_icon_theme_get_default ();

      icon_exists = gtk_icon_theme_has_icon (theme, META_DEFAULT_ICON_NAME);

      if (icon_exists)
          default_icon = gtk_icon_theme_load_icon (theme,
                                                   META_DEFAULT_ICON_NAME,
                                                   META_ICON_WIDTH,
                                                   0,
                                                   NULL);
      else
          default_icon = gtk_icon_theme_load_icon (theme,
                                                   "gtk-missing-image",
                                                   META_ICON_WIDTH,
                                                   0,
                                                   NULL);

      g_assert (default_icon);
    }

  g_object_ref (G_OBJECT (default_icon));
  
  return default_icon;
}

GdkPixbuf*
meta_ui_get_default_mini_icon (MetaUI *ui)
{
  static GdkPixbuf *default_icon = NULL;

  if (default_icon == NULL)
    {
      GtkIconTheme *theme;
      gboolean icon_exists;

      theme = gtk_icon_theme_get_default ();

      icon_exists = gtk_icon_theme_has_icon (theme, META_DEFAULT_ICON_NAME);

      if (icon_exists)
          default_icon = gtk_icon_theme_load_icon (theme,
                                                   META_DEFAULT_ICON_NAME,
                                                   META_MINI_ICON_WIDTH,
                                                   0,
                                                   NULL);
      else
          default_icon = gtk_icon_theme_load_icon (theme,
                                                   "gtk-missing-image",
                                                   META_MINI_ICON_WIDTH,
                                                   0,
                                                   NULL);

      g_assert (default_icon);
    }

  g_object_ref (G_OBJECT (default_icon));
  
  return default_icon;
}

gboolean
meta_ui_window_should_not_cause_focus (Display *xdisplay,
                                       Window   xwindow)
{
  GdkWindow *window;

  window = gdk_xid_table_lookup (xwindow);

  /* we shouldn't cause focus if we're an override redirect
   * toplevel which is not foreign
   */
  if (window && gdk_window_get_type (window) == GDK_WINDOW_TEMP)
    return TRUE;
  else
    return FALSE;
}

char*
meta_text_property_to_utf8 (Display             *xdisplay,
                            const XTextProperty *prop)
{
  char **list;
  int count;
  char *retval;
  
  list = NULL;

  count = gdk_text_property_to_utf8_list (gdk_x11_xatom_to_atom (prop->encoding),
                                          prop->format,
                                          prop->value,
                                          prop->nitems,
                                          &list);

  if (count == 0)
    retval = NULL;
  else
    {
      retval = list[0];
      list[0] = g_strdup (""); /* something to free */
    }
  
  g_strfreev (list);

  return retval;
}

void
meta_ui_theme_get_frame_borders (MetaUI *ui,
                                 MetaFrameType      type,
                                 MetaFrameFlags     flags,
                                 int               *top_height,
                                 int               *bottom_height,
                                 int               *left_width,
                                 int               *right_width)
{
  int text_height;
  PangoContext *context;
  const PangoFontDescription *font_desc;
  GtkStyle *default_style;

  if (meta_ui_have_a_theme ())
    {
      context = gtk_widget_get_pango_context (GTK_WIDGET (ui->frames));
      font_desc = meta_prefs_get_titlebar_font ();

      if (!font_desc)
        {
          default_style = gtk_widget_get_default_style ();
          font_desc = default_style->font_desc;
        }

      text_height = meta_pango_font_desc_get_text_height (font_desc, context);

      meta_theme_get_frame_borders (meta_theme_get_current (),
                                    type, text_height, flags,
                                    top_height, bottom_height,
                                    left_width, right_width);
    }
  else
    {
      *top_height = *bottom_height = *left_width = *right_width = 0;
    }
}

void
meta_ui_set_current_theme (const char *name,
                           gboolean    force_reload)
{
  meta_theme_set_current (name, force_reload);
  meta_invalidate_default_icons ();
}

gboolean
meta_ui_have_a_theme (void)
{
  return meta_theme_get_current () != NULL;
}

static void
meta_ui_accelerator_parse (const char      *accel,
                           guint           *keysym,
                           guint           *keycode,
                           GdkModifierType *keymask)
{
  if (accel[0] == '0' && accel[1] == 'x')
    {
      *keysym = 0;
      *keycode = (guint) strtoul (accel, NULL, 16);
      *keymask = 0;
    }
  else
    gtk_accelerator_parse (accel, keysym, keymask);
}

gboolean
meta_ui_parse_accelerator (const char          *accel,
                           unsigned int        *keysym,
                           unsigned int        *keycode,
                           MetaVirtualModifier *mask)
{
  GdkModifierType gdk_mask = 0;
  guint gdk_sym = 0;
  guint gdk_code = 0;
  
  *keysym = 0;
  *keycode = 0;
  *mask = 0;

  if (strcmp (accel, "disabled") == 0)
    return TRUE;
  
  meta_ui_accelerator_parse (accel, &gdk_sym, &gdk_code, &gdk_mask);
  if (gdk_mask == 0 && gdk_sym == 0 && gdk_code == 0)
    return FALSE;

  if (gdk_sym == None && gdk_code == 0)
    return FALSE;
  
  if (gdk_mask & GDK_RELEASE_MASK) /* we don't allow this */
    return FALSE;
  
  *keysym = gdk_sym;
  *keycode = gdk_code;

  if (gdk_mask & GDK_SHIFT_MASK)
    *mask |= META_VIRTUAL_SHIFT_MASK;
  if (gdk_mask & GDK_CONTROL_MASK)
    *mask |= META_VIRTUAL_CONTROL_MASK;
  if (gdk_mask & GDK_MOD1_MASK)
    *mask |= META_VIRTUAL_ALT_MASK;
  if (gdk_mask & GDK_MOD2_MASK)
    *mask |= META_VIRTUAL_MOD2_MASK;
  if (gdk_mask & GDK_MOD3_MASK)
    *mask |= META_VIRTUAL_MOD3_MASK;
  if (gdk_mask & GDK_MOD4_MASK)
    *mask |= META_VIRTUAL_MOD4_MASK;
  if (gdk_mask & GDK_MOD5_MASK)
    *mask |= META_VIRTUAL_MOD5_MASK;
  if (gdk_mask & GDK_SUPER_MASK)
    *mask |= META_VIRTUAL_SUPER_MASK;
  if (gdk_mask & GDK_HYPER_MASK)
    *mask |= META_VIRTUAL_HYPER_MASK;
  if (gdk_mask & GDK_META_MASK)
    *mask |= META_VIRTUAL_META_MASK;
  
  return TRUE;
}

/* Caller responsible for freeing return string of meta_ui_accelerator_name! */
gchar*
meta_ui_accelerator_name  (unsigned int        keysym,
                           MetaVirtualModifier mask)
{
  GdkModifierType mods = 0;
        
  if (keysym == 0 && mask == 0)
    {
      return g_strdup ("disabled");
    }

  if (mask & META_VIRTUAL_SHIFT_MASK)
    mods |= GDK_SHIFT_MASK;
  if (mask & META_VIRTUAL_CONTROL_MASK)
    mods |= GDK_CONTROL_MASK;
  if (mask & META_VIRTUAL_ALT_MASK)
    mods |= GDK_MOD1_MASK;
  if (mask & META_VIRTUAL_MOD2_MASK)
    mods |= GDK_MOD2_MASK;
  if (mask & META_VIRTUAL_MOD3_MASK)
    mods |= GDK_MOD3_MASK;
  if (mask & META_VIRTUAL_MOD4_MASK)
    mods |= GDK_MOD4_MASK;
  if (mask & META_VIRTUAL_MOD5_MASK)
    mods |= GDK_MOD5_MASK;
  if (mask & META_VIRTUAL_SUPER_MASK)
    mods |= GDK_SUPER_MASK;
  if (mask & META_VIRTUAL_HYPER_MASK)
    mods |= GDK_HYPER_MASK;
  if (mask & META_VIRTUAL_META_MASK)
    mods |= GDK_META_MASK;

  return gtk_accelerator_name (keysym, mods);

}

gboolean
meta_ui_parse_modifier (const char          *accel,
                        MetaVirtualModifier *mask)
{
  GdkModifierType gdk_mask = 0;
  guint gdk_sym = 0;
  guint gdk_code = 0;
  
  *mask = 0;

  if (accel == NULL || strcmp (accel, "disabled") == 0)
    return TRUE;
  
  meta_ui_accelerator_parse (accel, &gdk_sym, &gdk_code, &gdk_mask);
  if (gdk_mask == 0 && gdk_sym == 0 && gdk_code == 0)
    return FALSE;

  if (gdk_sym != None || gdk_code != 0)
    return FALSE;
  
  if (gdk_mask & GDK_RELEASE_MASK) /* we don't allow this */
    return FALSE;

  if (gdk_mask & GDK_SHIFT_MASK)
    *mask |= META_VIRTUAL_SHIFT_MASK;
  if (gdk_mask & GDK_CONTROL_MASK)
    *mask |= META_VIRTUAL_CONTROL_MASK;
  if (gdk_mask & GDK_MOD1_MASK)
    *mask |= META_VIRTUAL_ALT_MASK;
  if (gdk_mask & GDK_MOD2_MASK)
    *mask |= META_VIRTUAL_MOD2_MASK;
  if (gdk_mask & GDK_MOD3_MASK)
    *mask |= META_VIRTUAL_MOD3_MASK;
  if (gdk_mask & GDK_MOD4_MASK)
    *mask |= META_VIRTUAL_MOD4_MASK;
  if (gdk_mask & GDK_MOD5_MASK)
    *mask |= META_VIRTUAL_MOD5_MASK;
  if (gdk_mask & GDK_SUPER_MASK)
    *mask |= META_VIRTUAL_SUPER_MASK;
  if (gdk_mask & GDK_HYPER_MASK)
    *mask |= META_VIRTUAL_HYPER_MASK;
  if (gdk_mask & GDK_META_MASK)
    *mask |= META_VIRTUAL_META_MASK;
  
  return TRUE;
}

gboolean
meta_ui_window_is_widget (MetaUI *ui,
                          Window  xwindow)
{
  GdkWindow *window;

  window = gdk_xid_table_lookup (xwindow);

  if (window)
    {
      void *user_data = NULL;
      gdk_window_get_user_data (window, &user_data);
      return user_data != NULL && user_data != ui->frames;
    }
  else
    return FALSE;
}

/* stock icon code Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org> */
typedef struct
{
  char *stock_id;
  const guint8 *icon_data;
} MetaStockIcon;

static void
meta_stock_icons_init (void)
{
  GtkIconFactory *factory;
  int i;

  MetaStockIcon items[] =
  {
    { METACITY_STOCK_DELETE,   stock_delete_data   },
    { METACITY_STOCK_MINIMIZE, stock_minimize_data },
    { METACITY_STOCK_MAXIMIZE, stock_maximize_data }
  };

  factory = gtk_icon_factory_new ();
  gtk_icon_factory_add_default (factory);

  for (i = 0; i < (gint) G_N_ELEMENTS (items); i++)
    {
      GtkIconSet *icon_set;
      GdkPixbuf *pixbuf;

      pixbuf = gdk_pixbuf_new_from_inline (-1, items[i].icon_data,
					   FALSE,
					   NULL);

      icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
      gtk_icon_factory_add (factory, items[i].stock_id, icon_set);
      gtk_icon_set_unref (icon_set);
      
      g_object_unref (G_OBJECT (pixbuf));
    }

  g_object_unref (G_OBJECT (factory));
}

int
meta_ui_get_drag_threshold (MetaUI *ui)
{
  GtkSettings *settings;
  int threshold;

  settings = gtk_widget_get_settings (GTK_WIDGET (ui->frames));

  threshold = 8;
  g_object_get (G_OBJECT (settings), "gtk-dnd-drag-threshold", &threshold, NULL);

  return threshold;
}

MetaUIDirection
meta_ui_get_direction (void)
{
  if (gtk_widget_get_default_direction() == GTK_TEXT_DIR_RTL)
    return META_UI_DIRECTION_RTL;

  return META_UI_DIRECTION_LTR;
}

GdkPixbuf *
meta_ui_get_pixbuf_from_pixmap (Pixmap   pmap)
{
  GdkPixmap *gpmap;
  GdkScreen *screen;
  GdkPixbuf *pixbuf;
  GdkColormap *cmap;
  int width, height, depth;

  gpmap = gdk_pixmap_foreign_new (pmap);
  screen = gdk_drawable_get_screen (gpmap);

  gdk_drawable_get_size (GDK_DRAWABLE (gpmap), &width, &height);
  
  depth = gdk_drawable_get_depth (GDK_DRAWABLE (gpmap));
  if (depth <= 24)
    cmap = gdk_screen_get_rgb_colormap (screen);
  else
    cmap = gdk_screen_get_rgba_colormap (screen);
  
  pixbuf = gdk_pixbuf_get_from_drawable (NULL, gpmap, cmap, 0, 0, 0, 0,
                                         width, height);

  g_object_unref (gpmap);

  return pixbuf;
}
