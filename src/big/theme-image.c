/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* theme-image.c: Stretched image.

   Copyright (C) 2005-2008 Red Hat, Inc.
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

#include <glib.h>

#include <clutter/clutter.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

#include "theme-image.h"

typedef enum {
    BIG_THEME_IMAGE_UNSET,
    BIG_THEME_IMAGE_SVG,
    BIG_THEME_IMAGE_SURFACE

} BigThemeImageType;

struct BigThemeImage {
    ClutterCairoTexture parent_instance;

    guint border_top;
    guint border_bottom;
    guint border_left;
    guint border_right;

    BigThemeImageType type;

    union {
        RsvgHandle      *svg_handle;
        cairo_surface_t *surface;
    } u;

    guint render_idle;
    guint needs_render : 1;
};

struct BigThemeImageClass {
    ClutterCairoTextureClass parent_class;
};

G_DEFINE_TYPE(BigThemeImage, big_theme_image, CLUTTER_TYPE_CAIRO_TEXTURE)

enum
{
    PROP_0,

    PROP_BORDER_TOP,
    PROP_BORDER_BOTTOM,
    PROP_BORDER_LEFT,
    PROP_BORDER_RIGHT,

    PROP_FILENAME,
    PROP_PIXBUF
};

static void
big_theme_image_render(BigThemeImage *image)
{
    int source_width = 0;
    int source_height = 0;
    int source_x1 = 0, source_x2 = 0, source_y1 = 0, source_y2 = 0;
    int dest_x1 = 0, dest_x2 = 0, dest_y1 = 0, dest_y2 = 0;
    int i, j;
    int dest_width;
    int dest_height;
    ClutterGeometry geometry;
    cairo_t *cr;

    image->needs_render = FALSE;

    if (image->render_idle) {
        g_source_remove(image->render_idle);
        image->render_idle = 0;
    }


    /* To draw a theme image, we divide the source and destination into 9
     * pieces and draw each piece separately. (Some pieces may not exist
     * if we have 0-width borders, in which case they'll be skipped)
     *
     *                         i=0               i=1              i=2
     *                     border_left                        border_right
     *                    +------------+--------------------+--------------+
     * j=0: border_top    |            |                    |              |
     *                    +------------+--------------------+--------------+
     * j=1                |            |                    |              |
     *                    +------------+--------------------+--------------+
     * j=2: border_bottom |            |                    |              |
     *                    +------------+--------------------+--------------+
     *
     */

    switch (image->type) {
    case BIG_THEME_IMAGE_SURFACE:
        if (!image->u.surface)
            return;

        source_width = cairo_image_surface_get_width(image->u.surface);
        source_height = cairo_image_surface_get_height(image->u.surface);
        break;
    case BIG_THEME_IMAGE_SVG:
        {
            RsvgDimensionData dimensions;

            if (!image->u.svg_handle)
                return;

            rsvg_handle_get_dimensions(image->u.svg_handle, &dimensions);
            source_width = dimensions.width;
            source_height = dimensions.height;
            break;
        }
    default:
        return;
    }

    clutter_actor_get_allocation_geometry(CLUTTER_ACTOR(image), &geometry);

    dest_width = geometry.width;
    dest_height = geometry.height;

    cr = clutter_cairo_texture_create(CLUTTER_CAIRO_TEXTURE(image));

    for (j = 0; j < 3; j++) {
        switch (j) {
        case 0:
            source_y1 = 0;
            source_y2 = image->border_top;
            dest_y1 = 0;
            dest_y2 = image->border_top;
            break;
        case 1:
            source_y1 = image->border_top;
            source_y2 = source_height - image->border_bottom;
            dest_y1 = image->border_top;
            dest_y2 = dest_height - image->border_bottom;
            break;
        case 2:
            source_y1 = source_height - image->border_bottom;
            source_y2 = source_height;
            dest_y1 = dest_height - image->border_bottom;
            dest_y2 = dest_height;
            break;
        }

        if (dest_y2 <= dest_y1)
            continue;

        /* pixbuf-theme-engine has a nice interpretation of source_y2 == source_y1,
         * dest_y2 != dest_y1, which is to linearly interpolate between the surrounding
         * areas. We could do that for the surface case by setting
         *
         *   source_y1 == y - 0.5
         *   source_y2 == y + 0.5
         *
         * but it's hard for the SVG case. source_y2 < source_y1 is pathological ... someone
         * specified borders that sum up larger than the image.
         */
        if (source_y2 <= source_y1)
            continue;

        for (i = 0; i < 3; i++) {
            switch (i) {
            case 0:
                source_x1 = 0;
                source_x2 = image->border_left;
                dest_x1 = 0;
                dest_x2 = image->border_left;
                break;
            case 1:
                source_x1 = image->border_left;
                source_x2 = source_width - image->border_right;
                dest_x1 = image->border_left;
                dest_x2 = dest_width - image->border_right;
                break;
            case 2:
                source_x1 = source_width - image->border_right;
                source_x2 = source_width;
                dest_x1 = dest_width - image->border_right;
                dest_x2 = dest_width;
                break;
            }

            if (dest_x2 <= dest_x1)
                continue;

            if (source_x2 <= source_x1)
                continue;

            cairo_save(cr);

            cairo_rectangle(cr, dest_x1, dest_y1, dest_x2 - dest_x1, dest_y2 - dest_y1);
            cairo_clip(cr);

            cairo_translate(cr, dest_x1, dest_y1);
            cairo_scale(cr,
                        (double)(dest_x2 - dest_x1) / (source_x2 - source_x1),
                        (double)(dest_y2 - dest_y1) / (source_y2 - source_y1));

            switch (image->type) {
            case BIG_THEME_IMAGE_SURFACE:
                cairo_set_source_surface(cr, image->u.surface, - source_x1, - source_y1);
                cairo_paint(cr);
                break;
            case BIG_THEME_IMAGE_SVG:
                cairo_translate(cr, - source_x1, - source_y1);
                rsvg_handle_render_cairo(image->u.svg_handle, cr);
                break;
            default:
                break;
            }

            cairo_restore(cr);
        }
    }

    /* This will cause the surface content to be uploaded as
     * new texture content */
    cairo_destroy(cr);
}

static gboolean
big_theme_image_render_idle(gpointer data)
{
    BigThemeImage *image;

    image = data;
    big_theme_image_render(image);

    return FALSE;
}

static void
big_theme_image_queue_render(BigThemeImage *image)
{
    image->needs_render = TRUE;
    if (!image->render_idle)
        image->render_idle = g_idle_add(big_theme_image_render_idle,
                                        image);
}

static void
big_theme_image_paint(ClutterActor *actor)
{
    BigThemeImage *image;

    image = BIG_THEME_IMAGE(actor);

    if (image->needs_render)
        big_theme_image_render(image);

    if (CLUTTER_ACTOR_CLASS(big_theme_image_parent_class)->paint)
        CLUTTER_ACTOR_CLASS(big_theme_image_parent_class)->paint(actor);
}

static void
big_theme_image_allocate(ClutterActor          *actor,
                         const ClutterActorBox *box,
                         ClutterAllocationFlags flags)
{
    BigThemeImage *image;
    guint old_width;
    guint old_height;
    guint width;
    guint height;

    image = BIG_THEME_IMAGE(actor);

    width = ABS(box->x2 - box->x1);
    height = ABS(box->y2 - box->y1);

    g_object_get(actor,
                 "surface-width", &old_width,
                 "surface-height", &old_height,
                 NULL);

    if (width != old_width || height != old_height) {

        clutter_cairo_texture_set_surface_size(CLUTTER_CAIRO_TEXTURE(actor), width, height);

        big_theme_image_queue_render(image);
    }

    if (CLUTTER_ACTOR_CLASS(big_theme_image_parent_class))
        CLUTTER_ACTOR_CLASS(big_theme_image_parent_class)->allocate(actor,
                                                                    box,
                                                                    flags);
}

static void
big_theme_image_get_preferred_height(ClutterActor *actor,
                                     float         for_width,
                                     float        *min_height_p,
                                     float        *natural_height_p)
{
    BigThemeImage *image;

    image = BIG_THEME_IMAGE(actor);

    if (min_height_p)
        *min_height_p = 0;

    if (!natural_height_p)
        return;

    *natural_height_p = 0;

    switch (image->type) {
    case BIG_THEME_IMAGE_SURFACE:
        if (!image->u.surface)
            break;

        *natural_height_p = cairo_image_surface_get_height(image->u.surface);
        break;
    case BIG_THEME_IMAGE_SVG:
        {
            RsvgDimensionData dimensions;

            if (!image->u.svg_handle)
                return;

            rsvg_handle_get_dimensions(image->u.svg_handle, &dimensions);
            *natural_height_p = dimensions.height;
            break;
        }
    default:
        break;
    }
}

static void
big_theme_image_get_preferred_width(ClutterActor *actor,
                                    float         for_height,
                                    float        *min_width_p,
                                    float        *natural_width_p)
{
    BigThemeImage *image;

    image = BIG_THEME_IMAGE(actor);

    if (min_width_p)
        *min_width_p = 0;

    if (!natural_width_p)
        return;

    *natural_width_p = 0;

    switch (image->type) {
    case BIG_THEME_IMAGE_SURFACE:
        if (!image->u.surface)
            break;

        *natural_width_p = cairo_image_surface_get_width(image->u.surface);
        break;
    case BIG_THEME_IMAGE_SVG:
        {
            RsvgDimensionData dimensions;

            if (!image->u.svg_handle)
                return;

            rsvg_handle_get_dimensions(image->u.svg_handle, &dimensions);
            *natural_width_p = dimensions.width;
            break;
        }
    default:
        break;
    }

}

static void
big_theme_image_set_border_value(BigThemeImage *image, guint *old_value, const GValue *new_value)
{
    guint border_value;

    border_value = g_value_get_uint(new_value);

    if (*old_value != border_value) {
        *old_value = border_value;

        big_theme_image_queue_render(image);
    }
}

static void
big_theme_image_set_filename(BigThemeImage *image, const char *filename)
{
    if (!filename)
        return;

    if (g_str_has_suffix(filename, ".png") ||
        g_str_has_suffix(filename, ".PNG")) {

        image->type = BIG_THEME_IMAGE_SURFACE;

        image->u.surface = cairo_image_surface_create_from_png(filename);

        if (image->u.surface == NULL) {
            g_warning("Error when loading PNG from file %s", filename);
        }
    } else if (g_str_has_suffix(filename, ".svg") ||
               g_str_has_suffix(filename, ".SVG")) {

        GError *error;

        error = NULL;
        image->u.svg_handle = rsvg_handle_new_from_file(filename, &error);

        if (image->u.svg_handle == NULL) {
            g_warning("Error when loading SVG from file %s: %s", filename,
                      error?error->message:"Error not set by RSVG");
            if (error)
                g_error_free(error);

            return;
        }

        image->type = BIG_THEME_IMAGE_SVG;
    } else {
        g_warning("%s: Unsupported file type", filename);
        return;
    }

    big_theme_image_queue_render(image);
}

static cairo_surface_t *
create_surface_from_pixbuf(const GdkPixbuf *pixbuf)
{
    gint width = gdk_pixbuf_get_width (pixbuf);
    gint height = gdk_pixbuf_get_height (pixbuf);
    guchar *gdk_pixels = gdk_pixbuf_get_pixels (pixbuf);
    int gdk_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels (pixbuf);

    guchar *cairo_pixels;
    cairo_format_t format;
    cairo_surface_t *surface;
    static const cairo_user_data_key_t key;

    int j;

    if (n_channels == 3)
        format = CAIRO_FORMAT_RGB24;
    else
        format = CAIRO_FORMAT_ARGB32;

    cairo_pixels = g_malloc (4 * width * height);

    surface = cairo_image_surface_create_for_data((unsigned char *)cairo_pixels,
                                                   format,
                                                   width, height, 4 * width);

    cairo_surface_set_user_data(surface, &key,
                                cairo_pixels, (cairo_destroy_func_t)g_free);

    for (j = height; j; j--) {
        guchar *p = gdk_pixels;
        guchar *q = cairo_pixels;

        if (n_channels == 3) {
            guchar *end = p + 3 * width;

            while (p < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                q[0] = p[2];
                q[1] = p[1];
                q[2] = p[0];
#else
                q[1] = p[0];
                q[2] = p[1];
                q[3] = p[2];
#endif
                p += 3;
                q += 4;
            }
        } else {
            guchar *end = p + 4 * width;
            guint t1,t2,t3;

#define MULT(d,c,a,t) G_STMT_START { t = c * a + 0x7f; d = ((t >> 8) + t) >> 8; } G_STMT_END

            while (p < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                MULT(q[0], p[2], p[3], t1);
                MULT(q[1], p[1], p[3], t2);
                MULT(q[2], p[0], p[3], t3);
                q[3] = p[3];
#else
                q[0] = p[3];
                MULT(q[1], p[0], p[3], t1);
                MULT(q[2], p[1], p[3], t2);
                MULT(q[3], p[2], p[3], t3);
#endif

                p += 4;
                q += 4;
            }
#undef MULT
        }

        gdk_pixels += gdk_rowstride;
        cairo_pixels += 4 * width;
    }

    return surface;
}

static void
big_theme_image_set_pixbuf(BigThemeImage *image, GdkPixbuf *pixbuf)
{
    if (!pixbuf)
        return;

    image->type = BIG_THEME_IMAGE_SURFACE;

    image->u.surface = create_surface_from_pixbuf(pixbuf);

    g_assert(image->u.surface != NULL);

    big_theme_image_queue_render(image);
}

static void
big_theme_image_set_property(GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
    BigThemeImage *image;

    image = BIG_THEME_IMAGE(object);

    switch (prop_id) {
    case PROP_BORDER_TOP:
        big_theme_image_set_border_value(image, &image->border_top, value);
        break;

    case PROP_BORDER_BOTTOM:
        big_theme_image_set_border_value(image, &image->border_bottom, value);
        break;

    case PROP_BORDER_LEFT:
        big_theme_image_set_border_value(image, &image->border_left, value);
        break;

    case PROP_BORDER_RIGHT:
        big_theme_image_set_border_value(image, &image->border_right, value);
        break;

    case PROP_FILENAME:
        big_theme_image_set_filename(image, g_value_get_string(value));
        break;

    case PROP_PIXBUF:
        big_theme_image_set_pixbuf(image, g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;

    }
}

static void
big_theme_image_get_property(GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
    BigThemeImage *image;

    image = BIG_THEME_IMAGE(object);

    switch (prop_id) {
    case PROP_BORDER_TOP:
        g_value_set_uint(value, image->border_top);
        break;

    case PROP_BORDER_BOTTOM:
        g_value_set_uint(value, image->border_bottom);
        break;

    case PROP_BORDER_LEFT:
        g_value_set_uint(value, image->border_left);
        break;

    case PROP_BORDER_RIGHT:
        g_value_set_uint(value, image->border_right);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
big_theme_image_dispose(GObject *object)
{
    BigThemeImage *image;

    image = BIG_THEME_IMAGE(object);

    if (image->render_idle) {
        g_source_remove(image->render_idle);
        image->render_idle = 0;
    }

    switch (image->type) {
    case BIG_THEME_IMAGE_SVG:
        if (image->u.svg_handle) {
            g_object_unref(image->u.svg_handle);
            image->u.svg_handle = NULL;
        }
        break;
    case BIG_THEME_IMAGE_SURFACE:
        if (image->u.surface) {
            cairo_surface_destroy(image->u.surface);
            image->u.surface = NULL;
        }
        break;
    default:
        break;
    }

    if (G_OBJECT_CLASS(big_theme_image_parent_class)->dispose)
        G_OBJECT_CLASS(big_theme_image_parent_class)->dispose(object);

}


static void
big_theme_image_class_init(BigThemeImageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->dispose = big_theme_image_dispose;
  gobject_class->set_property = big_theme_image_set_property;
  gobject_class->get_property = big_theme_image_get_property;

  actor_class->allocate = big_theme_image_allocate;
  actor_class->get_preferred_width = big_theme_image_get_preferred_width;
  actor_class->get_preferred_height = big_theme_image_get_preferred_height;
  actor_class->paint = big_theme_image_paint;

  g_object_class_install_property
                 (gobject_class,
                  PROP_BORDER_TOP,
                  g_param_spec_uint("border-top",
                                    "Border top",
                                    "Top dimension of the image border "
                                    "(none-scaled part)",
                                    0, G_MAXUINT,
                                    0,
                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property
                 (gobject_class,
                  PROP_BORDER_BOTTOM,
                  g_param_spec_uint("border-bottom",
                                    "Border bottom",
                                    "Bottom dimension of the image border "
                                    "(none-scaled part)",
                                    0, G_MAXUINT,
                                    0,
                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property
                 (gobject_class,
                  PROP_BORDER_LEFT,
                  g_param_spec_uint("border-left",
                                    "Border left",
                                    "Left dimension of the image border "
                                    "(none-scaled part)",
                                    0, G_MAXUINT,
                                    0,
                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property
                 (gobject_class,
                  PROP_BORDER_RIGHT,
                  g_param_spec_uint("border-right",
                                    "Border right",
                                    "Right dimension of the image border "
                                    "(none-scaled part)",
                                    0, G_MAXUINT,
                                    0,
                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property
                 (gobject_class,
                  PROP_FILENAME,
                  g_param_spec_string("filename",
                                      "Filename",
                                      "Name of the file",
                                      NULL,
                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property
                 (gobject_class,
                  PROP_PIXBUF,
                  g_param_spec_object("pixbuf",
                                      "Pixbuf",
                                      "Pixbuf of the image",
                                      GDK_TYPE_PIXBUF,
                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
}

static void
big_theme_image_init(BigThemeImage *image)
{
}

ClutterActor *
big_theme_image_new_from_file(const char *filename,
                              guint       border_top,
                              guint       border_bottom,
                              guint       border_left,
                              guint       border_right)
{
    ClutterActor *actor;

    actor = g_object_new(BIG_TYPE_THEME_IMAGE,
                         /* FIXME ClutterCairo requires creating a bogus
                          * surface with nonzero size
                          */
                         "surface-width", 1,
                         "surface-height", 1,
                         "filename", filename,
                         "border-top", border_top,
                         "border-bottom", border_bottom,
                         "border-left", border_left,
                         "border-right", border_right,
                         NULL);

    return actor;
}

ClutterActor *
big_theme_image_new_from_pixbuf(GdkPixbuf  *pixbuf,
                                guint       border_top,
                                guint       border_bottom,
                                guint       border_left,
                                guint       border_right)
{
    ClutterActor *actor;

    actor = g_object_new(BIG_TYPE_THEME_IMAGE,
                         /* FIXME ClutterCairo requires creating a bogus
                          * surface with nonzero size
                          */
                         "surface-width", 1,
                         "surface-height", 1,
                         "pixbuf", pixbuf,
                         "border-top", border_top,
                         "border-bottom", border_bottom,
                         "border-left", border_left,
                         "border-right", border_right,
                         NULL);

    return actor;
}

