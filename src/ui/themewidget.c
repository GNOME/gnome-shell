/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity theme widget (displays themed draw operations) */

/* 
 * Copyright (C) 2002 Havoc Pennington
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

#include "themewidget.h"
#include <math.h>

static void meta_area_class_init   (MetaAreaClass  *klass);
static void meta_area_init         (MetaArea       *area);
static void meta_area_size_request (GtkWidget      *widget,
                                    GtkRequisition *req);
static gint meta_area_expose       (GtkWidget      *widget,
                                    GdkEventExpose *event);
static void meta_area_finalize     (GObject        *object);


static GtkMiscClass *parent_class;

GType
meta_area_get_type (void)
{
  static GType area_type = 0;

  if (!area_type)
    {
      static const GtkTypeInfo area_info =
      {
	"MetaArea",
	sizeof (MetaArea),
	sizeof (MetaAreaClass),
	(GtkClassInitFunc) meta_area_class_init,
	(GtkObjectInitFunc) meta_area_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      area_type = gtk_type_unique (GTK_TYPE_MISC, &area_info);
    }

  return area_type;
}

static void
meta_area_class_init (MetaAreaClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  parent_class = g_type_class_peek (gtk_misc_get_type ());

  gobject_class->finalize = meta_area_finalize;

  widget_class->expose_event = meta_area_expose;
  widget_class->size_request = meta_area_size_request;
}

static void
meta_area_init (MetaArea *area)
{
  GTK_WIDGET_SET_FLAGS (area, GTK_NO_WINDOW);
}

GtkWidget*
meta_area_new (void)
{
  MetaArea *area;
  
  area = gtk_type_new (META_TYPE_AREA);
  
  return GTK_WIDGET (area);
}

static void
meta_area_finalize (GObject *object)
{
  MetaArea *area;

  area = META_AREA (object);
  
  if (area->dnotify)
    (* area->dnotify) (area->user_data);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gint
meta_area_expose (GtkWidget      *widget,
                  GdkEventExpose *event)
{
  MetaArea *area;
  GtkMisc *misc;
  gint x, y;
  gfloat xalign;

  g_return_val_if_fail (META_IS_AREA (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      area = META_AREA (widget);
      misc = GTK_MISC (widget);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	xalign = misc->xalign;
      else
	xalign = 1.0 - misc->xalign;
  
      x = floor (widget->allocation.x + misc->xpad
		 + ((widget->allocation.width - widget->requisition.width) * xalign)
		 + 0.5);
      y = floor (widget->allocation.y + misc->ypad 
		 + ((widget->allocation.height - widget->requisition.height) * misc->yalign)
		 + 0.5);
      
      if (area->expose_func)
        {
          (* area->expose_func) (area, event, x, y,
                                 area->user_data);
        }
    }
  
  return FALSE;
}

static void
meta_area_size_request (GtkWidget      *widget,
                        GtkRequisition *req)
{
  MetaArea *area;

  area = META_AREA (widget);
  
  req->width = 0;
  req->height = 0;
  
  if (area->size_func)
    {
      (* area->size_func) (area, &req->width, &req->height,
                           area->user_data);
    }
}

void
meta_area_setup (MetaArea           *area,
                 MetaAreaSizeFunc    size_func,
                 MetaAreaExposeFunc  expose_func,
                 void               *user_data,
                 GDestroyNotify      dnotify)
{
  if (area->dnotify)
    (* area->dnotify) (area->user_data);
  
  area->size_func = size_func;
  area->expose_func = expose_func;
  area->user_data = user_data;
  area->dnotify = dnotify;

  gtk_widget_queue_resize (GTK_WIDGET (area));
}

