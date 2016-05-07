/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <meta/util.h>
#include <meta/meta-background.h>
#include <meta/meta-background-image.h>
#include "meta-background-private.h"
#include "cogl-utils.h"

#include <string.h>

enum
{
  CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _MetaBackgroundMonitor MetaBackgroundMonitor;

struct _MetaBackgroundMonitor
{
  gboolean dirty;
  CoglTexture *texture;
  CoglFramebuffer *fbo;
};

struct _MetaBackgroundPrivate
{
  MetaScreen *screen;
  MetaBackgroundMonitor *monitors;
  int n_monitors;

  GDesktopBackgroundStyle   style;
  GDesktopBackgroundShading shading_direction;
  ClutterColor              color;
  ClutterColor              second_color;

  GFile *file1;
  MetaBackgroundImage *background_image1;
  GFile *file2;
  MetaBackgroundImage *background_image2;

  CoglTexture *color_texture;
  CoglTexture *wallpaper_texture;

  float blend_factor;

  guint wallpaper_allocation_failed : 1;
};

enum
{
  PROP_META_SCREEN = 1,
  PROP_MONITOR,
};

G_DEFINE_TYPE (MetaBackground, meta_background, G_TYPE_OBJECT)

static gboolean texture_has_alpha (CoglTexture *texture);

static GSList *all_backgrounds = NULL;

static void
free_fbos (MetaBackground *self)
{
  MetaBackgroundPrivate *priv = self->priv;

  int i;

  for (i = 0; i < priv->n_monitors; i++)
    {
      MetaBackgroundMonitor *monitor = &priv->monitors[i];
      if (monitor->fbo)
        {
          cogl_object_unref (monitor->fbo);
          monitor->fbo = NULL;
        }
      if (monitor->texture)
        {
          cogl_object_unref (monitor->texture);
          monitor->texture = NULL;
        }
    }
}

static void
free_color_texture (MetaBackground *self)
{
  MetaBackgroundPrivate *priv = self->priv;

  if (priv->color_texture != NULL)
    {
      cogl_object_unref (priv->color_texture);
      priv->color_texture = NULL;
    }
}

static void
free_wallpaper_texture (MetaBackground *self)
{
  MetaBackgroundPrivate *priv = self->priv;

  if (priv->wallpaper_texture != NULL)
    {
      cogl_object_unref (priv->wallpaper_texture);
      priv->wallpaper_texture = NULL;
    }

  priv->wallpaper_allocation_failed = FALSE;
}

static void
on_monitors_changed (MetaScreen     *screen,
                     MetaBackground *self)
{
  MetaBackgroundPrivate *priv = self->priv;

  free_fbos (self);
  g_free (priv->monitors);
  priv->monitors = NULL;
  priv->n_monitors = 0;

  if (priv->screen)
    {
      int i;

      priv->n_monitors = meta_screen_get_n_monitors (screen);
      priv->monitors = g_new0 (MetaBackgroundMonitor, priv->n_monitors);

      for (i = 0; i < priv->n_monitors; i++)
        priv->monitors[i].dirty = TRUE;
    }
}

static void
set_screen (MetaBackground *self,
            MetaScreen     *screen)
{
  MetaBackgroundPrivate *priv = self->priv;

  if (priv->screen != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->screen,
                                            (gpointer)on_monitors_changed,
                                            self);
    }

  priv->screen = screen;

  if (priv->screen != NULL)
    {
      g_signal_connect (priv->screen, "monitors-changed",
                        G_CALLBACK (on_monitors_changed), self);
    }

  on_monitors_changed (priv->screen, self);
}

static void
meta_background_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_META_SCREEN:
      set_screen (META_BACKGROUND (object), g_value_get_object (value));
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
need_prerender (MetaBackground *self)
{
  MetaBackgroundPrivate *priv = self->priv;
  CoglTexture *texture1 = priv->background_image1 ? meta_background_image_get_texture (priv->background_image1) : NULL;
  CoglTexture *texture2 = priv->background_image2 ? meta_background_image_get_texture (priv->background_image2) : NULL;

  if (texture1 == NULL && texture2 == NULL)
    return FALSE;

  if (texture2 == NULL && priv->style == G_DESKTOP_BACKGROUND_STYLE_WALLPAPER)
    return FALSE;

  return TRUE;
}

static void
mark_changed (MetaBackground *self)
{
  MetaBackgroundPrivate *priv = self->priv;
  int i;

  if (!need_prerender (self))
    free_fbos (self);

  for (i = 0; i < priv->n_monitors; i++)
    priv->monitors[i].dirty = TRUE;

  g_signal_emit (self, signals[CHANGED], 0);
}

static void
on_background_loaded (MetaBackgroundImage *image,
                      MetaBackground      *self)
{
  mark_changed (self);
}

static gboolean
file_equal0 (GFile *file1,
             GFile *file2)
{
  if (file1 == file2)
    return TRUE;

  if ((file1 == NULL) || (file2 == NULL))
    return FALSE;

  return g_file_equal (file1, file2);
}

static void
set_file (MetaBackground       *self,
          GFile               **filep,
          MetaBackgroundImage **imagep,
          GFile                *file)
{
  if (!file_equal0 (*filep, file))
    {
      g_clear_object (filep);

      if (*imagep)
        {
          g_signal_handlers_disconnect_by_func (*imagep,
                                                (gpointer)on_background_loaded,
                                                self);
          g_object_unref (*imagep);
          *imagep = NULL;
        }

      if (file)
        {
          MetaBackgroundImageCache *cache = meta_background_image_cache_get_default ();

          *filep = g_object_ref (file);
          *imagep = meta_background_image_cache_load (cache, file);
          g_signal_connect (*imagep, "loaded",
                            G_CALLBACK (on_background_loaded), self);
        }
    }
}

static void
meta_background_dispose (GObject *object)
{
  MetaBackground        *self = META_BACKGROUND (object);
  MetaBackgroundPrivate *priv = self->priv;

  free_color_texture (self);
  free_wallpaper_texture (self);

  set_file (self, &priv->file1, &priv->background_image1, NULL);
  set_file (self, &priv->file2, &priv->background_image2, NULL);

  set_screen (self, NULL);

  G_OBJECT_CLASS (meta_background_parent_class)->dispose (object);
}

static void
meta_background_finalize (GObject *object)
{
  all_backgrounds = g_slist_remove (all_backgrounds, object);

  G_OBJECT_CLASS (meta_background_parent_class)->finalize (object);
}

static void
meta_background_constructed (GObject *object)
{
  MetaBackground        *self = META_BACKGROUND (object);
  MetaBackgroundPrivate *priv = self->priv;

  G_OBJECT_CLASS (meta_background_parent_class)->constructed (object);

  g_signal_connect_object (meta_screen_get_display (priv->screen), "gl-video-memory-purged",
                           G_CALLBACK (mark_changed), object, G_CONNECT_SWAPPED);
}

static void
meta_background_class_init (MetaBackgroundClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (MetaBackgroundPrivate));

  object_class->dispose = meta_background_dispose;
  object_class->finalize = meta_background_finalize;
  object_class->constructed = meta_background_constructed;
  object_class->set_property = meta_background_set_property;
  object_class->get_property = meta_background_get_property;

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  param_spec = g_param_spec_object ("meta-screen",
                                    "MetaScreen",
                                    "MetaScreen",
                                    META_TYPE_SCREEN,
                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_property (object_class,
                                   PROP_META_SCREEN,
                                   param_spec);

}

static void
meta_background_init (MetaBackground *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            META_TYPE_BACKGROUND,
                                            MetaBackgroundPrivate);
  all_backgrounds = g_slist_prepend (all_backgrounds, self);
}

static void
set_texture_area_from_monitor_area (cairo_rectangle_int_t *monitor_area,
                                    cairo_rectangle_int_t *texture_area)
{
  texture_area->x = 0;
  texture_area->y = 0;
  texture_area->width = monitor_area->width;
  texture_area->height = monitor_area->height;
}

static void
get_texture_area (MetaBackground          *self,
                  cairo_rectangle_int_t   *monitor_rect,
                  CoglTexture             *texture,
                  cairo_rectangle_int_t   *texture_area)
{
  MetaBackgroundPrivate *priv = self->priv;
  cairo_rectangle_int_t image_area;
  int screen_width, screen_height;
  float texture_width, texture_height;
  float monitor_x_scale, monitor_y_scale;

  texture_width = cogl_texture_get_width (texture);
  texture_height = cogl_texture_get_height (texture);

  switch (priv->style)
    {
    case G_DESKTOP_BACKGROUND_STYLE_STRETCHED:
    default:
      /* paint region is whole actor, and the texture
       * is scaled disproportionately to fit the actor
       */
      set_texture_area_from_monitor_area (monitor_rect, texture_area);
      break;
    case G_DESKTOP_BACKGROUND_STYLE_WALLPAPER:
      meta_screen_get_size (priv->screen, &screen_width, &screen_height);

      /* Start off by centering a tile in the middle of the
       * total screen area.
       */
      image_area.x = (screen_width - texture_width) / 2.0;
      image_area.y = (screen_height - texture_height) / 2.0;
      image_area.width = texture_width;
      image_area.height = texture_height;

      /* Translate into the coordinate system of the particular monitor */
      image_area.x -= monitor_rect->x;
      image_area.y -= monitor_rect->y;

      *texture_area = image_area;
      break;
    case G_DESKTOP_BACKGROUND_STYLE_CENTERED:
      /* paint region is the original image size centered in the actor,
       * and the texture is scaled to the original image size */
      image_area.width = texture_width;
      image_area.height = texture_height;
      image_area.x = monitor_rect->width / 2 - image_area.width / 2;
      image_area.y = monitor_rect->height / 2 - image_area.height / 2;

      *texture_area = image_area;
      break;
    case G_DESKTOP_BACKGROUND_STYLE_SCALED:
    case G_DESKTOP_BACKGROUND_STYLE_ZOOM:
      /* paint region is the actor size in one dimension, and centered and
       * scaled by proportional amount in the other dimension.
       *
       * SCALED forces the centered dimension to fit on screen.
       * ZOOM forces the centered dimension to grow off screen
       */
      monitor_x_scale = monitor_rect->width / texture_width;
      monitor_y_scale = monitor_rect->height / texture_height;

      if ((priv->style == G_DESKTOP_BACKGROUND_STYLE_SCALED &&
           (monitor_x_scale < monitor_y_scale)) ||
          (priv->style == G_DESKTOP_BACKGROUND_STYLE_ZOOM &&
           (monitor_x_scale > monitor_y_scale)))
        {
          /* Fill image to exactly fit actor horizontally */
          image_area.width = monitor_rect->width;
          image_area.height = texture_height * monitor_x_scale;

          /* Position image centered vertically in actor */
          image_area.x = 0;
          image_area.y = monitor_rect->height / 2 - image_area.height / 2;
        }
      else
        {
          /* Scale image to exactly fit actor vertically */
          image_area.width = texture_width * monitor_y_scale;
          image_area.height = monitor_rect->height;

          /* Position image centered horizontally in actor */
          image_area.x = monitor_rect->width / 2 - image_area.width / 2;
          image_area.y = 0;
        }

      *texture_area = image_area;
      break;

    case G_DESKTOP_BACKGROUND_STYLE_SPANNED:
      {
        /* paint region is the union of all monitors, with the origin
         * of the region set to align with monitor associated with the background.
         */
        meta_screen_get_size (priv->screen, &screen_width, &screen_height);

        /* unclipped texture area is whole screen */
        image_area.width = screen_width;
        image_area.height = screen_height;

        /* But make (0,0) line up with the appropriate monitor */
        image_area.x = -monitor_rect->x;
        image_area.y = -monitor_rect->y;

        *texture_area = image_area;
        break;
      }
    }
}

static gboolean
draw_texture (MetaBackground        *self,
              CoglFramebuffer       *framebuffer,
              CoglPipeline          *pipeline,
              CoglTexture           *texture,
              cairo_rectangle_int_t *monitor_area)
{
  MetaBackgroundPrivate *priv = self->priv;
  cairo_rectangle_int_t texture_area;
  gboolean bare_region_visible;

  get_texture_area (self, monitor_area, texture, &texture_area);

  switch (priv->style)
    {
    case G_DESKTOP_BACKGROUND_STYLE_STRETCHED:
    case G_DESKTOP_BACKGROUND_STYLE_WALLPAPER:
    case G_DESKTOP_BACKGROUND_STYLE_ZOOM:
    case G_DESKTOP_BACKGROUND_STYLE_SPANNED:
      /* Draw the entire monitor */
      cogl_framebuffer_draw_textured_rectangle (framebuffer,
                                                pipeline,
                                                0,
                                                0,
                                                monitor_area->width,
                                                monitor_area->height,
                                                - texture_area.x / (float)texture_area.width,
                                                - texture_area.y / (float)texture_area.height,
                                                (monitor_area->width - texture_area.x) / (float)texture_area.width,
                                                (monitor_area->height - texture_area.y) / (float)texture_area.height);

      bare_region_visible = texture_has_alpha (texture);

      /* Draw just the texture */
      break;
    case G_DESKTOP_BACKGROUND_STYLE_CENTERED:
    case G_DESKTOP_BACKGROUND_STYLE_SCALED:
      cogl_framebuffer_draw_textured_rectangle (framebuffer,
                                                pipeline,
                                                texture_area.x, texture_area.y,
                                                texture_area.x + texture_area.width,
                                                texture_area.y + texture_area.height,
                                                0, 0, 1.0, 1.0);
      bare_region_visible = texture_has_alpha (texture) || memcmp (&texture_area, monitor_area, sizeof (cairo_rectangle_int_t)) != 0;
      break;
    case G_DESKTOP_BACKGROUND_STYLE_NONE:
      bare_region_visible = TRUE;
      break;
    default:
      g_return_val_if_reached(FALSE);
    }

  return bare_region_visible;
}

static void
ensure_color_texture (MetaBackground *self)
{
  MetaBackgroundPrivate *priv = self->priv;

  if (priv->color_texture == NULL)
    {
      ClutterBackend *backend = clutter_get_default_backend ();
      CoglContext *ctx = clutter_backend_get_cogl_context (backend);
      CoglError *error = NULL;
      uint8_t pixels[6];
      int width, height;

      if (priv->shading_direction == G_DESKTOP_BACKGROUND_SHADING_SOLID)
        {
          width = 1;
          height = 1;

          pixels[0] = priv->color.red;
          pixels[1] = priv->color.green;
          pixels[2] = priv->color.blue;
        }
      else
        {
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

          pixels[0] = priv->color.red;
          pixels[1] = priv->color.green;
          pixels[2] = priv->color.blue;
          pixels[3] = priv->second_color.red;
          pixels[4] = priv->second_color.green;
          pixels[5] = priv->second_color.blue;
        }

      priv->color_texture = COGL_TEXTURE (cogl_texture_2d_new_from_data (ctx, width, height,
                                                                         COGL_PIXEL_FORMAT_RGB_888,
                                                                         width * 3,
                                                                         pixels,
                                                                         &error));

      if (error != NULL)
        {
          meta_warning ("Failed to allocate color texture: %s\n", error->message);
          cogl_error_free (error);
        }
    }
}

typedef enum {
  PIPELINE_REPLACE,
  PIPELINE_ADD,
  PIPELINE_OVER_REVERSE,
} PipelineType;

static CoglPipeline *
create_pipeline (PipelineType type)
{
  const char * const blend_strings[3] = {
    [PIPELINE_REPLACE] = "RGBA = ADD (SRC_COLOR, 0)",
    [PIPELINE_ADD] = "RGBA = ADD (SRC_COLOR, DST_COLOR)",
    [PIPELINE_OVER_REVERSE] = "RGBA = ADD (SRC_COLOR * (1 - DST_COLOR[A]), DST_COLOR)",
  };
  static CoglPipeline *templates[3];

  if (templates[type] == NULL)
    {
      templates[type] = meta_create_texture_pipeline (NULL);
      cogl_pipeline_set_blend (templates[type], blend_strings[type], NULL);
    }

  return cogl_pipeline_copy (templates[type]);
}

static gboolean
texture_has_alpha (CoglTexture *texture)
{
  if (!texture)
    return FALSE;

  switch (cogl_texture_get_components (texture))
    {
    case COGL_TEXTURE_COMPONENTS_A:
    case COGL_TEXTURE_COMPONENTS_RGBA:
      return TRUE;
    case COGL_TEXTURE_COMPONENTS_RG:
    case COGL_TEXTURE_COMPONENTS_RGB:
    case COGL_TEXTURE_COMPONENTS_DEPTH:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
ensure_wallpaper_texture (MetaBackground *self,
                          CoglTexture    *texture)
{
  MetaBackgroundPrivate *priv = self->priv;

  if (priv->wallpaper_texture == NULL && !priv->wallpaper_allocation_failed)
    {
      int width = cogl_texture_get_width (texture);
      int height = cogl_texture_get_height (texture);
      CoglOffscreen *offscreen;
      CoglFramebuffer *fbo;
      CoglError *catch_error = NULL;
      CoglPipeline *pipeline;

      priv->wallpaper_texture = meta_create_texture (width, height,
                                                     COGL_TEXTURE_COMPONENTS_RGBA,
                                                     META_TEXTURE_FLAGS_NONE);
      offscreen = cogl_offscreen_new_with_texture (priv->wallpaper_texture);
      fbo = COGL_FRAMEBUFFER (offscreen);

      if (!cogl_framebuffer_allocate (fbo, &catch_error))
        {
          /* This probably means that the size of the wallpapered texture is larger
           * than the maximum texture size; we treat it as permanent until the
           * background is changed again.
           */
          cogl_error_free (catch_error);

          cogl_object_unref (priv->wallpaper_texture);
          priv->wallpaper_texture = NULL;
          cogl_object_unref (fbo);

          priv->wallpaper_allocation_failed = TRUE;
          return FALSE;
        }

      cogl_framebuffer_orthographic (fbo, 0, 0,
                                     width, height, -1., 1.);

      pipeline = create_pipeline (PIPELINE_REPLACE);
      cogl_pipeline_set_layer_texture (pipeline, 0, texture);
      cogl_framebuffer_draw_textured_rectangle (fbo, pipeline, 0, 0, width, height,
                                                0., 0., 1., 1.);
      cogl_object_unref (pipeline);

      if (texture_has_alpha (texture))
        {
          ensure_color_texture (self);

          pipeline = create_pipeline (PIPELINE_OVER_REVERSE);
          cogl_pipeline_set_layer_texture (pipeline, 0, priv->color_texture);
          cogl_framebuffer_draw_rectangle (fbo, pipeline, 0, 0, width, height);
          cogl_object_unref (pipeline);
        }

      cogl_object_unref (fbo);
    }

  return priv->wallpaper_texture != NULL;
}

static CoglPipelineWrapMode
get_wrap_mode (GDesktopBackgroundStyle style)
{
  switch (style)
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

CoglTexture *
meta_background_get_texture (MetaBackground         *self,
                             int                     monitor_index,
                             cairo_rectangle_int_t  *texture_area,
                             CoglPipelineWrapMode   *wrap_mode)
{
  MetaBackgroundPrivate *priv;
  MetaBackgroundMonitor *monitor;
  MetaRectangle geometry;
  cairo_rectangle_int_t monitor_area;
  CoglTexture *texture1, *texture2;

  g_return_val_if_fail (META_IS_BACKGROUND (self), NULL);
  priv = self->priv;
  g_return_val_if_fail (monitor_index >= 0 && monitor_index < priv->n_monitors, NULL);

  monitor = &priv->monitors[monitor_index];

  meta_screen_get_monitor_geometry (priv->screen, monitor_index, &geometry);
  monitor_area.x = geometry.x;
  monitor_area.y = geometry.y;
  monitor_area.width = geometry.width;
  monitor_area.height = geometry.height;

  texture1 = priv->background_image1 ? meta_background_image_get_texture (priv->background_image1) : NULL;
  texture2 = priv->background_image2 ? meta_background_image_get_texture (priv->background_image2) : NULL;

  if (texture1 == NULL && texture2 == NULL)
    {
      ensure_color_texture (self);
      if (texture_area)
        set_texture_area_from_monitor_area (&monitor_area, texture_area);
      if (wrap_mode)
        *wrap_mode = COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;
      return priv->color_texture;
    }

  if (texture2 == NULL && priv->style == G_DESKTOP_BACKGROUND_STYLE_WALLPAPER &&
      priv->shading_direction == G_DESKTOP_BACKGROUND_SHADING_SOLID &&
      ensure_wallpaper_texture (self, texture1))
    {
      if (texture_area)
        get_texture_area (self, &monitor_area, priv->wallpaper_texture,
                          texture_area);
      if (wrap_mode)
        *wrap_mode = COGL_PIPELINE_WRAP_MODE_REPEAT;
      return priv->wallpaper_texture;
    }

  if (monitor->dirty)
    {
      CoglError *catch_error = NULL;
      gboolean bare_region_visible = FALSE;

      if (monitor->texture == NULL)
        {
          CoglOffscreen *offscreen;

          monitor->texture = meta_create_texture (monitor_area.width, monitor_area.height,
                                                  COGL_TEXTURE_COMPONENTS_RGBA,
                                                  META_TEXTURE_FLAGS_NONE);
          offscreen = cogl_offscreen_new_with_texture (monitor->texture);
          monitor->fbo = COGL_FRAMEBUFFER (offscreen);
        }

      if (!cogl_framebuffer_allocate (monitor->fbo, &catch_error))
        {
          /* Texture or framebuffer allocation failed; it's unclear why this happened;
           * we'll try again the next time this is called. (MetaBackgroundActor
           * caches the result, so user might be left without a background.)
           */
          cogl_object_unref (monitor->texture);
          monitor->texture = NULL;
          cogl_object_unref (monitor->fbo);
          monitor->fbo = NULL;

          cogl_error_free (catch_error);
          return NULL;
        }

      cogl_framebuffer_orthographic (monitor->fbo, 0, 0,
                                     monitor_area.width, monitor_area.height, -1., 1.);

      if (texture2 != NULL && priv->blend_factor != 0.0)
        {
          CoglPipeline *pipeline = create_pipeline (PIPELINE_REPLACE);
          cogl_pipeline_set_color4f (pipeline,
                                      priv->blend_factor, priv->blend_factor, priv->blend_factor, priv->blend_factor);
          cogl_pipeline_set_layer_texture (pipeline, 0, texture2);
          cogl_pipeline_set_layer_wrap_mode (pipeline, 0, get_wrap_mode (priv->style));

          bare_region_visible = draw_texture (self,
                                              monitor->fbo, pipeline,
                                              texture2, &monitor_area);

          cogl_object_unref (pipeline);
        }
      else
        {
          cogl_framebuffer_clear4f (monitor->fbo,
                                    COGL_BUFFER_BIT_COLOR,
                                    0.0, 0.0, 0.0, 0.0);
        }

      if (texture1 != NULL && priv->blend_factor != 1.0)
        {
          CoglPipeline *pipeline = create_pipeline (PIPELINE_ADD);
          cogl_pipeline_set_color4f (pipeline,
                                     (1 - priv->blend_factor),
                                     (1 - priv->blend_factor),
                                     (1 - priv->blend_factor),
                                     (1 - priv->blend_factor));;
          cogl_pipeline_set_layer_texture (pipeline, 0, texture1);
          cogl_pipeline_set_layer_wrap_mode (pipeline, 0, get_wrap_mode (priv->style));

          bare_region_visible = bare_region_visible || draw_texture (self,
                                                                     monitor->fbo, pipeline,
                                                                     texture1, &monitor_area);

          cogl_object_unref (pipeline);
        }

      if (bare_region_visible)
        {
          CoglPipeline *pipeline = create_pipeline (PIPELINE_OVER_REVERSE);

          ensure_color_texture (self);
          cogl_pipeline_set_layer_texture (pipeline, 0, priv->color_texture);
          cogl_framebuffer_draw_rectangle (monitor->fbo,
                                           pipeline,
                                           0, 0,
                                           monitor_area.width, monitor_area.height);
          cogl_object_unref (pipeline);
        }

      monitor->dirty = FALSE;
    }

  if (texture_area)
    set_texture_area_from_monitor_area (&monitor_area, texture_area);

  if (wrap_mode)
    *wrap_mode = COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;
  return monitor->texture;
}

MetaBackground *
meta_background_new  (MetaScreen *screen)
{
  return g_object_new (META_TYPE_BACKGROUND,
                       "meta-screen", screen,
                       NULL);
}

void
meta_background_set_color (MetaBackground *self,
                           ClutterColor   *color)
{
  ClutterColor dummy = { 0 };

  g_return_if_fail (META_IS_BACKGROUND (self));
  g_return_if_fail (color != NULL);

  meta_background_set_gradient (self,
                                G_DESKTOP_BACKGROUND_SHADING_SOLID,
                                color, &dummy);
}

void
meta_background_set_gradient (MetaBackground            *self,
                              GDesktopBackgroundShading  shading_direction,
                              ClutterColor              *color,
                              ClutterColor              *second_color)
{
  MetaBackgroundPrivate *priv;

  g_return_if_fail (META_IS_BACKGROUND (self));
  g_return_if_fail (color != NULL);
  g_return_if_fail (second_color != NULL);

  priv = self->priv;

  priv->shading_direction = shading_direction;
  priv->color = *color;
  priv->second_color = *second_color;

  free_color_texture (self);
  free_wallpaper_texture (self);
  mark_changed (self);
}

void
meta_background_set_file (MetaBackground            *self,
                          GFile                     *file,
                          GDesktopBackgroundStyle    style)
{
  g_return_if_fail (META_IS_BACKGROUND (self));

  meta_background_set_blend (self, file, NULL, 0.0, style);
}

void
meta_background_set_blend (MetaBackground          *self,
                           GFile                   *file1,
                           GFile                   *file2,
                           double                   blend_factor,
                           GDesktopBackgroundStyle  style)
{
  MetaBackgroundPrivate *priv;

  g_return_if_fail (META_IS_BACKGROUND (self));
  g_return_if_fail (blend_factor >= 0.0 && blend_factor <= 1.0);

  priv = self->priv;

  set_file (self, &priv->file1, &priv->background_image1, file1);
  set_file (self, &priv->file2, &priv->background_image2, file2);

  priv->blend_factor = blend_factor;
  priv->style = style;

  free_wallpaper_texture (self);
  mark_changed (self);
}

void
meta_background_refresh_all (void)
{
  GSList *l;

  for (l = all_backgrounds; l; l = l->next)
    mark_changed (l->data);
}
