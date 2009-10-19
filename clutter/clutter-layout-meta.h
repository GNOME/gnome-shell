/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation
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
 *      Emmanuele Bassi <ebassi@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_LAYOUT_META_H__
#define __CLUTTER_LAYOUT_META_H__

#include <clutter/clutter-types.h>
#include <clutter/clutter-child-meta.h>
#include <clutter/clutter-layout-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_LAYOUT_META                (clutter_layout_meta_get_type ())
#define CLUTTER_LAYOUT_META(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_LAYOUT_META, ClutterLayoutMeta))
#define CLUTTER_IS_LAYOUT_META(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_LAYOUT_META))
#define CLUTTER_LAYOUT_META_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_LAYOUT_META, ClutterLayoutMetaClass))
#define CLUTTER_IS_LAYOUT_META_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_LAYOUT_META))
#define CLUTTER_LAYOUT_META_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_LAYOUT_META, ClutterLayoutMetaClass))

/* ClutterLayoutMeta is defined in clutter-types.h */

typedef struct _ClutterLayoutMetaClass          ClutterLayoutMetaClass;

/**
 * ClutterLayoutMeta
 * @manager: the layout manager handling this data
 *
 * Sub-class of #ClutterChildMeta specific for layout managers
 *
 * A #ClutterLayoutManager sub-class should create a #ClutterLayoutMeta
 * instance by overriding the #ClutterLayoutManager::create_child_meta()
 * virtual function
 *
 * Since: 1.2
 */
struct _ClutterLayoutMeta
{
  /*< private >*/
  ClutterChildMeta parent_instance;

  /*< public >*/
  ClutterLayoutManager *manager;

  /*< private >*/
  /* padding */
  gpointer dummy;
};

struct _ClutterLayoutMetaClass
{
  ClutterChildMetaClass parent_class;
};

GType clutter_layout_meta_get_type (void) G_GNUC_CONST;

ClutterLayoutManager *clutter_layout_meta_get_manager (ClutterLayoutMeta *data);

G_END_DECLS

#endif /* __CLUTTER_LAYOUT_META_H__ */
