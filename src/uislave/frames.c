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

static void meta_frames_class_init (MetaFramesClass *klass);
static void meta_frames_init       (MetaFrames      *frames);
static void meta_frames_destroy    (GtkObject       *object);
static void meta_frames_finalize   (GObject         *object);
static void meta_frames_style_set  (GtkWidget       *widget,
                                    GtkStyle        *prev_style);

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
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("slider_width",
							     _("Slider Width"),
							     _("Width of scrollbar or scale thumb"),
							     0,
							     G_MAXINT,
							     14,
							     G_PARAM_READABLE));

}

static void
meta_frames_init (MetaFrames *frames)
{
  GTK_WINDOW (frames)->type = GTK_WINDOW_POPUP;
}

static void
meta_frames_destroy (GtkObject *object)
{


}

static void
meta_frames_finalize (GObject *object)
{


}

void
meta_frames_manage_window (MetaFrames *frames,
                           GdkWindow  *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  gdk_window_set_user_data (window, frames);

  gdk_window_set_events (window,
                         GDK_EXPOSURE_MASK |
                         GDK_POINTER_MOTION_MASK |
                         GDK_POINTER_MOTION_HINT_MASK |
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_STRUCTURE_MASK);
}


