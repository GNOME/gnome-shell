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

#ifndef __CLUTTER_FIXED_LAYOUT_H__
#define __CLUTTER_FIXED_LAYOUT_H__

#include <clutter/clutter-layout-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_FIXED_LAYOUT               (clutter_fixed_layout_get_type ())
#define CLUTTER_FIXED_LAYOUT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_FIXED_LAYOUT, ClutterFixedLayout))
#define CLUTTER_IS_FIXED_LAYOUT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_FIXED_LAYOUT))
#define CLUTTER_FIXED_LAYOUT_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_FIXED_LAYOUT, ClutterFixedLayoutClass))
#define CLUTTER_IS_FIXED_LAYOUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_FIXED_LAYOUT))
#define CLUTTER_FIXED_LAYOUT_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_FIXED_LAYOUT, ClutterFixedLayoutClass))

typedef struct _ClutterFixedLayout              ClutterFixedLayout;
typedef struct _ClutterFixedLayoutClass         ClutterFixedLayoutClass;

/**
 * ClutterFixedLayout:
 *
 * The #ClutterFixedLayout structure contains only private data and
 * it should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterFixedLayout
{
  /*< private >*/
  ClutterLayoutManager parent_instance;
};

/**
 * ClutterFixedLayoutClass:
 *
 * The #ClutterFixedLayoutClass structure contains only private data
 * and it should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterFixedLayoutClass
{
  /*< private >*/
  ClutterLayoutManagerClass parent_class;
};

GType clutter_fixed_layout_get_type (void) G_GNUC_CONST;

ClutterLayoutManager *clutter_fixed_layout_new (void);

G_END_DECLS

#endif /* __CLUTTER_FIXED_LAYOUT_H__ */
