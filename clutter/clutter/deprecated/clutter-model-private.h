/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Neil Jagdish Patel <njp@o-hand.com>
 *             Emmanuele Bassi <ebassi@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CLUTTER_MODEL_PRIVATE_H__
#define __CLUTTER_MODEL_PRIVATE_H__

#include "clutter-types.h"
#include "clutter-model.h"

G_BEGIN_DECLS

void            _clutter_model_set_n_columns    (ClutterModel *model,
                                                 gint          n_columns,
                                                 gboolean      set_types,
                                                 gboolean      set_names);
gboolean        _clutter_model_check_type       (GType         gtype);

void            _clutter_model_set_column_type  (ClutterModel *model,
                                                 gint          column,
                                                 GType         gtype);
void            _clutter_model_set_column_name  (ClutterModel *model,
                                                 gint          column,
                                                 const gchar  *name);

void            _clutter_model_iter_set_row     (ClutterModelIter *iter,
                                                 guint             row);

G_END_DECLS

#endif /* __CLUTTER_MODEL_PRIVATE_H__ */
