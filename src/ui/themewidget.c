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

#include "gtk-compat.h"

static void meta_area_size_request (GtkWidget      *widget,
                                    GtkRequisition *req);
static gint meta_area_expose       (GtkWidget      *widget,
                                    GdkEventExpose *event);
static void meta_area_finalize     (GObject        *object);


G_DEFINE_TYPE (MetaArea, meta_area, GTK_TYPE_MISC);

static void
meta_area_class_init (MetaAreaClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  gobject_class->finalize = meta_area_finalize;

  widget_class->expose_event = meta_area_expose;
  widget_class->size_request = meta_area_size_request;
}

static void
meta_area_init (MetaArea *area)
{
  gtk_widget_set_has_window (GTK_WIDGET (area), FALSE);
}

GtkWidget*
meta_area_new (void)
{
  MetaArea *area;
  
  area = g_object_new (META_TYPE_AREA, NULL);
  
  return GTK_WIDGET (area);
}

static void
meta_area_finalize (GObject *object)
{
  MetaArea *area;

  area = META_AREA (object);
  
  if (area->dnotify)
    (* area->dnotify) (area->user_data);
  
  G_OBJECT_CLASS (meta_area_parent_class)->finalize (object);
}

static gint
meta_area_expose (GtkWidget      *widget,
                  GdkEventExpose *event)
{
  MetaArea *area;
  GtkAllocation allocation;
  GtkMisc *misc;
  GtkRequisition requisition;
  gfloat xalign, yalign;
  gint x, y;
  gint xpad, ypad;

  g_return_val_if_fail (META_IS_AREA (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (gtk_widget_is_drawable (widget))
    {
      area = META_AREA (widget);
      misc = GTK_MISC (widget);

      gtk_widget_get_allocation (widget, &allocation);
      gtk_widget_get_requisition (widget, &requisition);
      gtk_misc_get_alignment (misc, &xalign, &yalign);
      gtk_misc_get_padding (misc, &xpad, &ypad);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
        xalign = 1.0 - xalign;
  
      x = floor (allocation.x + xpad
		 + ((allocation.width - requisition.width) * xalign)
		 + 0.5);
      y = floor (allocation.y + ypad
		 + ((allocation.height - requisition.height) * yalign)
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

