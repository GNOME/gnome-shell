/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity window frame manager widget */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2005, 2006 Elijah Newren
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <math.h>
#include <string.h>
#include <meta/boxes.h>
#include "frames.h"
#include <meta/util.h>
#include "core.h"
#include <meta/theme.h>
#include <meta/prefs.h>
#include "ui.h"

#include "core/window-private.h"
#include "core/frame.h"
#include "x11/window-x11.h"
#include "x11/window-x11-private.h"

#include <cairo-xlib.h>

#define DEFAULT_INNER_BUTTON_BORDER 3

static void meta_frames_destroy       (GtkWidget       *object);
static void meta_frames_finalize      (GObject         *object);
static void meta_frames_style_updated (GtkWidget       *widget);

static gboolean meta_frames_draw                  (GtkWidget           *widget,
                                                   cairo_t             *cr);

static void meta_ui_frame_attach_style (MetaUIFrame *frame);

static void meta_ui_frame_paint        (MetaUIFrame  *frame,
                                        cairo_t      *cr);

static void meta_ui_frame_calc_geometry (MetaUIFrame       *frame,
                                         MetaFrameGeometry *fgeom);

static void meta_ui_frame_update_prelit_control (MetaUIFrame     *frame,
                                                 MetaFrameControl control);

static void meta_frames_font_changed          (MetaFrames *frames);
static void meta_frames_button_layout_changed (MetaFrames *frames);


static GdkRectangle*    control_rect (MetaFrameControl   control,
                                      MetaFrameGeometry *fgeom);
static MetaFrameControl get_control  (MetaUIFrame       *frame,
                                      int                x,
                                      int                y);

G_DEFINE_TYPE (MetaFrames, meta_frames, GTK_TYPE_WINDOW);

static GObject *
meta_frames_constructor (GType                  gtype,
                         guint                  n_properties,
                         GObjectConstructParam *properties)
{
  GObject *object;
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (meta_frames_parent_class);
  object = gobject_class->constructor (gtype, n_properties, properties);

  g_object_set (object,
                "type", GTK_WINDOW_POPUP,
                NULL);

  return object;
}

static void
meta_frames_class_init (MetaFramesClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;

  gobject_class->constructor = meta_frames_constructor;
  gobject_class->finalize = meta_frames_finalize;

  widget_class->destroy = meta_frames_destroy;

  widget_class->style_updated = meta_frames_style_updated;

  widget_class->draw = meta_frames_draw;
}

static gint
unsigned_long_equal (gconstpointer v1,
                     gconstpointer v2)
{
  return *((const gulong*) v1) == *((const gulong*) v2);
}

static guint
unsigned_long_hash (gconstpointer v)
{
  gulong val = * (const gulong *) v;

  /* I'm not sure this works so well. */
#if GLIB_SIZEOF_LONG > 4
  return (guint) (val ^ (val >> 32));
#else
  return val;
#endif
}

static void
prefs_changed_callback (MetaPreference pref,
                        void          *data)
{
  switch (pref)
    {
    case META_PREF_TITLEBAR_FONT:
      meta_frames_font_changed (META_FRAMES (data));
      break;
    case META_PREF_BUTTON_LAYOUT:
      meta_frames_button_layout_changed (META_FRAMES (data));
      break;
    default:
      break;
    }
}

static void
invalidate_whole_window (MetaUIFrame *frame)
{
  gdk_window_invalidate_rect (frame->window, NULL, FALSE);
}

static MetaStyleInfo *
meta_frames_get_theme_variant (MetaFrames  *frames,
                               const gchar *variant)
{
  MetaStyleInfo *style_info;

  style_info = g_hash_table_lookup (frames->style_variants, variant);
  if (style_info == NULL)
    {
      style_info = meta_theme_create_style_info (gtk_widget_get_screen (GTK_WIDGET (frames)), variant);
      g_hash_table_insert (frames->style_variants, g_strdup (variant), style_info);
    }

  return style_info;
}

static void
update_style_contexts (MetaFrames *frames)
{
  MetaStyleInfo *style_info;
  GList *variants, *variant;
  GdkScreen *screen;

  screen = gtk_widget_get_screen (GTK_WIDGET (frames));

  if (frames->normal_style)
    meta_style_info_unref (frames->normal_style);
  frames->normal_style = meta_theme_create_style_info (screen, NULL);

  variants = g_hash_table_get_keys (frames->style_variants);
  for (variant = variants; variant; variant = variants->next)
    {
      style_info = meta_theme_create_style_info (screen, (char *)variant->data);
      g_hash_table_insert (frames->style_variants,
                           g_strdup (variant->data), style_info);
    }
  g_list_free (variants);
}

static void
meta_frames_init (MetaFrames *frames)
{
  frames->text_heights = g_hash_table_new (NULL, NULL);

  frames->frames = g_hash_table_new (unsigned_long_hash, unsigned_long_equal);

  frames->style_variants = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, (GDestroyNotify)meta_style_info_unref);

  update_style_contexts (frames);

  meta_prefs_add_listener (prefs_changed_callback, frames);
}

static void
listify_func (gpointer key, gpointer value, gpointer data)
{
  GSList **listp;

  listp = data;
  *listp = g_slist_prepend (*listp, value);
}

static void
meta_frames_destroy (GtkWidget *object)
{
  GSList *winlist;
  GSList *tmp;
  MetaFrames *frames;

  frames = META_FRAMES (object);

  winlist = NULL;
  g_hash_table_foreach (frames->frames, listify_func, &winlist);

  /* Unmanage all frames */
  for (tmp = winlist; tmp != NULL; tmp = tmp->next)
    {
      MetaUIFrame *frame = tmp->data;
      meta_ui_frame_unmanage (frame);
    }
  g_slist_free (winlist);

  if (frames->normal_style)
    {
      meta_style_info_unref (frames->normal_style);
      frames->normal_style = NULL;
    }

  if (frames->style_variants)
    {
      g_hash_table_destroy (frames->style_variants);
      frames->style_variants = NULL;
    }

  GTK_WIDGET_CLASS (meta_frames_parent_class)->destroy (object);
}

static void
meta_frames_finalize (GObject *object)
{
  MetaFrames *frames;

  frames = META_FRAMES (object);

  meta_prefs_remove_listener (prefs_changed_callback, frames);

  g_hash_table_destroy (frames->text_heights);

  g_assert (g_hash_table_size (frames->frames) == 0);
  g_hash_table_destroy (frames->frames);

  G_OBJECT_CLASS (meta_frames_parent_class)->finalize (object);
}

static void
queue_recalc_func (gpointer key, gpointer value, gpointer data)
{
  MetaUIFrame *frame = value;

  invalidate_whole_window (frame);
  meta_core_queue_frame_resize (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                frame->xwindow);

  g_clear_object (&frame->text_layout);
}

static void
meta_frames_font_changed (MetaFrames *frames)
{
  if (g_hash_table_size (frames->text_heights) > 0)
    {
      g_hash_table_destroy (frames->text_heights);
      frames->text_heights = g_hash_table_new (NULL, NULL);
    }

  /* Queue a draw/resize on all frames */
  g_hash_table_foreach (frames->frames,
                        queue_recalc_func, frames);

}

static void
queue_draw_func (gpointer key, gpointer value, gpointer data)
{
  MetaUIFrame *frame = value;
  invalidate_whole_window (frame);
}

static void
meta_frames_button_layout_changed (MetaFrames *frames)
{
  g_hash_table_foreach (frames->frames,
                        queue_draw_func, frames);
}

static void
reattach_style_func (gpointer key, gpointer value, gpointer data)
{
  MetaUIFrame *frame = value;
  meta_ui_frame_attach_style (frame);
}

static void
meta_frames_style_updated  (GtkWidget *widget)
{
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  meta_frames_font_changed (frames);

  update_style_contexts (frames);

  g_hash_table_foreach (frames->frames, reattach_style_func, NULL);

  meta_retheme_all ();

  GTK_WIDGET_CLASS (meta_frames_parent_class)->style_updated (widget);
}

static void
meta_ui_frame_ensure_layout (MetaUIFrame    *frame,
                             MetaFrameType   type)
{
  MetaFrames *frames = frame->frames;
  GtkWidget *widget;
  MetaFrameLayout *layout;

  widget = GTK_WIDGET (frames);

  g_return_if_fail (gtk_widget_get_realized (widget));

  layout = meta_theme_get_frame_layout (meta_theme_get_default (), type);

  if (layout != frame->cache_layout)
    g_clear_object (&frame->text_layout);

  frame->cache_layout = layout;

  if (frame->text_layout == NULL)
    {
      gpointer key, value;
      PangoFontDescription *font_desc;
      int size;

      frame->text_layout = gtk_widget_create_pango_layout (widget, frame->title);

      pango_layout_set_ellipsize (frame->text_layout, PANGO_ELLIPSIZE_END);
      pango_layout_set_auto_dir (frame->text_layout, FALSE);
      pango_layout_set_single_paragraph_mode (frame->text_layout, TRUE);

      font_desc = meta_style_info_create_font_desc (frame->style_info);
      meta_frame_layout_apply_scale (layout, font_desc);

      size = pango_font_description_get_size (font_desc);

      if (g_hash_table_lookup_extended (frames->text_heights,
                                        GINT_TO_POINTER (size),
                                        &key, &value))
        {
          frame->text_height = GPOINTER_TO_INT (value);
        }
      else
        {
          frame->text_height =
            meta_pango_font_desc_get_text_height (font_desc,
                                                  gtk_widget_get_pango_context (widget));

          g_hash_table_replace (frames->text_heights,
                                GINT_TO_POINTER (size),
                                GINT_TO_POINTER (frame->text_height));
        }

      pango_layout_set_font_description (frame->text_layout,
                                         font_desc);

      pango_font_description_free (font_desc);
    }
}

static void
meta_ui_frame_calc_geometry (MetaUIFrame       *frame,
                             MetaFrameGeometry *fgeom)
{
  MetaFrameFlags flags;
  MetaFrameType type;
  MetaButtonLayout button_layout;
  MetaWindowX11 *window_x11 = META_WINDOW_X11 (frame->meta_window);
  MetaWindowX11Private *priv = window_x11->priv;

  flags = meta_frame_get_flags (frame->meta_window->frame);
  type = meta_window_get_frame_type (frame->meta_window);

  meta_ui_frame_ensure_layout (frame, type);

  meta_prefs_get_button_layout (&button_layout);

  meta_theme_calc_geometry (meta_theme_get_default (),
                            frame->style_info,
                            type,
                            frame->text_height,
                            flags,
                            priv->client_rect.width,
                            priv->client_rect.height,
                            &button_layout,
                            fgeom);
}

MetaFrames*
meta_frames_new (int screen_number)
{
  GdkScreen *screen;
  MetaFrames *frames;

  screen = gdk_display_get_screen (gdk_display_get_default (),
                                   screen_number);

  frames = g_object_new (META_TYPE_FRAMES,
                         "screen", screen,
                         "type", GTK_WINDOW_POPUP,
                         NULL);

  /* Put the window at an arbitrary offscreen location; the one place
   * it can't be is at -100x-100, since the meta_window_new() will
   * mistake it for a window created via meta_create_offscreen_window()
   * and ignore it, and we need this window to get frame-synchronization
   * messages so that GTK+'s style change handling works.
   */
  gtk_window_move (GTK_WINDOW (frames), -200, -200);
  gtk_window_resize (GTK_WINDOW (frames), 1, 1);

  return frames;
}

static const char *
get_theme_variant_override (MetaFrames *frames)
{
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (frames));
  GtkSettings *settings = gtk_settings_get_for_screen (screen);
  gboolean dark_theme_requested;

  g_object_get (settings,
                "gtk-application-prefer-dark-theme", &dark_theme_requested,
                NULL);

  if (dark_theme_requested)
    return "dark";

  return NULL;
}

/* In order to use a style with a window it has to be attached to that
 * window. Actually, the colormaps just have to match, but since GTK+
 * already takes care of making sure that its cheap to attach a style
 * to multiple windows with the same colormap, we can just go ahead
 * and attach separately for each window.
 */
static void
meta_ui_frame_attach_style (MetaUIFrame *frame)
{
  MetaFrames *frames = frame->frames;
  const char *variant;
  const char *variant_override;

  if (frame->style_info != NULL)
    meta_style_info_unref (frame->style_info);

  variant_override = get_theme_variant_override (frame->frames);

  if (variant_override)
    variant = variant_override;
  else
    variant = frame->meta_window->gtk_theme_variant;

  if (variant == NULL || strcmp(variant, "normal") == 0)
    frame->style_info = meta_style_info_ref (frames->normal_style);
  else
    frame->style_info = meta_style_info_ref (meta_frames_get_theme_variant (frames,
                                                                            variant));
}

MetaUIFrame *
meta_frames_manage_window (MetaFrames *frames,
                           MetaWindow *meta_window,
                           Window      xwindow,
                           GdkWindow  *window)
{
  MetaUIFrame *frame;

  g_assert (window);

  frame = g_new (MetaUIFrame, 1);

  frame->frames = frames;
  frame->window = window;

  gdk_window_set_user_data (frame->window, frames);

  frame->style_info = NULL;

  /* Don't set event mask here, it's in frame.c */

  frame->xwindow = xwindow;
  frame->meta_window = meta_window;
  frame->cache_layout = NULL;
  frame->text_layout = NULL;
  frame->text_height = -1;
  frame->title = NULL;
  frame->prelit_control = META_FRAME_CONTROL_NONE;
  frame->button_state = META_BUTTON_STATE_NORMAL;

  meta_core_grab_buttons (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);

  g_hash_table_replace (frames->frames, &frame->xwindow, frame);

  return frame;
}

void
meta_ui_frame_unmanage (MetaUIFrame *frame)
{
  MetaFrames *frames = frame->frames;

  /* restore the cursor */
  meta_core_set_screen_cursor (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                               frame->xwindow,
                               META_CURSOR_DEFAULT);

  gdk_window_set_user_data (frame->window, NULL);

  g_hash_table_remove (frames->frames, &frame->xwindow);

  meta_style_info_unref (frame->style_info);

  gdk_window_destroy (frame->window);

  if (frame->text_layout)
    g_object_unref (G_OBJECT (frame->text_layout));

  g_free (frame->title);

  g_free (frame);
}

void
meta_ui_frame_get_borders (MetaUIFrame *frame,
                           MetaFrameBorders *borders)
{
  MetaFrameFlags flags;
  MetaFrameType type;

  flags = meta_frame_get_flags (frame->meta_window->frame);
  type = meta_window_get_frame_type (frame->meta_window);

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  meta_ui_frame_ensure_layout (frame, type);

  /* We can't get the full geometry, because that depends on
   * the client window size and probably we're being called
   * by the core move/resize code to decide on the client
   * window size
   */
  meta_theme_get_frame_borders (meta_theme_get_default (),
                                frame->style_info,
                                type,
                                frame->text_height,
                                flags,
                                borders);
}

/* The client rectangle surrounds client window; it subtracts both
 * the visible and invisible borders from the frame window's size.
 */
static void
get_client_rect (MetaFrameGeometry     *fgeom,
                 cairo_rectangle_int_t *rect)
{
  rect->x = fgeom->borders.total.left;
  rect->y = fgeom->borders.total.top;
  rect->width = fgeom->width - fgeom->borders.total.right - rect->x;
  rect->height = fgeom->height - fgeom->borders.total.bottom - rect->y;
}

/* The visible frame rectangle surrounds the visible portion of the
 * frame window; it subtracts only the invisible borders from the frame
 * window's size.
 */
static void
get_visible_frame_rect (MetaFrameGeometry     *fgeom,
                        cairo_rectangle_int_t *rect)
{
  rect->x = fgeom->borders.invisible.left;
  rect->y = fgeom->borders.invisible.top;
  rect->width = fgeom->width - fgeom->borders.invisible.right - rect->x;
  rect->height = fgeom->height - fgeom->borders.invisible.bottom - rect->y;
}

static cairo_region_t *
get_visible_region (MetaUIFrame       *frame,
                    MetaFrameGeometry *fgeom)
{
  cairo_region_t *corners_region;
  cairo_region_t *visible_region;
  cairo_rectangle_int_t rect;
  cairo_rectangle_int_t frame_rect;

  corners_region = cairo_region_create ();
  get_visible_frame_rect (fgeom, &frame_rect);

  if (fgeom->top_left_corner_rounded_radius != 0)
    {
      const int corner = fgeom->top_left_corner_rounded_radius;
      const float radius = corner;
      int i;

      for (i=0; i<corner; i++)
        {
          const int width = floor(0.5 + radius - sqrt(radius*radius - (radius-(i+0.5))*(radius-(i+0.5))));
          rect.x = frame_rect.x;
          rect.y = frame_rect.y + i;
          rect.width = width;
          rect.height = 1;

          cairo_region_union_rectangle (corners_region, &rect);
        }
    }

  if (fgeom->top_right_corner_rounded_radius != 0)
    {
      const int corner = fgeom->top_right_corner_rounded_radius;
      const float radius = corner;
      int i;

      for (i=0; i<corner; i++)
        {
          const int width = floor(0.5 + radius - sqrt(radius*radius - (radius-(i+0.5))*(radius-(i+0.5))));
          rect.x = frame_rect.x + frame_rect.width - width;
          rect.y = frame_rect.y + i;
          rect.width = width;
          rect.height = 1;

          cairo_region_union_rectangle (corners_region, &rect);
        }
    }

  if (fgeom->bottom_left_corner_rounded_radius != 0)
    {
      const int corner = fgeom->bottom_left_corner_rounded_radius;
      const float radius = corner;
      int i;

      for (i=0; i<corner; i++)
        {
          const int width = floor(0.5 + radius - sqrt(radius*radius - (radius-(i+0.5))*(radius-(i+0.5))));
          rect.x = frame_rect.x;
          rect.y = frame_rect.y + frame_rect.height - i - 1;
          rect.width = width;
          rect.height = 1;

          cairo_region_union_rectangle (corners_region, &rect);
        }
    }

  if (fgeom->bottom_right_corner_rounded_radius != 0)
    {
      const int corner = fgeom->bottom_right_corner_rounded_radius;
      const float radius = corner;
      int i;

      for (i=0; i<corner; i++)
        {
          const int width = floor(0.5 + radius - sqrt(radius*radius - (radius-(i+0.5))*(radius-(i+0.5))));
          rect.x = frame_rect.x + frame_rect.width - width;
          rect.y = frame_rect.y + frame_rect.height - i - 1;
          rect.width = width;
          rect.height = 1;

          cairo_region_union_rectangle (corners_region, &rect);
        }
    }

  visible_region = cairo_region_create_rectangle (&frame_rect);
  cairo_region_subtract (visible_region, corners_region);
  cairo_region_destroy (corners_region);

  return visible_region;
}

cairo_region_t *
meta_ui_frame_get_bounds (MetaUIFrame *frame)
{
  MetaFrameGeometry fgeom;
  meta_ui_frame_calc_geometry (frame, &fgeom);
  return get_visible_region (frame, &fgeom);
}

void
meta_ui_frame_move_resize (MetaUIFrame *frame,
                           int x, int y, int width, int height)
{
  int old_width, old_height;

  old_width = gdk_window_get_width (frame->window);
  old_height = gdk_window_get_height (frame->window);

  gdk_window_move_resize (frame->window, x, y, width, height);

  if (old_width != width || old_height != height)
    invalidate_whole_window (frame);
}

void
meta_ui_frame_queue_draw (MetaUIFrame *frame)
{
  invalidate_whole_window (frame);
}

void
meta_ui_frame_set_title (MetaUIFrame *frame,
                         const char *title)
{
  g_free (frame->title);
  frame->title = g_strdup (title);

  g_clear_object (&frame->text_layout);

  invalidate_whole_window (frame);
}

void
meta_ui_frame_update_style (MetaUIFrame *frame)
{
  meta_ui_frame_attach_style (frame);
  invalidate_whole_window (frame);
}

static void
redraw_control (MetaUIFrame *frame,
                MetaFrameControl control)
{
  MetaFrameGeometry fgeom;
  GdkRectangle *rect;

  meta_ui_frame_calc_geometry (frame, &fgeom);

  rect = control_rect (control, &fgeom);

  gdk_window_invalidate_rect (frame->window, rect, FALSE);
}

static gboolean
meta_frame_titlebar_event (MetaUIFrame *frame,
                           ClutterButtonEvent *event,
                           int action)
{
  MetaFrameFlags flags;
  Display *display;

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  flags = meta_frame_get_flags (frame->meta_window->frame);

  switch (action)
    {
    case G_DESKTOP_TITLEBAR_ACTION_TOGGLE_SHADE:
      {
        if (flags & META_FRAME_ALLOWS_SHADE)
          {
            if (flags & META_FRAME_SHADED)
              meta_window_unshade (frame->meta_window, event->time);
            else
              meta_window_shade (frame->meta_window, event->time);
          }
      }
      break;

    case G_DESKTOP_TITLEBAR_ACTION_TOGGLE_MAXIMIZE:
      {
        if (flags & META_FRAME_ALLOWS_MAXIMIZE)
          {
            meta_core_toggle_maximize (display, frame->xwindow);
          }
      }
      break;

    case G_DESKTOP_TITLEBAR_ACTION_TOGGLE_MAXIMIZE_HORIZONTALLY:
      {
        if (flags & META_FRAME_ALLOWS_MAXIMIZE)
          {
            meta_core_toggle_maximize_horizontally (display, frame->xwindow);
          }
      }
      break;

    case G_DESKTOP_TITLEBAR_ACTION_TOGGLE_MAXIMIZE_VERTICALLY:
      {
        if (flags & META_FRAME_ALLOWS_MAXIMIZE)
          {
            meta_core_toggle_maximize_vertically (display, frame->xwindow);
          }
      }
      break;

    case G_DESKTOP_TITLEBAR_ACTION_MINIMIZE:
      {
        if (flags & META_FRAME_ALLOWS_MINIMIZE)
          meta_window_minimize (frame->meta_window);
      }
      break;

    case G_DESKTOP_TITLEBAR_ACTION_NONE:
      /* Yaay, a sane user that doesn't use that other weird crap! */
      break;

    case G_DESKTOP_TITLEBAR_ACTION_LOWER:
      meta_core_user_lower_and_unfocus (display,
                                        frame->xwindow,
                                        event->time);
      break;

    case G_DESKTOP_TITLEBAR_ACTION_MENU:
      meta_core_show_window_menu (display,
                                  frame->xwindow,
                                  META_WINDOW_MENU_WM,
                                  event->x,
                                  event->y,
                                  event->time);
      break;
    }

  return TRUE;
}

static gboolean
meta_frame_double_click_event (MetaUIFrame  *frame,
                               ClutterButtonEvent *event)
{
  int action = meta_prefs_get_action_double_click_titlebar ();

  return meta_frame_titlebar_event (frame, event, action);
}

static gboolean
meta_frame_middle_click_event (MetaUIFrame *frame,
                               ClutterButtonEvent *event)
{
  int action = meta_prefs_get_action_middle_click_titlebar();

  return meta_frame_titlebar_event (frame, event, action);
}

static gboolean
meta_frame_right_click_event (MetaUIFrame *frame,
                              ClutterButtonEvent *event)
{
  int action = meta_prefs_get_action_right_click_titlebar();

  return meta_frame_titlebar_event (frame, event, action);
}

static gboolean
meta_frames_try_grab_op (MetaUIFrame *frame,
                         MetaGrabOp   op,
                         gdouble      grab_x,
                         gdouble      grab_y,
                         guint32      time)
{
  MetaFrames *frames = frame->frames;
  Display *display;
  gboolean ret;

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  ret = meta_core_begin_grab_op (display,
                                 frame->xwindow,
                                 op,
                                 FALSE,
                                 TRUE,
                                 frame->grab_button,
                                 0,
                                 time,
                                 grab_x, grab_y);
  if (!ret)
    {
      frames->current_grab_op = op;
      frames->grab_frame = frame;
      frames->grab_x = grab_x;
      frames->grab_y = grab_y;
    }

  return ret;
}

static gboolean
meta_frames_retry_grab_op (MetaFrames *frames,
                           guint       time)
{
  Display *display;
  MetaGrabOp op;

  if (frames->current_grab_op == META_GRAB_OP_NONE)
    return TRUE;

  op = frames->current_grab_op;
  frames->current_grab_op = META_GRAB_OP_NONE;
  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  return meta_core_begin_grab_op (display,
                                  frames->grab_frame->xwindow,
                                  op,
                                  FALSE,
                                  TRUE,
                                  frames->grab_frame->grab_button,
                                  0,
                                  time,
                                  frames->grab_x,
                                  frames->grab_y);
}

static MetaGrabOp
grab_op_from_resize_control (MetaFrameControl control)
{
  switch (control)
    {
    case META_FRAME_CONTROL_RESIZE_SE:
      return META_GRAB_OP_RESIZING_SE;
    case META_FRAME_CONTROL_RESIZE_S:
      return META_GRAB_OP_RESIZING_S;
    case META_FRAME_CONTROL_RESIZE_SW:
      return META_GRAB_OP_RESIZING_SW;
    case META_FRAME_CONTROL_RESIZE_NE:
      return META_GRAB_OP_RESIZING_NE;
    case META_FRAME_CONTROL_RESIZE_N:
      return META_GRAB_OP_RESIZING_N;
    case META_FRAME_CONTROL_RESIZE_NW:
      return META_GRAB_OP_RESIZING_NW;
    case META_FRAME_CONTROL_RESIZE_E:
      return META_GRAB_OP_RESIZING_E;
    case META_FRAME_CONTROL_RESIZE_W:
      return META_GRAB_OP_RESIZING_W;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
meta_frame_left_click_event (MetaUIFrame *frame,
                             ClutterButtonEvent *event)
{
  Display *display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  MetaFrameControl control = get_control (frame, event->x, event->y);

  switch (control)
    {
    case META_FRAME_CONTROL_MAXIMIZE:
    case META_FRAME_CONTROL_UNMAXIMIZE:
    case META_FRAME_CONTROL_MINIMIZE:
    case META_FRAME_CONTROL_DELETE:
    case META_FRAME_CONTROL_SHADE:
    case META_FRAME_CONTROL_UNSHADE:
    case META_FRAME_CONTROL_ABOVE:
    case META_FRAME_CONTROL_UNABOVE:
    case META_FRAME_CONTROL_STICK:
    case META_FRAME_CONTROL_UNSTICK:
    case META_FRAME_CONTROL_MENU:
    case META_FRAME_CONTROL_APPMENU:
      frame->grab_button = event->button;
      frame->button_state = META_BUTTON_STATE_PRESSED;
      frame->prelit_control = control;
      redraw_control (frame, control);

      if (control == META_FRAME_CONTROL_MENU ||
          control == META_FRAME_CONTROL_APPMENU)
        {
          MetaFrameGeometry fgeom;
          GdkRectangle *rect;
          MetaRectangle root_rect;
          MetaWindowMenuType menu;
          int win_x, win_y;

          meta_ui_frame_calc_geometry (frame, &fgeom);

          rect = control_rect (control, &fgeom);

          gdk_window_get_position (frame->window, &win_x, &win_y);

          root_rect.x = win_x + rect->x;
          root_rect.y = win_y + rect->y;
          root_rect.width = rect->width;
          root_rect.height = rect->height;

          menu = control == META_FRAME_CONTROL_MENU ? META_WINDOW_MENU_WM
            : META_WINDOW_MENU_APP;

          /* if the compositor takes a grab for showing the menu, we will
           * get a LeaveNotify event we want to ignore, to keep the pressed
           * button state while the menu is open
           */
          frame->maybe_ignore_leave_notify = TRUE;
          meta_core_show_window_menu_for_rect (display,
                                               frame->xwindow,
                                               menu,
                                               &root_rect,
                                               event->time);
        }
      else
        {
          meta_frames_try_grab_op (frame, META_GRAB_OP_FRAME_BUTTON,
                                   event->x, event->y,
                                   event->time);
        }

      return TRUE;
    case META_FRAME_CONTROL_RESIZE_SE:
    case META_FRAME_CONTROL_RESIZE_S:
    case META_FRAME_CONTROL_RESIZE_SW:
    case META_FRAME_CONTROL_RESIZE_NE:
    case META_FRAME_CONTROL_RESIZE_N:
    case META_FRAME_CONTROL_RESIZE_NW:
    case META_FRAME_CONTROL_RESIZE_E:
    case META_FRAME_CONTROL_RESIZE_W:
      meta_frames_try_grab_op (frame,
                               grab_op_from_resize_control (control),
                               event->x, event->y,
                               event->time);

      return TRUE;
    case META_FRAME_CONTROL_TITLE:
      {
        MetaFrameFlags flags = meta_frame_get_flags (frame->meta_window->frame);

        if (flags & META_FRAME_ALLOWS_MOVE)
          {
            meta_frames_try_grab_op (frame,
                                     META_GRAB_OP_MOVING,
                                     event->x, event->y,
                                     event->time);
          }
      }

      return TRUE;
    case META_FRAME_CONTROL_NONE:
      /* We can get this for example when trying to resize window
       * that cannot be resized (e. g. it is maximized and the theme
       * currently used has borders for maximized windows), see #751884 */
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
handle_button_press_event (MetaUIFrame *frame,
                           ClutterButtonEvent *event)
{
  MetaFrameControl control;
  Display *display;

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  control = get_control (frame, event->x, event->y);

  /* focus on click, even if click was on client area */
  if (event->button == 1 &&
      !(control == META_FRAME_CONTROL_MINIMIZE ||
        control == META_FRAME_CONTROL_DELETE ||
        control == META_FRAME_CONTROL_MAXIMIZE))
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing window with frame 0x%lx due to button 1 press\n",
                  frame->xwindow);
      meta_window_focus (frame->meta_window, event->time);
    }

  /* don't do the rest of this if on client area */
  if (control == META_FRAME_CONTROL_CLIENT_AREA)
    return FALSE; /* not on the frame, just passed through from client */

  /* We want to shade even if we have a GrabOp, since we'll have a move grab
   * if we double click the titlebar.
   */
  if (control == META_FRAME_CONTROL_TITLE &&
      event->button == 1 &&
      event->click_count == 2)
    {
      meta_core_end_grab_op (display, event->time);
      return meta_frame_double_click_event (frame, event);
    }

  if (meta_core_get_grab_op (display) != META_GRAB_OP_NONE)
    return FALSE; /* already up to something */

  frame->grab_button = event->button;

  switch (event->button)
    {
    case 1:
      return meta_frame_left_click_event (frame, event);
    case 2:
      return meta_frame_middle_click_event (frame, event);
    case 3:
      return meta_frame_right_click_event (frame, event);
    default:
      return FALSE;
    }
}

static gboolean
handle_button_release_event (MetaUIFrame *frame,
                             ClutterButtonEvent *event)
{
  Display *display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  meta_core_end_grab_op (display, event->time);

  /* We only handle the releases we handled the presses for (things
   * involving frame controls). Window ops that don't require a
   * frame are handled in the Xlib part of the code, display.c/window.c
   */
  if (((int) event->button) == frame->grab_button &&
      frame->button_state == META_BUTTON_STATE_PRESSED)
    {
      switch (frame->prelit_control)
        {
        case META_FRAME_CONTROL_MINIMIZE:
          meta_window_minimize (frame->meta_window);
          break;
        case META_FRAME_CONTROL_MAXIMIZE:
          /* Focus the window on the maximize */
          meta_window_focus (frame->meta_window, event->time);
          if (meta_prefs_get_raise_on_click ())
            meta_window_raise (frame->meta_window);
          meta_window_maximize (frame->meta_window, META_MAXIMIZE_BOTH);
          break;
        case META_FRAME_CONTROL_UNMAXIMIZE:
          if (meta_prefs_get_raise_on_click ())
            meta_window_raise (frame->meta_window);
          meta_window_unmaximize (frame->meta_window, META_MAXIMIZE_BOTH);
          break;
        case META_FRAME_CONTROL_DELETE:
          meta_window_delete (frame->meta_window, event->time);
          break;
        case META_FRAME_CONTROL_SHADE:
          meta_window_shade (frame->meta_window, event->time);
          break;
        case META_FRAME_CONTROL_UNSHADE:
          meta_window_unshade (frame->meta_window, event->time);
          break;
        case META_FRAME_CONTROL_ABOVE:
          meta_window_make_above (frame->meta_window);
          break;
        case META_FRAME_CONTROL_UNABOVE:
          meta_window_unmake_above (frame->meta_window);
          break;
        case META_FRAME_CONTROL_STICK:
          meta_window_stick (frame->meta_window);
          break;
        case META_FRAME_CONTROL_UNSTICK:
          meta_window_unstick (frame->meta_window);
          break;
        default:
          break;
        }

      /* Update the prelit control regardless of what button the mouse
       * was released over; needed so that the new button can become
       * prelit so to let the user know that it can now be pressed.
       * :)
       */
      MetaFrameControl control = get_control (frame, event->x, event->y);
      meta_ui_frame_update_prelit_control (frame, control);
    }

  return TRUE;
}

static void
meta_ui_frame_update_prelit_control (MetaUIFrame     *frame,
                                     MetaFrameControl control)
{
  MetaFrameControl old_control;
  MetaCursor cursor;

  meta_verbose ("Updating prelit control from %u to %u\n",
                frame->prelit_control, control);

  cursor = META_CURSOR_DEFAULT;

  switch (control)
    {
    case META_FRAME_CONTROL_CLIENT_AREA:
      break;
    case META_FRAME_CONTROL_NONE:
      break;
    case META_FRAME_CONTROL_TITLE:
      break;
    case META_FRAME_CONTROL_DELETE:
      break;
    case META_FRAME_CONTROL_MENU:
      break;
    case META_FRAME_CONTROL_APPMENU:
      break;
    case META_FRAME_CONTROL_MINIMIZE:
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
      break;
    case META_FRAME_CONTROL_UNMAXIMIZE:
      break;
    case META_FRAME_CONTROL_SHADE:
      break;
    case META_FRAME_CONTROL_UNSHADE:
      break;
    case META_FRAME_CONTROL_ABOVE:
      break;
    case META_FRAME_CONTROL_UNABOVE:
      break;
    case META_FRAME_CONTROL_STICK:
      break;
    case META_FRAME_CONTROL_UNSTICK:
      break;
    case META_FRAME_CONTROL_RESIZE_SE:
      cursor = META_CURSOR_SE_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_S:
      cursor = META_CURSOR_SOUTH_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_SW:
      cursor = META_CURSOR_SW_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_N:
      cursor = META_CURSOR_NORTH_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_NE:
      cursor = META_CURSOR_NE_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_NW:
      cursor = META_CURSOR_NW_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_W:
      cursor = META_CURSOR_WEST_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_E:
      cursor = META_CURSOR_EAST_RESIZE;
      break;
    }

  /* set/unset the prelight cursor */
  meta_core_set_screen_cursor (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                               frame->xwindow,
                               cursor);

  switch (control)
    {
    case META_FRAME_CONTROL_MENU:
    case META_FRAME_CONTROL_APPMENU:
    case META_FRAME_CONTROL_MINIMIZE:
    case META_FRAME_CONTROL_MAXIMIZE:
    case META_FRAME_CONTROL_DELETE:
    case META_FRAME_CONTROL_SHADE:
    case META_FRAME_CONTROL_UNSHADE:
    case META_FRAME_CONTROL_ABOVE:
    case META_FRAME_CONTROL_UNABOVE:
    case META_FRAME_CONTROL_STICK:
    case META_FRAME_CONTROL_UNSTICK:
    case META_FRAME_CONTROL_UNMAXIMIZE:
      /* leave control set */
      break;
    default:
      /* Only prelight buttons */
      control = META_FRAME_CONTROL_NONE;
      break;
    }

  if (control == frame->prelit_control &&
      frame->button_state == META_BUTTON_STATE_PRELIGHT)
    return;

  /* Save the old control so we can unprelight it */
  old_control = frame->prelit_control;

  frame->button_state = META_BUTTON_STATE_PRELIGHT;
  frame->prelit_control = control;

  redraw_control (frame, old_control);
  redraw_control (frame, control);
}

static gboolean
handle_motion_notify_event (MetaUIFrame *frame,
                            ClutterMotionEvent *event)
{
  MetaFrames *frames = frame->frames;
  MetaFrameControl control;

  control = get_control (frame, event->x, event->y);

  if (frame->button_state == META_BUTTON_STATE_PRESSED)
    {
      /* If the user leaves the frame button, set the state
       * back to normal and redraw. */
      if (frame->prelit_control != control)
        {
          frame->button_state = META_BUTTON_STATE_NORMAL;
          redraw_control (frame, frame->prelit_control);
        }
    }
  else
    {
      /* Update prelit control and cursor */
      meta_ui_frame_update_prelit_control (frame, control);
    }

  if ((event->modifier_state & CLUTTER_BUTTON1_MASK) &&
      frames->current_grab_op != META_GRAB_OP_NONE)
    meta_frames_retry_grab_op (frames, event->time);

  return TRUE;
}

static cairo_region_t *
get_visible_frame_border_region (MetaUIFrame *frame)
{
  cairo_rectangle_int_t area;
  cairo_region_t *frame_border;
  MetaFrameFlags flags;
  MetaFrameType type;
  MetaFrameBorders borders;
  MetaRectangle frame_rect = frame->meta_window->rect;

  flags = meta_frame_get_flags (frame->meta_window->frame);
  type = meta_window_get_frame_type (frame->meta_window);

  meta_theme_get_frame_borders (meta_theme_get_default (), frame->style_info,
                                type, frame->text_height, flags,
                                &borders);

  /* Visible frame rect */
  area.x = borders.invisible.left;
  area.y = borders.invisible.top;
  area.width = frame_rect.width;
  area.height = frame_rect.height;

  frame_border = cairo_region_create_rectangle (&area);

  /* Client rect */
  area.x += borders.visible.left;
  area.y += borders.visible.top;
  area.width -= borders.visible.left + borders.visible.right;
  area.height -= borders.visible.top + borders.visible.bottom;

  /* Visible frame border */
  cairo_region_subtract_rectangle (frame_border, &area);
  return frame_border;
}

/*
 * Draw the opaque and semi-opaque pixels of this frame into a mask.
 *
 * (0,0) in Cairo coordinates is assumed to be the top left corner of the
 * invisible border.
 *
 * The parts of @cr's surface in the clip region are assumed to be
 * initialized to fully-transparent, and the clip region is assumed to
 * contain the invisible border and the visible parts of the frame, but
 * not the client area.
 *
 * This function uses @cr to draw pixels of arbitrary color (it will
 * typically be drawing in a %CAIRO_FORMAT_A8 surface, so the color is
 * discarded anyway) with appropriate alpha values to reproduce this
 * frame's alpha channel, as a mask to be applied to an opaque pixmap.
 *
 * @frame: This frame
 * @xwindow: The X window for the frame, which has the client window as a child
 * @cr: Used to draw the resulting mask
 */
void
meta_ui_frame_get_mask (MetaUIFrame *frame,
                        cairo_t     *cr)
{
  MetaFrameBorders borders;
  MetaFrameFlags flags;
  MetaRectangle frame_rect;
  int scale = meta_theme_get_window_scaling_factor ();

  meta_window_get_frame_rect (frame->meta_window, &frame_rect);

  flags = meta_frame_get_flags (frame->meta_window->frame);

  meta_style_info_set_flags (frame->style_info, flags);
  meta_ui_frame_get_borders (frame, &borders);

  /* See comment in meta_frame_layout_draw_with_style() for details on HiDPI handling */
  cairo_scale (cr, scale, scale);
  gtk_render_background (frame->style_info->styles[META_STYLE_ELEMENT_FRAME], cr,
                         borders.invisible.left / scale,
                         borders.invisible.top / scale,
                         frame_rect.width / scale, frame_rect.height / scale);
}

/* XXX -- this is disgusting. Find a better approach here.
 * Use multiple widgets? */
static MetaUIFrame *
find_frame_to_draw (MetaFrames *frames,
                    cairo_t    *cr)
{
  GHashTableIter iter;
  MetaUIFrame *frame;

  g_hash_table_iter_init (&iter, frames->frames);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &frame))
    if (gtk_cairo_should_draw_window (cr, frame->window))
      return frame;

  return NULL;
}

static gboolean
meta_frames_draw (GtkWidget *widget,
                  cairo_t   *cr)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  cairo_region_t *region;

  frames = META_FRAMES (widget);

  frame = find_frame_to_draw (frames, cr);
  if (frame == NULL)
    return FALSE;

  region = get_visible_frame_border_region (frame);
  gdk_cairo_region (cr, region);
  cairo_clip (cr);

  meta_ui_frame_paint (frame, cr);
  cairo_region_destroy (region);

  return TRUE;
}

static void
meta_ui_frame_paint (MetaUIFrame  *frame,
                     cairo_t      *cr)
{
  MetaFrameFlags flags;
  MetaFrameType type;
  cairo_surface_t *mini_icon;
  MetaButtonState button_states[META_BUTTON_TYPE_LAST];
  int i;
  int button_type = -1;
  MetaButtonLayout button_layout;
  MetaWindowX11 *window_x11 = META_WINDOW_X11 (frame->meta_window);
  MetaWindowX11Private *priv = window_x11->priv;

  for (i = 0; i < META_BUTTON_TYPE_LAST; i++)
    button_states[i] = META_BUTTON_STATE_NORMAL;

  /* Set prelight state */
  switch (frame->prelit_control)
    {
    case META_FRAME_CONTROL_MENU:
      button_type = META_BUTTON_TYPE_MENU;
      break;
    case META_FRAME_CONTROL_APPMENU:
      button_type = META_BUTTON_TYPE_APPMENU;
      break;
    case META_FRAME_CONTROL_MINIMIZE:
      button_type = META_BUTTON_TYPE_MINIMIZE;
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
      button_type = META_BUTTON_TYPE_MAXIMIZE;
      break;
    case META_FRAME_CONTROL_UNMAXIMIZE:
      button_type = META_BUTTON_TYPE_MAXIMIZE;
      break;
    case META_FRAME_CONTROL_SHADE:
      button_type = META_BUTTON_TYPE_SHADE;
      break;
    case META_FRAME_CONTROL_UNSHADE:
      button_type = META_BUTTON_TYPE_UNSHADE;
      break;
    case META_FRAME_CONTROL_ABOVE:
      button_type = META_BUTTON_TYPE_ABOVE;
      break;
    case META_FRAME_CONTROL_UNABOVE:
      button_type = META_BUTTON_TYPE_UNABOVE;
      break;
    case META_FRAME_CONTROL_STICK:
      button_type = META_BUTTON_TYPE_STICK;
      break;
    case META_FRAME_CONTROL_UNSTICK:
      button_type = META_BUTTON_TYPE_UNSTICK;
      break;
    case META_FRAME_CONTROL_DELETE:
      button_type = META_BUTTON_TYPE_CLOSE;
      break;
    default:
      break;
    }

  if (button_type > -1)
    button_states[button_type] = frame->button_state;

  mini_icon = frame->meta_window->mini_icon;
  flags = meta_frame_get_flags (frame->meta_window->frame);
  type = meta_window_get_frame_type (frame->meta_window);

  meta_ui_frame_ensure_layout (frame, type);

  meta_prefs_get_button_layout (&button_layout);

  meta_theme_draw_frame (meta_theme_get_default (),
                         frame->style_info,
                         cr,
                         type,
                         flags,
                         priv->client_rect.width,
                         priv->client_rect.height,
                         frame->text_layout,
                         frame->text_height,
                         &button_layout,
                         button_states,
                         mini_icon);
}

static gboolean
handle_enter_notify_event (MetaUIFrame *frame,
                           ClutterCrossingEvent *event)
{
  MetaFrameControl control;

  frame->maybe_ignore_leave_notify = FALSE;

  control = get_control (frame, event->x, event->y);
  meta_ui_frame_update_prelit_control (frame, control);

  return TRUE;
}

static gboolean
handle_leave_notify_event (MetaUIFrame *frame,
                           ClutterCrossingEvent *event)
{
  Display *display;
  MetaGrabOp grab_op;

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  grab_op = meta_core_get_grab_op (display);

  /* ignore the first LeaveNotify event after opening a window menu
   * if it is the result of a compositor grab
   */
  frame->maybe_ignore_leave_notify = frame->maybe_ignore_leave_notify &&
                                     grab_op == META_GRAB_OP_COMPOSITOR;

  if (frame->maybe_ignore_leave_notify)
    return FALSE;

  meta_ui_frame_update_prelit_control (frame, META_FRAME_CONTROL_NONE);

  return TRUE;
}

gboolean
meta_ui_frame_handle_event (MetaUIFrame *frame,
                            const ClutterEvent *event)
{
  switch (event->any.type)
    {
    case CLUTTER_BUTTON_PRESS:
      return handle_button_press_event (frame, (ClutterButtonEvent *) event);
    case CLUTTER_BUTTON_RELEASE:
      return handle_button_release_event (frame, (ClutterButtonEvent *) event);
    case CLUTTER_MOTION:
      return handle_motion_notify_event (frame, (ClutterMotionEvent *) event);
    case CLUTTER_ENTER:
      return handle_enter_notify_event (frame, (ClutterCrossingEvent *) event);
    case CLUTTER_LEAVE:
      return handle_leave_notify_event (frame, (ClutterCrossingEvent *) event);
    default:
      return FALSE;
    }
}

static GdkRectangle*
control_rect (MetaFrameControl control,
              MetaFrameGeometry *fgeom)
{
  GdkRectangle *rect;

  rect = NULL;
  switch (control)
    {
    case META_FRAME_CONTROL_TITLE:
      rect = &fgeom->title_rect;
      break;
    case META_FRAME_CONTROL_DELETE:
      rect = &fgeom->close_rect.visible;
      break;
    case META_FRAME_CONTROL_MENU:
      rect = &fgeom->menu_rect.visible;
      break;
    case META_FRAME_CONTROL_APPMENU:
      rect = &fgeom->appmenu_rect.visible;
      break;
    case META_FRAME_CONTROL_MINIMIZE:
      rect = &fgeom->min_rect.visible;
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
    case META_FRAME_CONTROL_UNMAXIMIZE:
      rect = &fgeom->max_rect.visible;
      break;
    case META_FRAME_CONTROL_SHADE:
      rect = &fgeom->shade_rect.visible;
      break;
    case META_FRAME_CONTROL_UNSHADE:
      rect = &fgeom->unshade_rect.visible;
      break;
    case META_FRAME_CONTROL_ABOVE:
      rect = &fgeom->above_rect.visible;
      break;
    case META_FRAME_CONTROL_UNABOVE:
      rect = &fgeom->unabove_rect.visible;
      break;
    case META_FRAME_CONTROL_STICK:
      rect = &fgeom->stick_rect.visible;
      break;
    case META_FRAME_CONTROL_UNSTICK:
      rect = &fgeom->unstick_rect.visible;
      break;
    case META_FRAME_CONTROL_RESIZE_SE:
      break;
    case META_FRAME_CONTROL_RESIZE_S:
      break;
    case META_FRAME_CONTROL_RESIZE_SW:
      break;
    case META_FRAME_CONTROL_RESIZE_N:
      break;
    case META_FRAME_CONTROL_RESIZE_NE:
      break;
    case META_FRAME_CONTROL_RESIZE_NW:
      break;
    case META_FRAME_CONTROL_RESIZE_W:
      break;
    case META_FRAME_CONTROL_RESIZE_E:
      break;
    case META_FRAME_CONTROL_NONE:
      break;
    case META_FRAME_CONTROL_CLIENT_AREA:
      break;
    }

  return rect;
}

#define TOP_RESIZE_HEIGHT 4
#define CORNER_SIZE_MULT 2
static MetaFrameControl
get_control (MetaUIFrame *frame, int root_x, int root_y)
{
  MetaFrameGeometry fgeom;
  MetaFrameFlags flags;
  MetaFrameType type;
  gboolean has_vert, has_horiz;
  gboolean has_north_resize;
  cairo_rectangle_int_t client;
  int x, y;
  int win_x, win_y;

  gdk_window_get_position (frame->window, &win_x, &win_y);
  x = root_x - win_x;
  y = root_y - win_y;

  meta_ui_frame_calc_geometry (frame, &fgeom);
  get_client_rect (&fgeom, &client);

  if (POINT_IN_RECT (x, y, client))
    return META_FRAME_CONTROL_CLIENT_AREA;

  if (POINT_IN_RECT (x, y, fgeom.close_rect.clickable))
    return META_FRAME_CONTROL_DELETE;

  if (POINT_IN_RECT (x, y, fgeom.min_rect.clickable))
    return META_FRAME_CONTROL_MINIMIZE;

  if (POINT_IN_RECT (x, y, fgeom.menu_rect.clickable))
    return META_FRAME_CONTROL_MENU;

  if (POINT_IN_RECT (x, y, fgeom.appmenu_rect.clickable))
    return META_FRAME_CONTROL_APPMENU;

  flags = meta_frame_get_flags (frame->meta_window->frame);
  type = meta_window_get_frame_type (frame->meta_window);

  has_north_resize = (type != META_FRAME_TYPE_ATTACHED);
  has_vert = (flags & META_FRAME_ALLOWS_VERTICAL_RESIZE) != 0;
  has_horiz = (flags & META_FRAME_ALLOWS_HORIZONTAL_RESIZE) != 0;

  if (POINT_IN_RECT (x, y, fgeom.title_rect))
    {
      if (has_vert && y <= TOP_RESIZE_HEIGHT && has_north_resize)
        return META_FRAME_CONTROL_RESIZE_N;
      else
        return META_FRAME_CONTROL_TITLE;
    }

  if (POINT_IN_RECT (x, y, fgeom.max_rect.clickable))
    {
      if (flags & META_FRAME_MAXIMIZED)
        return META_FRAME_CONTROL_UNMAXIMIZE;
      else
        return META_FRAME_CONTROL_MAXIMIZE;
    }

  if (POINT_IN_RECT (x, y, fgeom.shade_rect.clickable))
    {
      return META_FRAME_CONTROL_SHADE;
    }

  if (POINT_IN_RECT (x, y, fgeom.unshade_rect.clickable))
    {
      return META_FRAME_CONTROL_UNSHADE;
    }

  if (POINT_IN_RECT (x, y, fgeom.above_rect.clickable))
    {
      return META_FRAME_CONTROL_ABOVE;
    }

  if (POINT_IN_RECT (x, y, fgeom.unabove_rect.clickable))
    {
      return META_FRAME_CONTROL_UNABOVE;
    }

  if (POINT_IN_RECT (x, y, fgeom.stick_rect.clickable))
    {
      return META_FRAME_CONTROL_STICK;
    }

  if (POINT_IN_RECT (x, y, fgeom.unstick_rect.clickable))
    {
      return META_FRAME_CONTROL_UNSTICK;
    }

  /* South resize always has priority over north resize,
   * in case of overlap.
   */

  if (y >= (fgeom.height - fgeom.borders.total.bottom * CORNER_SIZE_MULT) &&
      x >= (fgeom.width - fgeom.borders.total.right * CORNER_SIZE_MULT))
    {
      if (has_vert && has_horiz)
        return META_FRAME_CONTROL_RESIZE_SE;
      else if (has_vert)
        return META_FRAME_CONTROL_RESIZE_S;
      else if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_E;
    }
  else if (y >= (fgeom.height - fgeom.borders.total.bottom * CORNER_SIZE_MULT) &&
           x <= fgeom.borders.total.left * CORNER_SIZE_MULT)
    {
      if (has_vert && has_horiz)
        return META_FRAME_CONTROL_RESIZE_SW;
      else if (has_vert)
        return META_FRAME_CONTROL_RESIZE_S;
      else if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_W;
    }
  else if (y < (fgeom.borders.invisible.top * CORNER_SIZE_MULT) &&
           x <= (fgeom.borders.total.left * CORNER_SIZE_MULT) && has_north_resize)
    {
      if (has_vert && has_horiz)
        return META_FRAME_CONTROL_RESIZE_NW;
      else if (has_vert)
        return META_FRAME_CONTROL_RESIZE_N;
      else if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_W;
    }
  else if (y < (fgeom.borders.invisible.top * CORNER_SIZE_MULT) &&
           x >= (fgeom.width - fgeom.borders.total.right * CORNER_SIZE_MULT) && has_north_resize)
    {
      if (has_vert && has_horiz)
        return META_FRAME_CONTROL_RESIZE_NE;
      else if (has_vert)
        return META_FRAME_CONTROL_RESIZE_N;
      else if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_E;
    }
  else if (y < (fgeom.borders.invisible.top + TOP_RESIZE_HEIGHT))
    {
      if (has_vert && has_north_resize)
        return META_FRAME_CONTROL_RESIZE_N;
    }
  else if (y >= (fgeom.height - fgeom.borders.total.bottom))
    {
      if (has_vert)
        return META_FRAME_CONTROL_RESIZE_S;
    }
  else if (x <= fgeom.borders.total.left)
    {
      if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_W;
    }
  else if (x >= (fgeom.width - fgeom.borders.total.right))
    {
      if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_E;
    }

  if (y >= fgeom.borders.total.top)
    return META_FRAME_CONTROL_NONE;
  else
    return META_FRAME_CONTROL_TITLE;
}
