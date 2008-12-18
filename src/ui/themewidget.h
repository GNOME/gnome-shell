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

#include "theme.h"
#include <gtk/gtk.h>

#ifndef META_THEME_WIDGET_H
#define META_THEME_WIDGET_H

#define META_TYPE_AREA			 (meta_area_get_type ())
#define META_AREA(obj)			 (GTK_CHECK_CAST ((obj), META_TYPE_AREA, MetaArea))
#define META_AREA_CLASS(klass)		 (GTK_CHECK_CLASS_CAST ((klass), META_TYPE_AREA, MetaAreaClass))
#define META_IS_AREA(obj)		 (GTK_CHECK_TYPE ((obj), META_TYPE_AREA))
#define META_IS_AREA_CLASS(klass)	 (GTK_CHECK_CLASS_TYPE ((klass), META_TYPE_AREA))
#define META_AREA_GET_CLASS(obj)         (GTK_CHECK_GET_CLASS ((obj), META_TYPE_AREA, MetaAreaClass))

typedef struct _MetaArea	MetaArea;
typedef struct _MetaAreaClass	MetaAreaClass;


typedef void (* MetaAreaSizeFunc)   (MetaArea *area,
                                     int      *width,
                                     int      *height,
                                     void     *user_data);

typedef void (* MetaAreaExposeFunc) (MetaArea       *area,
                                     GdkEventExpose *event,
                                     int             x_offset,
                                     int             y_offset,
                                     void           *user_data);

struct _MetaArea
{
  GtkMisc misc;

  MetaAreaSizeFunc size_func;
  MetaAreaExposeFunc expose_func;
  void *user_data;
  GDestroyNotify dnotify;
};

struct _MetaAreaClass
{
  GtkMiscClass parent_class;
};


GtkType    meta_area_get_type	 (void) G_GNUC_CONST;
GtkWidget* meta_area_new	 (void);

void meta_area_setup (MetaArea           *area,
                      MetaAreaSizeFunc    size_func,
                      MetaAreaExposeFunc  expose_func,
                      void               *user_data,
                      GDestroyNotify      dnotify);


#endif
