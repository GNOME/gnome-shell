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

#include "frames.h"
#include "util.h"
#include "core.h"

#if 0
struct _MetaFrameActionGrab
{
  MetaFrameAction action;
  /* initial mouse position for drags */
  int start_root_x, start_root_y;
  /* initial window size or initial window position for drags */
  int start_window_x, start_window_y;
  /* button doing the dragging */
  int start_button;
};
#endif

struct _MetaUIFrame
{
  Window xwindow;
  GdkWindow *window;
  PangoLayout *layout;
};

struct _MetaFrameProperties
{
  /* Size of left/right/bottom sides */
  int left_width;
  int right_width;
  int bottom_height;
  
  /* Border of blue title region */
  GtkBorder title_border;

  /* Border inside title region, around title */
  GtkBorder text_border;  
 
  /* padding on either side of spacer */
  int spacer_padding;

  /* Size of spacer */
  int spacer_width;
  int spacer_height;

  /* indent of buttons from edges of frame */
  int right_inset;
  int left_inset;
  
  /* Size of buttons */
  int button_width;
  int button_height;

  /* Space around buttons */
  GtkBorder button_border;

  /* Space inside button which is clickable but doesn't draw the
   * button icon
   */
  GtkBorder inner_button_border;
};

typedef struct _MetaFrameGeometry MetaFrameGeometry;

struct _MetaFrameGeometry
{
  int left_width;
  int right_width;
  int top_height;
  int bottom_height;

  GdkRectangle close_rect;
  GdkRectangle max_rect;
  GdkRectangle min_rect;
  GdkRectangle spacer_rect;
  GdkRectangle menu_rect;
  GdkRectangle title_rect;  
};

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
    }

  return rect;
}

static void meta_frames_class_init (MetaFramesClass *klass);
static void meta_frames_init       (MetaFrames      *frames);
static void meta_frames_destroy    (GtkObject       *object);
static void meta_frames_finalize   (GObject         *object);
static void meta_frames_style_set  (GtkWidget       *widget,
                                    GtkStyle        *prev_style);

gboolean meta_frames_button_press_event      (GtkWidget           *widget,
                                              GdkEventButton      *event);
gboolean meta_frames_button_release_event    (GtkWidget           *widget,
                                              GdkEventButton      *event);
gboolean meta_frames_motion_notify_event     (GtkWidget           *widget,
                                              GdkEventMotion      *event);
gboolean meta_frames_destroy_event           (GtkWidget           *widget,
                                              GdkEventAny         *event);
gboolean meta_frames_expose_event            (GtkWidget           *widget,
                                              GdkEventExpose      *event);
gboolean meta_frames_key_press_event         (GtkWidget           *widget,
                                              GdkEventKey         *event);
gboolean meta_frames_key_release_event       (GtkWidget           *widget,
                                              GdkEventKey         *event);
gboolean meta_frames_enter_notify_event      (GtkWidget           *widget,
                                              GdkEventCrossing    *event);
gboolean meta_frames_leave_notify_event      (GtkWidget           *widget,
                                              GdkEventCrossing    *event);
gboolean meta_frames_configure_event         (GtkWidget           *widget,
                                              GdkEventConfigure   *event);
gboolean meta_frames_focus_in_event          (GtkWidget           *widget,
                                              GdkEventFocus       *event);
gboolean meta_frames_focus_out_event         (GtkWidget           *widget,
                                              GdkEventFocus       *event);
gboolean meta_frames_map_event               (GtkWidget           *widget,
                                              GdkEventAny         *event);
gboolean meta_frames_unmap_event             (GtkWidget           *widget,
                                              GdkEventAny         *event);
gboolean meta_frames_property_notify_event   (GtkWidget           *widget,
                                              GdkEventProperty    *event);
gboolean meta_frames_client_event            (GtkWidget           *widget,
                                              GdkEventClient      *event);
gboolean meta_frames_window_state_event      (GtkWidget           *widget,
                                              GdkEventWindowState *event);


static void meta_frames_calc_geometry (MetaFrames        *frames,
                                       MetaUIFrame         *frame,
                                       MetaFrameGeometry *fgeom);

static MetaUIFrame* meta_frames_lookup_window (MetaFrames *frames,
                                               Window      xwindow);

enum
{
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

  widget_class->expose_event = meta_frames_expose_event;
  
  INT_PROPERTY ("left_width", 6, _("Left edge"), _("Left window edge width"));
  INT_PROPERTY ("right_width", 6, _("Right edge"), _("Right window edge width"));
  INT_PROPERTY ("bottom_height", 7, _("Bottom edge"), _("Bottom window edge height"));
  
  BORDER_PROPERTY ("title_border", _("Title border"), _("Border around title area"));
  BORDER_PROPERTY ("text_border", _("Text border"), _("Border around window title text"));

  INT_PROPERTY ("spacer_padding", 3, _("Spacer padding"), _("Padding on either side of spacer"));
  INT_PROPERTY ("spacer_width", 2, _("Spacer width"), _("Width of spacer"));
  INT_PROPERTY ("spacer_height", 10, _("Spacer height"), _("Height of spacer"));

  /* same as right_width left_width by default */
  INT_PROPERTY ("right_inset", 6, _("Right inset"), _("Distance of buttons from right edge of frame"));
  INT_PROPERTY ("left_inset", 6, _("Left inset"), _("Distance of menu button from left edge of frame"));
  
  INT_PROPERTY ("button_width", 14, _("Button width"), _("Width of buttons"));
  INT_PROPERTY ("button_height", 14, _("Button height"), _("Height of buttons"));

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

  frames->props = g_new0 (MetaFrameProperties, 1);

  frames->frames = g_hash_table_new (unsigned_long_hash, unsigned_long_equal);
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
  
  g_free (frames->props);

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
}

static void
meta_frames_style_set  (GtkWidget *widget,
                        GtkStyle  *prev_style)
{
  MetaFrames *frames;
  /* left, right, top, bottom */
  static GtkBorder default_title_border = { 3, 4, 4, 3 };
  static GtkBorder default_text_border = { 2, 2, 2, 2 };
  static GtkBorder default_button_border = { 1, 1, 1, 1 };
  static GtkBorder default_inner_button_border = { 3, 3, 3, 3 };
  GtkBorder *title_border;
  GtkBorder *text_border;
  GtkBorder *button_border;
  GtkBorder *inner_button_border;
  MetaFrameProperties props;

  frames = META_FRAMES (widget);
  
  gtk_widget_style_get (widget,
                        "left_width",
                        &props.left_width,
                        "right_width",
                        &props.right_width,
                        "bottom_height",
                        &props.bottom_height,
                        "title_border",
                        &title_border,
                        "text_border",
                        &text_border,
                        "spacer_padding",
                        &props.spacer_padding,
                        "spacer_width",
                        &props.spacer_width,
                        "spacer_height",
                        &props.spacer_height,
                        "right_inset",
                        &props.right_inset,
                        "left_inset",
                        &props.left_inset,
                        "button_width",
                        &props.button_width,
                        "button_height",
                        &props.button_height,
                        "button_border",
                        &button_border,
                        "inner_button_border",
                        &inner_button_border,
                        NULL);
  
  if (title_border)
    props.title_border = *title_border;
  else
    props.title_border = default_title_border;

  g_free (title_border);
  
  if (text_border)
    props.text_border = *text_border;
  else
    props.text_border = default_text_border;

  g_free (text_border);

  if (button_border)
    props.button_border = *button_border;
  else
    props.button_border = default_button_border;

  g_free (button_border);

  if (inner_button_border)
    props.inner_button_border = *inner_button_border;
  else
    props.inner_button_border = default_inner_button_border;

  g_free (inner_button_border);

  *(frames->props) = props;

  {
    PangoFontMetrics metrics;
    PangoFont *font;
    PangoLanguage *lang;

    font = pango_context_load_font (gtk_widget_get_pango_context (widget),
                                    widget->style->font_desc);
    lang = pango_context_get_language (gtk_widget_get_pango_context (widget));
    pango_font_get_metrics (font, lang, &metrics);
    
    g_object_unref (G_OBJECT (font));
    
    frames->text_height = PANGO_PIXELS (metrics.ascent + metrics.descent);
  }

  /* Queue a draw/resize on all frames */
  g_hash_table_foreach (frames->frames,
                        queue_recalc_func, frames);
}

static void
meta_frames_calc_geometry (MetaFrames        *frames,
                           MetaUIFrame         *frame,
                           MetaFrameGeometry *fgeom)
{
  int x;
  int button_y;
  int title_right_edge;
  MetaFrameProperties props;
  int buttons_height, title_height, spacer_height;
  int width, height;
  MetaFrameFlags flags;
  
  props = *(frames->props);

  meta_core_get_frame_size (gdk_display, frame->xwindow,
                            &width, &height);

  flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);
  
  buttons_height = props.button_height +
    props.button_border.top + props.button_border.bottom;
  title_height = frames->text_height +
    props.text_border.top + props.text_border.bottom +
    props.title_border.top + props.title_border.bottom;
  spacer_height = props.spacer_height;
  
  fgeom->top_height = MAX (buttons_height, title_height);
  fgeom->top_height = MAX (fgeom->top_height, spacer_height);

  fgeom->left_width = props.left_width;
  fgeom->right_width = props.right_width;

  if (flags & META_FRAME_SHADED)
    fgeom->bottom_height = 0;
  else
    fgeom->bottom_height = props.bottom_height;

  x = width - props.right_inset;

  /* center buttons */
  button_y = (fgeom->top_height -
              (props.button_height + props.button_border.top + props.button_border.bottom)) / 2 + props.button_border.top;

  if ((flags & META_FRAME_ALLOWS_DELETE) &&
      x >= 0)
    {
      fgeom->close_rect.x = x - props.button_border.right - props.button_width;
      fgeom->close_rect.y = button_y;
      fgeom->close_rect.width = props.button_width;
      fgeom->close_rect.height = props.button_height;

      x = fgeom->close_rect.x - props.button_border.left;
    }
  else
    {
      fgeom->close_rect.x = 0;
      fgeom->close_rect.y = 0;
      fgeom->close_rect.width = 0;
      fgeom->close_rect.height = 0;
    }

  if ((flags & META_FRAME_ALLOWS_MAXIMIZE) &&
      x >= 0)
    {
      fgeom->max_rect.x = x - props.button_border.right - props.button_width;
      fgeom->max_rect.y = button_y;
      fgeom->max_rect.width = props.button_width;
      fgeom->max_rect.height = props.button_height;

      x = fgeom->max_rect.x - props.button_border.left;
    }
  else
    {
      fgeom->max_rect.x = 0;
      fgeom->max_rect.y = 0;
      fgeom->max_rect.width = 0;
      fgeom->max_rect.height = 0;
    }
  
  if ((flags & META_FRAME_ALLOWS_MINIMIZE) &&
      x >= 0)
    {
      fgeom->min_rect.x = x - props.button_border.right - props.button_width;
      fgeom->min_rect.y = button_y;
      fgeom->min_rect.width = props.button_width;
      fgeom->min_rect.height = props.button_height;

      x = fgeom->min_rect.x - props.button_border.left;
    }
  else
    {
      fgeom->min_rect.x = 0;
      fgeom->min_rect.y = 0;
      fgeom->min_rect.width = 0;
      fgeom->min_rect.height = 0;
    }

  if ((fgeom->close_rect.width > 0 ||
       fgeom->max_rect.width > 0 ||
       fgeom->min_rect.width > 0) &&
      x >= 0)
    {
      fgeom->spacer_rect.x = x - props.spacer_padding - props.spacer_width;
      fgeom->spacer_rect.y = (fgeom->top_height - props.spacer_height) / 2;
      fgeom->spacer_rect.width = props.spacer_width;
      fgeom->spacer_rect.height = props.spacer_height;

      x = fgeom->spacer_rect.x - props.spacer_padding;
    }
  else
    {
      fgeom->spacer_rect.x = 0;
      fgeom->spacer_rect.y = 0;
      fgeom->spacer_rect.width = 0;
      fgeom->spacer_rect.height = 0;
    }

  title_right_edge = x - props.title_border.right;

  /* Now x changes to be position from the left */
  x = props.left_inset;
  
  if ((flags & META_FRAME_ALLOWS_MENU) &&
      x < title_right_edge)
    {
      fgeom->menu_rect.x = x + props.button_border.left;
      fgeom->menu_rect.y = button_y;
      fgeom->menu_rect.width = props.button_width;
      fgeom->menu_rect.height = props.button_height;

      x = fgeom->menu_rect.x + fgeom->menu_rect.width + props.button_border.right;
    }
  else
    {
      fgeom->menu_rect.x = 0;
      fgeom->menu_rect.y = 0;
      fgeom->menu_rect.width = 0;
      fgeom->menu_rect.height = 0;
    }

  /* If menu overlaps close button, then the menu wins since it
   * lets you perform any operation including close
   */
  if (fgeom->close_rect.width > 0 &&
      fgeom->close_rect.x < (fgeom->menu_rect.x + fgeom->menu_rect.height))
    {
      fgeom->close_rect.width = 0;
      fgeom->close_rect.height = 0;
    }
  
  /* We always fill as much vertical space as possible with title rect,
   * rather than centering it like the buttons and spacer
   */
  fgeom->title_rect.x = x + props.title_border.left;
  fgeom->title_rect.y = props.title_border.top;
  fgeom->title_rect.width = title_right_edge - fgeom->title_rect.x;
  fgeom->title_rect.height = fgeom->top_height - props.title_border.top - props.title_border.bottom;

  /* Nuke title if it won't fit */
  if (fgeom->title_rect.width < 0 ||
      fgeom->title_rect.height < 0)
    {
      fgeom->title_rect.width = 0;
      fgeom->title_rect.height = 0;
    }
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

#if 0
  /* Add events in frame.c */
  gdk_window_set_events (frame->window,
                         GDK_EXPOSURE_MASK |
                         GDK_POINTER_MOTION_MASK |
                         GDK_POINTER_MOTION_HINT_MASK |
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_STRUCTURE_MASK);
#endif
  
  /* This shouldn't be required if we don't select for button
   * press in frame.c?
   */
  XGrabButton (gdk_display, AnyButton, AnyModifier,
               xwindow, False,
               ButtonPressMask | ButtonReleaseMask |    
               PointerMotionMask | PointerMotionHintMask,
               GrabModeAsync, GrabModeAsync,
               False, None);

  XFlush (gdk_display);
  
  frame->xwindow = xwindow;
  frame->layout = NULL;
  
  g_hash_table_insert (frames->frames, &frame->xwindow, frame);
}

void
meta_frames_unmanage_window (MetaFrames *frames,
                             Window      xwindow)
{
  MetaUIFrame *frame;

  frame = g_hash_table_lookup (frames->frames, &xwindow);

  if (frame)
    {
      g_hash_table_remove (frames->frames, &frame->xwindow);
      
      g_object_unref (G_OBJECT (frame->window));

      if (frame->layout)
        g_object_unref (G_OBJECT (frame->layout));
      
      g_free (frame);
    }
  else
    meta_warning ("Frame 0x%lx not managed, can't unmanage\n", xwindow);
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
  MetaFrameGeometry fgeom;

  MetaUIFrame *frame;

  frame = meta_frames_lookup_window (frames, xwindow);

  if (frame == NULL)
    meta_bug ("No such frame 0x%lx\n", xwindow);

  meta_frames_calc_geometry (frames, frame, &fgeom);

  if (top_height)
    *top_height = fgeom.top_height;
  if (bottom_height)
    *bottom_height = fgeom.bottom_height;
  if (left_width)
    *left_width = fgeom.left_width;
  if (right_width)
    *right_width = fgeom.right_width;
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

gboolean
meta_frames_button_press_event (GtkWidget      *widget,
                                GdkEventButton *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  return TRUE;
}

gboolean
meta_frames_button_release_event    (GtkWidget           *widget,
                                     GdkEventButton      *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  return TRUE;
}

gboolean
meta_frames_motion_notify_event     (GtkWidget           *widget,
                                     GdkEventMotion      *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  return TRUE;
}

gboolean
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

static void
draw_current_control_bg (MetaFrames        *frames,
                         MetaUIFrame         *frame,
                         MetaFrameGeometry *fgeom)
{
  GdkRectangle *rect;
#if 0
  rect = control_rect (frames->current_control, fgeom);

  if (rect == NULL)
    return;

  if (frames->current_control == META_FRAME_CONTROL_TITLE)
    return;
  
 switch (frames->current_control_state)
    {
      /* FIXME turn this off after testing */
    case META_STATE_PRELIGHT:
      XFillRectangle (info->display,
                      info->drawable,
                      screen_data->prelight_gc,
                      xoff + rect->x,
                      yoff + rect->y,
                      rect->width, rect->height);
      break;

    case META_STATE_ACTIVE:
      XFillRectangle (info->display,
                      info->drawable,
                      screen_data->active_gc,
                      xoff + rect->x,
                      yoff + rect->y,
                      rect->width, rect->height);
      break;

    default:
      break;
    }
#endif
}

gboolean
meta_frames_expose_event            (GtkWidget           *widget,
                                     GdkEventExpose      *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaFrameGeometry fgeom;
  MetaFrameFlags flags;
  int width, height;
  GtkBorder inner;
  GdkGCValues vals;
  
  frames = META_FRAMES (widget);
  
  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  meta_frames_calc_geometry (frames, frame, &fgeom);
  flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);
  meta_core_get_frame_size (gdk_display, frame->xwindow, &width, &height);
  
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

  draw_current_control_bg (frames, frame, &fgeom);

  if (event->area.y < fgeom.top_height &&
      fgeom.title_rect.width > 0 && fgeom.title_rect.height > 0)
    {
      GdkRectangle clip;
      GdkGC *layout_gc;
      
      clip = fgeom.title_rect;
      clip.x += frames->props->text_border.left;
      clip.width -= frames->props->text_border.left +
        frames->props->text_border.right;

      layout_gc = widget->style->text_gc[GTK_STATE_NORMAL];
      if (flags & META_FRAME_HAS_FOCUS)
        {
          layout_gc = widget->style->text_gc[GTK_STATE_SELECTED];

          /* Draw blue background */
          gdk_draw_rectangle (frame->window,
                              widget->style->base_gc[GTK_STATE_SELECTED],
                              TRUE,
                              fgeom.title_rect.x,
                              fgeom.title_rect.y,
                              fgeom.title_rect.width,
                              fgeom.title_rect.height);
        }

      if (frame->layout)
        {
          gdk_gc_set_clip_rectangle (layout_gc, &clip);
          gdk_draw_layout (frame->window,
                           layout_gc,
                           fgeom.title_rect.x + frames->props->text_border.left,
                           fgeom.title_rect.y + frames->props->text_border.top,
                           frame->layout);
          gdk_gc_set_clip_rectangle (layout_gc, NULL);
        }
    }

  inner = frames->props->inner_button_border;
  
  if (fgeom.close_rect.width > 0 && fgeom.close_rect.height > 0)
    {
      gdk_draw_line (frame->window,
                     widget->style->fg_gc[GTK_STATE_NORMAL],
                     fgeom.close_rect.x + inner.left,
                     fgeom.close_rect.y + inner.top,
                     fgeom.close_rect.x + fgeom.close_rect.width - inner.right,
                     fgeom.close_rect.y + fgeom.close_rect.height - inner.bottom);

      gdk_draw_line (frame->window,
                     widget->style->fg_gc[GTK_STATE_NORMAL],
                     fgeom.close_rect.x + inner.left,
                     fgeom.close_rect.y + fgeom.close_rect.height - inner.bottom,
                     fgeom.close_rect.x + fgeom.close_rect.width - inner.right,
                     fgeom.close_rect.y + inner.top);
    }

  if (fgeom.max_rect.width > 0 && fgeom.max_rect.height > 0)
    {      
      gdk_draw_rectangle (frame->window,
                          widget->style->fg_gc[GTK_STATE_NORMAL],
                          FALSE,
                          fgeom.max_rect.x + inner.left,
                          fgeom.max_rect.y + inner.top,
                          fgeom.max_rect.width - inner.left - inner.right,
                          fgeom.max_rect.height - inner.top - inner.bottom);
      
      vals.line_width = 3;
      gdk_gc_set_values (widget->style->fg_gc[GTK_STATE_NORMAL],
                         &vals,
                         GDK_GC_LINE_WIDTH);

      gdk_draw_line (frame->window,
                     widget->style->fg_gc[GTK_STATE_NORMAL],
                     fgeom.max_rect.x + inner.left,
                     fgeom.max_rect.y + inner.top,
                     fgeom.max_rect.x + fgeom.max_rect.width - inner.right,
                     fgeom.max_rect.y + fgeom.max_rect.height - inner.bottom);
      
      vals.line_width = 0;
      gdk_gc_set_values (widget->style->fg_gc[GTK_STATE_NORMAL],
                         &vals,
                         GDK_GC_LINE_WIDTH); 
    }

  if (fgeom.min_rect.width > 0 && fgeom.min_rect.height > 0)
    {
      
      vals.line_width = 3;
      gdk_gc_set_values (widget->style->fg_gc[GTK_STATE_NORMAL],
                         &vals,
                         GDK_GC_LINE_WIDTH);

      gdk_draw_line (frame->window,
                     widget->style->fg_gc[GTK_STATE_NORMAL],
                     fgeom.min_rect.x + inner.left,
                     fgeom.min_rect.y + inner.top,
                     fgeom.min_rect.x + fgeom.min_rect.width - inner.right,
                     fgeom.min_rect.y + fgeom.min_rect.height - inner.bottom);
      
      vals.line_width = 0;
      gdk_gc_set_values (widget->style->fg_gc[GTK_STATE_NORMAL],
                         &vals,
                         GDK_GC_LINE_WIDTH);
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
      x = fgeom.menu_rect.x;
      y = fgeom.menu_rect.y;
      x += (fgeom.menu_rect.width - 7) / 2;
      y += (fgeom.menu_rect.height - 5) / 2;

      gtk_paint_arrow (widget->style,
                       frame->window,
                       GTK_STATE_NORMAL,
                       GTK_SHADOW_OUT,
                       &event->area,
                       widget,
                       "metacity_menu_button",
                       GTK_ARROW_DOWN,
                       TRUE,
                       x, y, 7, 5);
    }
  
  return TRUE;
}

gboolean
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

gboolean
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

gboolean
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

gboolean
meta_frames_leave_notify_event      (GtkWidget           *widget,
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

gboolean
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

gboolean
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

gboolean
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

gboolean
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

gboolean
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

gboolean
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

gboolean
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

gboolean
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


#if 0

static void
frame_query_root_pointer (MetaUIFrame *frame,
                          int *x, int *y)
{
  Window root_return, child_return;
  int root_x_return, root_y_return;
  int win_x_return, win_y_return;
  unsigned int mask_return;

  XQueryPointer (frame->window->display->xdisplay,
                 frame->xwindow,
                 &root_return,
                 &child_return,
                 &root_x_return,
                 &root_y_return,
                 &win_x_return,
                 &win_y_return,
                 &mask_return);

  if (x)
    *x = root_x_return;
  if (y)
    *y = root_y_return;
}

static void
show_tip_now (MetaUIFrame *frame)
{
  const char *tiptext;

  tiptext = NULL;
  switch (frame->current_control)
    {
    case META_FRAME_CONTROL_TITLE:
      break;
    case META_FRAME_CONTROL_DELETE:
      tiptext = _("Close Window");
      break;
    case META_FRAME_CONTROL_MENU:
      tiptext = _("Menu");
      break;
    case META_FRAME_CONTROL_ICONIFY:
      tiptext = _("Minimize Window");
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
      tiptext = _("Maximize Window");
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
    }

  if (tiptext)
    {
      int x, y, width, height;      
      MetaFrameInfo info;

      meta_frame_init_info (frame, &info);
      frame->window->screen->engine->get_control_rect (&info,
                                                       frame->current_control,
                                                       &x, &y, &width, &height,
                                                       frame->theme_data);

      /* Display tip a couple pixels below control */
      meta_screen_show_tip (frame->window->screen,
                            frame->rect.x + x,
                            frame->rect.y + y + height + 2,
                            tiptext);
    }
}

static gboolean
tip_timeout_func (gpointer data)
{
  MetaUIFrame *frame;

  frame = data;

  show_tip_now (frame);

  return FALSE;
}

#define TIP_DELAY 250
static void
queue_tip (MetaUIFrame *frame)
{
  if (frame->tooltip_timeout)
    g_source_remove (frame->tooltip_timeout);

  frame->tooltip_timeout = g_timeout_add (250,
                                          tip_timeout_func,
                                          frame);  
}

static void
clear_tip (MetaUIFrame *frame)
{
  if (frame->tooltip_timeout)
    {
      g_source_remove (frame->tooltip_timeout);
      frame->tooltip_timeout = 0;
    }
  meta_screen_hide_tip (frame->window->screen);
}

static MetaFrameControl
frame_get_control (MetaUIFrame *frame,
                   int x, int y)
{
  MetaFrameInfo info;

  if (x < 0 || y < 0 ||
      x > frame->rect.width || y > frame->rect.height)
    return META_FRAME_CONTROL_NONE;
  
  meta_frame_init_info (frame, &info);
  
  return frame->window->screen->engine->get_control (&info,
                                                     x, y,
                                                     frame->theme_data);
}

static void
update_move (MetaUIFrame *frame,
             int        x,
             int        y)
{
  int dx, dy;
  
  dx = x - frame->grab->start_root_x;
  dy = y - frame->grab->start_root_y;

  frame->window->user_has_moved = TRUE;
  meta_window_move (frame->window,
                    frame->grab->start_window_x + dx,
                    frame->grab->start_window_y + dy);
}

static void
update_resize_se (MetaUIFrame *frame,
                  int x, int y)
{
  int dx, dy;
  
  dx = x - frame->grab->start_root_x;
  dy = y - frame->grab->start_root_y;

  frame->window->user_has_resized = TRUE;
  meta_window_resize (frame->window,
                      frame->grab->start_window_x + dx,
                      frame->grab->start_window_y + dy);
}

static void
update_current_control (MetaUIFrame *frame,
                        int x_root, int y_root)
{
  MetaFrameControl old;

  if (frame->grab)
    return;
  
  old = frame->current_control;

  frame->current_control = frame_get_control (frame,
                                              x_root - frame->rect.x,
                                              y_root - frame->rect.y);

  if (old != frame->current_control)
    {
      meta_frame_queue_draw (frame);

      if (frame->current_control == META_FRAME_CONTROL_NONE)
        clear_tip (frame);
      else
        queue_tip (frame);
    }
}

static void
grab_action (MetaUIFrame      *frame,
             MetaFrameAction action,
             Time            time)
{
  meta_verbose ("Grabbing action %d\n", action);
  
  frame->grab = g_new0 (MetaFrameActionGrab, 1);
  
  if (XGrabPointer (frame->window->display->xdisplay,
                    frame->xwindow,
                    False,
                    ButtonPressMask | ButtonReleaseMask |
                    PointerMotionMask | PointerMotionHintMask,
                    GrabModeAsync, GrabModeAsync,
                    None,
                    None,
                    time) != GrabSuccess)
    meta_warning ("Grab for frame action failed\n");

  frame->grab->action = action;

  /* display ACTIVE state */
  meta_frame_queue_draw (frame);

  clear_tip (frame);
}

static void
ungrab_action (MetaUIFrame      *frame,
               Time            time)
{
  int x, y;

  meta_verbose ("Ungrabbing action %d\n", frame->grab->action);
  
  XUngrabPointer (frame->window->display->xdisplay,
                  time);
  
  g_free (frame->grab);
  frame->grab = NULL;
  
  frame_query_root_pointer (frame, &x, &y);
  update_current_control (frame, x, y);

  /* undisplay ACTIVE state */
  meta_frame_queue_draw (frame);

  queue_tip (frame);
}

static void
get_menu_items (MetaUIFrame *frame,
                MetaFrameInfo *info,
                MetaMessageWindowMenuOps *ops,
                MetaMessageWindowMenuOps *insensitive)
{
  *ops = 0;
  *insensitive = 0;
  
  if (info->flags & META_FRAME_CONTROL_MAXIMIZE)
    {
      if (frame->window->maximized)
        *ops |= META_MESSAGE_MENU_UNMAXIMIZE;
      else
        *ops |= META_MESSAGE_MENU_MAXIMIZE;
    }

  if (frame->window->shaded)
    *ops |= META_MESSAGE_MENU_UNSHADE;
  else
    *ops |= META_MESSAGE_MENU_SHADE;

  if (frame->window->on_all_workspaces)
    *ops |= META_MESSAGE_MENU_UNSTICK;
  else
    *ops |= META_MESSAGE_MENU_STICK;
  
  *ops |= (META_MESSAGE_MENU_DELETE | META_MESSAGE_MENU_WORKSPACES | META_MESSAGE_MENU_MINIMIZE);

  if (!(info->flags & META_FRAME_CONTROL_ICONIFY))
    *insensitive |= META_MESSAGE_MENU_MINIMIZE;
  
  if (!(info->flags & META_FRAME_CONTROL_DELETE))
    *insensitive |= META_MESSAGE_MENU_DELETE;
}

gboolean
meta_frame_event (MetaUIFrame *frame,
                  XEvent    *event)
{
  switch (event->type)
    {
    case KeyPress:
      break;
    case KeyRelease:
      break;
    case ButtonPress:
      /* you can use button 2 to move a window without raising it */
      if (event->xbutton.button == 1)
        meta_window_raise (frame->window);
      
      update_current_control (frame,
                              event->xbutton.x_root,
                              event->xbutton.y_root);
      
      if (frame->grab == NULL)
        {
          MetaFrameControl control;
          control = frame->current_control;

          if (control == META_FRAME_CONTROL_TITLE &&
              event->xbutton.button == 1 &&
              meta_display_is_double_click (frame->window->display))
            {
              meta_verbose ("Double click on title\n");

              /* FIXME this catches double click that starts elsewhere
               * with the second click on title, maybe no one will
               * ever notice
               */

              if (frame->window->shaded)
                meta_window_unshade (frame->window);
              else
                meta_window_shade (frame->window);
            }
          else if (((control == META_FRAME_CONTROL_TITLE ||
                     control == META_FRAME_CONTROL_NONE) &&
                    event->xbutton.button == 1) ||
                   event->xbutton.button == 2)
            {
              meta_verbose ("Begin move on %s\n",
                            frame->window->desc);
              grab_action (frame, META_FRAME_ACTION_MOVING,
                           event->xbutton.time);
              frame->grab->start_root_x = event->xbutton.x_root;
              frame->grab->start_root_y = event->xbutton.y_root;
              /* pos of client in root coords */
              frame->grab->start_window_x =
                frame->rect.x + frame->window->rect.x;
              frame->grab->start_window_y =
                frame->rect.y + frame->window->rect.y;
              frame->grab->start_button = event->xbutton.button; 
            }
          else if (control == META_FRAME_CONTROL_DELETE &&
                   event->xbutton.button == 1)
            {
              meta_verbose ("Close control clicked on %s\n",
                            frame->window->desc);
              grab_action (frame, META_FRAME_ACTION_DELETING,
                           event->xbutton.time);
              frame->grab->start_button = event->xbutton.button;
            }
          else if (control == META_FRAME_CONTROL_MAXIMIZE &&
                   event->xbutton.button == 1)
            {
              meta_verbose ("Maximize control clicked on %s\n",
                            frame->window->desc);
              grab_action (frame, META_FRAME_ACTION_TOGGLING_MAXIMIZE,
                           event->xbutton.time);
              frame->grab->start_button = event->xbutton.button;
            }
          else if (control == META_FRAME_CONTROL_RESIZE_SE &&
                   event->xbutton.button == 1)
            {
              meta_verbose ("Resize control clicked on %s\n",
                            frame->window->desc);
              grab_action (frame, META_FRAME_ACTION_RESIZING_SE,
                           event->xbutton.time);
              frame->grab->start_root_x = event->xbutton.x_root;
              frame->grab->start_root_y = event->xbutton.y_root;
              frame->grab->start_window_x = frame->window->rect.width;
              frame->grab->start_window_y = frame->window->rect.height;
              frame->grab->start_button = event->xbutton.button;
            }
          else if (control == META_FRAME_CONTROL_MENU &&
                   event->xbutton.button == 1)
            {
              int x, y, width, height;      
              MetaFrameInfo info;
              MetaMessageWindowMenuOps ops;
              MetaMessageWindowMenuOps insensitive;
              
              meta_verbose ("Menu control clicked on %s\n",
                            frame->window->desc);
              
              meta_frame_init_info (frame, &info);
              frame->window->screen->engine->get_control_rect (&info,
                                                               META_FRAME_CONTROL_MENU,
                                                               &x, &y, &width, &height,
                                                               frame->theme_data);

              /* Let the menu get a grab. The user could release button
               * before the menu gets the grab, in which case the
               * menu gets somewhat confused, but it's not that
               * disastrous.
               */
              XUngrabPointer (frame->window->display->xdisplay,
                              event->xbutton.time);

              get_menu_items (frame, &info, &ops, &insensitive);
              
              meta_ui_slave_show_window_menu (frame->window->screen->uislave,
                                              frame->window,
                                              frame->rect.x + x,
                                              frame->rect.y + y + height,
                                              event->xbutton.button,
                                              ops, insensitive,
                                              event->xbutton.time);      
            }
        }
      break;
    case ButtonRelease:
      if (frame->grab)
        meta_debug_spew ("Here! grab %p action %d buttons %d %d\n",
                         frame->grab, frame->grab->action, frame->grab->start_button, event->xbutton.button);
      if (frame->grab &&
          event->xbutton.button == frame->grab->start_button)
        {
          switch (frame->grab->action)
            {
            case META_FRAME_ACTION_MOVING:
              update_move (frame, event->xbutton.x_root, event->xbutton.y_root);
              ungrab_action (frame, event->xbutton.time);
              update_current_control (frame,
                                      event->xbutton.x_root, event->xbutton.y_root);
              break;
              
            case META_FRAME_ACTION_RESIZING_SE:
              update_resize_se (frame, event->xbutton.x_root, event->xbutton.y_root);
              ungrab_action (frame, event->xbutton.time);
              update_current_control (frame,
                                      event->xbutton.x_root, event->xbutton.y_root);
              break;

            case META_FRAME_ACTION_DELETING:
              /* Must ungrab before getting "real" control position */
              ungrab_action (frame, event->xbutton.time);
              update_current_control (frame,
                                      event->xbutton.x_root,
                                      event->xbutton.y_root);
              /* delete if we're still over the button */
              if (frame->current_control == META_FRAME_CONTROL_DELETE)
                meta_window_delete (frame->window, event->xbutton.time);
              break;
            case META_FRAME_ACTION_TOGGLING_MAXIMIZE:
              /* Must ungrab before getting "real" control position */
              ungrab_action (frame, event->xbutton.time);
              update_current_control (frame,
                                      event->xbutton.x_root,
                                      event->xbutton.y_root);
              /* delete if we're still over the button */
              if (frame->current_control == META_FRAME_CONTROL_MAXIMIZE)
                {
                  if (frame->window->maximized)
                    meta_window_unmaximize (frame->window);
                  else
                    meta_window_maximize (frame->window);
                }
              break;
            default:
              meta_warning ("Unhandled action in button release\n");
              break;
            }
        }
      break;
    case MotionNotify:
      {
        int x, y;

        frame_query_root_pointer (frame, &x, &y);
        if (frame->grab)
          {
            switch (frame->grab->action)
              {
              case META_FRAME_ACTION_MOVING:
                update_move (frame, x, y);
                break;
                
              case META_FRAME_ACTION_RESIZING_SE:
                update_resize_se (frame, x, y);
                break;
                
              case META_FRAME_ACTION_NONE:
                
                break;
              default:
                break;
              }
          }
        else
          {
            update_current_control (frame, x, y);
          }
        }
      break;
    case EnterNotify:
      /* We handle it here if a decorated window
       * is involved, otherwise we handle it in display.c
       */
      /* do this even if window->has_focus to avoid races */
      meta_window_focus (frame->window,
                         event->xcrossing.time);
      break;
    case LeaveNotify:
      update_current_control (frame, -1, -1);
      break;
    case FocusIn:
      break;
    case FocusOut:
      break;
    case KeymapNotify:
      break;
    case Expose:
      {
        gboolean title_was_exposed = frame->title_exposed;
        meta_frame_queue_draw (frame);
        if (!title_was_exposed &&
            event->xexpose.y > frame->child_y)
          frame->title_exposed = FALSE;
      }
      break;
    case GraphicsExpose:
      break;
    case NoExpose:
      break;
    case VisibilityNotify:
      break;
    case CreateNotify:
      break;
    case DestroyNotify:
      {
        MetaDisplay *display;
        
        meta_warning ("Unexpected destruction of frame 0x%lx, not sure if this should silently fail or be considered a bug\n", frame->xwindow);
        display = frame->window->display;
        meta_error_trap_push (display);
        meta_window_destroy_frame (frame->window);
        meta_error_trap_pop (display);
        return TRUE;
      }
      break;
    case UnmapNotify:
      if (frame->grab)
        ungrab_action (frame, CurrentTime);
      break;
    case MapNotify:
      if (frame->grab)
        ungrab_action (frame, CurrentTime);
      break;
    case MapRequest:
      break;
    case ReparentNotify:
      break;
    case ConfigureNotify:
      break;
    case ConfigureRequest:
      {
        /* This is a request from the UISlave, or else a client
         * that's completely out of line. We call
         * meta_window_move_resize() using this information.
         */

      }
      break;
    case GravityNotify:
      break;
    case ResizeRequest:
      break;
    case CirculateNotify:
      break;
    case CirculateRequest:
      break;
    case PropertyNotify:
      break;
    case SelectionClear:
      break;
    case SelectionRequest:
      break;
    case SelectionNotify:
      break;
    case ColormapNotify:
      break;
    case ClientMessage:
      break;
    case MappingNotify:
      break;
    default:
      break;
    }

  return FALSE;
}
#endif
