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

#ifndef __CLUTTER_LAYOUT_MANAGER_H__
#define __CLUTTER_LAYOUT_MANAGER_H__

#include <clutter/clutter-actor.h>
#include <clutter/clutter-container.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_LAYOUT_MANAGER             (clutter_layout_manager_get_type ())
#define CLUTTER_LAYOUT_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_LAYOUT_MANAGER, ClutterLayoutManager))
#define CLUTTER_IS_LAYOUT_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_LAYOUT_MANAGER))
#define CLUTTER_LAYOUT_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_LAYOUT_MANAGER, ClutterLayoutManagerClass))
#define CLUTTER_IS_LAYOUT_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_LAYOUT_MANAGER))
#define CLUTTER_LAYOUT_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_LAYOUT_MANAGER, ClutterLayoutManagerClass))

typedef struct _ClutterLayoutManager            ClutterLayoutManager;
typedef struct _ClutterLayoutManagerClass       ClutterLayoutManagerClass;

/**
 * ClutterLayoutManager:
 *
 * The #ClutterLayoutManager structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterLayoutManager
{
  /*< private >*/
  GInitiallyUnowned parent_instance;
};

/**
 * ClutterLayoutManagerClass:
 * @get_preferred_width: virtual function; override to provide a preferred
 *   width for the layout manager. See also the get_preferred_width()
 *   virtual function in #ClutterActor
 * @get_preferred_height: virtual function; override to provide a preferred
 *   height for the layout manager. See also the get_preferred_height()
 *   virtual function in #ClutterActor
 * @allocate: virtual function; override to allocate the children of the
 *   layout manager. See also the allocate() virtual function in
 *   #ClutterActor
 * @create_child_meta: virtual function; override to create a
 *   #ClutterChildMeta instance associated to a #ClutterContainer and a
 *   child #ClutterActor, used to maintain layout manager specific properties
 * @layout_changed: class handler for the #ClutterLayoutManager::layout-changed
 *   signal
 *
 * The #ClutterLayoutManagerClass structure contains only private
 * data and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterLayoutManagerClass
{
  /*< private >*/
  GInitiallyUnownedClass parent_class;

  /*< public >*/
  void              (* get_preferred_width)  (ClutterLayoutManager   *manager,
                                              ClutterContainer       *container,
                                              gfloat                  for_height,
                                              gfloat                 *minimum_width_p,
                                              gfloat                 *natural_width_p);
  void              (* get_preferred_height) (ClutterLayoutManager   *manager,
                                              ClutterContainer       *container,
                                              gfloat                  for_width,
                                              gfloat                 *minimum_height_p,
                                              gfloat                 *natural_height_p);
  void              (* allocate)             (ClutterLayoutManager   *manager,
                                              ClutterContainer       *container,
                                              const ClutterActorBox  *allocation,
                                              ClutterAllocationFlags  flags);

  ClutterChildMeta *(* create_child_meta)    (ClutterLayoutManager *manager,
                                              ClutterContainer     *container,
                                              ClutterActor         *actor);

  void              (* layout_changed)       (ClutterLayoutManager   *manager);

  /*< private >*/
  /* padding for future expansion */
  void (* _clutter_padding_1) (void);
  void (* _clutter_padding_2) (void);
  void (* _clutter_padding_3) (void);
  void (* _clutter_padding_4) (void);
  void (* _clutter_padding_5) (void);
  void (* _clutter_padding_6) (void);
  void (* _clutter_padding_7) (void);
  void (* _clutter_padding_8) (void);
};

GType clutter_layout_manager_get_type (void) G_GNUC_CONST;

void              clutter_layout_manager_get_preferred_width  (ClutterLayoutManager   *manager,
                                                               ClutterContainer       *container,
                                                               gfloat                  for_height,
                                                               gfloat                 *min_width_p,
                                                               gfloat                 *nat_width_p);
void              clutter_layout_manager_get_preferred_height (ClutterLayoutManager   *manager,
                                                               ClutterContainer       *container,
                                                               gfloat                  for_width,
                                                               gfloat                 *min_height_p,
                                                               gfloat                 *nat_height_p);
void              clutter_layout_manager_allocate             (ClutterLayoutManager   *manager,
                                                               ClutterContainer       *container,
                                                               const ClutterActorBox  *allocation,
                                                               ClutterAllocationFlags  flags);

void              clutter_layout_manager_layout_changed       (ClutterLayoutManager   *manager);

ClutterChildMeta *clutter_layout_manager_get_child_meta       (ClutterLayoutManager   *manager,
                                                               ClutterContainer       *container,
                                                               ClutterActor           *actor);
void              clutter_layout_manager_add_child_meta       (ClutterLayoutManager   *manager,
                                                               ClutterContainer       *container,
                                                               ClutterActor           *actor);
void              clutter_layout_manager_remove_child_meta    (ClutterLayoutManager   *manager,
                                                               ClutterContainer       *container,
                                                               ClutterActor           *actor);

void              clutter_layout_manager_child_set            (ClutterLayoutManager   *manager,
                                                               ClutterContainer       *container,
                                                               ClutterActor           *actor,
                                                               const gchar            *first_property,
                                                               ...) G_GNUC_NULL_TERMINATED;
void              clutter_layout_manager_child_get            (ClutterLayoutManager   *manager,
                                                               ClutterContainer       *container,
                                                               ClutterActor           *actor,
                                                               const gchar            *first_property,
                                                               ...) G_GNUC_NULL_TERMINATED;
void              clutter_layout_manager_child_set_property   (ClutterLayoutManager   *manager,
                                                               ClutterContainer       *container,
                                                               ClutterActor           *actor,
                                                               const gchar            *property_name,
                                                               const GValue           *value);
void              clutter_layout_manager_child_get_property   (ClutterLayoutManager   *manager,
                                                               ClutterContainer       *container,
                                                               ClutterActor           *actor,
                                                               const gchar            *property_name,
                                                               GValue                 *value);

G_END_DECLS

#endif /* __CLUTTER_LAYOUT_MANAGER_H__ */
