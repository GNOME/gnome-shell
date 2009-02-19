/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __TIDY_GRID_H__
#define __TIDY_GRID_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define TIDY_TYPE_GRID               (tidy_grid_get_type())
#define TIDY_GRID(obj)                                       \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                        \
                               TIDY_TYPE_GRID,               \
                               TidyGrid))
#define TIDY_GRID_CLASS(klass)                               \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                         \
                            TIDY_TYPE_GRID,                  \
                            TidyGridClass))
#define TIDY_IS_GRID(obj)                                    \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                        \
                               TIDY_TYPE_GRID))
#define TIDY_IS_GRID_CLASS(klass)                            \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                         \
                            TIDY_TYPE_GRID))
#define TIDY_GRID_GET_CLASS(obj)                             \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                         \
                              TIDY_TYPE_GRID,                \
                              TidyGridClass))

typedef struct _TidyGrid        TidyGrid;
typedef struct _TidyGridClass   TidyGridClass;
typedef struct _TidyGridPrivate TidyGridPrivate;

struct _TidyGridClass
{
  ClutterActorClass parent_class;
};

struct _TidyGrid
{
  ClutterActor parent;

  TidyGridPrivate *priv;
};

GType tidy_grid_get_type (void) G_GNUC_CONST;

ClutterActor *tidy_grid_new                    (void);
void          tidy_grid_set_end_align          (TidyGrid    *self,
                                                gboolean     value);
gboolean      tidy_grid_get_end_align          (TidyGrid    *self);
void          tidy_grid_set_homogenous_rows    (TidyGrid    *self,
                                                gboolean     value);
gboolean      tidy_grid_get_homogenous_rows    (TidyGrid    *self);
void          tidy_grid_set_homogenous_columns (TidyGrid    *self,
                                                gboolean     value);
gboolean      tidy_grid_get_homogenous_columns (TidyGrid    *self);
void          tidy_grid_set_column_major       (TidyGrid    *self,
                                                gboolean     value);
gboolean      tidy_grid_get_column_major       (TidyGrid    *self);
void          tidy_grid_set_row_gap            (TidyGrid    *self,
                                                ClutterUnit  value);
ClutterUnit   tidy_grid_get_row_gap            (TidyGrid    *self);
void          tidy_grid_set_column_gap         (TidyGrid    *self,
                                                ClutterUnit  value);
ClutterUnit   tidy_grid_get_column_gap         (TidyGrid    *self);
void          tidy_grid_set_valign             (TidyGrid    *self,
                                                gdouble      value);
gdouble       tidy_grid_get_valign             (TidyGrid    *self);
void          tidy_grid_set_halign             (TidyGrid    *self,
                                                gdouble      value);
gdouble       tidy_grid_get_halign             (TidyGrid    *self);

G_END_DECLS

#endif /* __TIDY_GRID_H__ */
