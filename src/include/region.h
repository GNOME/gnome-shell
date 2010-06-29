#ifndef META_REGION_H
#define META_REGION_H

#ifndef PACKAGE_NAME
#error "<config.h> must be included before region.h"
#endif

#include <gdk/gdk.h>

#ifdef USE_CAIRO_REGION
#include <cairo.h>

#define MetaRegion    cairo_region_t

typedef enum {
  META_REGION_OVERLAP_IN   = CAIRO_REGION_OVERLAP_IN,
  META_REGION_OVERLAP_OUT  = CAIRO_REGION_OVERLAP_OUT,
  META_REGION_OVERLAP_PART = CAIRO_REGION_OVERLAP_PART
} MetaOverlapType;

#define meta_region_new()                    cairo_region_create()
#define meta_region_new_from_rectangle(rect) cairo_region_create_rectangle(rect)
#define meta_region_copy(r)                  cairo_region_copy(r)
#define meta_region_destroy(r)               cairo_region_destroy(r)
#define meta_region_is_empty(r)              cairo_region_is_empty(r)
#define meta_region_union_rectangle(r, rect) cairo_region_union_rectangle(r, rect)
#define meta_region_subtract(r, other)       cairo_region_subtract(r, other)
#define meta_region_translate(r, x, y)       cairo_region_translate(r, x, y)
#define meta_region_intersect(r, other)      cairo_region_intersect(r, other)
#define meta_region_contains_rectangle(r, rect) cairo_region_contains_rectangle(r, rect)

void     meta_region_get_rectangles      (MetaRegion    *region,
                                          GdkRectangle **rectangles,
                                          int           *n_rectangles);

#else

#define MetaRegion    GdkRegion

typedef enum {
  META_REGION_OVERLAP_IN   = GDK_OVERLAP_RECTANGLE_IN,
  META_REGION_OVERLAP_OUT  = GDK_OVERLAP_RECTANGLE_OUT,
  META_REGION_OVERLAP_PART = GDK_OVERLAP_RECTANGLE_PART
} MetaOverlapType;

#define meta_region_new()                    gdk_region_new()
#define meta_region_new_from_rectangle(rect) gdk_region_rectangle(rect)
#define meta_region_copy(r)                  gdk_region_copy(r)
#define meta_region_destroy(r)               gdk_region_destroy(r)
#define meta_region_is_empty(r)              gdk_region_empty(r)
#define meta_region_union_rectangle(r, rect) gdk_region_union_with_rect(r, rect)
#define meta_region_subtract(r, other)       gdk_region_subtract(r, other)
#define meta_region_translate(r, x, y)       gdk_region_offset(r, x, y)
#define meta_region_intersect(r, other)      gdk_region_intersect(r, other)
#define meta_region_contains_rectangle(r, rect) gdk_region_rect_in(r, rect)
#define meta_region_get_rectangles(r, rects, num) gdk_region_get_rectangles(r, rects, num)

#endif /* HAVE_CAIRO_REGION */

#endif /* META_REGION_H */
