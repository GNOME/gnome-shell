/* Metacity window frame manager widget */

/* 
 * Copyright (C) 2001 Havoc Pennington
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

#include <config.h>
#include "frames.h"
#include "util.h"
#include "core.h"
#include "menu.h"
#include "fixedtip.h"
#include "theme.h"
#include "prefs.h"

#ifdef HAVE_SHAPE
#include <X11/extensions/shape.h>
#endif

#define DEFAULT_INNER_BUTTON_BORDER 3

static void meta_frames_class_init (MetaFramesClass *klass);
static void meta_frames_init       (MetaFrames      *frames);
static void meta_frames_destroy    (GtkObject       *object);
static void meta_frames_finalize   (GObject         *object);
static void meta_frames_style_set  (GtkWidget       *widget,
                                    GtkStyle        *prev_style);
static void meta_frames_realize    (GtkWidget       *widget);
static void meta_frames_unrealize  (GtkWidget       *widget);

static gboolean meta_frames_button_press_event    (GtkWidget           *widget,
                                                   GdkEventButton      *event);
static gboolean meta_frames_button_release_event  (GtkWidget           *widget,
                                                   GdkEventButton      *event);
static gboolean meta_frames_motion_notify_event   (GtkWidget           *widget,
                                                   GdkEventMotion      *event);
static gboolean meta_frames_destroy_event         (GtkWidget           *widget,
                                                   GdkEventAny         *event);
static gboolean meta_frames_expose_event          (GtkWidget           *widget,
                                                   GdkEventExpose      *event);
static gboolean meta_frames_enter_notify_event    (GtkWidget           *widget,
                                                   GdkEventCrossing    *event);
static gboolean meta_frames_leave_notify_event    (GtkWidget           *widget,
                                                   GdkEventCrossing    *event);

static void meta_frames_paint_to_drawable (MetaFrames   *frames,
                                           MetaUIFrame  *frame,
                                           GdkDrawable  *drawable,
                                           GdkRegion    *region);

static void meta_frames_calc_geometry (MetaFrames        *frames,
                                       MetaUIFrame         *frame,
                                       MetaFrameGeometry *fgeom);

static void meta_frames_ensure_layout (MetaFrames      *frames,
                                       MetaUIFrame     *frame);

static MetaUIFrame* meta_frames_lookup_window (MetaFrames *frames,
                                               Window      xwindow);

static void meta_frames_font_changed (MetaFrames *frames);

static GdkRectangle*    control_rect (MetaFrameControl   control,
                                      MetaFrameGeometry *fgeom);
static MetaFrameControl get_control  (MetaFrames        *frames,
                                      MetaUIFrame       *frame,
                                      int                x,
                                      int                y);
static void clear_tip (MetaFrames *frames);

static GtkWidgetClass *parent_class = NULL;

GtkType
meta_frames_get_type (void)
{
  static GtkType frames_type = 0;

  if (!frames_type)
    {
      static const GtkTypeInfo frames_info =
      {
	"MetaFrames",
	sizeof (MetaFrames),
	sizeof (MetaFramesClass),
	(GtkClassInitFunc) meta_frames_class_init,
	(GtkObjectInitFunc) meta_frames_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      frames_type = gtk_type_unique (GTK_TYPE_WINDOW, &frames_info);
    }

  return frames_type;
}

static void
meta_frames_class_init (MetaFramesClass *class)
{
  GObjectClass   *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = meta_frames_finalize;
  object_class->destroy = meta_frames_destroy;

  widget_class->style_set = meta_frames_style_set;

  widget_class->realize = meta_frames_realize;
  widget_class->unrealize = meta_frames_unrealize;
  
  widget_class->expose_event = meta_frames_expose_event;
  widget_class->destroy_event = meta_frames_destroy_event;  
  widget_class->button_press_event = meta_frames_button_press_event;
  widget_class->button_release_event = meta_frames_button_release_event;
  widget_class->motion_notify_event = meta_frames_motion_notify_event;
  widget_class->enter_notify_event = meta_frames_enter_notify_event;
  widget_class->leave_notify_event = meta_frames_leave_notify_event;
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
#if G_SIZEOF_LONG > 4
  return (guint) (val ^ (val >> 32));
#else
  return val;
#endif
}

static void
font_changed_callback (MetaPreference pref,
		       void          *data)
{
  if (pref == META_PREF_TITLEBAR_FONT)
    {
      meta_frames_font_changed (META_FRAMES (data));
    }
}

static void
meta_frames_init (MetaFrames *frames)
{
  GTK_WINDOW (frames)->type = GTK_WINDOW_POPUP;

  frames->text_heights = g_hash_table_new (g_int_hash, g_int_equal);
  
  frames->frames = g_hash_table_new (unsigned_long_hash, unsigned_long_equal);

  frames->tooltip_timeout = 0;

  frames->expose_delay_count = 0;

  gtk_widget_set_double_buffered (GTK_WIDGET (frames), FALSE);

  meta_prefs_add_listener (font_changed_callback, frames);
}

static void
listify_func (gpointer key, gpointer value, gpointer data)
{
  GSList **listp;

  listp = data;
  *listp = g_slist_prepend (*listp, value);
}

static void
meta_frames_destroy (GtkObject *object)
{
  GSList *winlist;
  GSList *tmp;
  MetaFrames *frames;
  
  frames = META_FRAMES (object);

  clear_tip (frames);
  
  winlist = NULL;
  g_hash_table_foreach (frames->frames,
                        listify_func,
                        &winlist);

  /* Unmanage all frames */
  tmp = winlist;
  while (tmp != NULL)
    {
      MetaUIFrame *frame;

      frame = tmp->data;

      meta_frames_unmanage_window (frames, frame->xwindow);
      
      tmp = tmp->next;
    }
  g_slist_free (winlist);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
meta_frames_finalize (GObject *object)
{
  MetaFrames *frames;
  
  frames = META_FRAMES (object);

  meta_prefs_remove_listener (font_changed_callback, frames);
  
  g_hash_table_destroy (frames->text_heights);
  
  g_assert (g_hash_table_size (frames->frames) == 0);
  g_hash_table_destroy (frames->frames);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
queue_recalc_func (gpointer key, gpointer value, gpointer data)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (data);
  frame = value;

  /* If a resize occurs it will cause a redraw, but the
   * resize may not actually be needed so we always redraw
   * in case of color change.
   */
  gtk_style_set_background (GTK_WIDGET (frames)->style,
                            frame->window, GTK_STATE_NORMAL);
  gdk_window_invalidate_rect (frame->window, NULL, FALSE);
  meta_core_queue_frame_resize (gdk_display,
                                frame->xwindow);
  if (frame->layout)
    {
      /* save title to recreate layout */
      g_free (frame->title);
      
      frame->title = g_strdup (pango_layout_get_text (frame->layout));

      g_object_unref (G_OBJECT (frame->layout));
      frame->layout = NULL;
    }
}

static void
meta_frames_font_changed (MetaFrames *frames)
{
  if (g_hash_table_size (frames->text_heights) > 0)
    {
      g_hash_table_destroy (frames->text_heights);
      frames->text_heights = g_hash_table_new (g_int_hash, g_int_equal);
    }
  
  /* Queue a draw/resize on all frames */
  g_hash_table_foreach (frames->frames,
                        queue_recalc_func, frames);

}

static void
meta_frames_style_set  (GtkWidget *widget,
                        GtkStyle  *prev_style)
{
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  meta_frames_font_changed (frames);

  GTK_WIDGET_CLASS (parent_class)->style_set (widget, prev_style);
}

static void
meta_frames_ensure_layout (MetaFrames  *frames,
                           MetaUIFrame *frame)
{
  GtkWidget *widget;
  MetaFrameFlags flags;
  MetaFrameType type;
  MetaFrameStyle *style;
  
  g_return_if_fail (GTK_WIDGET_REALIZED (frames));

  widget = GTK_WIDGET (frames);
      
  flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);
  type = meta_core_get_frame_type (gdk_display, frame->xwindow);

  style = meta_theme_get_frame_style (meta_theme_get_current (),
                                      type, flags);

  if (style != frame->cache_style)
    {
      if (frame->layout)
        {
          /* save title to recreate layout */
          g_free (frame->title);
          
          frame->title = g_strdup (pango_layout_get_text (frame->layout));

          g_object_unref (G_OBJECT (frame->layout));
          frame->layout = NULL;
        }
    }

  frame->cache_style = style;
  
  if (frame->layout == NULL)
    {
      gpointer key, value;
      PangoFontDescription *font_desc;
      double scale;
      int size;
      
      scale = meta_theme_get_title_scale (meta_theme_get_current (),
                                          type,
                                          flags);
      
      frame->layout = gtk_widget_create_pango_layout (widget, frame->title);
      
      font_desc = meta_gtk_widget_get_font_desc (widget, scale,
						 meta_prefs_get_titlebar_font ());

      size = pango_font_description_get_size (font_desc);
      
      if (g_hash_table_lookup_extended (frames->text_heights,
                                        &size,
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
                                &size,
                                GINT_TO_POINTER (frame->text_height));
        }
      
      pango_layout_set_font_description (frame->layout, 
					 font_desc);
      
      pango_font_description_free (font_desc);

      /* Save some RAM */
      g_free (frame->title);
      frame->title = NULL;
    }
}

static void
meta_frames_calc_geometry (MetaFrames        *frames,
                           MetaUIFrame       *frame,
                           MetaFrameGeometry *fgeom)
{
  int width, height;
  MetaFrameFlags flags;
  MetaFrameType type;
  
  meta_core_get_client_size (gdk_display, frame->xwindow,
                             &width, &height);
  
  flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);
  type = meta_core_get_frame_type (gdk_display, frame->xwindow);

  meta_frames_ensure_layout (frames, frame);
  
  meta_theme_calc_geometry (meta_theme_get_current (),
                            type,
                            frame->text_height,
                            flags,
                            width, height,
                            fgeom);
}

MetaFrames*
meta_frames_new (int screen_number)
{
#ifdef HAVE_GTK_MULTIHEAD
  GdkScreen *screen;

  screen = gdk_display_get_screen (gdk_get_default_display (),
				   screen_number);

  return g_object_new (META_TYPE_FRAMES,
		       "screen", screen,
		       NULL);
#else
  return g_object_new (META_TYPE_FRAMES,
		       NULL);
#endif
  
}

void
meta_frames_manage_window (MetaFrames *frames,
                           Window      xwindow)
{
  MetaUIFrame *frame;

  frame = g_new (MetaUIFrame, 1);
  
  frame->window = gdk_window_foreign_new (xwindow);

  if (frame->window == NULL)
    {
      g_free (frame);
      meta_bug ("Frame 0x%lx doesn't exist\n", xwindow);
      return;
    }
  
  gdk_window_set_user_data (frame->window, frames);

  /* Don't set event mask here, it's in frame.c */
  
  frame->xwindow = xwindow;
  frame->cache_style = NULL;
  frame->layout = NULL;
  frame->text_height = -1;
  frame->title = NULL;
  frame->expose_delayed = FALSE;
  frame->prelit_control = META_FRAME_CONTROL_NONE;
  
  meta_core_grab_buttons (gdk_display, frame->xwindow);
  
  g_hash_table_replace (frames->frames, &frame->xwindow, frame);
}

void
meta_frames_unmanage_window (MetaFrames *frames,
                             Window      xwindow)
{
  MetaUIFrame *frame;

  clear_tip (frames);
  
  frame = g_hash_table_lookup (frames->frames, &xwindow);

  if (frame)
    {
      /* restore the cursor */
      meta_core_set_screen_cursor (gdk_display,
				   frame->xwindow,
				   META_CURSOR_DEFAULT);

      gdk_window_set_user_data (frame->window, NULL);

      if (frames->last_motion_frame == frame)
        frames->last_motion_frame = NULL;
      
      g_hash_table_remove (frames->frames, &frame->xwindow);

      g_object_unref (G_OBJECT (frame->window));

      if (frame->layout)
        g_object_unref (G_OBJECT (frame->layout));

      if (frame->title)
        g_free (frame->title);
      
      g_free (frame);
    }
  else
    meta_warning ("Frame 0x%lx not managed, can't unmanage\n", xwindow);
}

static void
meta_frames_realize (GtkWidget *widget)
{
  MetaFrames *frames;

  frames = META_FRAMES (widget);
  
  if (GTK_WIDGET_CLASS (parent_class)->realize)
    GTK_WIDGET_CLASS (parent_class)->realize (widget);
}

static void
meta_frames_unrealize (GtkWidget *widget)
{
  MetaFrames *frames;

  frames = META_FRAMES (widget);
  
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static MetaUIFrame*
meta_frames_lookup_window (MetaFrames *frames,
                           Window      xwindow)
{
  MetaUIFrame *frame;

  frame = g_hash_table_lookup (frames->frames, &xwindow);

  return frame;
}

void
meta_frames_get_geometry (MetaFrames *frames,
                          Window xwindow,
                          int *top_height, int *bottom_height,
                          int *left_width, int *right_width)
{
  MetaFrameFlags flags;
  MetaUIFrame *frame;
  MetaFrameType type;
  
  frame = meta_frames_lookup_window (frames, xwindow);

  if (frame == NULL)
    meta_bug ("No such frame 0x%lx\n", xwindow);
  
  flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);  
  type = meta_core_get_frame_type (gdk_display, frame->xwindow);

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  meta_frames_ensure_layout (frames, frame);
  
  /* We can't get the full geometry, because that depends on
   * the client window size and probably we're being called
   * by the core move/resize code to decide on the client
   * window size
   */
  meta_theme_get_frame_borders (meta_theme_get_current (),
                                type,
                                frame->text_height,
                                flags,
                                top_height, bottom_height,
                                left_width, right_width);
}

void
meta_frames_reset_bg (MetaFrames *frames,
                      Window  xwindow)
{
  GtkWidget *widget;
  MetaUIFrame *frame;
  
  widget = GTK_WIDGET (frames);

  frame = meta_frames_lookup_window (frames, xwindow);
  
  gtk_style_set_background (widget->style, frame->window, GTK_STATE_NORMAL);
}

static void
set_background_none (Display *xdisplay,
                     Window   xwindow)
{
  XSetWindowAttributes attrs;

  attrs.background_pixmap = None;
  XChangeWindowAttributes (xdisplay, xwindow,
                           CWBackPixmap, &attrs);
}

void
meta_frames_unflicker_bg (MetaFrames *frames,
                          Window      xwindow,
                          int         target_width,
                          int         target_height)
{
  GtkWidget *widget;
  MetaUIFrame *frame;
  
  widget = GTK_WIDGET (frames);

  frame = meta_frames_lookup_window (frames, xwindow);
  g_return_if_fail (frame != NULL);

#if 0
  pixmap = gdk_pixmap_new (frame->window,
                           width, height,
                           -1);

  /* Oops, no way to get the background here */
  
  meta_frames_paint_to_drawable (frames, frame, pixmap);
#endif

  set_background_none (gdk_display, frame->xwindow);
}

void
meta_frames_apply_shapes (MetaFrames *frames,
                          Window      xwindow,
                          int         new_window_width,
                          int         new_window_height)
{
#ifdef HAVE_SHAPE
  /* Apply shapes as if window had new_window_width, new_window_height */
  GtkWidget *widget;
  MetaUIFrame *frame;
  MetaFrameGeometry fgeom;
  XRectangle xrect;
  Region corners_xregion;
  Region window_xregion;
  
  widget = GTK_WIDGET (frames);

  frame = meta_frames_lookup_window (frames, xwindow);
  g_return_if_fail (frame != NULL);

  meta_frames_calc_geometry (frames, frame, &fgeom);

  if (!(fgeom.top_left_corner_rounded ||
        fgeom.top_right_corner_rounded ||
        fgeom.bottom_left_corner_rounded ||
        fgeom.bottom_right_corner_rounded))
    {
      XShapeCombineMask (gdk_display, frame->xwindow,
                         ShapeBounding, 0, 0, None, ShapeSet);
      
      return; /* nothing to do */
    }
  
  corners_xregion = XCreateRegion ();
  
  if (fgeom.top_left_corner_rounded)
    {
      xrect.x = 0;
      xrect.y = 0;
      xrect.width = 5;
      xrect.height = 1;
      
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);

      xrect.y = 1;
      xrect.width = 3;
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
      
      xrect.y = 2;
      xrect.width = 2;
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);

      xrect.y = 3;
      xrect.width = 1;
      xrect.height = 2;
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
    }

  if (fgeom.top_right_corner_rounded)
    {
      xrect.x = new_window_width - 5;
      xrect.y = 0;
      xrect.width = 5;
      xrect.height = 1;
      
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);

      xrect.y = 1;
      xrect.x = new_window_width - 3;
      xrect.width = 3;
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
      
      xrect.y = 2;
      xrect.x = new_window_width - 2;
      xrect.width = 2;
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);

      xrect.y = 3;
      xrect.x = new_window_width - 1;
      xrect.width = 1;
      xrect.height = 2;
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
    }

  if (fgeom.bottom_left_corner_rounded)
    {
      xrect.x = 0;
      xrect.y = new_window_height - 1;
      xrect.width = 5;
      xrect.height = 1;
      
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
      
      xrect.y = new_window_height - 2;
      xrect.width = 3;
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
      
      xrect.y = new_window_height - 3;
      xrect.width = 2;
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);

      xrect.y = new_window_height - 5;
      xrect.width = 1;
      xrect.height = 2;
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
    }

  if (fgeom.bottom_right_corner_rounded)
    {
      xrect.x = new_window_width - 5;
      xrect.y = new_window_height - 1;
      xrect.width = 5;
      xrect.height = 1;
      
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
      
      xrect.y = new_window_height - 2;
      xrect.x = new_window_width - 3;
      xrect.width = 3;
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
      
      xrect.y = new_window_height - 3;
      xrect.x = new_window_width - 2;
      xrect.width = 2;
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);

      xrect.y = new_window_height - 5;
      xrect.x = new_window_width - 1;
      xrect.width = 1;
      xrect.height = 2;
      XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
    }
  
  window_xregion = XCreateRegion ();
  
  xrect.x = 0;
  xrect.y = 0;
  xrect.width = new_window_width;
  xrect.height = new_window_height;

  XUnionRectWithRegion (&xrect, window_xregion, window_xregion);

  XSubtractRegion (window_xregion, corners_xregion, window_xregion);

  XShapeCombineRegion (gdk_display, frame->xwindow,
                       ShapeBounding, 0, 0, window_xregion, ShapeSet);
  
  XDestroyRegion (window_xregion);
  XDestroyRegion (corners_xregion);
#endif
}

void
meta_frames_queue_draw (MetaFrames *frames,
                        Window      xwindow)
{
  GtkWidget *widget;
  MetaUIFrame *frame;
  
  widget = GTK_WIDGET (frames);

  frame = meta_frames_lookup_window (frames, xwindow);

  gdk_window_invalidate_rect (frame->window, NULL, FALSE);
}

void
meta_frames_set_title (MetaFrames *frames,
                       Window      xwindow,
                       const char *title)
{
  GtkWidget *widget;
  MetaUIFrame *frame;
  
  widget = GTK_WIDGET (frames);

  frame = meta_frames_lookup_window (frames, xwindow);

  g_assert (frame);
  
  g_free (frame->title);
  frame->title = g_strdup (title);
  
  if (frame->layout)
    {
      g_object_unref (frame->layout);
      frame->layout = NULL;
    }
  
  gdk_window_invalidate_rect (frame->window, NULL, FALSE);
}

void
meta_frames_repaint_frame (MetaFrames *frames,
                           Window      xwindow)
{
  GtkWidget *widget;
  MetaUIFrame *frame;
  
  widget = GTK_WIDGET (frames);

  frame = meta_frames_lookup_window (frames, xwindow);

  g_assert (frame);

  gdk_window_process_updates (frame->window, TRUE);
}

static void
show_tip_now (MetaFrames *frames)
{
  const char *tiptext;
  MetaUIFrame *frame;
  int x, y, root_x, root_y;
  Window root, child;
  guint mask;
  MetaFrameControl control;
  
  frame = frames->last_motion_frame;
  if (frame == NULL)
    return;

  XQueryPointer (gdk_display,
                 frame->xwindow,
                 &root, &child,
                 &root_x, &root_y,
                 &x, &y,
                 &mask);
  
  control = get_control (frames, frame, x, y);
  
  tiptext = NULL;
  switch (control)
    {
    case META_FRAME_CONTROL_TITLE:
      break;
    case META_FRAME_CONTROL_DELETE:
      tiptext = _("Close Window");
      break;
    case META_FRAME_CONTROL_MENU:
      tiptext = _("Window Menu");
      break;
    case META_FRAME_CONTROL_MINIMIZE:
      tiptext = _("Minimize Window");
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
      tiptext = _("Maximize Window");
      break;
    case META_FRAME_CONTROL_UNMAXIMIZE:
      tiptext = _("Unmaximize Window");
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

  if (tiptext)
    {
      MetaFrameGeometry fgeom;
      GdkRectangle *rect;
      int dx, dy;
      int screen_number;
      
      meta_frames_calc_geometry (frames, frame, &fgeom);
      
      rect = control_rect (control, &fgeom);

      /* get conversion delta for root-to-frame coords */
      dx = root_x - x;
      dy = root_y - y;
#ifdef HAVE_GTK_MULTIHEAD
      screen_number = gdk_screen_get_number (gtk_widget_get_screen (GTK_WIDGET (frames)));
#else
      screen_number = DefaultScreen (gdk_display);
#endif
      meta_fixed_tip_show (gdk_display,
			   screen_number,
                           rect->x + dx,
                           rect->y + rect->height + 2 + dy,
                           tiptext);
    }
}

static gboolean
tip_timeout_func (gpointer data)
{
  MetaFrames *frames;

  frames = data;

  show_tip_now (frames);

  return FALSE;
}

#define TIP_DELAY 450
static void
queue_tip (MetaFrames *frames)
{
  clear_tip (frames);
  
  frames->tooltip_timeout = g_timeout_add (TIP_DELAY,
                                           tip_timeout_func,
                                           frames);  
}

static void
clear_tip (MetaFrames *frames)
{
  if (frames->tooltip_timeout)
    {
      g_source_remove (frames->tooltip_timeout);
      frames->tooltip_timeout = 0;
    }
  meta_fixed_tip_hide ();
}

static void
redraw_control (MetaFrames *frames,
                MetaUIFrame *frame,
                MetaFrameControl control)
{
  MetaFrameGeometry fgeom;
  GdkRectangle *rect;
  
  meta_frames_calc_geometry (frames, frame, &fgeom);

  rect = control_rect (control, &fgeom);

  gdk_window_invalidate_rect (frame->window, rect, FALSE);
}

static gboolean
point_in_control (MetaFrames *frames,
                  MetaUIFrame *frame,
                  MetaFrameControl control,
                  int x, int y)
{
  return control == get_control (frames, frame, x, y);
}

static gboolean
meta_frames_button_press_event (GtkWidget      *widget,
                                GdkEventButton *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaFrameControl control;
  
  frames = META_FRAMES (widget);

  /* Remember that the display may have already done something with this event.
   * If so there's probably a GrabOp in effect.
   */
  
  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  clear_tip (frames);
  
  control = get_control (frames, frame, event->x, event->y);

  if (control == META_FRAME_CONTROL_CLIENT_AREA)
    return FALSE; /* not on the frame, just passed through from client */

  if (event->button == 1 &&
      !(control == META_FRAME_CONTROL_MINIMIZE ||
        control == META_FRAME_CONTROL_DELETE))
    {
      meta_core_user_raise (gdk_display,
                            frame->xwindow);
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing window with frame 0x%lx due to button 1 press\n",
                  frame->xwindow);
      meta_core_user_focus (gdk_display,
                            frame->xwindow,
                            event->time);      
    }
  
  /* We want to shade even if we have a GrabOp, since we'll have a move grab
   * if we double click the titlebar.
   */
  if (control == META_FRAME_CONTROL_TITLE &&
      event->button == 1 &&
      event->type == GDK_2BUTTON_PRESS)
    {
      MetaFrameFlags flags;
      
      flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);

      if (flags & META_FRAME_ALLOWS_SHADE)
        {
          if (flags & META_FRAME_SHADED)
            meta_core_unshade (gdk_display,
                               frame->xwindow);
          else
            meta_core_shade (gdk_display,
                             frame->xwindow);
        }

      return TRUE;
    }

  if (meta_core_get_grab_op (gdk_display) !=
      META_GRAB_OP_NONE)
    return FALSE; /* already up to something */  

  if (event->button == 1 &&
      (control == META_FRAME_CONTROL_MAXIMIZE ||
       control == META_FRAME_CONTROL_UNMAXIMIZE ||
       control == META_FRAME_CONTROL_MINIMIZE ||
       control == META_FRAME_CONTROL_DELETE ||
       control == META_FRAME_CONTROL_MENU))
    {
      MetaGrabOp op = META_GRAB_OP_NONE;

      switch (control)
        {
        case META_FRAME_CONTROL_MINIMIZE:
          op = META_GRAB_OP_CLICKING_MINIMIZE;
          break;
        case META_FRAME_CONTROL_MAXIMIZE:
          op = META_GRAB_OP_CLICKING_MAXIMIZE;
          break;
        case META_FRAME_CONTROL_UNMAXIMIZE:
          op = META_GRAB_OP_CLICKING_UNMAXIMIZE;
          break;
        case META_FRAME_CONTROL_DELETE:
          op = META_GRAB_OP_CLICKING_DELETE;
          break;
        case META_FRAME_CONTROL_MENU:
          op = META_GRAB_OP_CLICKING_MENU;
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      meta_core_begin_grab_op (gdk_display,
                               frame->xwindow,
                               op,
                               TRUE,
                               event->button,
                               0,
                               event->time,
                               event->x_root,
                               event->y_root);      
      
      redraw_control (frames, frame, control);

      if (op == META_GRAB_OP_CLICKING_MENU)
        {
          MetaFrameGeometry fgeom;
          GdkRectangle *rect;
          int dx, dy;
          
          meta_frames_calc_geometry (frames, frame, &fgeom);
          
          rect = control_rect (META_FRAME_CONTROL_MENU, &fgeom);

          /* get delta to convert to root coords */
          dx = event->x_root - event->x;
          dy = event->y_root - event->y;
          
          meta_core_show_window_menu (gdk_display,
                                      frame->xwindow,
                                      rect->x + dx,
                                      rect->y + rect->height + dy,
                                      event->button,
                                      event->time);
        }
    }
  else if (event->button == 1 &&
           (control == META_FRAME_CONTROL_RESIZE_SE ||
            control == META_FRAME_CONTROL_RESIZE_S ||
            control == META_FRAME_CONTROL_RESIZE_SW ||
            control == META_FRAME_CONTROL_RESIZE_NE ||
            control == META_FRAME_CONTROL_RESIZE_N ||
            control == META_FRAME_CONTROL_RESIZE_NW ||
            control == META_FRAME_CONTROL_RESIZE_E ||
            control == META_FRAME_CONTROL_RESIZE_W))
    {
      MetaGrabOp op;
      
      op = META_GRAB_OP_NONE;
      
      switch (control)
        {
        case META_FRAME_CONTROL_RESIZE_SE:
          op = META_GRAB_OP_RESIZING_SE;
          break;
        case META_FRAME_CONTROL_RESIZE_S:
          op = META_GRAB_OP_RESIZING_S;
          break;
        case META_FRAME_CONTROL_RESIZE_SW:
          op = META_GRAB_OP_RESIZING_SW;
          break;
        case META_FRAME_CONTROL_RESIZE_NE:
          op = META_GRAB_OP_RESIZING_NE;
          break;
        case META_FRAME_CONTROL_RESIZE_N:
          op = META_GRAB_OP_RESIZING_N;
          break;
        case META_FRAME_CONTROL_RESIZE_NW:
          op = META_GRAB_OP_RESIZING_NW;
          break;
        case META_FRAME_CONTROL_RESIZE_E:
          op = META_GRAB_OP_RESIZING_E;
          break;
        case META_FRAME_CONTROL_RESIZE_W:
          op = META_GRAB_OP_RESIZING_W;
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      meta_core_begin_grab_op (gdk_display,
                               frame->xwindow,
                               op,
                               TRUE,
                               event->button,
                               0,
                               event->time,
                               event->x_root,
                               event->y_root);
    }
  else if ((control == META_FRAME_CONTROL_TITLE ||
            control == META_FRAME_CONTROL_NONE) &&
           event->button == 1)
    {
      MetaFrameFlags flags;
      
      flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);
      
      if (flags & META_FRAME_ALLOWS_MOVE)
        {          
          meta_core_begin_grab_op (gdk_display,
                                   frame->xwindow,
                                   META_GRAB_OP_MOVING,
                                   TRUE,
                                   event->button,
                                   0,
                                   event->time,
                                   event->x_root,
                                   event->y_root);
        }
    }
  else if (event->button == 3)
    {
      meta_core_show_window_menu (gdk_display,
                                  frame->xwindow,
                                  event->x_root,
                                  event->y_root,
                                  event->button,
                                  event->time);
    }
  
  return TRUE;
}

void
meta_frames_notify_menu_hide (MetaFrames *frames)
{
  if (meta_core_get_grab_op (gdk_display) ==
      META_GRAB_OP_CLICKING_MENU)
    {
      Window grab_frame;

      grab_frame = meta_core_get_grab_frame (gdk_display);

      if (grab_frame != None)
        {
          MetaUIFrame *frame;

          frame = meta_frames_lookup_window (frames, grab_frame);

          if (frame)
            {
              redraw_control (frames, frame,
                              META_FRAME_CONTROL_MENU);
              meta_core_end_grab_op (gdk_display, CurrentTime);
            }
        }
    }
}

static gboolean
meta_frames_button_release_event    (GtkWidget           *widget,
                                     GdkEventButton      *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaGrabOp op;
  
  frames = META_FRAMES (widget);
  
  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  clear_tip (frames);

  op = meta_core_get_grab_op (gdk_display);

  if (op == META_GRAB_OP_NONE)
    return FALSE;

  /* We only handle the releases we handled the presses for (things
   * involving frame controls). Window ops that don't require a
   * frame are handled in the Xlib part of the code, display.c/window.c
   */
  if (frame->xwindow == meta_core_get_grab_frame (gdk_display) &&
      ((int) event->button) == meta_core_get_grab_button (gdk_display))
    {
      gboolean end_grab;

      end_grab = FALSE;

      switch (op)
        {
        case META_GRAB_OP_CLICKING_MINIMIZE:
          if (point_in_control (frames, frame,
                                META_FRAME_CONTROL_MINIMIZE,
                                event->x, event->y))
            meta_core_minimize (gdk_display, frame->xwindow);

          redraw_control (frames, frame,
                          META_FRAME_CONTROL_MINIMIZE);
          end_grab = TRUE;
          break;

        case META_GRAB_OP_CLICKING_MAXIMIZE:
          if (point_in_control (frames, frame,
                                META_FRAME_CONTROL_MAXIMIZE,
                                event->x, event->y))
            meta_core_maximize (gdk_display, frame->xwindow);

          redraw_control (frames, frame,
                          META_FRAME_CONTROL_MAXIMIZE);
          end_grab = TRUE;
          break;

        case META_GRAB_OP_CLICKING_UNMAXIMIZE:
          if (point_in_control (frames, frame,
                                META_FRAME_CONTROL_UNMAXIMIZE,
                                event->x, event->y))
            meta_core_unmaximize (gdk_display, frame->xwindow);

          redraw_control (frames, frame,
                          META_FRAME_CONTROL_MAXIMIZE);
          end_grab = TRUE;
          break;
          
        case META_GRAB_OP_CLICKING_DELETE:
          if (point_in_control (frames, frame,
                                META_FRAME_CONTROL_DELETE,
                                event->x, event->y))
            meta_core_delete (gdk_display, frame->xwindow, event->time);
          redraw_control (frames, frame,
                          META_FRAME_CONTROL_DELETE);
          end_grab = TRUE;
          break;
          
        case META_GRAB_OP_CLICKING_MENU:
          redraw_control (frames, frame,
                          META_FRAME_CONTROL_MENU);
          end_grab = TRUE;
          break;

        default:
          break;
        }

      if (end_grab)
        meta_core_end_grab_op (gdk_display, event->time);
    }
  
  return TRUE;
}

static void
meta_frames_update_prelit_control (MetaFrames      *frames,
				   MetaUIFrame     *frame,
				   MetaFrameControl control)
{
  MetaFrameControl old_control;

  /* Only prelight buttons */
  if (control != META_FRAME_CONTROL_MENU &&
      control != META_FRAME_CONTROL_MINIMIZE &&
      control != META_FRAME_CONTROL_MAXIMIZE &&
      control != META_FRAME_CONTROL_DELETE)
    control = META_FRAME_CONTROL_NONE;

  if (control == frame->prelit_control)
    return;

  /* Save the old control so we can unprelight it */
  old_control = frame->prelit_control;

  frame->prelit_control = control;

  redraw_control (frames, frame, old_control);
  redraw_control (frames, frame, control);
}

static gboolean
meta_frames_motion_notify_event     (GtkWidget           *widget,
                                     GdkEventMotion      *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  
  frames = META_FRAMES (widget);
  
  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  clear_tip (frames);

  frames->last_motion_frame = frame;
  
  switch (meta_core_get_grab_op (gdk_display))
    {
    case META_GRAB_OP_CLICKING_MENU:
    case META_GRAB_OP_CLICKING_DELETE:
    case META_GRAB_OP_CLICKING_MINIMIZE:
    case META_GRAB_OP_CLICKING_MAXIMIZE:
    case META_GRAB_OP_CLICKING_UNMAXIMIZE:
      break;
      
    case META_GRAB_OP_NONE:
      {
        MetaFrameControl control;
        int x, y;
        MetaCursor cursor;
        
        gdk_window_get_pointer (frame->window, &x, &y, NULL);

        control = get_control (frames, frame, x, y);

        /* Update prelit control */
	meta_frames_update_prelit_control (frames, frame, control);
	
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
          case META_FRAME_CONTROL_MINIMIZE:
            break;
          case META_FRAME_CONTROL_MAXIMIZE:
            break;
          case META_FRAME_CONTROL_UNMAXIMIZE:
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
        meta_core_set_screen_cursor (gdk_display,
                                     frame->xwindow,
                                     cursor);
        
        queue_tip (frames);
      }
      break;

    default:
      break;
    }
      
  return TRUE;
}

static gboolean
meta_frames_destroy_event           (GtkWidget           *widget,
                                     GdkEventAny         *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;
  
  return TRUE;
}

static gboolean
meta_frames_expose_event (GtkWidget           *widget,
                          GdkEventExpose      *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);
    
  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  if (frames->expose_delay_count > 0)
    {
      /* Redraw this entire frame later */
      frame->expose_delayed = TRUE;
      return TRUE;
    }

  meta_frames_paint_to_drawable (frames, frame, frame->window, event->region);

  return TRUE;
}

static void
meta_frames_paint_to_drawable (MetaFrames   *frames,
                               MetaUIFrame  *frame,
                               GdkDrawable  *drawable,
                               GdkRegion    *region)
{
  GtkWidget *widget;
  MetaFrameFlags flags;
  MetaFrameType type;
  GdkPixbuf *mini_icon;
  GdkPixbuf *icon;
  int w, h;
  MetaButtonState button_states[META_BUTTON_TYPE_LAST];
  Window grab_frame;
  int i;
  int top, bottom, left, right;
  GdkRegion *edges;
  GdkRegion *tmp_region;
  GdkRectangle area;
  GdkRectangle *areas;
  int n_areas;
  int screen_width, screen_height;
  
  widget = GTK_WIDGET (frames);

  i = 0;
  while (i < META_BUTTON_TYPE_LAST)
    {
      button_states[i] = META_BUTTON_STATE_NORMAL;
      
      ++i;
    }

  /* Set prelight state */
  switch (frame->prelit_control)
    {
    case META_FRAME_CONTROL_MENU:
      button_states[META_BUTTON_TYPE_MENU] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_MINIMIZE:
      button_states[META_BUTTON_TYPE_MINIMIZE] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
      button_states[META_BUTTON_TYPE_MAXIMIZE] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_DELETE:
      button_states[META_BUTTON_TYPE_CLOSE] = META_BUTTON_STATE_PRELIGHT;
      break;
    default:
    }
  
  grab_frame = meta_core_get_grab_frame (gdk_display);

  if (frame->xwindow == grab_frame)
    {
      switch (meta_core_get_grab_op (gdk_display))
        {
        case META_GRAB_OP_CLICKING_MENU:
          button_states[META_BUTTON_TYPE_MENU] =
            META_BUTTON_STATE_PRESSED;
          break;
        case META_GRAB_OP_CLICKING_DELETE:
          button_states[META_BUTTON_TYPE_CLOSE] =
            META_BUTTON_STATE_PRESSED;
          break;
        case META_GRAB_OP_CLICKING_MAXIMIZE:
          button_states[META_BUTTON_TYPE_MAXIMIZE] =
            META_BUTTON_STATE_PRESSED;
          break;
        case META_GRAB_OP_CLICKING_UNMAXIMIZE:
          button_states[META_BUTTON_TYPE_MAXIMIZE] =
            META_BUTTON_STATE_PRESSED;
          break;
	case META_GRAB_OP_CLICKING_MINIMIZE:
          button_states[META_BUTTON_TYPE_MINIMIZE] =
            META_BUTTON_STATE_PRESSED;
          break;
        default:
          break;
        }
    }
  
  flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);
  type = meta_core_get_frame_type (gdk_display, frame->xwindow);
  mini_icon = meta_core_get_mini_icon (gdk_display, frame->xwindow);
  icon = meta_core_get_icon (gdk_display, frame->xwindow);

  meta_core_get_client_size (gdk_display, frame->xwindow,
                             &w, &h);

  meta_frames_ensure_layout (frames, frame);

  meta_theme_get_frame_borders (meta_theme_get_current (),
                                type, frame->text_height, flags, 
                                &top, &bottom, &left, &right);

  /* Repaint each side of the frame */
  
  edges = gdk_region_copy (region);

  /* Punch out the client area */
  area.x = left;
  area.y = top;
  area.width = w;
  area.height = h;
  tmp_region = gdk_region_rectangle (&area);
  gdk_region_subtract (edges, tmp_region);
  gdk_region_destroy (tmp_region);

  /* Chop off stuff outside the screen; this optimization
   * is crucial to handle huge client windows,
   * like "xterm -geometry 1000x1000"
   */
  meta_core_get_frame_extents (gdk_display,
                               frame->xwindow,
                               &area.x, &area.y,
                               &area.width, &area.height);

  meta_core_get_screen_size (gdk_display,
                             frame->xwindow,
                             &screen_width, &screen_height);

  if ((area.x + area.width) > screen_width)
    area.width = screen_width - area.x;
  if (area.width < 0)
    area.width = 0;
  
  if ((area.y + area.height) > screen_height)
    area.height = screen_height - area.y;
  if (area.height < 0)
    area.height = 0;

  area.x = 0; /* make relative to frame rather than screen */
  area.y = 0;
  
  tmp_region = gdk_region_rectangle (&area);
  gdk_region_intersect (edges, tmp_region);
  gdk_region_destroy (tmp_region);

  /* Now draw remaining portion of region */
  gdk_region_get_rectangles (edges, &areas, &n_areas);
  
  i = 0;
  while (i < n_areas)
    {      
      if (GDK_IS_WINDOW (drawable))
        gdk_window_begin_paint_rect (drawable, &areas[i]);
      
      meta_theme_draw_frame (meta_theme_get_current (),
                             widget,
                             drawable,
                             &areas[i],
                             0, 0,
                             type,
                             flags,
                             w, h,
                             frame->layout,
                             frame->text_height,
                             button_states,
                             mini_icon, icon);

      if (GDK_IS_WINDOW (drawable))
        gdk_window_end_paint (drawable);
      
      ++i;
    }

  gdk_region_destroy (edges);
  g_free (areas);
}

static gboolean
meta_frames_enter_notify_event      (GtkWidget           *widget,
                                     GdkEventCrossing    *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaFrameControl control;
  
  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  control = get_control (frames, frame, event->x, event->y);
  meta_frames_update_prelit_control (frames, frame, control);
  
  return TRUE;
}

static gboolean
meta_frames_leave_notify_event      (GtkWidget           *widget,
                                     GdkEventCrossing    *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  meta_frames_update_prelit_control (frames, frame, META_FRAME_CONTROL_NONE);
  
  clear_tip (frames);

  meta_core_set_screen_cursor (gdk_display,
                               frame->xwindow,
                               META_CURSOR_DEFAULT);

  return TRUE;
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
      rect = &fgeom->close_rect;
      break;
    case META_FRAME_CONTROL_MENU:
      rect = &fgeom->menu_rect;
      break;
    case META_FRAME_CONTROL_MINIMIZE:
      rect = &fgeom->min_rect;
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
    case META_FRAME_CONTROL_UNMAXIMIZE:
      rect = &fgeom->max_rect;
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

#define RESIZE_EXTENDS 15
static MetaFrameControl
get_control (MetaFrames *frames,
             MetaUIFrame *frame,
             int x, int y)
{
  MetaFrameGeometry fgeom;
  MetaFrameFlags flags;
  gboolean has_vert, has_horiz;
  GdkRectangle client;
  
  meta_frames_calc_geometry (frames, frame, &fgeom);

  client.x = fgeom.left_width;
  client.y = fgeom.top_height;
  client.width = fgeom.width - fgeom.left_width - fgeom.right_width;
  client.height = fgeom.height - fgeom.top_height - fgeom.bottom_height;

  if (POINT_IN_RECT (x, y, client))
    return META_FRAME_CONTROL_CLIENT_AREA;
  
  if (POINT_IN_RECT (x, y, fgeom.close_rect))
    return META_FRAME_CONTROL_DELETE;

  if (POINT_IN_RECT (x, y, fgeom.min_rect))
    return META_FRAME_CONTROL_MINIMIZE;

  if (POINT_IN_RECT (x, y, fgeom.menu_rect))
    return META_FRAME_CONTROL_MENU;
  
  if (POINT_IN_RECT (x, y, fgeom.title_rect))
    return META_FRAME_CONTROL_TITLE;
  
  flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);

  if (POINT_IN_RECT (x, y, fgeom.max_rect))
    {
      if (flags & META_FRAME_MAXIMIZED)
        return META_FRAME_CONTROL_UNMAXIMIZE;
      else
        return META_FRAME_CONTROL_MAXIMIZE;
    }
  
  has_vert = (flags & META_FRAME_ALLOWS_VERTICAL_RESIZE) != 0;
  has_horiz = (flags & META_FRAME_ALLOWS_HORIZONTAL_RESIZE) != 0;

  if (has_vert || has_horiz)
    {
      int bottom_of_titlebar;

      bottom_of_titlebar = fgeom.title_rect.y + fgeom.title_rect.height;

      if (y < bottom_of_titlebar)
        goto noresize;
      
      /* South resize always has priority over north resize,
       * in case of overlap.
       */

      if (y >= (fgeom.height - fgeom.bottom_height - RESIZE_EXTENDS) &&
          x >= (fgeom.width - fgeom.right_width - RESIZE_EXTENDS))
        {
          if (has_vert && has_horiz)
            return META_FRAME_CONTROL_RESIZE_SE;
          else if (has_vert)
            return META_FRAME_CONTROL_RESIZE_S;
          else
            return META_FRAME_CONTROL_RESIZE_E;
        }
      else if (y >= (fgeom.height - fgeom.bottom_height - RESIZE_EXTENDS) &&
               x <= (fgeom.left_width + RESIZE_EXTENDS))
        {
          if (has_vert && has_horiz)
            return META_FRAME_CONTROL_RESIZE_SW;
          else if (has_vert)
            return META_FRAME_CONTROL_RESIZE_S;
          else
            return META_FRAME_CONTROL_RESIZE_W;
        }
      else if (y < (fgeom.top_height + RESIZE_EXTENDS) &&
               x < RESIZE_EXTENDS)
        {
          if (has_vert && has_horiz)
            return META_FRAME_CONTROL_RESIZE_NW;
          else if (has_vert)
            return META_FRAME_CONTROL_RESIZE_N;
          else
            return META_FRAME_CONTROL_RESIZE_W;
        }
      else if (y < (fgeom.top_height + RESIZE_EXTENDS) &&
               x >= (fgeom.width - RESIZE_EXTENDS))
        {
          if (has_vert && has_horiz)
            return META_FRAME_CONTROL_RESIZE_NE;
          else if (has_vert)
            return META_FRAME_CONTROL_RESIZE_N;
          else
            return META_FRAME_CONTROL_RESIZE_E;
        }
      else if (y >= (fgeom.height - fgeom.bottom_height - RESIZE_EXTENDS))
        {
          if (has_vert)
            return META_FRAME_CONTROL_RESIZE_S;
        }
      else if (y >= bottom_of_titlebar && y < fgeom.top_height)
        {
          if (has_vert)
            return META_FRAME_CONTROL_RESIZE_N;
        }
      else if (x <= fgeom.left_width)
        {
          if (has_horiz)
            return META_FRAME_CONTROL_RESIZE_W;
        }
      else if (x >= (fgeom.width - fgeom.right_width))
        {
          if (has_horiz)
            return META_FRAME_CONTROL_RESIZE_E;
        }
    }

 noresize:
  
  return META_FRAME_CONTROL_NONE;
}

void
meta_frames_push_delay_exposes (MetaFrames *frames)
{
  frames->expose_delay_count += 1;
}

static void
queue_pending_exposes_func (gpointer key, gpointer value, gpointer data)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (data);
  frame = value;

  if (frame->expose_delayed)
    {
      gdk_window_invalidate_rect (frame->window, NULL, FALSE);
      frame->expose_delayed = FALSE;
    }
}

void
meta_frames_pop_delay_exposes  (MetaFrames *frames)
{
  g_return_if_fail (frames->expose_delay_count > 0);
  
  frames->expose_delay_count -= 1;

  if (frames->expose_delay_count == 0)
    {
      g_hash_table_foreach (frames->frames,
                            queue_pending_exposes_func,
                            frames);
    }
}
