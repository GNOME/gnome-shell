#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include "clutter-layout-manager.h"

/**
 * clutter_layout_manager_begin_animation:
 * @manager: a #ClutterLayoutManager
 * @duration: the duration of the animation, in milliseconds
 * @mode: the easing mode of the animation
 *
 * Begins an animation of @duration milliseconds, using the provided
 * easing @mode
 *
 * The easing mode can be specified either as a #ClutterAnimationMode
 * or as a logical id returned by clutter_alpha_register_func()
 *
 * The result of this function depends on the @manager implementation
 *
 * Return value: (transfer none): The #ClutterAlpha created by the
 *   layout manager; the returned instance is owned by the layout
 *   manager and should not be unreferenced
 *
 * Since: 1.2
 *
 * Deprecated: 1.12
 */
ClutterAlpha *
clutter_layout_manager_begin_animation (ClutterLayoutManager *manager,
                                        guint                 duration,
                                        gulong                mode)
{
  ClutterLayoutManagerClass *klass;

  g_return_val_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager), NULL);

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);

  return klass->begin_animation (manager, duration, mode);
}

/**
 * clutter_layout_manager_end_animation:
 * @manager: a #ClutterLayoutManager
 *
 * Ends an animation started by clutter_layout_manager_begin_animation()
 *
 * The result of this call depends on the @manager implementation
 *
 * Since: 1.2
 *
 * Deprecated: 1.12
 */
void
clutter_layout_manager_end_animation (ClutterLayoutManager *manager)
{
  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));

  CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager)->end_animation (manager);
}

/**
 * clutter_layout_manager_get_animation_progress:
 * @manager: a #ClutterLayoutManager
 *
 * Retrieves the progress of the animation, if one has been started by
 * clutter_layout_manager_begin_animation()
 *
 * The returned value has the same semantics of the #ClutterAlpha:alpha
 * value
 *
 * Return value: the progress of the animation
 *
 * Since: 1.2
 *
 * Deprecated: 1.12
 */
gdouble
clutter_layout_manager_get_animation_progress (ClutterLayoutManager *manager)
{
  ClutterLayoutManagerClass *klass;

  g_return_val_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager), 1.0);

  klass = CLUTTER_LAYOUT_MANAGER_GET_CLASS (manager);

  return klass->get_animation_progress (manager);
}
