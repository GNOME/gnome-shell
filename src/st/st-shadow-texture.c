/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#include <math.h>
#include <string.h>

#include "st-shadow-texture.h"

/**
 * SECTION: st-shadow-texture
 * @short_description: a class for creating soft shadow textures
 *
 * #StShadowTexture is a #ClutterTexture holding a soft shadow texture for
 * another #ClutterActor.
 * It is used to implement the box-shadow property in StWidget and should
 * not be used stand-alone.
 */

struct _StShadowTexture {
  ClutterTexture parent;

  CoglColor color;
  gdouble sigma;
  gdouble blur_radius;
};

struct _StShadowTextureClass {
  ClutterTextureClass parent_class;
};

G_DEFINE_TYPE (StShadowTexture, st_shadow_texture, CLUTTER_TYPE_TEXTURE);

static gdouble *
calculate_gaussian_kernel (gdouble sigma, guint n_values)
{
  gdouble *ret, sum;
  gdouble exp_divisor;
  gint half, i;

  g_return_val_if_fail ((int) sigma > 0, NULL);

  half = n_values / 2;

  ret = g_malloc (n_values * sizeof (gdouble));
  sum = 0.0;

  exp_divisor = 2 * sigma * sigma;

  /* n_values of 1D Gauss function */
  for (i = 0; i < n_values; i++)
    {
      ret[i] = exp (-(i - half) * (i - half) / exp_divisor);
      sum += ret[i];
    }

  /* normalize */
  for (i = 0; i < n_values; i++)
    ret[i] /= sum;

  return ret;
}

static void
st_shadow_texture_create_shadow (StShadowTexture *st,
                                 ClutterActor    *actor)
{
  CoglHandle  texture, material;
  guchar     *pixels_in, *pixels_out;
  gint        width_in, height_in, rowstride_in;
  gint        width_out, height_out, rowstride_out;

  g_return_if_fail (ST_IS_SHADOW_TEXTURE (st));

  /* Right now we only deal with actors of type ClutterTexture.
     It would be nice to extend this to generic actors with some
     clutter_texture_new_from_actor magic in the future */
  g_return_if_fail (CLUTTER_IS_TEXTURE (actor));

  texture = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (actor));
  if (texture == COGL_INVALID_HANDLE)
    return;

  width_in  = cogl_texture_get_width  (texture);
  height_in = cogl_texture_get_height (texture);
  rowstride_in = (width_in + 3) & ~3;

  pixels_in  = g_malloc0 (rowstride_in * height_in);

  cogl_texture_get_data (texture, COGL_PIXEL_FORMAT_A_8,
                         rowstride_in, pixels_in);
  cogl_texture_unref (texture);

  if ((guint) st->blur_radius == 0)
    {
      width_out  = width_in;
      height_out = height_in;
      rowstride_out = rowstride_in;
      pixels_out = g_memdup (pixels_in, rowstride_out * height_out);
    }
  else
    {
      gdouble *kernel;
      guchar  *line;
      gint     n_values, half;
      gint     x_in, y_in, x_out, y_out, i;

      n_values = (gint) 5 * st->sigma;
      half = n_values / 2;

      width_out  = width_in  + 2 * half;
      height_out = height_in + 2 * half;
      rowstride_out = (width_out + 3) & ~3;

      pixels_out = g_malloc0 (rowstride_out * height_out);
      line       = g_malloc0 (rowstride_out);

      kernel = calculate_gaussian_kernel (st->sigma, n_values);

      /* vertical blur */
      for (x_in = 0; x_in < width_in; x_in++)
        for (y_out = 0; y_out < height_out; y_out++)
          {
            guchar *pixel_in, *pixel_out;
            gint i0, i1;

            y_in = y_out - half;

            /* We read from the source at 'y = y_in + i - half'; clamp the
             * full i range [0, n_values) so that y is in [0, height_in).
             */
            i0 = MAX (half - y_in, 0);
            i1 = MIN (height_in + half - y_in, n_values);

            pixel_in  =  pixels_in + (y_in + i0 - half) * rowstride_in + x_in;
            pixel_out =  pixels_out + y_out * rowstride_out + (x_in + half);

            for (i = i0; i < i1; i++)
              {
                *pixel_out += *pixel_in * kernel[i];
                pixel_in += rowstride_in;
              }
          }

      /* horizontal blur */
      for (y_out = 0; y_out < height_out; y_out++)
        {
          memcpy (line, pixels_out + y_out * rowstride_out, rowstride_out);

          for (x_out = 0; x_out < width_out; x_out++)
            {
              gint i0, i1;
              guchar *pixel_out, *pixel_in;

              /* We read from the source at 'x = x_out + i - half'; clamp the
               * full i range [0, n_values) so that x is in [0, width_out).
               */
              i0 = MAX (half - x_out, 0);
              i1 = MIN (width_out + half - x_out, n_values);

              pixel_in  = line + x_out + i0 - half;
              pixel_out = pixels_out + rowstride_out * y_out + x_out;

              *pixel_out = 0;
              for (i = i0; i < i1; i++)
                {
                  *pixel_out += *pixel_in * kernel[i];
                  pixel_in++;
                }
            }
        }
      g_free (kernel);
      g_free (line);
    }

  material = cogl_material_new ();
  texture = cogl_texture_new_from_data (width_out,
                                        height_out,
                                        COGL_TEXTURE_NONE,
                                        COGL_PIXEL_FORMAT_A_8,
                                        COGL_PIXEL_FORMAT_A_8,
                                        rowstride_out,
                                        pixels_out);

  cogl_material_set_layer_combine_constant (material, 0, &st->color);
  cogl_material_set_layer (material, 0, texture);

  /* We ignore the material color, which encodes the overall opacity of the
   * actor, so setting an ancestor of the shadow to partially opaque won't
   * work. The easiest way to fix this would be to override paint(). */

  cogl_material_set_layer_combine (material, 0,
                                   "RGBA = MODULATE (CONSTANT, TEXTURE[A])",
                                   NULL);

  clutter_texture_set_cogl_material (CLUTTER_TEXTURE (st), material);

  cogl_texture_unref  (texture);
  cogl_material_unref (material);

  g_free (pixels_in);
  g_free (pixels_out);
}


/**
 * st_shadow_texture_adjust_allocation:
 * @shadow: a #StShadowTexture
 * @allocation: the original allocation of @shadow
 *
 * Adjust @allocation to account for size change caused by blurrimg
 */
void
st_shadow_texture_adjust_allocation (StShadowTexture *shadow,
                                     ClutterActorBox *allocation)
{
  g_return_if_fail (ST_IS_SHADOW_TEXTURE (shadow));
  g_return_if_fail (allocation != NULL);

  allocation->x1 -= shadow->blur_radius;
  allocation->y1 -= shadow->blur_radius;
  allocation->x2 += shadow->blur_radius;
  allocation->y2 += shadow->blur_radius;
}


/**
 * st_shadow_texture_new:
 * @actor: the original actor
 * @color: (allow-none): the shadow color
 * @blur: the shadow's blur radius
 *
 * Create a shadow texture for @actor. When %NULL is passed for @color, it
 * defaults to fully opaque black.
 *
 * Returns: a new #ClutterActor holding a shadow texture for @actor
 */
ClutterActor *
st_shadow_texture_new (ClutterActor *actor,
                       ClutterColor *color,
                       gdouble       blur)
{
  StShadowTexture *st = g_object_new (ST_TYPE_SHADOW_TEXTURE, NULL);

  if (color)
    {
      cogl_color_set_from_4ub (&st->color,
                               color->red,  color->green,
                               color->blue, color->alpha);
      cogl_color_premultiply (&st->color);
    }

  st->blur_radius = blur;
  /* we use an approximation of the sigma - blur radius relationship used
     in Firefox for doing SVG blurs; see
     http://mxr.mozilla.org/mozilla-central/source/gfx/thebes/src/gfxBlur.cpp#280
  */
  st->sigma = blur / 1.9;

  st_shadow_texture_create_shadow (st, actor);

  return CLUTTER_ACTOR (st);
}

static void
st_shadow_texture_init (StShadowTexture *st)
{
  st->sigma  = 0.0;
  st->blur_radius = 0.0;

  cogl_color_set_from_4ub (&st->color, 0x0, 0x0, 0x0, 0xff);
}

static void
st_shadow_texture_class_init (StShadowTextureClass *klass)
{
}
