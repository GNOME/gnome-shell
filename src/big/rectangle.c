/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* rectangle.c: Rounded rectangle.

   Copyright (C) 2008 litl, LLC.

   The libbigwidgets-lgpl is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The libbigwidgets-lgpl is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the libbigwidgets-lgpl; see the file COPYING.LIB.
   If not, write to the Free Software Foundation, Inc., 59 Temple Place -
   Suite 330, Boston, MA 02111-1307, USA.
*/

#include <string.h>
#include <math.h>

#include <glib.h>
#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <cairo/cairo.h>

#include "rectangle.h"

typedef struct {
    gint         ref_count;

    ClutterColor color;
    ClutterColor border_color;
    int          radius;
    int          border_width;

    CoglHandle   texture;
    guint8      *data;
} Corner;

struct BigRectangle {
    ClutterRectangle parent_instance;
    ClutterUnit      radius;
    Corner          *corner;
    CoglHandle       corner_material;
    CoglHandle       border_material;
    CoglHandle       background_material;
    gboolean         corners_dirty;
};

/* map of { radius, border_width, border_color, color } to Corner textures */
static GHashTable *all_corners = NULL;

struct BigRectangleClass {
    ClutterRectangleClass parent_class;
};

G_DEFINE_TYPE(BigRectangle, big_rectangle, CLUTTER_TYPE_RECTANGLE)

enum
{
    PROP_0,

    PROP_CORNER_RADIUS
};

static gboolean
corner_equal(gconstpointer a,
             gconstpointer b)
{
    const Corner *corner_a;
    const Corner *corner_b;

    corner_a = a;
    corner_b = b;

    return *((guint32 *)&corner_a->color) == *((guint32 *)&corner_b->color) &&
           *((guint32 *)&corner_a->border_color) == *((guint32 *)&corner_b->border_color) &&
           corner_a->border_width == corner_b->border_width &&
           corner_a->radius == corner_b->radius;
}

static guint
corner_hash(gconstpointer key)
{
    const Corner *corner;
    guint hashed[4];

    corner = key;

    hashed[0] = *((guint *)&(corner->color));
    hashed[1] = *((guint *)&(corner->border_color));
    hashed[2] = *((guint *)&(corner->border_width));
    hashed[3] = *((guint *)&(corner->radius));

    return hashed[0] ^ hashed[1] ^ hashed[2] ^ hashed[3];
}

static Corner *
create_corner_texture(Corner *src)
{
    Corner *corner;
    CoglHandle texture;
    cairo_t *cr;
    cairo_surface_t *surface;
    guint x, y;
    guint rowstride;
    guint8 *data;
    guint32 *src_p;
    guint8 *dst_p;
    guint size;

    corner = g_memdup(src, sizeof(Corner));

    size = 2 * MAX(corner->border_width, corner->radius);
    rowstride = size * 4;
    data = g_new0(guint8, size * rowstride);

    surface = cairo_image_surface_create_for_data(data,
                                                  CAIRO_FORMAT_ARGB32,
                                                  size, size,
                                                  rowstride);
    cr = cairo_create(surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_scale(cr, size, size);

    if (corner->border_width < corner->radius) {
        double internal_radius = 0.5 * (1.0 - (double) corner->border_width / corner->radius);

        if (corner->border_width != 0) {
            cairo_set_source_rgba(cr,
                                  (double)corner->border_color.red / G_MAXUINT8,
                                  (double)corner->border_color.green / G_MAXUINT8,
                                  (double)corner->border_color.blue / G_MAXUINT8,
                                  (double)corner->border_color.alpha / G_MAXUINT8);

            cairo_arc(cr, 0.5, 0.5, 0.5, 0, 2 * M_PI);
            cairo_fill(cr);
        }

        cairo_set_source_rgba(cr,
                              (double)corner->color.red / G_MAXUINT8,
                              (double)corner->color.green / G_MAXUINT8,
                              (double)corner->color.blue / G_MAXUINT8,
                              (double)corner->color.alpha / G_MAXUINT8);
        cairo_arc(cr, 0.5, 0.5, internal_radius, 0, 2 * M_PI);
        cairo_fill(cr);

    } else {
        double radius;

        radius = (gdouble)corner->radius / corner->border_width;

        cairo_set_source_rgba(cr,
                              (double)corner->border_color.red / G_MAXUINT8,
                              (double)corner->border_color.green / G_MAXUINT8,
                              (double)corner->border_color.blue / G_MAXUINT8,
                              (double)corner->border_color.alpha / G_MAXUINT8);

        cairo_arc(cr, radius, radius, radius, M_PI, 3 * M_PI / 2);
        cairo_line_to(cr, 1.0 - radius, 0.0);
        cairo_arc(cr, 1.0 - radius, radius, radius, 3 * M_PI / 2, 2*M_PI);
        cairo_line_to(cr, 1.0, 1.0 - radius);
        cairo_arc(cr, 1.0 - radius, 1.0 - radius, radius, 0, M_PI / 2);
        cairo_line_to(cr, radius, 1.0);
        cairo_arc(cr, radius, 1.0 - radius, radius, M_PI / 2, M_PI);
        cairo_fill(cr);
    }
    cairo_destroy(cr);

    cairo_surface_destroy(surface);

    corner->data = g_new0(guint8, size * rowstride);

    /* cogl doesn't seem to support the conversion, do it manually */
    /* borrowed from clutter-cairo, conversion from ARGB pre-multiplied
     * to RGBA */
    for (y = 0; y < size; y++) {
        src_p = (guint32 *) (data + y * rowstride);
        dst_p = corner->data + y * rowstride;

        for (x = 0; x < size; x++) {
            guint8 alpha = (*src_p >> 24) & 0xff;

            if (alpha == 0) {
                dst_p[0] = dst_p[1] = dst_p[2] = dst_p[3] = alpha;
            } else {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                dst_p[0] = (((*src_p >> 16) & 0xff) * 255 ) / alpha;
                dst_p[1] = (((*src_p >> 8) & 0xff) * 255 ) / alpha;
                dst_p[2] = (((*src_p >> 0) & 0xff) * 255 ) / alpha;
                dst_p[3] = alpha;
#elif G_BYTE_ORDER == G_BIG_ENDIAN
                dst_p[0] = alpha;
                dst_p[1] = (((*src_p >> 0) & 0xff) * 255 ) / alpha;
                dst_p[2] = (((*src_p >> 8) & 0xff) * 255 ) / alpha;
                dst_p[3] = (((*src_p >> 16) & 0xff) * 255 ) / alpha;
#else /* !G_LITTLE_ENDIAN && !G_BIG_ENDIAN */
#error unknown ENDIAN type
#endif /* !G_LITTLE_ENDIAN && !G_BIG_ENDIAN */
            }
            dst_p += 4;
            src_p++;
        }
    }

    g_free(data);

    texture = cogl_texture_new_from_data(size, size,
                                         0,
                                         FALSE,
                                         COGL_PIXEL_FORMAT_RGBA_8888,
                                         COGL_PIXEL_FORMAT_ANY,
                                         rowstride,
                                         corner->data);
    g_assert(texture != COGL_INVALID_HANDLE);

    corner->ref_count = 1;
    corner->texture = texture;

    g_hash_table_insert(all_corners, corner, corner);

    return corner;
}

static void
corner_unref(Corner *corner)
{
    corner->ref_count --;

    if (corner->ref_count == 0) {
        g_hash_table_remove(all_corners, corner);

        cogl_texture_unref(corner->texture);
        g_free(corner->data);
        g_free(corner);
    }
}

static Corner *
corner_get(guint         radius,
           ClutterColor *color,
           guint         border_width,
           ClutterColor *border_color)
{
    Corner key;
    Corner *corner;

    if (all_corners == NULL) {
        all_corners = g_hash_table_new(corner_hash, corner_equal);
    }

    key.radius = radius;
    key.color = *color;
    key.border_color = *border_color;
    key.border_width = border_width;

    corner = g_hash_table_lookup(all_corners, &key);

    if (!corner) {
        corner = create_corner_texture(&key);
    } else {
        corner->ref_count ++;
    }

    return corner;
}

static void
big_rectangle_update_corners(BigRectangle *rectangle)
{
    Corner *corner;

    corner = NULL;

    if (rectangle->radius != 0) {
        ClutterColor *color;
        ClutterColor *border_color;
        guint border_width;

        g_object_get(rectangle,
                     "border-color", &border_color,
                     "border-width", &border_width,
                     "color", &color,
                     NULL);

        corner = corner_get(CLUTTER_UNITS_TO_DEVICE(rectangle->radius),
                            color,
                            border_width,
                            border_color);

        clutter_color_free(border_color);
        clutter_color_free(color);
    }

    if (rectangle->corner) {
        corner_unref(rectangle->corner);
    }

    rectangle->corner = corner;

    if (corner) {
        if (!rectangle->corner_material)
            rectangle->corner_material = cogl_material_new();

        cogl_material_set_layer (rectangle->corner_material, 0,
                                 rectangle->corner->texture);
    }

    rectangle->corners_dirty = FALSE;
}

static void
big_rectangle_paint(ClutterActor *actor)
{
    BigRectangle *rectangle;
    ClutterColor *color;
    ClutterColor *border_color;
    guint8 actor_opacity;
    CoglColor tmp_color;
    guint border_width;
    ClutterActorBox box;
    float radius;
    float width;
    float height;
    float max;

    rectangle = BIG_RECTANGLE(actor);

    if (rectangle->radius == 0) {
        /* In that case we are no different than our parent class,
         * so don't bother */
        CLUTTER_ACTOR_CLASS(big_rectangle_parent_class)->paint(actor);
        return;
    }

    if (rectangle->corners_dirty)
        big_rectangle_update_corners(rectangle);

    g_object_get(rectangle,
                 "border-color", &border_color,
                 "border-width", &border_width,
                 "color", &color,
                 NULL);

    actor_opacity = clutter_actor_get_paint_opacity (actor);

    clutter_actor_get_allocation_box(actor, &box);

    /* translation was already done */
    box.x2 -= box.x1;
    box.y2 -= box.y1;

    width = box.x2;
    height = box.y2;

    radius = rectangle->radius;

    max = MAX(border_width, radius);

    if (radius != 0) {
        cogl_color_set_from_4ub(&tmp_color,
                                0xff, 0xff, 0xff, actor_opacity);
        cogl_material_set_color(rectangle->corner_material, &tmp_color);
        cogl_set_source(rectangle->corner_material);

        /* NW */
        cogl_rectangle_with_texture_coords(0, 0,
                                           max, max,
                                           0, 0,
                                           0.5, 0.5);

        /* NE */
        cogl_rectangle_with_texture_coords(width - max, 0,
                                           width, max,
                                           0.5, 0,
                                           1.0, 0.5);

        /* SW */
        cogl_rectangle_with_texture_coords(0, height - max,
                                           max, height,
                                           0, 0.5,
                                           0.5, 1.0);

        /* SE */
        cogl_rectangle_with_texture_coords(width - max, height - max,
                                           width, height,
                                           0.5, 0.5,
                                           1.0, 1.0);

    }

    if (border_width != 0) {
        if (!rectangle->border_material)
            rectangle->border_material = cogl_material_new ();

        cogl_color_set_from_4ub(&tmp_color,
                                border_color->red,
                                border_color->green,
                                border_color->blue,
                                actor_opacity * border_color->alpha / 255);
        cogl_material_set_color(rectangle->border_material, &tmp_color);
        cogl_set_source(rectangle->border_material);

        /* NORTH */
        cogl_rectangle(max, 0,
                       width - max, border_width);

        /* EAST */
        cogl_rectangle(width - border_width, max,
                       width, height - max);

        /* SOUTH */
        cogl_rectangle(max, height - border_width,
                       width - max, height);

        /* WEST */
        cogl_rectangle(0, max,
                       border_width, height - max);
    }

    if (!rectangle->background_material)
        rectangle->background_material = cogl_material_new ();

    cogl_color_set_from_4ub(&tmp_color,
                            color->red,
                            color->green,
                            color->blue,
                            actor_opacity * color->alpha / 255);
    cogl_material_set_color(rectangle->background_material, &tmp_color);
    cogl_set_source(rectangle->background_material);

    if (radius > border_width) {
        /* Once we've drawn the borders and corners, if the corners are bigger
         * the the border width, the remaining area is shaped like
         *
         *  ########
         * ##########
         * ##########
         *  ########
         *
         * We draw it in 3 pieces - first the top and bottom, then the main
         * rectangle
         */
        cogl_rectangle(radius, border_width,
                       width - radius, radius);
        cogl_rectangle(radius, height - radius,
                       width - radius, height - border_width);
    }

    cogl_rectangle(border_width, max,
                   width - border_width, height - max);

    clutter_color_free(border_color);
    clutter_color_free(color);
}

static void
big_rectangle_notify(GObject    *object,
                     GParamSpec *pspec)
{
    BigRectangle *rectangle;

    rectangle = BIG_RECTANGLE(object);

    if (g_str_equal(pspec->name, "border-width") ||
        g_str_equal(pspec->name, "color") ||
        g_str_equal(pspec->name, "border-color")) {
        rectangle->corners_dirty = TRUE;
    }
}

static void
big_rectangle_set_property(GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    BigRectangle *rectangle;

    rectangle = BIG_RECTANGLE(object);

    switch (prop_id) {
    case PROP_CORNER_RADIUS:
        rectangle->radius = CLUTTER_UNITS_FROM_DEVICE(g_value_get_uint(value));
        rectangle->corners_dirty = TRUE;
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;

    }
}

static void
big_rectangle_get_property(GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
    BigRectangle *rectangle;

    rectangle = BIG_RECTANGLE(object);

    switch (prop_id) {
    case PROP_CORNER_RADIUS:
        g_value_set_uint(value, CLUTTER_UNITS_TO_DEVICE(rectangle->radius));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
big_rectangle_dispose(GObject *object)
{
    BigRectangle *rectangle;

    rectangle = BIG_RECTANGLE(object);

    if (rectangle->corner) {
        corner_unref(rectangle->corner);
        rectangle->corner = NULL;
    }

    if (rectangle->corner_material) {
        cogl_material_unref (rectangle->corner_material);
        rectangle->corner_material = NULL;
    }

    if (rectangle->background_material) {
        cogl_material_unref (rectangle->background_material);
        rectangle->background_material = NULL;
    }

    if (rectangle->border_material) {
        cogl_material_unref (rectangle->border_material);
        rectangle->border_material = NULL;
    }

    if (G_OBJECT_CLASS(big_rectangle_parent_class)->dispose)
        G_OBJECT_CLASS(big_rectangle_parent_class)->dispose(object);
}


static void
big_rectangle_class_init(BigRectangleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->dispose = big_rectangle_dispose;
  gobject_class->set_property = big_rectangle_set_property;
  gobject_class->get_property = big_rectangle_get_property;
  gobject_class->notify = big_rectangle_notify;

  actor_class->paint = big_rectangle_paint;

  g_object_class_install_property
                 (gobject_class,
                  PROP_CORNER_RADIUS,
                  g_param_spec_uint("corner-radius",
                                    "Corner radius",
                                    "Radius of the rectangle rounded corner",
                                    0, G_MAXUINT,
                                    0,
                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

}

static void
big_rectangle_init(BigRectangle *rectangle)
{
}
