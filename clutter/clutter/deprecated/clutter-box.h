/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009,2010  Intel Corporation.
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

#ifndef __CLUTTER_BOX_H__
#define __CLUTTER_BOX_H__

#include <clutter/clutter-actor.h>
#include <clutter/clutter-container.h>
#include <clutter/clutter-layout-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BOX                (clutter_box_get_type ())
#define CLUTTER_BOX(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BOX, ClutterBox))
#define CLUTTER_IS_BOX(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BOX))
#define CLUTTER_BOX_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BOX, ClutterBoxClass))
#define CLUTTER_IS_BOX_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BOX))
#define CLUTTER_BOX_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BOX, ClutterBoxClass))

typedef struct _ClutterBox              ClutterBox;
typedef struct _ClutterBoxPrivate       ClutterBoxPrivate;
typedef struct _ClutterBoxClass         ClutterBoxClass;

/**
 * ClutterBox:
 *
 * The #ClutterBox structure contains only private data and should
 * be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterBox
{
  /*< private >*/
  ClutterActor parent_instance;

  ClutterBoxPrivate *priv;
};

/**
 * ClutterBoxClass:
 *
 * The #ClutterBoxClass structure contains only private data
 *
 * Since: 1.2
 */
struct _ClutterBoxClass
{
  /*< private >*/
  ClutterActorClass parent_class;

  /* padding, for future expansion */
  void (*clutter_padding_1) (void);
  void (*clutter_padding_2) (void);
  void (*clutter_padding_3) (void);
  void (*clutter_padding_4) (void);
  void (*clutter_padding_5) (void);
  void (*clutter_padding_6) (void);
};

CLUTTER_DEPRECATED_IN_1_10
GType clutter_box_get_type (void) G_GNUC_CONST;

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_new)
ClutterActor *        clutter_box_new                (ClutterLayoutManager *manager);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_set_layout_manager)
void                  clutter_box_set_layout_manager (ClutterBox           *box,
                                                      ClutterLayoutManager *manager);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_get_layout_manager)
ClutterLayoutManager *clutter_box_get_layout_manager (ClutterBox           *box);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_set_background_color)
void                  clutter_box_set_color          (ClutterBox           *box,
                                                      const ClutterColor   *color);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_get_background_color)
void                  clutter_box_get_color          (ClutterBox           *box,
                                                      ClutterColor         *color);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_add_child)
void                  clutter_box_pack               (ClutterBox           *box,
                                                      ClutterActor         *actor,
                                                      const gchar          *first_property,
                                                      ...);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_add_child)
void                  clutter_box_packv              (ClutterBox           *box,
                                                      ClutterActor         *actor,
                                                      guint                 n_properties,
                                                      const gchar * const   properties[],
                                                      const GValue         *values);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_insert_child_above)
void                  clutter_box_pack_after         (ClutterBox           *box,
                                                      ClutterActor         *actor,
                                                      ClutterActor         *sibling,
                                                      const gchar          *first_property,
                                                      ...);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_insert_child_below)
void                  clutter_box_pack_before        (ClutterBox           *box,
                                                      ClutterActor         *actor,
                                                      ClutterActor         *sibling,
                                                      const gchar          *first_property,
                                                      ...);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_insert_child_at_index)
void                  clutter_box_pack_at            (ClutterBox           *box,
                                                      ClutterActor         *actor,
                                                      gint                  position,
                                                      const gchar          *first_property,
                                                      ...);

G_END_DECLS

#endif /* __CLUTTER_BOX_H__ */
