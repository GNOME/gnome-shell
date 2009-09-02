/**
 * SECTION:clutter-layout-manager
 * @short_description: Layout managers base class
 *
 * #ClutterLayoutManager is FIXME
 *
 * #ClutterLayoutManager is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-debug.h"
#include "clutter-layout-manager.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

#define LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED(m,method)   G_STMT_START {  \
        GObject *_obj = G_OBJECT (m);                                   \
        g_warning ("Layout managers of type %s do not implement "       \
                   "the ClutterLayoutManager::%s method",               \
                   G_OBJECT_TYPE_NAME (_obj),                           \
                   (method));                           } G_STMT_END

G_DEFINE_ABSTRACT_TYPE (ClutterLayoutManager,
                        clutter_layout_manager,
                        G_TYPE_INITIALLY_UNOWNED);

static void
layout_manager_real_get_preferred_width (ClutterLayoutManager *manager,
                                         ClutterContainer     *container,
                                         gfloat                for_height,
                                         gfloat               *min_width_p,
                                         gfloat               *nat_width_p)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, "get_preferred_width");

  if (min_width_p)
    *min_width_p = 0.0;

  if (nat_width_p)
    *nat_width_p = 0.0;
}

static void
layout_manager_real_get_preferred_height (ClutterLayoutManager *manager,
                                          ClutterContainer     *container,
                                          gfloat                for_width,
                                          gfloat               *min_height_p,
                                          gfloat               *nat_height_p)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, "get_preferred_height");

  if (min_height_p)
    *min_height_p = 0.0;

  if (nat_height_p)
    *nat_height_p = 0.0;
}

static void
layout_manager_real_allocate (ClutterLayoutManager   *manager,
                              ClutterContainer       *container,
                              const ClutterActorBox  *allocation,
                              ClutterAllocationFlags  flags)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, "allocate");
}

static void
clutter_layout_manager_class_init (ClutterLayoutManagerClass *klass)
{
  klass->get_preferred_width = layout_manager_real_get_preferred_width;
  klass->get_preferred_height = layout_manager_real_get_preferred_height;
  klass->allocate = layout_manager_real_allocate;
}

static void
clutter_layout_manager_init (ClutterLayoutManager *manager)
{
}

/**
 * clutter_layout_manager_get_preferred_width:
 * @manager: a #ClutterLayoutManager
 * @container: the #ClutterContainer using @manager
 * @for_height: the height for which the width should be computed, or -1
 * @min_width_p: (out) (allow-none): return location for the minimum width
 *   of the layout, or %NULL
 * @nat_width_p: (out) (allow-none): return location for the natural width
 *   of the layout, or %NULL
 *
 * Computes the minimum and natural widths of the @container according
 * to @manager.
 *
 * See also clutter_actor_get_preferred_width()
 *
 * Since: 1.2
 */
void
clutter_layout_manager_get_preferred_width (ClutterLayoutManager *manager,
                                            ClutterContainer     *container,
                                            gfloat                for_height,
                                            gfloat               *min_width_p,
                                            gfloat               *nat_width_p)
{
  ClutterLayoutManagerClass *klass;

  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);
  klass->get_preferred_width (manager, container, for_height,
                              min_width_p,
                              nat_width_p);
}

/**
 * clutter_layout_manager_get_preferred_height:
 * @manager: a #ClutterLayoutManager
 * @container: the #ClutterContainer using @manager
 * @for_width: the width for which the height should be computed, or -1
 * @min_height_p: (out) (allow-none): return location for the minimum height
 *   of the layout, or %NULL
 * @nat_height_p: (out) (allow-none): return location for the natural height
 *   of the layout, or %NULL
 *
 * Computes the minimum and natural heights of the @container according
 * to @manager.
 *
 * See also clutter_actor_get_preferred_height()
 *
 * Since: 1.2
 */
void
clutter_layout_manager_get_preferred_height (ClutterLayoutManager *manager,
                                             ClutterContainer     *container,
                                             gfloat                for_width,
                                             gfloat               *min_height_p,
                                             gfloat               *nat_height_p)
{
  ClutterLayoutManagerClass *klass;

  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);
  klass->get_preferred_height (manager, container, for_width,
                               min_height_p,
                               nat_height_p);
}

/**
 * clutter_layout_manager_allocate:
 * @manager: a #ClutterLayoutManager
 * @container: the #ClutterContainer using @manager
 * @allocation: the #ClutterActorBox containing the allocated area
 *   of @container
 * @flags: the allocation flags
 *
 * Allocates the children of @container given an area
 *
 * See also clutter_actor_allocate()
 *
 * Since: 1.2
 */
void
clutter_layout_manager_allocate (ClutterLayoutManager   *manager,
                                 ClutterContainer       *container,
                                 const ClutterActorBox  *allocation,
                                 ClutterAllocationFlags  flags)
{
  ClutterLayoutManagerClass *klass;

  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (allocation != NULL);

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);
  klass->allocate (manager, container, allocation, flags);
}
