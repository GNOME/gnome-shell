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
#include "menu.h"
#include "fixedtip.h"

#define DEFAULT_INNER_BUTTON_BORDER 3

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

  int width;
  int height;
  
  GdkRectangle close_rect;
  GdkRectangle max_rect;
  GdkRectangle min_rect;
  GdkRectangle spacer_rect;
  GdkRectangle menu_rect;
  GdkRectangle title_rect;  
};

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
  
  INT_PROPERTY ("button_width", 15, _("Button width"), _("Width of buttons"));
  INT_PROPERTY ("button_height", 15, _("Button height"), _("Height of buttons"));

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

  frames->tooltip_timeout = 0;
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
  static GtkBorder default_button_border = { 1, 1, 1, 1 };
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

  fgeom->width = width;
  fgeom->height = height;
  
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
  
  if (flags & META_FRAME_ALLOWS_MENU)
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

  /* Check for maximize overlap */
  if (fgeom->max_rect.width > 0 &&
      fgeom->max_rect.x < (fgeom->menu_rect.x + fgeom->menu_rect.height))
    {
      fgeom->max_rect.width = 0;
      fgeom->max_rect.height = 0;
    }
  
  /* Check for minimize overlap */
  if (fgeom->min_rect.width > 0 &&
      fgeom->min_rect.x < (fgeom->menu_rect.x + fgeom->menu_rect.height))
    {
      fgeom->min_rect.width = 0;
      fgeom->min_rect.height = 0;
    }

  /* Check for spacer overlap */
  if (fgeom->spacer_rect.width > 0 &&
      fgeom->spacer_rect.x < (fgeom->menu_rect.x + fgeom->menu_rect.height))
    {
      fgeom->spacer_rect.width = 0;
      fgeom->spacer_rect.height = 0;
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

  /* Don't set event mask here, it's in frame.c */
  
  frame->xwindow = xwindow;
  frame->layout = NULL;
  
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
            {
              if (meta_core_get_frame_flags (gdk_display, frame->xwindow) &
                  META_FRAME_MAXIMIZED)
                meta_core_unmaximize (gdk_display, frame->xwindow);
              else
                meta_core_maximize (gdk_display, frame->xwindow);
            }
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

static void
draw_control (MetaFrames  *frames,
              GdkDrawable *drawable,
              GdkGC       *override,
              MetaFrameControl control,
              int x, int y, int width, int height)
{
  GtkWidget *widget;
  GdkGCValues vals;
  GdkGC *gc;

  widget = GTK_WIDGET (frames);

#define THICK_LINE_WIDTH 3

  gc = override ? override : widget->style->fg_gc[GTK_STATE_NORMAL];
  
  switch (control)
    {
    case META_FRAME_CONTROL_DELETE:
      {
        gdk_draw_line (drawable,
                       gc,
                       x, y, x + width - 1, y + height - 1);
        
        gdk_draw_line (drawable,
                       gc,
                       x, y + height - 1, x + width - 1, y);
      }
      break;

    case META_FRAME_CONTROL_MAXIMIZE:
      {
        gdk_draw_rectangle (drawable,
                            gc,
                            FALSE,
                            x, y, width - 1, height - 1);
        
        vals.line_width = THICK_LINE_WIDTH;
        gdk_gc_set_values (gc,
                           &vals,
                           GDK_GC_LINE_WIDTH);
        
        gdk_draw_line (drawable,
                       gc,
                       x, y + 1, x + width, y + 1);
        
        vals.line_width = 0;
        gdk_gc_set_values (gc,
                           &vals,
                           GDK_GC_LINE_WIDTH);
      }
      break;

    case META_FRAME_CONTROL_MINIMIZE:
      {
              
      vals.line_width = THICK_LINE_WIDTH;
      gdk_gc_set_values (gc,
                         &vals,
                         GDK_GC_LINE_WIDTH);

      gdk_draw_line (drawable,
                     gc,
                     x,         y + height - THICK_LINE_WIDTH + 1,
                     x + width, y + height - THICK_LINE_WIDTH + 1);
      
      vals.line_width = 0;
      gdk_gc_set_values (gc,
                         &vals,
                         GDK_GC_LINE_WIDTH);
      }
      break;
      
    default:
      break;
    }
#undef THICK_LINE_WIDTH
}

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
  GdkGC *mgc;
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

  color.pixel = 0;
  gdk_gc_set_foreground (mgc, &color);
  gdk_draw_rectangle (mask, mgc, TRUE, 0, 0, -1, -1);

  color.pixel = 1;
  gdk_gc_set_foreground (mgc, &color);
  draw_control (frames, mask, mgc, control, 0, 0, w, h);

  gdk_gc_unref (mgc);

  draw_control (frames, pix, NULL, control, 0, 0, w, h);

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
      draw_control_bg (frames, frame, META_FRAME_CONTROL_DELETE, &fgeom);

      draw_control (frames, frame->window,
                    NULL,
                    META_FRAME_CONTROL_DELETE,
                    fgeom.close_rect.x + inner.left,
                    fgeom.close_rect.y + inner.top,
                    fgeom.close_rect.width - inner.right - inner.left,
                    fgeom.close_rect.height - inner.bottom - inner.top);
    }

  if (fgeom.max_rect.width > 0 && fgeom.max_rect.height > 0)
    {
      draw_control_bg (frames, frame, META_FRAME_CONTROL_MAXIMIZE, &fgeom);
      
      draw_control (frames, frame->window,                    
                    NULL,
                    META_FRAME_CONTROL_MAXIMIZE,
                    fgeom.max_rect.x + inner.left,
                    fgeom.max_rect.y + inner.top,
                    fgeom.max_rect.width - inner.left - inner.right,
                    fgeom.max_rect.height - inner.top - inner.bottom);
    }

  if (fgeom.min_rect.width > 0 && fgeom.min_rect.height > 0)
    {
      draw_control_bg (frames, frame, META_FRAME_CONTROL_MINIMIZE, &fgeom);

      draw_control (frames, frame->window,
                    NULL,
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
    return META_FRAME_CONTROL_NONE;
  
  if (POINT_IN_RECT (x, y, fgeom.close_rect))
    return META_FRAME_CONTROL_DELETE;

  if (POINT_IN_RECT (x, y, fgeom.min_rect))
    return META_FRAME_CONTROL_MINIMIZE;

  if (POINT_IN_RECT (x, y, fgeom.max_rect))
    return META_FRAME_CONTROL_MAXIMIZE;

  if (POINT_IN_RECT (x, y, fgeom.menu_rect))
    return META_FRAME_CONTROL_MENU;
  
  if (POINT_IN_RECT (x, y, fgeom.title_rect))
    return META_FRAME_CONTROL_TITLE;
      
  flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);

  has_vert = (flags & META_FRAME_ALLOWS_VERTICAL_RESIZE) != 0;
  has_horiz = (flags & META_FRAME_ALLOWS_HORIZONTAL_RESIZE) != 0;

  if (has_vert || has_horiz)
    {
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
