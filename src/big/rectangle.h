/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
   Copyright (C) 2008 litl, LLC.

   The libbigwidgets-lgpl is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The libbigwidgets-lgpl is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the libbigwidgets-lgpl; see the file COPYING.LIB.
   If not, write to the Free Software Foundation, Inc., 59 Temple Place -
   Suite 330, Boston, MA 02111-1307, USA.
*/
#ifndef __BIG_RECTANGLE_H__
#define __BIG_RECTANGLE_H__

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define BIG_TYPE_RECTANGLE            (big_rectangle_get_type ())
#define BIG_RECTANGLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), BIG_TYPE_RECTANGLE, BigRectangle))
#define BIG_RECTANGLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  BIG_TYPE_RECTANGLE, BigRectangleClass))
#define BIG_IS_RECTANGLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BIG_TYPE_RECTANGLE))
#define BIG_IS_RECTANGLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  BIG_TYPE_RECTANGLE))
#define BIG_RECTANGLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  BIG_TYPE_RECTANGLE, BigRectangleClass))

typedef struct BigRectangle      BigRectangle;
typedef struct BigRectangleClass BigRectangleClass;

GType          big_rectangle_get_type           (void) G_GNUC_CONST;

G_END_DECLS

#endif  /* __BIG_RECTANGLE_H__ */
