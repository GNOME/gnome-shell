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
 *
 * Creates a new #StShadow
 *
 * Returns: the newly allocated shadow. Use st_shadow_free() when done
 */
StShadow *
st_shadow_new (ClutterColor *color,
               gdouble       xoffset,
               gdouble       yoffset,
               gdouble       blur)
{
  StShadow *shadow;

  shadow = g_slice_new (StShadow);

  shadow->color   = *color;
  shadow->xoffset = xoffset;
  shadow->yoffset = yoffset;
  shadow->blur    = blur;

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
