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

struct _MetaFrame
{
  Window xwindow;
  GdkWindow *window;
  PangoLayout *layout;
  MetaFrameFlags flags;
  /* w/h of frame window */
  int width;
  int height;
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

  MetaRectangle close_rect;
  MetaRectangle max_rect;
  MetaRectangle min_rect;
  MetaRectangle spacer_rect;
  MetaRectangle menu_rect;
  MetaRectangle title_rect;  
};

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
                                       MetaFrameGeometry *fgeom);

static MetaFrame* meta_frames_lookup_window (MetaFrames *frames,
                                             Window      xwindow);

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

      frames_type = gtk_type_unique (GTK_TYPE_WIDGET, &frames_info);
    }

  return frames_type;
}

#define BORDER_PROPERTY (name, blurb, docs)                                        \
  gtk_widget_class_install_style_property (widget_class,                           \
					   g_param_spec_boxed (name,               \
							       blurb,              \
							       docs,               \
							       GTK_TYPE_BORDER,    \
							       G_PARAM_READABLE))

#define INT_PROPERTY (name, default, blurb, docs)                               \
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
meta_frames_destroy (GtkObject *object)
{

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
listify_func (gpointer key, gpointer value, gpointer data)
{
  GSList **listp;

  listp = data;
  *listp = g_slist_prepend (*listp, value);
}

static void
meta_frames_finalize (GObject *object)
{
  MetaFrames *frames;
  GSList *winlist;
  GSList *tmp;
  
  frames = META_FRAMES (object);

  winlist = NULL;
  g_hash_table_foreach (frames->frames,
                        listify_func,
                        &winlist);

  /* Unmanage all frames */
  tmp = winlist;
  while (tmp != NULL)
    {
      MetaFrame *frame;

      frame = tmp->data;

      meta_frames_unmanage_window (frames, frame->xwindow);
      
      tmp = tmp->next;
    }
  g_slist_free (winlist);

  g_assert (g_hash_table_size (frames->frames) == 0);
  g_hash_table_destroy (frames->frames);
  
  g_free (frames->props);

  G_OBJECT_CLASS (parent_class)->finalize (object);
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
    gchar *lang;

    font = pango_context_load_font (gtk_widget_get_pango_context (widget),
                                    widget->style->font_desc);
    lang = pango_context_get_lang (gtk_widget_get_pango_context (widget));
    pango_font_get_metrics (font, lang, &metrics);
    g_free (lang);
    
    g_object_unref (G_OBJECT (font));
    
    frames->text_height = metrics.ascent + metrics.descent;
  }
}

static void
meta_frames_calc_geometry (MetaFrames        *frames,
                           MetaFrame         *frame,
                           MetaFrameGeometry *fgeom)
{
  int x;
  int button_y;
  int title_right_edge;
  gboolean shaded;
  MetaFrameProperties props;
  int buttons_height, title_height, spacer_height;
  
  props = *(frames->props);

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

  if (frame->flags & META_FRAME_SHADED)
    fgeom->bottom_height = 0;
  else
    fgeom->bottom_height = props.bottom_height;

  x = frame->width - fgeom->button_inset;

  /* center buttons */
  button_y = (fgeom->top_height -
              (props.button_height + props.button_border.top + props.button_border.bottom)) / 2 + props.button_border.top;

  if ((frame->flags & META_FRAME_ALLOWS_DELETE) &&
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

  if ((frame->flags & META_FRAME_ALLOWS_MAXIMIZE) &&
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
  
  if ((frame->flags & META_FRAME_ALLOWS_MINIMIZE) &&
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
  x = fgeom->left_inset;
  
  if ((frame->flags & META_FRAME_ALLOWS_MENU) &&
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
  fgeom->title_rect.height = frames->top_height - props.title_border.top - props.title_border.bottom;

  /* Nuke title if it won't fit */
  if (fgeom->title_rect.width < 0 ||
      fgeom->title_rect.height < 0)
    {
      fgeom->title_rect.width = 0;
      fgeom->title_rect.height = 0;
    }
}

void
meta_frames_manage_window (MetaFrames *frames,
                           Window      xwindow)
{
  MetaFrame *frame;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  frame = g_new (MetaFrame, 1);

  gdk_error_trap_push ();
  
  frame->window = gdk_window_foreign_new (xwindow);

  if (frame->window == NULL)
    {
      gdk_flush ();
      gdk_error_trap_pop ();
      g_free (frame);
      meta_ui_warning ("Frame 0x%lx disappeared as we managed it\n", xwindow);
      return;
    }
  
  gdk_window_set_user_data (frame->window, frames);
  
  gdk_window_set_events (frame->window,
                         GDK_EXPOSURE_MASK |
                         GDK_POINTER_MOTION_MASK |
                         GDK_POINTER_MOTION_HINT_MASK |
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_STRUCTURE_MASK);

  gdk_drawable_get_size (GDK_DRAWABLE (frame->window),
                         &frame->width, &frame->height);

  /* This shouldn't be required if we don't select for button
   * press in frame.c?
   */
  XGrabButton (gdk_display, AnyButton, AnyModifier,
               xwindow, False,
               ButtonPressMask | ButtonReleaseMask |    
               PointerMotionMask | PointerMotionHintMask,
               GrabModeAsync, GrabModeAsync,
               False, None);

  gdk_flush ();
  if (gdk_error_trap_pop ())
    {
      g_object_unref (G_OBJECT (frame->window));
      g_free (frame);
      meta_ui_warning ("Errors managing frame 0x%lx\n", xwindow);
      return;
    }

  frame->xwindow = xwindow;
  frame->layout = NULL;
  frame->flags = 0;
  
  g_hash_table_insert (frames->frames, &frame->xwindow);
}

void
meta_frames_unmanage_window (MetaFrames *frames,
                             Window      xwindow)
{
  MetaFrame *frame;

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
    meta_ui_warning ("Frame 0x%lx not managed, can't unmanage\n", xwindow);
}

static MetaFrame*
meta_frames_lookup_window (MetaFrames *frames,
                           Window      xwindow)
{
  MetaFrame *frame;

  frame = g_hash_table_lookup (frames->frames, &xwindow);

  return frame;
}

gboolean
meta_frames_button_press_event (GtkWidget      *widget,
                                GdkEventButton *event)
{
  MetaFrame *frame;
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
  MetaFrame *frame;
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
  MetaFrame *frame;
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
  MetaFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  return TRUE;
}

gboolean
meta_frames_expose_event            (GtkWidget           *widget,
                                     GdkEventExpose      *event)
{
  MetaFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  return TRUE;
}

gboolean
meta_frames_key_press_event         (GtkWidget           *widget,
                                     GdkEventKey         *event)
{
  MetaFrame *frame;
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
  MetaFrame *frame;
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
  MetaFrame *frame;
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
  MetaFrame *frame;
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
  MetaFrame *frame;
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
  MetaFrame *frame;
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
  MetaFrame *frame;
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
  MetaFrame *frame;
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
  MetaFrame *frame;
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
  MetaFrame *frame;
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
  MetaFrame *frame;
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
  MetaFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  return TRUE;
}

