/* -*- mode: C; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* big-box.h: Box container.

   Copyright (C) 2006-2008 Red Hat, Inc.
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

#ifndef __BIG_BOX_H__
#define __BIG_BOX_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define BIG_TYPE_BOX            (big_box_get_type ())
#define BIG_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), BIG_TYPE_BOX, BigBox))
#define BIG_IS_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BIG_TYPE_BOX))
#define BIG_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  BIG_TYPE_BOX, BigBoxClass))
#define BIG_IS_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  BIG_TYPE_BOX))
#define BIG_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  BIG_TYPE_BOX, BigBoxClass))

typedef struct _BigBox          BigBox;
typedef struct _BigBoxPrivate   BigBoxPrivate;
typedef struct _BigBoxClass     BigBoxClass;

typedef enum
{
    BIG_BOX_PACK_NONE                 = 0,
    BIG_BOX_PACK_EXPAND               = 1 << 0,
    BIG_BOX_PACK_END                  = 1 << 1,
    BIG_BOX_PACK_IF_FITS              = 1 << 2,
    BIG_BOX_PACK_FIXED                = 1 << 3,
    BIG_BOX_PACK_ALLOCATE_WHEN_HIDDEN = 1 << 4
} BigBoxPackFlags;

typedef enum
{
  BIG_BOX_ALIGNMENT_FIXED  = 0,
  BIG_BOX_ALIGNMENT_FILL   = 1,
  BIG_BOX_ALIGNMENT_START  = 2,
  BIG_BOX_ALIGNMENT_END    = 3,
  BIG_BOX_ALIGNMENT_CENTER = 4
} BigBoxAlignment;

typedef enum
{
  BIG_BOX_ORIENTATION_VERTICAL   = 1,
  BIG_BOX_ORIENTATION_HORIZONTAL = 2
} BigBoxOrientation;

typedef enum
{
  BIG_BOX_BACKGROUND_REPEAT_NONE = 0,
  BIG_BOX_BACKGROUND_REPEAT_X    = 1,
  BIG_BOX_BACKGROUND_REPEAT_Y    = 2,
  BIG_BOX_BACKGROUND_REPEAT_BOTH = 3,
} BigBoxBackgroundRepeat;

struct _BigBox
{
  ClutterActor parent_instance;

  BigBoxPrivate *priv;
};

struct _BigBoxClass
{
  ClutterActorClass parent_class;
};

GType          big_box_get_type          (void) G_GNUC_CONST;

ClutterActor  *big_box_new               (BigBoxOrientation    orientation);

void           big_box_prepend           (BigBox              *box,
                                          ClutterActor        *child,
                                          BigBoxPackFlags      flags);

void           big_box_append            (BigBox              *box,
                                          ClutterActor        *child,
                                          BigBoxPackFlags      flags);

gboolean       big_box_is_empty          (BigBox              *box);

void           big_box_remove_all        (BigBox              *box);

void           big_box_insert_after      (BigBox              *box,
                                          ClutterActor        *child,
                                          ClutterActor        *ref_child,
                                          BigBoxPackFlags      flags);

void           big_box_insert_before     (BigBox              *box,
                                          ClutterActor        *child,
                                          ClutterActor        *ref_child,
                                          BigBoxPackFlags      flags);

void           big_box_set_child_packing (BigBox              *box,
                                          ClutterActor        *child,
                                          BigBoxPackFlags      flags);

void           big_box_set_child_align   (BigBox              *box,
                                          ClutterActor        *child,
                                          BigBoxAlignment      fixed_x_align,
                                          BigBoxAlignment      fixed_y_align);

void           big_box_set_padding       (BigBox              *box,
                                          int                  padding);

void           big_box_set_border_width  (BigBox              *box,
                                          int                  border_width);

G_END_DECLS

#endif /* __BIG_BOX_H__ */
