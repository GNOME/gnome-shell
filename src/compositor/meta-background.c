/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * meta-background.c: CoglTexture for painting the system background
 *
 * Copyright 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <cogl/cogl-texture-pixmap-x11.h>

#include <clutter/clutter.h>

#include "cogl-utils.h"
#include "compositor-private.h"
#include "mutter-enum-types.h"
#include <meta/errors.h>
#include <meta/meta-background.h>
#include "meta-background-actor-private.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define TEXTURE_FORMAT COGL_PIXEL_FORMAT_BGRA_8888_PRE
#else
#define TEXTURE_FORMAT COGL_PIXEL_FORMAT_ARGB_8888_PRE
#endif

#define TEXTURE_LOOKUP_SHADER_DECLARATIONS                                     \
"uniform vec2 pixel_step;\n"                                                   \
"vec4 apply_blur(in sampler2D texture, in vec2 coordinates) {\n"               \
" vec4 texel;\n"                                                               \
" texel  = texture2D(texture, coordinates.st);\n"                                \
" texel += texture2D(texture, coordinates.st + pixel_step * vec2(-1.0, -1.0));\n"\
" texel += texture2D(texture, coordinates.st + pixel_step * vec2( 0.0, -1.0));\n"\
" texel += texture2D(texture, coordinates.st + pixel_step * vec2(+1.0, -1.0));\n"\
" texel += texture2D(texture, coordinates.st + pixel_step * vec2(-1.0,  0.0));\n"\
" texel += texture2D(texture, coordinates.st + pixel_step * vec2(+1.0,  0.0));\n"\
" texel += texture2D(texture, coordinates.st + pixel_step * vec2(-1.0, +1.0));\n"\
" texel += texture2D(texture, coordinates.st + pixel_step * vec2( 0.0, +1.0));\n"\
" texel += texture2D(texture, coordinates.st + pixel_step * vec2(+1.0, +1.0));\n"\
" texel /= 9.0;\n"                                                             \
" return texel;\n"                                                             \
"}\n"                                                                          \
"uniform float saturation;\n"                                                  \
"vec3 desaturate(const vec3 color)\n"                                          \
"{\n"                                                                          \
"   const vec3 gray_conv = vec3(0.299, 0.587, 0.114);\n"                       \
"   vec3 gray = vec3(dot(gray_conv, color));\n"                                \
"   return vec3(mix(color.rgb, gray, 1.0 - saturation));\n"                    \
"}\n"                                                                          \

#define DESATURATE_CODE                                                        \
"cogl_texel.rgb = desaturate(cogl_texel.rgb);\n"

#define BLUR_CODE                                                              \
"cogl_texel = apply_blur(cogl_sampler, cogl_tex_coord.st);\n"

#define FRAGMENT_SHADER_DECLARATIONS                                           \
"uniform float brightness;\n"                                                  \
"uniform float vignette_sharpness;\n"                                          \

#define VIGNETTE_CODE                                                          \
"float unit_length = 0.5;\n"                                                   \
"vec2 center = vec2(unit_length, unit_length);\n"                              \
"vec2 position = cogl_tex_coord_in[0].xy - center;\n"                          \
"float t = min(length(position), unit_length) / unit_length;\n"                \
"float pixel_brightness = mix(1.0, 1.0 - vignette_sharpness, t);\n"            \
"cogl_color_out.rgb = cogl_color_out.rgb * pixel_brightness * brightness;\n"

/* We allow creating multiple MetaBackgrounds for the same monitor to
 * allow different rendering options to be set for different copies.
 * But we want to share the same underlying CoglTextures for efficiency and
 * to avoid driver bugs that might occur if we created multiple CoglTexturePixmaps
 * for the same pixmap.
 *
 * This object provides a ClutterContent object to assist in sharing between actors.
 */
typedef struct _MetaBackgroundPrivate MetaBackgroundPrivate;

struct _MetaBackgroundPrivate
{
  MetaScreen   *screen;
  CoglTexture  *texture;
  CoglPipeline *pipeline;
  int           monitor;

  MetaBackgroundEffects effects;

  GDesktopBackgroundStyle   style;
  GDesktopBackgroundShading shading_direction;
  ClutterColor              color;
  ClutterColor              second_color;

  char  *filename;

  float brightness;
  float vignette_sharpness;
  float saturation;
};

enum
{
  PROP_META_SCREEN = 1,
  PROP_MONITOR,
  PROP_EFFECTS,
  PROP_BRIGHTNESS,
  PROP_VIGNETTE_SHARPNESS,
  PROP_SATURATION
};

static void clutter_content_iface_init (ClutterContentIface *iface);
static void unset_texture (MetaBackground *self);

G_DEFINE_TYPE_WITH_CODE (MetaBackground, meta_background, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTENT,
                                                clutter_content_iface_init))


static gboolean
meta_background_get_preferred_size (ClutterContent *content,
                                    gfloat         *width,
                                    gfloat         *height)
{
  MetaBackgroundPrivate *priv = META_BACKGROUND (content)->priv;
  MetaRectangle monitor_geometry;

  if (priv->texture == NULL)
    return FALSE;

  meta_screen_get_monitor_geometry (priv->screen, priv->monitor, &monitor_geometry);

  if (width != NULL)
    *width = monitor_geometry.width;

  if (height != NULL)
    *height = monitor_geometry.height;

  return TRUE;
}

static void
get_texture_area_and_scale (MetaBackground        *self,
                            ClutterActorBox       *actor_box,
                            cairo_rectangle_int_t *texture_area,
                            float                 *texture_x_scale,
                            float                 *texture_y_scale)
{
  MetaBackgroundPrivate *priv = self->priv;
  MetaRectangle monitor_geometry;
  cairo_rectangle_int_t actor_pixel_rect;
  cairo_rectangle_int_t image_area;
  int screen_width, screen_height;
  float texture_width, texture_height;
  float actor_x_scale, actor_y_scale;
  float monitor_x_scale, monitor_y_scale;

  meta_screen_get_monitor_geometry (priv->screen, priv->monitor, &monitor_geometry);

  actor_pixel_rect.x = actor_box->x1;
  actor_pixel_rect.y = actor_box->y1;
  actor_pixel_rect.width = actor_box->x2 - actor_box->x1;
  actor_pixel_rect.height = actor_box->y2 - actor_box->y1;

  texture_width = cogl_texture_get_width (priv->texture);
  actor_x_scale = (1.0 * actor_pixel_rect.width / monitor_geometry.width);

  texture_height = cogl_texture_get_height (priv->texture);
  actor_y_scale = (1.0 * actor_pixel_rect.height / monitor_geometry.height);

  switch (priv->style)
    {
      case G_DESKTOP_BACKGROUND_STYLE_STRETCHED:
      default:
          /* paint region is whole actor, and the texture
           * is scaled disproportionately to fit the actor
           */
          *texture_area = actor_pixel_rect;
          *texture_x_scale = 1.0 / actor_pixel_rect.width;
          *texture_y_scale = 1.0 / actor_pixel_rect.height;
          break;
      case G_DESKTOP_BACKGROUND_STYLE_WALLPAPER:
          /* paint region is whole actor, and the texture
           * is left unscaled
           */
          image_area = actor_pixel_rect;
          *texture_x_scale = 1.0 / texture_width;
          *texture_y_scale = 1.0 / texture_height;

          *texture_area = image_area;
          break;
      case G_DESKTOP_BACKGROUND_STYLE_CENTERED:
          /* paint region is the original image size centered in the actor,
           * and the texture is scaled to the original image size */
          image_area.width = texture_width;
          image_area.height = texture_height;
          image_area.x = actor_pixel_rect.x + actor_pixel_rect.width / 2 - image_area.width / 2;
          image_area.y = actor_pixel_rect.y + actor_pixel_rect.height / 2 - image_area.height / 2;

          *texture_area = image_area;
          *texture_x_scale = 1.0 / texture_width;
          *texture_y_scale = 1.0 / texture_height;
          break;
      case G_DESKTOP_BACKGROUND_STYLE_SCALED:
      case G_DESKTOP_BACKGROUND_STYLE_ZOOM:
          /* paint region is the actor size in one dimension, and centered and
           * scaled by proportional amount in the other dimension.
           *
           * SCALED forces the centered dimension to fit on screen.
           * ZOOM forces the centered dimension to grow off screen
           */
          monitor_x_scale = monitor_geometry.width / texture_width;
          monitor_y_scale = monitor_geometry.height / texture_height;

          if ((priv->style == G_DESKTOP_BACKGROUND_STYLE_SCALED &&
                (monitor_x_scale < monitor_y_scale)) ||
              (priv->style == G_DESKTOP_BACKGROUND_STYLE_ZOOM &&
                (monitor_x_scale > monitor_y_scale)))
            {
              /* Fill image to exactly fit actor horizontally */
              image_area.width = actor_pixel_rect.width;
              image_area.height = texture_height * monitor_x_scale * actor_y_scale;

              /* Position image centered vertically in actor */
              image_area.x = actor_pixel_rect.x;
              image_area.y = actor_pixel_rect.y + actor_pixel_rect.height / 2 - image_area.height / 2;
            }
          else
            {
              /* Scale image to exactly fit actor vertically */
              image_area.width = texture_width * monitor_y_scale * actor_x_scale;
              image_area.height = actor_pixel_rect.height;

              /* Position image centered horizontally in actor */
              image_area.x = actor_pixel_rect.x + actor_pixel_rect.width / 2 - image_area.width / 2;
              image_area.y = actor_pixel_rect.y;
            }

          *texture_area = image_area;
          *texture_x_scale = 1.0 / image_area.width;
          *texture_y_scale = 1.0 / image_area.height;
          break;

      case G_DESKTOP_BACKGROUND_STYLE_SPANNED:
        {
          /* paint region is the union of all monitors, with the origin
           * of the region set to align with monitor associated with the background.
           */
          meta_screen_get_size (priv->screen, &screen_width, &screen_height);

          /* unclipped texture area is whole screen */
          image_area.width = screen_width * actor_x_scale;
          image_area.height = screen_height * actor_y_scale;

          /* But make (0,0) line up with the appropriate monitor */
          image_area.x = -monitor_geometry.x * actor_x_scale;
          image_area.y = -monitor_geometry.y * actor_y_scale;

          *texture_area = image_area;
          *texture_x_scale = 1.0 / image_area.width;
          *texture_y_scale = 1.0 / image_area.height;
          break;
        }
    }
}

static CoglPipelineWrapMode
get_wrap_mode (MetaBackground *self)
{
  MetaBackgroundPrivate *priv = self->priv;
  switch (priv->style)
    {
      case G_DESKTOP_BACKGROUND_STYLE_WALLPAPER:
          return COGL_PIPELINE_WRAP_MODE_REPEAT;
      case G_DESKTOP_BACKGROUND_STYLE_NONE:
      case G_DESKTOP_BACKGROUND_STYLE_STRETCHED:
      case G_DESKTOP_BACKGROUND_STYLE_CENTERED:
      case G_DESKTOP_BACKGROUND_STYLE_SCALED:
      case G_DESKTOP_BACKGROUND_STYLE_ZOOM:
      case G_DESKTOP_BACKGROUND_STYLE_SPANNED:
      default:
          return COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;
    }
}

static ClutterPaintNode *
meta_background_paint_node_new (MetaBackground *self,
                                ClutterActor   *actor)
{
  MetaBackgroundPrivate *priv = self->priv;
  ClutterPaintNode *node;
  guint8 opacity;
  guint8 color_component;

  opacity = clutter_actor_get_paint_opacity (actor);
  color_component = (guint8) (0.5 + opacity * priv->brightness);

  cogl_pipeline_set_color4ub (priv->pipeline,
                              color_component,
                              color_component,
                              color_component,
                              opacity);

  node = clutter_pipeline_node_new (priv->pipeline);

  return node;
}

static void
clip_region_to_actor_box (cairo_region_t  *region,
                          ClutterActorBox *actor_box)
{
  cairo_rectangle_int_t clip_rect;

  clip_rect.x = actor_box->x1;
  clip_rect.y = actor_box->y1;
  clip_rect.width = actor_box->x2 - actor_box->x1;
  clip_rect.height = actor_box->y2 - actor_box->y1;

  cairo_region_intersect_rectangle (region, &clip_rect);
}

static void
set_blur_parameters (MetaBackground  *self,
                     ClutterActorBox *actor_box)
{
  MetaBackgroundPrivate *priv = self->priv;
  float pixel_step[2];

  if (!(priv->effects & META_BACKGROUND_EFFECTS_BLUR))
    return;

  pixel_step[0] = 1.0 / (actor_box->x2 - actor_box->x1);
  pixel_step[1] = 1.0 / (actor_box->y2 - actor_box->y1);

  cogl_pipeline_set_uniform_float (priv->pipeline,
                                   cogl_pipeline_get_uniform_location (priv->pipeline,
                                                                       "pixel_step"),
                                   2, 1, pixel_step);
}

static void
meta_background_paint_content (ClutterContent   *content,
                               ClutterActor     *actor,
                               ClutterPaintNode *root)
{
  MetaBackground *self = META_BACKGROUND (content);
  MetaBackgroundPrivate *priv = self->priv;
  ClutterPaintNode *node;
  ClutterActorBox actor_box;
  cairo_rectangle_int_t texture_area;
  cairo_region_t *paintable_region = NULL;
  int n_texture_subareas;
  int i;
  float texture_x_scale, texture_y_scale;
  float tx1 = 0.0, ty1 = 0.0, tx2 = 1.0, ty2 = 1.0;

  if (priv->texture == NULL)
    return;

  node = meta_background_paint_node_new (self, actor);

  clutter_actor_get_content_box (actor, &actor_box);

  set_blur_parameters (self, &actor_box);

  /* First figure out where on the monitor the texture is supposed to be painted.
   * If the actor is not the size of the monitor, this function makes sure to scale
   * everything down to fit in the actor.
   */
  get_texture_area_and_scale (self,
                              &actor_box,
                              &texture_area,
                              &texture_x_scale,
                              &texture_y_scale);

  /* Now figure out what to actually paint. We start by clipping the texture area to
   * the actor's bounds.
   */
  paintable_region = cairo_region_create_rectangle (&texture_area);

  clip_region_to_actor_box (paintable_region, &actor_box);

  /* And then cut out any parts occluded by window actors
   */
  if (META_IS_BACKGROUND_ACTOR (actor))
    {
      cairo_region_t *visible_region;
      visible_region = meta_background_actor_get_visible_region (META_BACKGROUND_ACTOR (actor));

      if (visible_region != NULL)
        {
          cairo_region_intersect (paintable_region, visible_region);
          cairo_region_destroy (visible_region);
        }
    }

  /* Finally, split the paintable region up into distinct areas
   * and paint each area one by one
   */
  n_texture_subareas = cairo_region_num_rectangles (paintable_region);
  for (i = 0; i < n_texture_subareas; i++)
    {
      cairo_rectangle_int_t texture_subarea;
      ClutterActorBox texture_rectangle;

      cairo_region_get_rectangle (paintable_region, i, &texture_subarea);

      tx1 = (texture_subarea.x - texture_area.x) * texture_x_scale;
      ty1 = (texture_subarea.y - texture_area.y) * texture_y_scale;
      tx2 = (texture_subarea.x + texture_subarea.width - texture_area.x) * texture_x_scale;
      ty2 = (texture_subarea.y + texture_subarea.height - texture_area.y) * texture_y_scale;
      texture_rectangle.x1 = texture_subarea.x;
      texture_rectangle.y1 = texture_subarea.y;
      texture_rectangle.x2 = texture_subarea.x + texture_subarea.width;
      texture_rectangle.y2 = texture_subarea.y + texture_subarea.height;

      clutter_paint_node_add_texture_rectangle (node, &texture_rectangle, tx1, ty1, tx2, ty2);
    }
  cairo_region_destroy (paintable_region);

  clutter_paint_node_add_child (root, node);
  clutter_paint_node_unref (node);
}

static void
clutter_content_iface_init (ClutterContentIface *iface)
{
  iface->get_preferred_size = meta_background_get_preferred_size;
  iface->paint_content = meta_background_paint_content;
}

static void
meta_background_dispose (GObject *object)
{
  MetaBackground        *self = META_BACKGROUND (object);
  MetaBackgroundPrivate *priv = self->priv;

  unset_texture (self);

  g_clear_pointer (&priv->pipeline,
                   (GDestroyNotify)
                   cogl_object_unref);

  G_OBJECT_CLASS (meta_background_parent_class)->dispose (object);
}

static void
ensure_pipeline (MetaBackground *self)
{
  if (self->priv->pipeline == NULL)
    self->priv->pipeline = COGL_PIPELINE (meta_create_texture_pipeline (NULL));
}

static void
set_brightness (MetaBackground *self,
                gfloat          brightness)
{
  MetaBackgroundPrivate *priv = self->priv;

  if (priv->brightness == brightness)
    return;

  priv->brightness = brightness;

  if (priv->effects & META_BACKGROUND_EFFECTS_VIGNETTE)
    {
      ensure_pipeline (self);
      cogl_pipeline_set_uniform_1f (priv->pipeline,
                                    cogl_pipeline_get_uniform_location (priv->pipeline,
                                                                        "brightness"),
                                    priv->brightness);
    }

  clutter_content_invalidate (CLUTTER_CONTENT (self));

  g_object_notify (G_OBJECT (self), "brightness");
}

static void
set_vignette_sharpness (MetaBackground *self,
                        gfloat          sharpness)
{
  MetaBackgroundPrivate *priv = self->priv;

  if (priv->vignette_sharpness == sharpness)
    return;

  priv->vignette_sharpness = sharpness;

  if (priv->effects & META_BACKGROUND_EFFECTS_VIGNETTE)
    {
      ensure_pipeline (self);
      cogl_pipeline_set_uniform_1f (priv->pipeline,
                                    cogl_pipeline_get_uniform_location (priv->pipeline,
                                                                        "vignette_sharpness"),
                                    priv->vignette_sharpness);
    }

  clutter_content_invalidate (CLUTTER_CONTENT (self));

  g_object_notify (G_OBJECT (self), "vignette-sharpness");
}

static void
set_saturation (MetaBackground *self,
                gfloat          saturation)
{
  MetaBackgroundPrivate *priv = self->priv;

  if (priv->saturation == saturation)
    return;

  priv->saturation = saturation;

  ensure_pipeline (self);

  cogl_pipeline_set_uniform_1f (priv->pipeline,
				cogl_pipeline_get_uniform_location (priv->pipeline,
								    "saturation"),
				priv->saturation);


  clutter_content_invalidate (CLUTTER_CONTENT (self));

  g_object_notify (G_OBJECT (self), "saturation");
}

static void
add_texture_lookup_shader (MetaBackground *self)
{
  MetaBackgroundPrivate *priv = self->priv;
  CoglSnippet *snippet;
  const char *code;

  ensure_pipeline (self);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              TEXTURE_LOOKUP_SHADER_DECLARATIONS,
                              NULL);
  if ((priv->effects & META_BACKGROUND_EFFECTS_BLUR) &&
      (priv->effects & META_BACKGROUND_EFFECTS_DESATURATE))
    code = BLUR_CODE "\n" DESATURATE_CODE;
  else if (priv->effects & META_BACKGROUND_EFFECTS_BLUR)
    code = BLUR_CODE;
  else if (priv->effects & META_BACKGROUND_EFFECTS_DESATURATE)
    code = DESATURATE_CODE;

  cogl_snippet_set_replace (snippet, code);
  cogl_pipeline_add_layer_snippet (priv->pipeline, 0, snippet);
  cogl_object_unref (snippet);

  if (priv->effects & META_BACKGROUND_EFFECTS_DESATURATE)
      cogl_pipeline_set_uniform_1f (priv->pipeline,
                                    cogl_pipeline_get_uniform_location (priv->pipeline,
                                                                        "saturation"),
                                    priv->saturation);
}

static void
add_vignette (MetaBackground *self)
{
  MetaBackgroundPrivate *priv = self->priv;
  CoglSnippet *snippet;

  ensure_pipeline (self);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT, FRAGMENT_SHADER_DECLARATIONS, VIGNETTE_CODE);
  cogl_pipeline_add_snippet (priv->pipeline, snippet);
  cogl_object_unref (snippet);

  cogl_pipeline_set_uniform_1f (priv->pipeline,
                                cogl_pipeline_get_uniform_location (priv->pipeline,
                                                                    "brightness"),
                                priv->brightness);

  cogl_pipeline_set_uniform_1f (priv->pipeline,
                                cogl_pipeline_get_uniform_location (priv->pipeline,
                                                                    "vignette_sharpness"),
                                priv->vignette_sharpness);
}

static void
set_effects (MetaBackground        *self,
             MetaBackgroundEffects  effects)
{
  MetaBackgroundPrivate *priv = self->priv;

  priv->effects = effects;

  if ((priv->effects & META_BACKGROUND_EFFECTS_BLUR) ||
      (priv->effects & META_BACKGROUND_EFFECTS_DESATURATE))
    add_texture_lookup_shader (self);

  if ((priv->effects & META_BACKGROUND_EFFECTS_VIGNETTE))
    add_vignette (self);

  clutter_content_invalidate (CLUTTER_CONTENT (self));
}

static void
meta_background_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  MetaBackground        *self = META_BACKGROUND (object);
  MetaBackgroundPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_META_SCREEN:
      priv->screen = g_value_get_object (value);
      break;
    case PROP_MONITOR:
      priv->monitor = g_value_get_int (value);
      break;
    case PROP_EFFECTS:
      set_effects (self, g_value_get_flags (value));
      break;
    case PROP_BRIGHTNESS:
      set_brightness (self, g_value_get_float (value));
      break;
    case PROP_VIGNETTE_SHARPNESS:
      set_vignette_sharpness (self, g_value_get_float (value));
      break;
    case PROP_SATURATION:
      set_saturation (self, g_value_get_float (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_background_get_property (GObject      *object,
                              guint         prop_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  MetaBackgroundPrivate *priv = META_BACKGROUND (object)->priv;

  switch (prop_id)
    {
    case PROP_META_SCREEN:
      g_value_set_object (value, priv->screen);
      break;
    case PROP_MONITOR:
      g_value_set_int (value, priv->monitor);
      break;
    case PROP_EFFECTS:
      g_value_set_flags (value, priv->effects);
      break;
    case PROP_BRIGHTNESS:
      g_value_set_float (value, priv->brightness);
      break;
    case PROP_VIGNETTE_SHARPNESS:
      g_value_set_float (value, priv->vignette_sharpness);
      break;
    case PROP_SATURATION:
      g_value_set_float (value, priv->saturation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_background_class_init (MetaBackgroundClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *param_spec;

  g_type_class_add_private (klass, sizeof (MetaBackgroundPrivate));

  object_class->dispose = meta_background_dispose;
  object_class->set_property = meta_background_set_property;
  object_class->get_property = meta_background_get_property;

  param_spec = g_param_spec_object ("meta-screen",
                                    "MetaScreen",
                                    "MetaScreen",
                                    META_TYPE_SCREEN,
                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_META_SCREEN,
                                   param_spec);

  param_spec = g_param_spec_int ("monitor",
                                 "monitor",
                                 "monitor",
                                 0, G_MAXINT, 0,
                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_MONITOR,
                                   param_spec);

  param_spec = g_param_spec_float ("brightness",
                                   "brightness",
                                   "Values less than 1.0 dim background",
                                   0.0, 1.0,
                                   1.0,
                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_BRIGHTNESS, param_spec);

  param_spec = g_param_spec_float ("vignette-sharpness",
                                   "vignette-sharpness",
                                   "How obvious the vignette fringe is",
                                   0.0, 1.0,
                                   0.7,
                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_VIGNETTE_SHARPNESS, param_spec);

  param_spec = g_param_spec_float ("saturation",
                                   "saturation",
                                   "Values less than 1.0 grays background",
                                   0.0, 1.0,
                                   1.0,
                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_SATURATION, param_spec);

  param_spec = g_param_spec_flags ("effects",
                                   "Effects",
                                   "Set to alter saturation, to blur, etc",
				   meta_background_effects_get_type (),
                                   META_BACKGROUND_EFFECTS_NONE,
                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_EFFECTS, param_spec);
}

static void
meta_background_init (MetaBackground *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            META_TYPE_BACKGROUND,
                                            MetaBackgroundPrivate);
}

static void
unset_texture (MetaBackground *self)
{
  MetaBackgroundPrivate *priv = self->priv;
  cogl_pipeline_set_layer_texture (priv->pipeline, 0, NULL);

  g_clear_pointer (&priv->texture,
                   (GDestroyNotify)
                   cogl_object_unref);
}

static void
set_texture (MetaBackground *self,
             CoglTexture    *texture)
{
  MetaBackgroundPrivate *priv = self->priv;

  priv->texture = texture;
  cogl_pipeline_set_layer_texture (priv->pipeline, 0, priv->texture);
}

static void
set_style (MetaBackground          *self,
           GDesktopBackgroundStyle  style)
{
  MetaBackgroundPrivate *priv = self->priv;
  CoglPipelineWrapMode   wrap_mode;

  priv->style = style;

  wrap_mode = get_wrap_mode (self);
  cogl_pipeline_set_layer_wrap_mode (priv->pipeline, 0, wrap_mode);
}

static void
set_filename (MetaBackground *self,
              const char     *filename)
{
  MetaBackgroundPrivate *priv = self->priv;

  g_free (priv->filename);
  priv->filename = g_strdup (filename);
}

static Pixmap
get_still_frame_for_monitor (MetaScreen *screen,
                             int         monitor)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  Window xroot = meta_screen_get_xroot (screen);
  Pixmap pixmap;
  GC gc;
  XGCValues values;
  MetaRectangle geometry;
  int depth;

  meta_screen_get_monitor_geometry (screen, monitor, &geometry);

  depth = DefaultDepth (xdisplay, meta_screen_get_screen_number (screen));

  pixmap = XCreatePixmap (xdisplay,
                          xroot,
                          geometry.width, geometry.height, depth);

  values.function = GXcopy;
  values.plane_mask = AllPlanes;
  values.fill_style = FillSolid;
  values.subwindow_mode = IncludeInferiors;

  gc = XCreateGC (xdisplay,
                  xroot,
                  GCFunction | GCPlaneMask | GCFillStyle | GCSubwindowMode,
                  &values);

  XCopyArea (xdisplay,
             xroot, pixmap, gc,
             geometry.x, geometry.y,
             geometry.width, geometry.height,
             0, 0);

  XFreeGC (xdisplay, gc);

  return pixmap;
}

/**
 * meta_background_load_still_frame:
 * @self: the #MetaBackground
 *
 * Takes a screenshot of the desktop and uses it as the background
 * source.
 */
void
meta_background_load_still_frame (MetaBackground *self)
{
  MetaBackgroundPrivate *priv = self->priv;
  MetaDisplay *display = meta_screen_get_display (priv->screen);
  Pixmap still_frame;
  CoglTexture *texture;
  CoglContext *context = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  GError *error = NULL;

  ensure_pipeline (self);

  unset_texture (self);
  set_style (self, G_DESKTOP_BACKGROUND_STYLE_STRETCHED);

  still_frame = get_still_frame_for_monitor (priv->screen, priv->monitor);
  XSync (meta_display_get_xdisplay (display), False);

  meta_error_trap_push (display);
  texture = COGL_TEXTURE (cogl_texture_pixmap_x11_new (context, still_frame, FALSE, &error));
  meta_error_trap_pop (display);

  if (error != NULL)
    {
      g_warning ("Failed to create background texture from pixmap: %s",
                 error->message);
      g_error_free (error);
      return;
    }

  set_texture (self, texture);
}

/**
 * meta_background_load_gradient:
 * @self: the #MetaBackground
 * @shading_direction: the orientation of the gradient
 * @color: the start color of the gradient
 * @second_color: the end color of the gradient
 *
 * Clears any previously set background, and sets the background gradient.
 * The gradient starts with @color and
 * progresses toward @second_color in the direction of @shading_direction.
 */
void
meta_background_load_gradient (MetaBackground             *self,
                               GDesktopBackgroundShading   shading_direction,
                               ClutterColor               *color,
                               ClutterColor               *second_color)
{
  MetaBackgroundPrivate *priv = self->priv;
  CoglTexture *texture;
  guint width, height;
  uint8_t pixels[8];

  ensure_pipeline (self);

  unset_texture (self);
  set_style (self, G_DESKTOP_BACKGROUND_STYLE_NONE);

  priv->shading_direction = shading_direction;

  switch (priv->shading_direction)
    {
      case G_DESKTOP_BACKGROUND_SHADING_VERTICAL:
          width = 1;
          height = 2;
          break;
      case G_DESKTOP_BACKGROUND_SHADING_HORIZONTAL:
          width = 2;
          height = 1;
          break;
      default:
          g_return_if_reached ();
    }

  pixels[0] = color->red;
  pixels[1] = color->green;
  pixels[2] = color->blue;
  pixels[3] = color->alpha;
  pixels[4] = second_color->red;
  pixels[5] = second_color->green;
  pixels[6] = second_color->blue;
  pixels[7] = second_color->alpha;
  texture = cogl_texture_new_from_data (width, height,
                                        COGL_TEXTURE_NO_SLICING,
                                        TEXTURE_FORMAT,
                                        COGL_PIXEL_FORMAT_ANY,
                                        4,
                                        pixels);
  set_texture (self, COGL_TEXTURE (texture));
}

/**
 * meta_background_load_color:
 * @self: the #MetaBackground
 * @color: a #ClutterColor to solid fill background with
 *
 * Clears any previously set background, and sets the
 * background to a solid color
 *
 * If @color is %NULL the stage color will be used.
 */
void
meta_background_load_color (MetaBackground *self,
                            ClutterColor   *color)
{
  MetaBackgroundPrivate *priv = self->priv;
  CoglTexture  *texture;
  ClutterActor *stage = meta_get_stage_for_screen (priv->screen);
  ClutterColor  stage_color;

  ensure_pipeline (self);

  unset_texture (self);
  set_style (self, G_DESKTOP_BACKGROUND_STYLE_NONE);

  if (color == NULL)
    {
      clutter_actor_get_background_color (stage, &stage_color);
      color = &stage_color;
    }

  texture = meta_create_color_texture_4ub (color->red,
                                           color->green,
                                           color->blue,
                                           0xff,
                                           COGL_TEXTURE_NO_SLICING);
  set_texture (self, COGL_TEXTURE (texture));
}

typedef struct
{
  GDesktopBackgroundStyle style;
  char *filename;
} LoadFileTaskData;

static LoadFileTaskData *
load_file_task_data_new (const char              *filename,
                         GDesktopBackgroundStyle  style)
{
  LoadFileTaskData *task_data;

  task_data = g_slice_new (LoadFileTaskData);
  task_data->style = style;
  task_data->filename = g_strdup (filename);

  return task_data;
}

static void
load_file_task_data_free (LoadFileTaskData *task_data)
{
  g_free (task_data->filename);
  g_slice_free (LoadFileTaskData, task_data);
}

static void
load_file (GTask            *task,
           MetaBackground   *self,
           LoadFileTaskData *task_data,
           GCancellable     *cancellable)
{
  GError *error = NULL;
  GdkPixbuf *pixbuf;

  pixbuf = gdk_pixbuf_new_from_file (task_data->filename,
                                     &error);

  if (pixbuf == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, pixbuf, (GDestroyNotify) g_object_unref);
}

/**
 * meta_background_load_file_async:
 * @self: the #MetaBackground
 * @filename: the image file to load
 * @style: a #GDesktopBackgroundStyle to specify how background is laid out
 * @cancellable: a #GCancellable
 * @callback: call back to call when file is loaded or failed to load
 * @user_data: user data for callback
 *
 * Loads the specified image and uses it as the background source.
 */
void
meta_background_load_file_async (MetaBackground          *self,
                                 const char              *filename,
                                 GDesktopBackgroundStyle  style,
                                 GCancellable            *cancellable,
                                 GAsyncReadyCallback      callback,
                                 gpointer                 user_data)
{
    LoadFileTaskData *task_data;
    GTask *task;

    task = g_task_new (self, cancellable, callback, user_data);

    task_data = load_file_task_data_new (filename, style);
    g_task_set_task_data (task, task_data, (GDestroyNotify) load_file_task_data_free);

    g_task_run_in_thread (task, (GTaskThreadFunc) load_file);
}

/**
 * meta_background_load_file_finish:
 * @self: the #MetaBackground
 * @result: the result from the #GAsyncReadyCallback passed
 *          to meta_background_load_file_async()
 * @error: a #GError
 *
 * The finish function for meta_background_load_file_async().
 *
 * Returns: whether or not the image was loaded
 */
gboolean
meta_background_load_file_finish (MetaBackground  *self,
                                  GAsyncResult    *result,
                                  GError         **error)
{
  static CoglUserDataKey key;
  GTask *task;
  LoadFileTaskData *task_data;
  CoglTexture *texture;
  GdkPixbuf *pixbuf;
  int width, height, row_stride;
  guchar *pixels;
  gboolean has_alpha;

  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  task = G_TASK (result);

  pixbuf = g_task_propagate_pointer (task, error);

  if (pixbuf == NULL)
    return FALSE;

  task_data = g_task_get_task_data (task);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  row_stride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  texture = cogl_texture_new_from_data (width,
                                        height,
                                        COGL_TEXTURE_NO_SLICING,
                                        has_alpha ?
                                        COGL_PIXEL_FORMAT_RGBA_8888 :
                                        COGL_PIXEL_FORMAT_RGB_888,
                                        COGL_PIXEL_FORMAT_ANY,
                                        row_stride,
                                        pixels);

  if (texture == NULL)
    {
      g_set_error_literal (error,
                           COGL_BITMAP_ERROR,
                           COGL_BITMAP_ERROR_FAILED,
                           _("background texture could not be created from file"));
      return FALSE;
    }

  cogl_object_set_user_data (COGL_OBJECT (texture),
                             &key,
                             g_object_ref (pixbuf),
                             (CoglUserDataDestroyCallback)
                             g_object_unref);

  ensure_pipeline (self);
  unset_texture (self);
  set_style (self, task_data->style);
  set_filename (self, task_data->filename);
  set_texture (self, texture);

  clutter_content_invalidate (CLUTTER_CONTENT (self));

  return TRUE;
}

/**
 * meta_background_copy:
 * @self: a #MetaBackground to copy
 * @monitor: a monitor
 * @effects: effects to use on copy of @self
 *
 * Creates a new #MetaBackground to draw the background for the given monitor.
 * Background will be loaded from @self and will share state
 * with @self, but may have different effects applied to it.
 *
 * Return value: (transfer full): the newly created background content
 */
MetaBackground *
meta_background_copy (MetaBackground        *self,
                      int                    monitor,
                      MetaBackgroundEffects  effects)
{
  MetaBackground *background;

  background = META_BACKGROUND (g_object_new (META_TYPE_BACKGROUND,
                                              "meta-screen", self->priv->screen,
                                              "monitor", monitor,
                                              NULL));

  background->priv->brightness = self->priv->brightness;

  background->priv->shading_direction = self->priv->shading_direction;
  background->priv->color = self->priv->color;
  background->priv->second_color = self->priv->second_color;
  background->priv->filename = g_strdup (self->priv->filename);

  /* we can reuse the pipeline if it has no effects applied, or
   * if it has the same effects applied
   */
  if (effects == self->priv->effects ||
      self->priv->effects == META_BACKGROUND_EFFECTS_NONE)
    {
      ensure_pipeline (self);
      background->priv->pipeline = cogl_pipeline_copy (self->priv->pipeline);
      background->priv->texture = cogl_object_ref (self->priv->texture);
      background->priv->style = self->priv->style;
      background->priv->saturation = self->priv->saturation;

      if (effects != self->priv->effects)
        {
          set_effects (background, effects);

          if (effects & META_BACKGROUND_EFFECTS_DESATURATE)
            set_saturation (background, self->priv->saturation);

          if (effects & META_BACKGROUND_EFFECTS_VIGNETTE)
            {
              set_brightness (background, self->priv->brightness);
              set_vignette_sharpness (background, self->priv->vignette_sharpness);
            }
        }
      else
        {
          background->priv->effects = self->priv->effects;
        }

    }
  else
    {
      ensure_pipeline (background);
      if (self->priv->texture != NULL)
        set_texture (background, cogl_object_ref (self->priv->texture));
      set_style (background, self->priv->style);
      set_effects (background, effects);

      if (effects & META_BACKGROUND_EFFECTS_DESATURATE)
        set_saturation (background, self->priv->saturation);

      if (effects & META_BACKGROUND_EFFECTS_VIGNETTE)
        {
          set_brightness (background, self->priv->brightness);
          set_vignette_sharpness (background, self->priv->vignette_sharpness);
        }
    }

  clutter_content_invalidate (CLUTTER_CONTENT (background));

  return background;
}
/**
 * meta_background_new:
 * @screen: the #MetaScreen
 * @monitor: a monitor in @screen
 * @effects: which effect flags to enable
 *
 * Creates a new #MetaBackground to draw the background for the given monitor.
 * The returned object should be set on a #MetaBackgroundActor with
 * clutter_actor_set_content().
 *
 * The background may be desaturated, blurred, or given a vignette depending
 * on @effects.
 *
 * Return value: the newly created background content
 */
MetaBackground *
meta_background_new (MetaScreen            *screen,
                     int                    monitor,
                     MetaBackgroundEffects  effects)
{
  MetaBackground *background;

  background = META_BACKGROUND (g_object_new (META_TYPE_BACKGROUND,
                                              "meta-screen", screen,
                                              "monitor", monitor,
                                              "effects", effects,
                                              NULL));
  return background;
}

/**
 * meta_background_get_style:
 * @self: a #MetaBackground
 *
 * Returns the current background style.
 *
 * Return value: a #GDesktopBackgroundStyle
 */
GDesktopBackgroundStyle
meta_background_get_style (MetaBackground *self)
{
    return self->priv->style;
}

/**
 * meta_background_get_shading:
 * @self: a #MetaBackground
 *
 * Returns whether @self is a solid color,
 * vertical gradient, horizontal gradient,
 * or none of the above.
 *
 * Return value: a #GDesktopBackgroundShading
 */
GDesktopBackgroundShading
meta_background_get_shading (MetaBackground *self)
{
    return self->priv->shading_direction;
}

/**
 * meta_background_get_color:
 * @self: a #MetaBackground
 *
 * Returns the first color of @self. If self
 * is a gradient, the second color can be returned
 * with meta_background_get_second_color().
 *
 * Return value: (transfer none): a #ClutterColor
 */
const ClutterColor *
meta_background_get_color (MetaBackground *self)
{
    return &self->priv->color;
}

/**
 * meta_background_get_second_color:
 * @self: a #MetaBackground
 *
 * Returns the second color of @self. If @self
 * is not a gradient this function is undefined.
 *
 * Return value: (transfer none): a #ClutterColor
 */
const ClutterColor *
meta_background_get_second_color (MetaBackground *self)
{
    return &self->priv->second_color;
}

/**
 * meta_background_get_filename:
 * @self: a #MetaBackground
 *
 * Returns the filename of the currently loaded file.
 * IF @self is not loaded from a file this function is
 * undefined.
 *
 * Return value: (transfer none): the filename
 */
const char *
meta_background_get_filename (MetaBackground *self)
{
    return self->priv->filename;
}
