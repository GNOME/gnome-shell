#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-cairo.h"
#include "clutter-color.h"

/**
 * clutter_cairo_set_source_color:
 * @cr: a Cairo context
 * @color: a #ClutterColor
 *
 * Utility function for setting the source color of @cr using
 * a #ClutterColor. This function is the equivalent of:
 *
 * |[
 *   cairo_set_source_rgba (cr,
 *                          color->red / 255.0,
 *                          color->green / 255.0,
 *                          color->blue / 255.0,
 *                          color->alpha / 255.0);
 * ]|
 *
 * Since: 1.0
 */
void
clutter_cairo_set_source_color (cairo_t            *cr,
                                const ClutterColor *color)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (color != NULL);

  if (color->alpha == 0xff)
    cairo_set_source_rgb (cr,
                          color->red / 255.0,
                          color->green / 255.0,
                          color->blue / 255.0);
  else
    cairo_set_source_rgba (cr,
                           color->red / 255.0,
                           color->green / 255.0,
                           color->blue / 255.0,
                           color->alpha / 255.0);
}

/**
 * clutter_cairo_clear:
 * @cr: a Cairo context
 *
 * Utility function to clear a Cairo context.
 *
 * Since: 1.12
 */
void
clutter_cairo_clear (cairo_t *cr)
{
  cairo_save (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_restore (cr);
}
