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

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_LAYOUT_MANAGER             (clutter_layout_manager_get_type ())
#define CLUTTER_LAYOUT_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_LAYOUT_MANAGER, ClutterLayoutManager))
#define CLUTTER_IS_LAYOUT_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_LAYOUT_MANAGER))
#define CLUTTER_LAYOUT_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_LAYOUT_MANAGER, ClutterLayoutManagerClass))
#define CLUTTER_IS_LAYOUT_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_LAYOUT_MANAGER))
#define CLUTTER_LAYOUT_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_LAYOUT_MANAGER, ClutterLayoutManagerClass))

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

  gpointer CLUTTER_PRIVATE_FIELD (dummy);
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
 * @set_container: virtual function; override to set a back pointer
 *   on the #ClutterContainer using the layout manager. The implementation
 *   should not take a reference on the container, but just take a weak
 *   reference, to avoid potential leaks due to reference cycles
 * @get_child_meta_type: virtual function; override to return the #GType
 *   of the #ClutterLayoutMeta sub-class used by the #ClutterLayoutManager
 * @create_child_meta: virtual function; override to create a
 *   #ClutterLayoutMeta instance associated to a #ClutterContainer and a
 *   child #ClutterActor, used to maintain layout manager specific properties
 * @begin_animation: virtual function; override to control the animation
 *   of a #ClutterLayoutManager with the given duration and easing mode.
 *   This virtual function is deprecated, and it should not be overridden
 *   in newly written code.
 * @end_animation: virtual function; override to end an animation started
 *   by clutter_layout_manager_begin_animation(). This virtual function is
 *   deprecated, and it should not be overriden in newly written code.
 * @get_animation_progress: virtual function; override to control the
 *   progress of the animation of a #ClutterLayoutManager. This virtual
 *   function is deprecated, and it should not be overridden in newly written
 *   code.
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
  void               (* get_preferred_width)    (ClutterLayoutManager   *manager,
                                                 ClutterContainer       *container,
                                                 gfloat                  for_height,
                                                 gfloat                 *min_width_p,
                                                 gfloat                 *nat_width_p);
  void               (* get_preferred_height)   (ClutterLayoutManager   *manager,
                                                 ClutterContainer       *container,
                                                 gfloat                  for_width,
                                                 gfloat                 *min_height_p,
                                                 gfloat                 *nat_height_p);
  void               (* allocate)               (ClutterLayoutManager   *manager,
                                                 ClutterContainer       *container,
                                                 const ClutterActorBox  *allocation,
                                                 ClutterAllocationFlags  flags);

  void               (* set_container)          (ClutterLayoutManager   *manager,
                                                 ClutterContainer       *container);

  GType              (* get_child_meta_type)    (ClutterLayoutManager   *manager);
  ClutterLayoutMeta *(* create_child_meta)      (ClutterLayoutManager   *manager,
                                                 ClutterContainer       *container,
                                                 ClutterActor           *actor);

  /* deprecated */
  ClutterAlpha *     (* begin_animation)        (ClutterLayoutManager   *manager,
                                                 guint                   duration,
                                                 gulong                  mode);
  /* deprecated */
  gdouble            (* get_animation_progress) (ClutterLayoutManager   *manager);
  /* deprecated */
  void               (* end_animation)          (ClutterLayoutManager   *manager);

  void               (* layout_changed)         (ClutterLayoutManager   *manager);

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

void               clutter_layout_manager_get_preferred_width   (ClutterLayoutManager   *manager,
                                                                 ClutterContainer       *container,
                                                                 gfloat                  for_height,
                                                                 gfloat                 *min_width_p,
                                                                 gfloat                 *nat_width_p);
void               clutter_layout_manager_get_preferred_height  (ClutterLayoutManager   *manager,
                                                                 ClutterContainer       *container,
                                                                 gfloat                  for_width,
                                                                 gfloat                 *min_height_p,
                                                                 gfloat                 *nat_height_p);
void               clutter_layout_manager_allocate              (ClutterLayoutManager   *manager,
                                                                 ClutterContainer       *container,
                                                                 const ClutterActorBox  *allocation,
                                                                 ClutterAllocationFlags  flags);

void               clutter_layout_manager_set_container         (ClutterLayoutManager   *manager,
                                                                 ClutterContainer       *container);
void               clutter_layout_manager_layout_changed        (ClutterLayoutManager   *manager);

GParamSpec *       clutter_layout_manager_find_child_property   (ClutterLayoutManager   *manager,
                                                                 const gchar            *name);
GParamSpec **      clutter_layout_manager_list_child_properties (ClutterLayoutManager   *manager,
                                                                 guint                  *n_pspecs);

ClutterLayoutMeta *clutter_layout_manager_get_child_meta        (ClutterLayoutManager   *manager,
                                                                 ClutterContainer       *container,
                                                                 ClutterActor           *actor);

void               clutter_layout_manager_child_set             (ClutterLayoutManager   *manager,
                                                                 ClutterContainer       *container,
                                                                 ClutterActor           *actor,
                                                                 const gchar            *first_property,
                                                                 ...) G_GNUC_NULL_TERMINATED;
void               clutter_layout_manager_child_get             (ClutterLayoutManager   *manager,
                                                                 ClutterContainer       *container,
                                                                 ClutterActor           *actor,
                                                                 const gchar            *first_property,
                                                                 ...) G_GNUC_NULL_TERMINATED;
void               clutter_layout_manager_child_set_property    (ClutterLayoutManager   *manager,
                                                                 ClutterContainer       *container,
                                                                 ClutterActor           *actor,
                                                                 const gchar            *property_name,
                                                                 const GValue           *value);
void               clutter_layout_manager_child_get_property    (ClutterLayoutManager   *manager,
                                                                 ClutterContainer       *container,
                                                                 ClutterActor           *actor,
                                                                 const gchar            *property_name,
                                                                 GValue                 *value);

CLUTTER_DEPRECATED_IN_1_12
ClutterAlpha *     clutter_layout_manager_begin_animation       (ClutterLayoutManager   *manager,
                                                                 guint                   duration,
                                                                 gulong                  mode);
CLUTTER_DEPRECATED_IN_1_12
void               clutter_layout_manager_end_animation         (ClutterLayoutManager   *manager);
CLUTTER_DEPRECATED_IN_1_12
gdouble            clutter_layout_manager_get_animation_progress (ClutterLayoutManager   *manager);

G_END_DECLS

#endif /* __CLUTTER_LAYOUT_MANAGER_H__ */
