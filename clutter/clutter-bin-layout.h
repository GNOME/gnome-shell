/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation.
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
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_BIN_LAYOUT_H__
#define __CLUTTER_BIN_LAYOUT_H__

#include <clutter/clutter-layout-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BIN_LAYOUT                 (clutter_bin_layout_get_type ())
#define CLUTTER_BIN_LAYOUT(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BIN_LAYOUT, ClutterBinLayout))
#define CLUTTER_IS_BIN_LAYOUT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BIN_LAYOUT))
#define CLUTTER_BIN_LAYOUT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BIN_LAYOUT, ClutterBinLayoutClass))
#define CLUTTER_IS_BIN_LAYOUT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BIN_LAYOUT))
#define CLUTTER_BIN_LAYOUT_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BIN_LAYOUT, ClutterBinLayoutClass))

typedef struct _ClutterBinLayout                ClutterBinLayout;
typedef struct _ClutterBinLayoutPrivate         ClutterBinLayoutPrivate;
typedef struct _ClutterBinLayoutClass           ClutterBinLayoutClass;

/**
 * ClutterBinLayout:
 *
 * The #ClutterBinLayout structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterBinLayout
{
  /*< private >*/
  ClutterLayoutManager parent_instance;

  ClutterBinLayoutPrivate *priv;
};

/**
 * ClutterBinLayoutClass:
 *
 * The #ClutterBinLayoutClass structure contains only private
 * data and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterBinLayoutClass
{
  /*< private >*/
  ClutterLayoutManagerClass parent_class;
};

CLUTTER_AVAILABLE_IN_1_2
GType clutter_bin_layout_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_2
ClutterLayoutManager *  clutter_bin_layout_new  (ClutterBinAlignment x_align,
                                                 ClutterBinAlignment y_align);

G_END_DECLS

#endif /* __CLUTTER_BIN_LAYOUT_H__ */
