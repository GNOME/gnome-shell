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
static gboolean meta_frames_key_press_event       (GtkWidget           *widget,
                                                   GdkEventKey         *event);
static gboolean meta_frames_key_release_event     (GtkWidget           *widget,
                                                   GdkEventKey         *event);
static gboolean meta_frames_enter_notify_event    (GtkWidget           *widget,
                                                   GdkEventCrossing    *event);
static gboolean meta_frames_leave_notify_event    (GtkWidget           *widget,
                                                   GdkEventCrossing    *event);
static gboolean meta_frames_configure_event       (GtkWidget           *widget,
                                                   GdkEventConfigure   *event);
static gboolean meta_frames_focus_in_event        (GtkWidget           *widget,
                                                   GdkEventFocus       *event);
static gboolean meta_frames_focus_out_event       (GtkWidget           *widget,
                                                   GdkEventFocus       *event);
static gboolean meta_frames_map_event             (GtkWidget           *widget,
                                                   GdkEventAny         *event);
static gboolean meta_frames_unmap_event           (GtkWidget           *widget,
                                                   GdkEventAny         *event);
static gboolean meta_frames_property_notify_event (GtkWidget           *widget,
                                                   GdkEventProperty    *event);
static gboolean meta_frames_client_event          (GtkWidget           *widget,
                                                   GdkEventClient      *event);
static gboolean meta_frames_window_state_event    (GtkWidget           *widget,
                                                   GdkEventWindowState *event);



static void meta_frames_calc_geometry (MetaFrames        *frames,
                                       MetaUIFrame         *frame,
                                       MetaFrameGeometry *fgeom);

static MetaUIFrame* meta_frames_lookup_window (MetaFrames *frames,
                                               Window      xwindow);


static GdkRectangle*    control_rect (MetaFrameControl   control,
                                      MetaFrameGeometry *fgeom);
static MetaFrameControl get_control  (MetaFrames        *frames,
                                      MetaUIFrame       *frame,
                                      int                x,
                                      int                y);
static void clear_tip (MetaFrames *frames);

enum
{
  dummy, /* remove this when you add more signals */
  LAST_SIGNAL
};

static GtkWidgetClass *parent_class = NULL;
static guint signals[LAST_SIGNAL];

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

#define BORDER_PROPERTY(name, blurb, docs)                                        \
  gtk_widget_class_install_style_property (widget_class,                           \
					   g_param_spec_boxed (name,               \
							       blurb,              \
							       docs,               \
							       GTK_TYPE_BORDER,    \
							       G_PARAM_READABLE))

#define INT_PROPERTY(name, default, blurb, docs)                               \
  gtk_widget_class_install_style_property (widget_class,                        \
					   g_param_spec_int (name,              \
                                                             blurb,             \
                                                             docs,              \
							     0,                 \
							     G_MAXINT,          \
                                                             default,           \
							     G_PARAM_READABLE))

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
  widget_class->unmap_event = meta_frames_unmap_event;
  widget_class->destroy_event = meta_frames_destroy_event;
  widget_class->button_press_event = meta_frames_button_press_event;
  widget_class->button_release_event = meta_frames_button_release_event;
  widget_class->motion_notify_event = meta_frames_motion_notify_event;
  widget_class->leave_notify_event = meta_frames_leave_notify_event;
  
  INT_PROPERTY ("left_width", 6, _("Left edge"), _("Left window edge width"));
  INT_PROPERTY ("right_width", 6, _("Right edge"), _("Right window edge width"));
  INT_PROPERTY ("bottom_height", 7, _("Bottom edge"), _("Bottom window edge height"));
  
  BORDER_PROPERTY ("title_border", _("Title border"), _("Border around title area"));
  BORDER_PROPERTY ("text_border", _("Text border"), _("Border around window title text"));

  INT_PROPERTY ("spacer_padding", 3, _("Spacer padding"), _("Padding on either side of spacer"));
  INT_PROPERTY ("spacer_width", 2, _("Spacer width"), _("Width of spacer"));
  INT_PROPERTY ("spacer_height", 11, _("Spacer height"), _("Height of spacer"));

  /* same as right_width left_width by default */
  INT_PROPERTY ("right_inset", 6, _("Right inset"), _("Distance of buttons from right edge of frame"));
  INT_PROPERTY ("left_inset", 6, _("Left inset"), _("Distance of menu button from left edge of frame"));
  
  INT_PROPERTY ("button_width", 17, _("Button width"), _("Width of buttons"));
  INT_PROPERTY ("button_height", 17, _("Button height"), _("Height of buttons"));

  BORDER_PROPERTY ("button_border", _("Button border"), _("Border around buttons"));
  BORDER_PROPERTY ("inner_button_border", _("Inner button border"), _("Border around the icon inside buttons"));
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
meta_frames_init (MetaFrames *frames)
{
  GTK_WINDOW (frames)->type = GTK_WINDOW_POPUP;

  frames->layout = meta_frame_layout_new ();

  frames->frames = g_hash_table_new (unsigned_long_hash, unsigned_long_equal);

  frames->tooltip_timeout = 0;

  frames->expose_delay_count = 0;
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

  g_assert (g_hash_table_size (frames->frames) == 0);
  g_hash_table_destroy (frames->frames);
  
  meta_frame_layout_free (frames->layout);

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
      /* recreate layout */
      char *text;
      
      text = g_strdup (pango_layout_get_text (frame->layout));

      g_object_unref (G_OBJECT (frame->layout));
      
      frame->layout = gtk_widget_create_pango_layout (GTK_WIDGET (frames),
                                                      text);

      g_free (text);
    }
}

static void
meta_frames_style_set  (GtkWidget *widget,
                        GtkStyle  *prev_style)
{
  MetaFrames *frames;
  /* left, right, top, bottom */
  static GtkBorder default_title_border = { 3, 4, 4, 3 };
  static GtkBorder default_text_border = { 2, 2, 2, 2 };
  static GtkBorder default_button_border = { 0, 0, 1, 1 };
  static GtkBorder default_inner_button_border = {
    DEFAULT_INNER_BUTTON_BORDER,
    DEFAULT_INNER_BUTTON_BORDER,
    DEFAULT_INNER_BUTTON_BORDER,
    DEFAULT_INNER_BUTTON_BORDER
  };
  GtkBorder *title_border;
  GtkBorder *text_border;
  GtkBorder *button_border;
  GtkBorder *inner_button_border;
  MetaFrameLayout layout;

  frames = META_FRAMES (widget);
  
  gtk_widget_style_get (widget,
                        "left_width",
                        &layout.left_width,
                        "right_width",
                        &layout.right_width,
                        "bottom_height",
                        &layout.bottom_height,
                        "title_border",
                        &title_border,
                        "text_border",
                        &text_border,
                        "spacer_padding",
                        &layout.spacer_padding,
                        "spacer_width",
                        &layout.spacer_width,
                        "spacer_height",
                        &layout.spacer_height,
                        "right_inset",
                        &layout.right_inset,
                        "left_inset",
                        &layout.left_inset,
                        "button_width",
                        &layout.button_width,
                        "button_height",
                        &layout.button_height,
                        "button_border",
                        &button_border,
                        "inner_button_border",
                        &inner_button_border,
                        NULL);
  
  if (title_border)
    layout.title_border = *title_border;
  else
    layout.title_border = default_title_border;

  g_free (title_border);
  
  if (text_border)
    layout.text_border = *text_border;
  else
    layout.text_border = default_text_border;

  g_free (text_border);

  if (button_border)
    layout.button_border = *button_border;
  else
    layout.button_border = default_button_border;

  g_free (button_border);

  if (inner_button_border)
    layout.inner_button_border = *inner_button_border;
  else
    layout.inner_button_border = default_inner_button_border;

  g_free (inner_button_border);

  *(frames->layout) = layout;

  {
    PangoFontMetrics *metrics;
    PangoFont *font;
    PangoLanguage *lang;

    font = pango_context_load_font (gtk_widget_get_pango_context (widget),
                                    widget->style->font_desc);
    lang = pango_context_get_language (gtk_widget_get_pango_context (widget));
    metrics = pango_font_get_metrics (font, lang);
    
    g_object_unref (G_OBJECT (font));
    
    frames->text_height = 
      PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) + 
		    pango_font_metrics_get_descent (metrics));

    pango_font_metrics_unref (metrics);
  }

  /* Queue a draw/resize on all frames */
  g_hash_table_foreach (frames->frames,
                        queue_recalc_func, frames);
}

static void
meta_frames_calc_geometry (MetaFrames        *frames,
                           MetaUIFrame       *frame,
                           MetaFrameGeometry *fgeom)
{
  int width, height;
  MetaFrameFlags flags;
  
  meta_core_get_client_size (gdk_display, frame->xwindow,
                             &width, &height);
  
  flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);

  meta_frame_layout_calc_geometry (frames->layout,
                                   GTK_WIDGET (frames),
                                   frames->text_height,
                                   flags,
                                   width, height,
                                   fgeom);
}

MetaFrames*
meta_frames_new (void)
{
  return g_object_new (META_TYPE_FRAMES, NULL);
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
  frame->layout = NULL;
  frame->expose_delayed = FALSE;
  
  meta_core_grab_buttons (gdk_display, frame->xwindow);
  
  g_hash_table_insert (frames->frames, &frame->xwindow, frame);
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
      gdk_window_set_user_data (frame->window, NULL);

      if (frames->last_motion_frame == frame)
        frames->last_motion_frame = NULL;
      
      g_hash_table_remove (frames->frames, &frame->xwindow);

      g_object_unref (G_OBJECT (frame->window));

      if (frame->layout)
        g_object_unref (G_OBJECT (frame->layout));
      
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

  frame = meta_frames_lookup_window (frames, xwindow);

  if (frame == NULL)
    meta_bug ("No such frame 0x%lx\n", xwindow);
  
  flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);

  /* We can't get the full geometry, because that depends on
   * the client window size and probably we're being called
   * by the core move/resize code to decide on the client
   * window size
   */
  meta_frame_layout_get_borders (frames->layout,
                                 GTK_WIDGET (frames),
                                 frames->text_height,
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
  
  if (frame->layout == NULL)
    frame->layout = gtk_widget_create_pango_layout (widget,
                                                    title);
  else
    pango_layout_set_text (frame->layout, title, -1);
  
  gdk_window_invalidate_rect (frame->window, NULL, FALSE);
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
      
      meta_frames_calc_geometry (frames, frame, &fgeom);
      
      rect = control_rect (control, &fgeom);

      /* get conversion delta for root-to-frame coords */
      dx = root_x - x;
      dy = root_y - y;
      
      meta_fixed_tip_show (gdk_display,
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

  if (event->button == 1)
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
      event->button == meta_core_get_grab_button (gdk_display))
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

#define THICK_LINE_WIDTH 3
static void
draw_mini_window (MetaFrames  *frames,
                  GdkDrawable *drawable,
                  GdkGC       *fg_gc,
                  GdkGC       *bg_gc,
                  gboolean     thin_title,
                  int x, int y, int width, int height)
{
  GdkGCValues vals;
  
  gdk_draw_rectangle (drawable,
                      bg_gc,
                      TRUE, 
                      x, y, width - 1, height - 1);
  
  gdk_draw_rectangle (drawable,
                      fg_gc,
                      FALSE,
                      x, y, width - 1, height - 1);
  
  vals.line_width = thin_title ? THICK_LINE_WIDTH - 1 : THICK_LINE_WIDTH;
  gdk_gc_set_values (fg_gc,
                     &vals,
                     GDK_GC_LINE_WIDTH);
  
  gdk_draw_line (drawable,
                 fg_gc,
                 x, y + 1, x + width, y + 1);
  
  vals.line_width = 0;
  gdk_gc_set_values (fg_gc,
                     &vals,
                     GDK_GC_LINE_WIDTH);
}

static void
draw_control (MetaFrames  *frames,
              GdkDrawable *drawable,
              GdkGC       *fg_override,
              GdkGC       *bg_override,
              MetaFrameControl control,
              int x, int y, int width, int height)
{
  GtkWidget *widget;
  GdkGCValues vals;
  GdkGC *fg_gc;
  GdkGC *bg_gc;

  widget = GTK_WIDGET (frames);

  fg_gc = fg_override ? fg_override : widget->style->fg_gc[GTK_STATE_NORMAL];
  bg_gc = bg_override ? bg_override : widget->style->bg_gc[GTK_STATE_NORMAL];
  
  switch (control)
    {
    case META_FRAME_CONTROL_DELETE:
      {
        gdk_draw_line (drawable,
                       fg_gc,
                       x, y, x + width - 1, y + height - 1);
        
        gdk_draw_line (drawable,
                       fg_gc,
                       x, y + height - 1, x + width - 1, y);
      }
      break;

    case META_FRAME_CONTROL_MAXIMIZE:
      {
        draw_mini_window (frames, drawable, fg_gc, bg_gc, FALSE,
                          x, y, width, height);
      }
      break;

    case META_FRAME_CONTROL_UNMAXIMIZE:
      {
        int w_delta = width * 0.3;
        int h_delta = height * 0.3;

        w_delta = MAX (w_delta, 3);
        h_delta = MAX (h_delta, 3);
        
        draw_mini_window (frames, drawable, fg_gc, bg_gc, TRUE,
                          x, y, width - w_delta, height - h_delta);
        draw_mini_window (frames, drawable, fg_gc, bg_gc, TRUE,
                          x + w_delta, y + h_delta,
                          width - w_delta, height - h_delta);
      }
      break;
      
    case META_FRAME_CONTROL_MINIMIZE:
      {
              
      vals.line_width = THICK_LINE_WIDTH;
      gdk_gc_set_values (fg_gc,
                         &vals,
                         GDK_GC_LINE_WIDTH);

      gdk_draw_line (drawable,
                     fg_gc,
                     x,         y + height - THICK_LINE_WIDTH + 1,
                     x + width, y + height - THICK_LINE_WIDTH + 1);
      
      vals.line_width = 0;
      gdk_gc_set_values (fg_gc,
                         &vals,
                         GDK_GC_LINE_WIDTH);
      }
      break;
      
    default:
      break;
    }
}
#undef THICK_LINE_WIDTH

void
meta_frames_get_pixmap_for_control (MetaFrames      *frames,
                                    MetaFrameControl control,
                                    GdkPixmap      **pixmapp,
                                    GdkBitmap      **maskp)
{
  int w, h;
  GdkPixmap *pix;
  GdkBitmap *mask;
  GtkWidget *widget;
  GdkGC *mgc, *mgc_bg;
  GdkColor color;
  
  widget = GTK_WIDGET (frames);
  
  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);

  w -= DEFAULT_INNER_BUTTON_BORDER * 2;
  h -= DEFAULT_INNER_BUTTON_BORDER * 2;

  /* avoid crashing on bizarre icon sizes */
  if (w < 1)
    w = 1;
  if (h < 1)
    h = 1;
  
  pix = gdk_pixmap_new (NULL, w, h, gtk_widget_get_visual (widget)->depth);
  mask = gdk_pixmap_new (NULL, w, h, 1);

  mgc = gdk_gc_new (mask);
  mgc_bg = gdk_gc_new (mask);
  
  color.pixel = 0;
  gdk_gc_set_foreground (mgc_bg, &color);
  color.pixel = 1;
  gdk_gc_set_foreground (mgc, &color);

  gdk_draw_rectangle (mask, mgc_bg, TRUE, 0, 0, -1, -1);

  draw_control (frames, mask, mgc, mgc_bg, control, 0, 0, w, h);

  gdk_gc_unref (mgc);
  gdk_gc_unref (mgc_bg);

  draw_control (frames, pix, NULL, NULL, control, 0, 0, w, h);

  *pixmapp = pix;
  *maskp = mask;
}

static void
draw_control_bg (MetaFrames         *frames,
                 MetaUIFrame        *frame,
                 MetaFrameControl    control,
                 MetaFrameGeometry  *fgeom)
{
  GdkRectangle *rect;
  GtkWidget *widget;
  gboolean draw = FALSE;
  Window grab_frame;
  
  widget = GTK_WIDGET (frames);

  grab_frame = meta_core_get_grab_frame (gdk_display);
  
  if (frame->xwindow == grab_frame)
    {
      switch (meta_core_get_grab_op (gdk_display))
        {
        case META_GRAB_OP_CLICKING_MENU:
          draw = control == META_FRAME_CONTROL_MENU;
          break;
        case META_GRAB_OP_CLICKING_DELETE:
          draw = control == META_FRAME_CONTROL_DELETE;
          break;
        case META_GRAB_OP_CLICKING_MAXIMIZE:
          draw = control == META_FRAME_CONTROL_MAXIMIZE;
          break;
        case META_GRAB_OP_CLICKING_UNMAXIMIZE:
          draw = control == META_FRAME_CONTROL_UNMAXIMIZE;
          break;
        case META_GRAB_OP_CLICKING_MINIMIZE:
          draw = control == META_FRAME_CONTROL_MINIMIZE;
          break;
        default:
          break;
        }
    }

  if (draw)
    {        
      rect = control_rect (control, fgeom);
      
      if (rect == NULL)
        return;
      
      gtk_paint_box (widget->style, frame->window,
                     GTK_STATE_ACTIVE,
                     GTK_SHADOW_IN, NULL,
                     widget, "button",
                     rect->x, rect->y, rect->width, rect->height);
    }
}

static gboolean
meta_frames_expose_event            (GtkWidget           *widget,
                                     GdkEventExpose      *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaFrameGeometry fgeom;
  MetaFrameFlags flags;
  int width, height;
  GtkBorder inner;
  
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
  
  meta_frames_calc_geometry (frames, frame, &fgeom);
  flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);
  width = fgeom.width;
  height = fgeom.height;
  
  /* Black line around outside to give definition */
  gdk_draw_rectangle (frame->window,
                      widget->style->black_gc,
                      FALSE,
                      0, 0, width - 1, height - 1);

  /* Light GC on top/left edges */
  gdk_draw_line (frame->window,
                 widget->style->light_gc[GTK_STATE_NORMAL],
                 1, 1,
                 1, height - 2);
  gdk_draw_line (frame->window,
                 widget->style->light_gc[GTK_STATE_NORMAL],
                 1, 1,
                 width - 2, 1);
  /* Dark on bottom/right */
  gdk_draw_line (frame->window,
                 widget->style->dark_gc[GTK_STATE_NORMAL],
                 width - 2, 1,
                 width - 2, height - 2);
  gdk_draw_line (frame->window,
                 widget->style->dark_gc[GTK_STATE_NORMAL],
                 1, height - 2,
                 width - 2, height - 2);

  if (flags & META_FRAME_HAS_FOCUS)
    {
      /* Black line around inside while we have focus */

      gdk_draw_rectangle (frame->window,
                          widget->style->black_gc,
                          FALSE,
                          fgeom.left_width - 1,
                          fgeom.top_height - 1,
                          width - fgeom.right_width - fgeom.left_width + 1,
                          height - fgeom.bottom_height - fgeom.top_height + 1);
    }

  if (event->area.y < fgeom.top_height &&
      fgeom.title_rect.width > 0 && fgeom.title_rect.height > 0)
    {
      GdkRectangle clip;
      GdkGC *layout_gc;
      
      clip = fgeom.title_rect;
      clip.x += frames->layout->text_border.left;
      clip.width -= frames->layout->text_border.left +
        frames->layout->text_border.right;

      layout_gc = widget->style->fg_gc[GTK_STATE_NORMAL];
      if (flags & META_FRAME_HAS_FOCUS)
        {
          GdkPixbuf *gradient;
          GdkColor selected_faded;
          const GdkColor *bg = &widget->style->bg[GTK_STATE_NORMAL];

          /* alpha blend selection color into normal color */
#define ALPHA 25000
          selected_faded = widget->style->bg[GTK_STATE_SELECTED];
          selected_faded.red = selected_faded.red + (((bg->red - selected_faded.red) * ALPHA + 32768) >> 16);
          selected_faded.green = selected_faded.green + (((bg->green - selected_faded.green) * ALPHA + 32768) >> 16);
          selected_faded.blue = selected_faded.blue + (((bg->blue - selected_faded.blue) * ALPHA + 32768) >> 16);
          
          layout_gc = widget->style->fg_gc[GTK_STATE_SELECTED];

          gradient = meta_gradient_create_simple (fgeom.title_rect.width,
                                                  fgeom.title_rect.height,
                                                  &selected_faded,
                                                  &widget->style->bg[GTK_STATE_SELECTED],
                                                  META_GRADIENT_DIAGONAL);
          
          if (gradient != NULL)
            {            
              gdk_pixbuf_render_to_drawable (gradient,
                                             frame->window,
                                             widget->style->bg_gc[GTK_STATE_SELECTED],
                                             0, 0,
                                             fgeom.title_rect.x,
                                             fgeom.title_rect.y,
                                             fgeom.title_rect.width,
                                             fgeom.title_rect.height,
                                             GDK_RGB_DITHER_MAX,
                                             0, 0);
                
              g_object_unref (G_OBJECT (gradient));
            }
          else
            {
              /* Fallback to plain selection color */
              gdk_draw_rectangle (frame->window,
                                  widget->style->bg_gc[GTK_STATE_SELECTED],
                                  TRUE,
                                  fgeom.title_rect.x,
                                  fgeom.title_rect.y,
                                  fgeom.title_rect.width,
                                  fgeom.title_rect.height);
            }
        }

      if (frame->layout)
        {
          PangoRectangle layout_rect;
          int x, y, icon_x, icon_y;
          GdkPixbuf *icon;
          int icon_w, icon_h;
          int area_w, area_h;

#define ICON_TEXT_SPACING 2
          
          icon = meta_core_get_mini_icon (gdk_display,
                                          frame->xwindow);

          icon_w = gdk_pixbuf_get_width (icon);
          icon_h = gdk_pixbuf_get_height (icon);
          
          pango_layout_get_pixel_extents (frame->layout,
                                          NULL,
                                          &layout_rect);

          /* corner of whole title area */
          x = fgeom.title_rect.x + frames->layout->text_border.left;
          y = fgeom.title_rect.y + frames->layout->text_border.top;

          area_w = fgeom.title_rect.width -
            frames->layout->text_border.left -
            frames->layout->text_border.right;

          area_h = fgeom.title_rect.height -
            frames->layout->text_border.top -
            frames->layout->text_border.bottom;
          
          /* center icon vertically */
          icon_y = y + MAX ((area_h - icon_h) / 2, 0);  
          /* center text vertically */
          y = y + MAX ((area_h - layout_rect.height) / 2, 0);

          /* Center icon + text combo */
          icon_x = x + MAX ((area_w - layout_rect.width - icon_w - ICON_TEXT_SPACING) / 2, 0);
          x = icon_x + icon_w + ICON_TEXT_SPACING;
          
          gdk_gc_set_clip_rectangle (layout_gc, &clip);

          {
            /* grumble, render_to_drawable_alpha does not accept a clip
             * mask, so we have to go through some BS
             */
            GdkRectangle pixbuf_rect;
            GdkRectangle draw_rect;
            
            pixbuf_rect.x = icon_x;
            pixbuf_rect.y = icon_y;
            pixbuf_rect.width = icon_w;
            pixbuf_rect.height = icon_h;

            if (gdk_rectangle_intersect (&clip, &pixbuf_rect, &draw_rect))
              {
                gdk_pixbuf_render_to_drawable_alpha (icon,
                                                     frame->window,
                                                     draw_rect.x - pixbuf_rect.x,
                                                     draw_rect.y - pixbuf_rect.y,
                                                     draw_rect.x, draw_rect.y,
                                                     draw_rect.width,
                                                     draw_rect.height,
                                                     GDK_PIXBUF_ALPHA_FULL,
                                                     128,
                                                     GDK_RGB_DITHER_NORMAL,
                                                     0, 0);
              }
          }
          
          gdk_draw_layout (frame->window,
                           layout_gc,
                           x, y,
                           frame->layout);
          gdk_gc_set_clip_rectangle (layout_gc, NULL);
        }
    }

  inner = frames->layout->inner_button_border;
  
  if (fgeom.close_rect.width > 0 && fgeom.close_rect.height > 0)
    {
      draw_control_bg (frames, frame, META_FRAME_CONTROL_DELETE, &fgeom);

      draw_control (frames, frame->window,
                    NULL, NULL,
                    META_FRAME_CONTROL_DELETE,
                    fgeom.close_rect.x + inner.left,
                    fgeom.close_rect.y + inner.top,
                    fgeom.close_rect.width - inner.right - inner.left,
                    fgeom.close_rect.height - inner.bottom - inner.top);
    }

  if (fgeom.max_rect.width > 0 && fgeom.max_rect.height > 0)
    {
      MetaFrameControl ctrl;

      if (flags & META_FRAME_MAXIMIZED)
        ctrl = META_FRAME_CONTROL_UNMAXIMIZE;
      else
        ctrl = META_FRAME_CONTROL_MAXIMIZE;
      
      draw_control_bg (frames, frame, ctrl, &fgeom);
      
      draw_control (frames, frame->window,                    
                    NULL, NULL,
                    ctrl,
                    fgeom.max_rect.x + inner.left,
                    fgeom.max_rect.y + inner.top,
                    fgeom.max_rect.width - inner.left - inner.right,
                    fgeom.max_rect.height - inner.top - inner.bottom);
    }

  if (fgeom.min_rect.width > 0 && fgeom.min_rect.height > 0)
    {
      draw_control_bg (frames, frame, META_FRAME_CONTROL_MINIMIZE, &fgeom);

      draw_control (frames, frame->window,
                    NULL, NULL,
                    META_FRAME_CONTROL_MINIMIZE,
                    fgeom.min_rect.x + inner.left,
                    fgeom.min_rect.y + inner.top,
                    fgeom.min_rect.width - inner.left - inner.right,
                    fgeom.min_rect.height - inner.top - inner.bottom);
    }
  
  if (fgeom.spacer_rect.width > 0 && fgeom.spacer_rect.height > 0)
    {
      gtk_paint_vline (widget->style,
                       frame->window,
                       GTK_STATE_NORMAL,
                       &event->area,
                       widget,
                       "metacity_frame_spacer",
                       fgeom.spacer_rect.y,
                       fgeom.spacer_rect.y + fgeom.spacer_rect.height,
                       fgeom.spacer_rect.x + fgeom.spacer_rect.width / 2);
    }

  if (fgeom.menu_rect.width > 0 && fgeom.menu_rect.height > 0)
    {
      int x, y;
#define ARROW_WIDTH 7
#define ARROW_HEIGHT 5
      
      draw_control_bg (frames, frame, META_FRAME_CONTROL_MENU, &fgeom);
      
      x = fgeom.menu_rect.x;
      y = fgeom.menu_rect.y;
      x += (fgeom.menu_rect.width - ARROW_WIDTH) / 2;
      y += (fgeom.menu_rect.height - ARROW_HEIGHT) / 2;

      gtk_paint_arrow (widget->style,
                       frame->window,
                       GTK_STATE_NORMAL,
                       GTK_SHADOW_OUT,
                       &event->area,
                       widget,
                       "metacity_menu_button",
                       GTK_ARROW_DOWN,
                       TRUE,
                       x, y, ARROW_WIDTH, ARROW_HEIGHT);
    }
  
  return TRUE;
}

static gboolean
meta_frames_key_press_event         (GtkWidget           *widget,
                                     GdkEventKey         *event)
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
meta_frames_key_release_event       (GtkWidget           *widget,
                                     GdkEventKey         *event)
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
meta_frames_enter_notify_event      (GtkWidget           *widget,
                                     GdkEventCrossing    *event)
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
meta_frames_leave_notify_event      (GtkWidget           *widget,
                                     GdkEventCrossing    *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  clear_tip (frames);

  meta_core_set_screen_cursor (gdk_display,
                               frame->xwindow,
                               META_CURSOR_DEFAULT);
  
  return TRUE;
}

static gboolean
meta_frames_configure_event         (GtkWidget           *widget,
                                     GdkEventConfigure   *event)
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
meta_frames_focus_in_event          (GtkWidget           *widget,
                                     GdkEventFocus       *event)
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
meta_frames_focus_out_event         (GtkWidget           *widget,
                                     GdkEventFocus       *event)
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
meta_frames_map_event               (GtkWidget           *widget,
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
meta_frames_unmap_event             (GtkWidget           *widget,
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
meta_frames_property_notify_event   (GtkWidget           *widget,
                                     GdkEventProperty    *event)
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
meta_frames_client_event            (GtkWidget           *widget,
                                     GdkEventClient      *event)
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
meta_frames_window_state_event      (GtkWidget           *widget,
                                     GdkEventWindowState *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

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

#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

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
      if (y < fgeom.top_height && x < RESIZE_EXTENDS)
        {
          if (has_vert && has_horiz)
            return META_FRAME_CONTROL_RESIZE_NW;
          else if (has_vert)
            return META_FRAME_CONTROL_RESIZE_N;
          else
            return META_FRAME_CONTROL_RESIZE_W;

        }
      else if (y < fgeom.top_height && x >= (fgeom.width - RESIZE_EXTENDS))
        {
          if (has_vert && has_horiz)
            return META_FRAME_CONTROL_RESIZE_NE;
          else if (has_vert)
            return META_FRAME_CONTROL_RESIZE_N;
          else
            return META_FRAME_CONTROL_RESIZE_E;

        }
      else if (y >= (fgeom.height - fgeom.bottom_height - RESIZE_EXTENDS) &&
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
      else if (y < fgeom.top_height)
        {
          if (has_vert)
            return META_FRAME_CONTROL_RESIZE_N;
        }
      else if (y >= (fgeom.height - fgeom.bottom_height - RESIZE_EXTENDS))
        {
          if (has_vert)
            return META_FRAME_CONTROL_RESIZE_S;
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
