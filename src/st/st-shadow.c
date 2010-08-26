/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#include "config.h"

#include "st-shadow.h"

/**
 * SECTION: st-shadow
 * @short_description: Boxed type for -st-shadow attributes
 *
 * #StShadow is a boxed type for storing attributes of the -st-shadow
 * property, modelled liberally after the CSS3 box-shadow property.
 * See http://www.css3.info/preview/box-shadow/
 *
 */

/**
 * st_shadow_new:
 * @color: shadow's color
 * @xoffset: horizontal offset
 * @yoffset: vertical offset
 * @blur: blur radius
 * @spread: spread radius
 *
 * Creates a new #StShadow
 *
 * Returns: the newly allocated shadow. Use st_shadow_free() when done
 */
StShadow *
st_shadow_new (ClutterColor *color,
               gdouble       xoffset,
               gdouble       yoffset,
               gdouble       blur,
               gdouble       spread)
{
  StShadow *shadow;

  shadow = g_slice_new (StShadow);

  shadow->color   = *color;
  shadow->xoffset = xoffset;
  shadow->yoffset = yoffset;
  shadow->blur    = blur;
  shadow->spread  = spread;

  return shadow;
}

/**
 * st_shadow_copy:
 * @shadow: a #StShadow
 *
 * Makes a copy of @shadow.
 *
 * Returns: an allocated copy of @shadow - the result must be freed with
 *          st_shadow_free() when done
 */
StShadow *
st_shadow_copy (const StShadow *shadow)
{
  g_return_val_if_fail (shadow != NULL, NULL);

  return g_slice_dup (StShadow, shadow);
}

/**
 * st_shadow_free:
 * @shadow: a #StShadow
 *
 * Frees the shadow structure created with st_shadow_new() or
 * st_shadow_copy()
 */
void
st_shadow_free (StShadow *shadow)
{
  g_return_if_fail (shadow != NULL);

  g_slice_free (StShadow, shadow);
}

/**
 * st_shadow_equal:
 * @shadow: a #StShadow
 * @other: a different #StShadow
 *
 * Check if two shadow objects are identical. Note that two shadows may
 * compare non-identically if they differ only by floating point rounding
 * errors.
 *
 * Return value: %TRUE if the two shadows are identical
 */
gboolean
st_shadow_equal (StShadow *shadow,
                 StShadow *other)
{
  g_return_val_if_fail (shadow != NULL, FALSE);
  g_return_val_if_fail (other != NULL, FALSE);

  /* We use strict equality to compare double quantities; this means
   * that, for example, a shadow offset of 0.25in does not necessarily
   * compare equal to a shadow offset of 18pt in this test. Assume
   * that a few false negatives are mostly harmless.
   */

  return (clutter_color_equal (&shadow->color, &other->color) &&
          shadow->xoffset == other->xoffset &&
          shadow->yoffset == other->yoffset &&
          shadow->blur == other->blur &&
          shadow->spread == other->spread);
}

/**
 * st_shadow_get_box:
 * @shadow: a #StShadow
 * @actor_box: the box allocated to a #ClutterAlctor
 * @shadow_box: computed box occupied by @shadow
 *
 * Gets the box used to paint @shadow, which will be partly
 * outside of @actor_box
 */
void
st_shadow_get_box (StShadow              *shadow,
                   const ClutterActorBox *actor_box,
                   ClutterActorBox       *shadow_box)
{
  g_return_if_fail (shadow != NULL);
  g_return_if_fail (actor_box != NULL);
  g_return_if_fail (shadow_box != NULL);

  shadow_box->x1 = actor_box->x1 + shadow->xoffset
                   - shadow->blur - shadow->spread;
  shadow_box->x2 = actor_box->x2 + shadow->xoffset
                   + shadow->blur + shadow->spread;
  shadow_box->y1 = actor_box->y1 + shadow->yoffset
                   - shadow->blur - shadow->spread;
  shadow_box->y2 = actor_box->y2 + shadow->yoffset
                   + shadow->blur + shadow->spread;
}

GType
st_shadow_get_type (void)
{
  static GType _st_shadow_type = 0;

  if (G_UNLIKELY (_st_shadow_type == 0))
    _st_shadow_type =
        g_boxed_type_register_static ("StShadow",
                                      (GBoxedCopyFunc) st_shadow_copy,
                                      (GBoxedFreeFunc) st_shadow_free);

  return _st_shadow_type;
}
