/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-texture-cache.h: Object for loading and caching images as textures
 *
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2010, Maxim Ermilov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "st-texture-cache.h"
#include "st-private.h"
#include <gtk/gtk.h>
#include <string.h>
#include <glib.h>

#define CACHE_PREFIX_ICON "icon:"
#define CACHE_PREFIX_FILE "file:"
#define CACHE_PREFIX_FILE_FOR_CAIRO "file-for-cairo:"

struct _StTextureCachePrivate
{
  GtkIconTheme *icon_theme;

  /* Things that were loaded with a cache policy != NONE */
  GHashTable *keyed_cache; /* char * -> CoglTexture* */
  GHashTable *keyed_surface_cache; /* char * -> cairo_surface_t* */

  /* Presently this is used to de-duplicate requests for GIcons and async URIs. */
  GHashTable *outstanding_requests; /* char * -> AsyncTextureLoadData * */

  /* File monitors to evict cache data on changes */
  GHashTable *file_monitors; /* char * -> GFileMonitor * */
};

static void st_texture_cache_dispose (GObject *object);
static void st_texture_cache_finalize (GObject *object);

enum
{
  ICON_THEME_CHANGED,
  TEXTURE_FILE_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };
G_DEFINE_TYPE(StTextureCache, st_texture_cache, G_TYPE_OBJECT);

/* We want to preserve the aspect ratio by default, also the default
 * pipeline for an empty texture is full opacity white, which we
 * definitely don't want.  Skip that by setting 0 opacity.
 */
static ClutterTexture *
create_default_texture (void)
{
  ClutterTexture * texture = CLUTTER_TEXTURE (clutter_texture_new ());
  g_object_set (texture, "keep-aspect-ratio", TRUE, "opacity", 0, NULL);
  return texture;
}

/* Reverse the opacity we added while loading */
static void
set_texture_cogl_texture (ClutterTexture *clutter_texture, CoglTexture *cogl_texture)
{
  clutter_texture_set_cogl_texture (clutter_texture, cogl_texture);
  g_object_set (clutter_texture, "opacity", 255, NULL);
}

static void
st_texture_cache_class_init (StTextureCacheClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->dispose = st_texture_cache_dispose;
  gobject_class->finalize = st_texture_cache_finalize;

  signals[ICON_THEME_CHANGED] =
    g_signal_new ("icon-theme-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, /* no default handler slot */
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[TEXTURE_FILE_CHANGED] =
    g_signal_new ("texture-file-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, /* no default handler slot */
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_FILE);
}

/* Evicts all cached textures for named icons */
static void
st_texture_cache_evict_icons (StTextureCache *cache)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_hash_table_iter_init (&iter, cache->priv->keyed_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const char *cache_key = key;

      /* This is too conservative - it takes out all cached textures
       * for GIcons even when they aren't named icons, but it's not
       * worth the complexity of parsing the key and calling
       * g_icon_new_for_string(); icon theme changes aren't normal */
      if (g_str_has_prefix (cache_key, CACHE_PREFIX_ICON))
        g_hash_table_iter_remove (&iter);
    }
}

static void
on_icon_theme_changed (GtkIconTheme   *icon_theme,
                       StTextureCache *cache)
{
  st_texture_cache_evict_icons (cache);
  g_signal_emit (cache, signals[ICON_THEME_CHANGED], 0);
}

static void
st_texture_cache_init (StTextureCache *self)
{
  self->priv = g_new0 (StTextureCachePrivate, 1);

  self->priv->icon_theme = gtk_icon_theme_get_default ();
  g_signal_connect (self->priv->icon_theme, "changed",
                    G_CALLBACK (on_icon_theme_changed), self);

  self->priv->keyed_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   g_free, cogl_object_unref);
  self->priv->keyed_surface_cache = g_hash_table_new_full (g_str_hash,
                                                           g_str_equal,
                                                           g_free,
                                                           (GDestroyNotify) cairo_surface_destroy);
  self->priv->outstanding_requests = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                            g_free, NULL);
  self->priv->file_monitors = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
                                                     g_object_unref, g_object_unref);

}

static void
st_texture_cache_dispose (GObject *object)
{
  StTextureCache *self = (StTextureCache*)object;

  if (self->priv->icon_theme)
    {
      g_signal_handlers_disconnect_by_func (self->priv->icon_theme,
                                            (gpointer) on_icon_theme_changed,
                                            self);
      self->priv->icon_theme = NULL;
    }

  g_clear_pointer (&self->priv->keyed_cache, g_hash_table_destroy);
  g_clear_pointer (&self->priv->keyed_surface_cache, g_hash_table_destroy);
  g_clear_pointer (&self->priv->outstanding_requests, g_hash_table_destroy);
  g_clear_pointer (&self->priv->file_monitors, g_hash_table_destroy);

  G_OBJECT_CLASS (st_texture_cache_parent_class)->dispose (object);
}

static void
st_texture_cache_finalize (GObject *object)
{
  G_OBJECT_CLASS (st_texture_cache_parent_class)->finalize (object);
}

static void
compute_pixbuf_scale (gint      width,
                      gint      height,
                      gint      available_width,
                      gint      available_height,
                      gint     *new_width,
                      gint     *new_height)
{
  int scaled_width, scaled_height;

  if (width == 0 || height == 0)
    {
      *new_width = *new_height = 0;
      return;
    }

  if (available_width >= 0 && available_height >= 0)
    {
      /* This should keep the aspect ratio of the image intact, because if
       * available_width < (available_height * width) / height
       * then
       * (available_width * height) / width < available_height
       * So we are guaranteed to either scale the image to have an available_width
       * for width and height scaled accordingly OR have the available_height
       * for height and width scaled accordingly, whichever scaling results
       * in the image that can fit both available dimensions.
       */
      scaled_width = MIN (available_width, (available_height * width) / height);
      scaled_height = MIN (available_height, (available_width * height) / width);
    }
  else if (available_width >= 0)
    {
      scaled_width = available_width;
      scaled_height = (available_width * height) / width;
    }
  else if (available_height >= 0)
    {
      scaled_width = (available_height * width) / height;
      scaled_height = available_height;
    }
  else
    {
      scaled_width = scaled_height = 0;
    }

  /* Scale the image only if that will not increase its original dimensions. */
  if (scaled_width > 0 && scaled_height > 0 && scaled_width < width && scaled_height < height)
    {
      *new_width = scaled_width;
      *new_height = scaled_height;
    }
  else
    {
      *new_width = width;
      *new_height = height;
    }
}

static void
rgba_from_clutter (GdkRGBA      *rgba,
                   ClutterColor *color)
{
  rgba->red = color->red / 255.;
  rgba->green = color->green / 255.;
  rgba->blue = color->blue / 255.;
  rgba->alpha = color->alpha / 255.;
}

/* A private structure for keeping width, height and scale. */
typedef struct {
  int width;
  int height;
  int scale;
} Dimensions;

/* This struct corresponds to a request for an texture.
 * It's creasted when something needs a new texture,
 * and destroyed when the texture data is loaded. */
typedef struct {
  StTextureCache *cache;
  StTextureCachePolicy policy;
  char *key;

  guint width;
  guint height;
  guint scale;
  GSList *textures;

  GtkIconInfo *icon_info;
  StIconColors *colors;
  GFile *file;
} AsyncTextureLoadData;

static void
texture_load_data_free (gpointer p)
{
  AsyncTextureLoadData *data = p;

  if (data->icon_info)
    {
      g_object_unref (data->icon_info);
      if (data->colors)
        st_icon_colors_unref (data->colors);
    }
  else if (data->file)
    g_object_unref (data->file);

  if (data->key)
    g_free (data->key);

  if (data->textures)
    g_slist_free_full (data->textures, (GDestroyNotify) g_object_unref);

  g_free (data);
}

/**
 * on_image_size_prepared:
 * @pixbuf_loader: #GdkPixbufLoader loading the image
 * @width: the original width of the image
 * @height: the original height of the image
 * @data: pointer to the #Dimensions sructure containing available width and height for the image,
 *        available width or height can be -1 if the dimension is not limited
 *
 * Private function.
 *
 * Sets the size of the image being loaded to fit the available width and height dimensions,
 * but never scales up the image beyond its actual size.
 * Intended to be used as a callback for #GdkPixbufLoader "size-prepared" signal.
 */
static void
on_image_size_prepared (GdkPixbufLoader *pixbuf_loader,
                        gint             width,
                        gint             height,
                        gpointer         data)
{
  Dimensions *available_dimensions = data;
  int available_width = available_dimensions->width;
  int available_height = available_dimensions->height;
  int scale_factor = available_dimensions->scale;
  int scaled_width;
  int scaled_height;

  compute_pixbuf_scale (width, height, available_width, available_height,
                        &scaled_width, &scaled_height);

  gdk_pixbuf_loader_set_size (pixbuf_loader,
                              scaled_width * scale_factor,
                              scaled_height * scale_factor);
}

static GdkPixbuf *
impl_load_pixbuf_data (const guchar   *data,
                       gsize           size,
                       int             available_width,
                       int             available_height,
                       int             scale,
                       GError        **error)
{
  GdkPixbufLoader *pixbuf_loader = NULL;
  GdkPixbuf *rotated_pixbuf = NULL;
  GdkPixbuf *pixbuf;
  gboolean success;
  Dimensions available_dimensions;
  int width_before_rotation, width_after_rotation;

  pixbuf_loader = gdk_pixbuf_loader_new ();

  available_dimensions.width = available_width;
  available_dimensions.height = available_height;
  available_dimensions.scale = scale;
  g_signal_connect (pixbuf_loader, "size-prepared",
                    G_CALLBACK (on_image_size_prepared), &available_dimensions);

  success = gdk_pixbuf_loader_write (pixbuf_loader, data, size, error);
  if (!success)
    goto out;
  success = gdk_pixbuf_loader_close (pixbuf_loader, error);
  if (!success)
    goto out;

  pixbuf = gdk_pixbuf_loader_get_pixbuf (pixbuf_loader);

  width_before_rotation = gdk_pixbuf_get_width (pixbuf);

  rotated_pixbuf = gdk_pixbuf_apply_embedded_orientation (pixbuf);
  width_after_rotation = gdk_pixbuf_get_width (rotated_pixbuf);

  /* There is currently no way to tell if the pixbuf will need to be rotated before it is loaded,
   * so we only check that once it is loaded, and reload it again if it needs to be rotated in order
   * to use the available width and height correctly.
   * See http://bugzilla.gnome.org/show_bug.cgi?id=579003
   */
  if (width_before_rotation != width_after_rotation)
    {
      g_object_unref (pixbuf_loader);
      g_object_unref (rotated_pixbuf);
      rotated_pixbuf = NULL;

      pixbuf_loader = gdk_pixbuf_loader_new ();

      /* We know that the image will later be rotated, so we reverse the available dimensions. */
      available_dimensions.width = available_height;
      available_dimensions.height = available_width;
      available_dimensions.scale = scale;
      g_signal_connect (pixbuf_loader, "size-prepared",
                        G_CALLBACK (on_image_size_prepared), &available_dimensions);

      success = gdk_pixbuf_loader_write (pixbuf_loader, data, size, error);
      if (!success)
        goto out;

      success = gdk_pixbuf_loader_close (pixbuf_loader, error);
      if (!success)
        goto out;

      pixbuf = gdk_pixbuf_loader_get_pixbuf (pixbuf_loader);

      rotated_pixbuf = gdk_pixbuf_apply_embedded_orientation (pixbuf);
    }

out:
  if (pixbuf_loader)
    g_object_unref (pixbuf_loader);
  return rotated_pixbuf;
}

static GdkPixbuf *
impl_load_pixbuf_file (GFile          *file,
                       int             available_width,
                       int             available_height,
                       int             scale,
                       GError        **error)
{
  GdkPixbuf *pixbuf = NULL;
  char *contents = NULL;
  gsize size;

  if (g_file_load_contents (file, NULL, &contents, &size, NULL, error))
    {
      pixbuf = impl_load_pixbuf_data ((const guchar *) contents, size,
                                      available_width, available_height,
                                      scale,
                                      error);
    }

  g_free (contents);

  return pixbuf;
}

static void
load_pixbuf_thread (GTask        *result,
                    gpointer      source,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  GdkPixbuf *pixbuf;
  AsyncTextureLoadData *data = task_data;
  GError *error = NULL;

  g_assert (data != NULL);
  g_assert (data->file != NULL);

  pixbuf = impl_load_pixbuf_file (data->file, data->width, data->height, data->scale, &error);

  if (error != NULL)
    g_task_return_error (result, error);
  else if (pixbuf)
    g_task_return_pointer (result, g_object_ref (pixbuf), g_object_unref);

  g_clear_object (&pixbuf);
}

static GdkPixbuf *
load_pixbuf_async_finish (StTextureCache *cache, GAsyncResult *result, GError **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static CoglTexture *
pixbuf_to_cogl_texture (GdkPixbuf *pixbuf)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  CoglContext *ctx = clutter_backend_get_cogl_context (backend);
  CoglError *error = NULL;
  CoglTexture2D *texture;

  texture = cogl_texture_2d_new_from_data (ctx,
                                           gdk_pixbuf_get_width (pixbuf),
                                           gdk_pixbuf_get_height (pixbuf),
                                           gdk_pixbuf_get_has_alpha (pixbuf) ? COGL_PIXEL_FORMAT_RGBA_8888 : COGL_PIXEL_FORMAT_RGB_888,
                                           gdk_pixbuf_get_rowstride (pixbuf),
                                           gdk_pixbuf_get_pixels (pixbuf),
                                           &error);

  if (error)
    {
      g_warning ("Failed to allocate texture: %s", error->message);
      cogl_error_free (error);
    }

  return texture ? COGL_TEXTURE (texture) : NULL;
}

static cairo_surface_t *
pixbuf_to_cairo_surface (GdkPixbuf *pixbuf)
{
  cairo_surface_t *dummy_surface;
  cairo_pattern_t *pattern;
  cairo_surface_t *surface;
  cairo_t *cr;

  dummy_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);

  cr = cairo_create (dummy_surface);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  pattern = cairo_get_source (cr);
  cairo_pattern_get_surface (pattern, &surface);
  cairo_surface_reference (surface);
  cairo_destroy (cr);
  cairo_surface_destroy (dummy_surface);

  return surface;
}

static void
finish_texture_load (AsyncTextureLoadData *data,
                     GdkPixbuf            *pixbuf)
{
  GSList *iter;
  StTextureCache *cache;
  CoglTexture *texdata = NULL;

  cache = data->cache;

  g_hash_table_remove (cache->priv->outstanding_requests, data->key);

  if (pixbuf == NULL)
    goto out;

  texdata = pixbuf_to_cogl_texture (pixbuf);
  if (!texdata)
    goto out;

  if (data->policy != ST_TEXTURE_CACHE_POLICY_NONE)
    {
      gpointer orig_key, value;

      if (!g_hash_table_lookup_extended (cache->priv->keyed_cache, data->key,
                                         &orig_key, &value))
        {
          cogl_object_ref (texdata);
          g_hash_table_insert (cache->priv->keyed_cache, g_strdup (data->key),
                               texdata);
        }
    }

  for (iter = data->textures; iter; iter = iter->next)
    {
      ClutterTexture *texture = iter->data;
      set_texture_cogl_texture (texture, texdata);
    }

out:
  if (texdata)
    cogl_object_unref (texdata);

  texture_load_data_free (data);
}

static void
on_symbolic_icon_loaded (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  GdkPixbuf *pixbuf;
  pixbuf = gtk_icon_info_load_symbolic_finish (GTK_ICON_INFO (source), result, NULL, NULL);
  finish_texture_load (user_data, pixbuf);
  g_clear_object (&pixbuf);
}

static void
on_icon_loaded (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  GdkPixbuf *pixbuf;
  pixbuf = gtk_icon_info_load_icon_finish (GTK_ICON_INFO (source), result, NULL);
  finish_texture_load (user_data, pixbuf);
  g_clear_object (&pixbuf);
}

static void
on_pixbuf_loaded (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GdkPixbuf *pixbuf;
  pixbuf = load_pixbuf_async_finish (ST_TEXTURE_CACHE (source), result, NULL);
  finish_texture_load (user_data, pixbuf);
  g_clear_object (&pixbuf);
}

static void
load_texture_async (StTextureCache       *cache,
                    AsyncTextureLoadData *data)
{
  if (data->file)
    {
      GTask *task = g_task_new (cache, NULL, on_pixbuf_loaded, data);
      g_task_set_task_data (task, data, NULL);
      g_task_run_in_thread (task, load_pixbuf_thread);
      g_object_unref (task);
    }
  else if (data->icon_info)
    {
      StIconColors *colors = data->colors;
      if (colors)
        {
          GdkRGBA foreground_color;
          GdkRGBA success_color;
          GdkRGBA warning_color;
          GdkRGBA error_color;

          rgba_from_clutter (&foreground_color, &colors->foreground);
          rgba_from_clutter (&success_color, &colors->success);
          rgba_from_clutter (&warning_color, &colors->warning);
          rgba_from_clutter (&error_color, &colors->error);

          gtk_icon_info_load_symbolic_async (data->icon_info,
                                             &foreground_color, &success_color,
                                             &warning_color, &error_color,
                                             NULL, on_symbolic_icon_loaded, data);
        }
      else
        {
          gtk_icon_info_load_icon_async (data->icon_info, NULL, on_icon_loaded, data);
        }
    }
  else
    g_assert_not_reached ();
}

typedef struct {
  StTextureCache *cache;
  ClutterTexture *texture;
  GObject *source;
  guint notify_signal_id;
  gboolean weakref_active;
} StTextureCachePropertyBind;

static void
st_texture_cache_reset_texture (StTextureCachePropertyBind *bind,
                                const char                 *propname)
{
  cairo_surface_t *surface;
  CoglTexture *texdata;
  ClutterBackend *backend = clutter_get_default_backend ();
  CoglContext *ctx = clutter_backend_get_cogl_context (backend);

  g_object_get (bind->source, propname, &surface, NULL);

  if (surface != NULL &&
      cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE &&
      (cairo_image_surface_get_format (surface) == CAIRO_FORMAT_ARGB32 ||
       cairo_image_surface_get_format (surface) == CAIRO_FORMAT_RGB24))
    {
      CoglError *error = NULL;

      texdata = COGL_TEXTURE (cogl_texture_2d_new_from_data (ctx,
                                                             cairo_image_surface_get_width (surface),
                                                             cairo_image_surface_get_height (surface),
                                                             cairo_image_surface_get_format (surface) == CAIRO_FORMAT_ARGB32 ?
                                                             COGL_PIXEL_FORMAT_BGRA_8888 : COGL_PIXEL_FORMAT_BGR_888,
                                                             cairo_image_surface_get_stride (surface),
                                                             cairo_image_surface_get_data (surface),
                                                             &error));

      if (texdata)
        {
          clutter_texture_set_cogl_texture (bind->texture, texdata);
          cogl_object_unref (texdata);
        }
      else if (error)
        {
          g_warning ("Failed to allocate texture: %s", error->message);
          cogl_error_free (error);
        }

      clutter_actor_set_opacity (CLUTTER_ACTOR (bind->texture), 255);
    }
  else
    clutter_actor_set_opacity (CLUTTER_ACTOR (bind->texture), 0);
}

static void
st_texture_cache_on_pixbuf_notify (GObject           *object,
                                   GParamSpec        *paramspec,
                                   gpointer           data)
{
  StTextureCachePropertyBind *bind = data;
  st_texture_cache_reset_texture (bind, paramspec->name);
}

static void
st_texture_cache_bind_weak_notify (gpointer     data,
                                   GObject     *source_location)
{
  StTextureCachePropertyBind *bind = data;
  bind->weakref_active = FALSE;
  g_signal_handler_disconnect (bind->source, bind->notify_signal_id);
}

static void
st_texture_cache_free_bind (gpointer data)
{
  StTextureCachePropertyBind *bind = data;
  if (bind->weakref_active)
    g_object_weak_unref (G_OBJECT(bind->texture), st_texture_cache_bind_weak_notify, bind);
  g_free (bind);
}

/**
 * st_texture_cache_bind_cairo_surface_property:
 * @cache:
 * @object: A #GObject with a property @property_name of type #GdkPixbuf
 * @property_name: Name of a property
 *
 * Create a #ClutterTexture which tracks the #cairo_surface_t value of a GObject property
 * named by @property_name.  Unlike other methods in StTextureCache, the underlying
 * #CoglTexture is not shared by default with other invocations to this method.
 *
 * If the source object is destroyed, the texture will continue to show the last
 * value of the property.
 *
 * Return value: (transfer none): A new #ClutterActor
 */
ClutterActor *
st_texture_cache_bind_cairo_surface_property (StTextureCache    *cache,
                                              GObject           *object,
                                              const char        *property_name)
{
  ClutterTexture *texture;
  gchar *notify_key;
  StTextureCachePropertyBind *bind;

  texture = CLUTTER_TEXTURE (clutter_texture_new ());

  bind = g_new0 (StTextureCachePropertyBind, 1);
  bind->cache = cache;
  bind->texture = texture;
  bind->source = object;
  g_object_weak_ref (G_OBJECT (texture), st_texture_cache_bind_weak_notify, bind);
  bind->weakref_active = TRUE;

  st_texture_cache_reset_texture (bind, property_name);

  notify_key = g_strdup_printf ("notify::%s", property_name);
  bind->notify_signal_id = g_signal_connect_data (object, notify_key, G_CALLBACK(st_texture_cache_on_pixbuf_notify),
                                                  bind, (GClosureNotify)st_texture_cache_free_bind, 0);
  g_free (notify_key);

  return CLUTTER_ACTOR(texture);
}

/**
 * st_texture_cache_load: (skip)
 * @cache: A #StTextureCache
 * @key: Arbitrary string used to refer to item
 * @policy: Caching policy
 * @load: Function to create the texture, if not already cached
 * @data: User data passed to @load
 * @error: A #GError
 *
 * Load an arbitrary texture, caching it.  The string chosen for @key
 * should be of the form "type-prefix:type-uuid".  For example,
 * "url:file:///usr/share/icons/hicolor/48x48/apps/firefox.png", or
 * "stock-icon:gtk-ok".
 *
 * Returns: (transfer full): A newly-referenced handle to the texture
 */
CoglTexture *
st_texture_cache_load (StTextureCache       *cache,
                       const char           *key,
                       StTextureCachePolicy  policy,
                       StTextureCacheLoader  load,
                       void                 *data,
                       GError              **error)
{
  CoglTexture *texture;

  texture = g_hash_table_lookup (cache->priv->keyed_cache, key);
  if (!texture)
    {
      texture = load (cache, key, data, error);
      if (texture && policy == ST_TEXTURE_CACHE_POLICY_FOREVER)
        g_hash_table_insert (cache->priv->keyed_cache, g_strdup (key), texture);
    }

  if (texture && policy == ST_TEXTURE_CACHE_POLICY_FOREVER)
    cogl_object_ref (texture);

  return texture;
}

/**
 * ensure_request:
 * @cache:
 * @key: A cache key
 * @policy: Cache policy
 * @request: (out): If no request is outstanding, one will be created and returned here
 * @texture: A texture to be added to the request
 *
 * Check for any outstanding load for the data represented by @key.  If there
 * is already a request pending, append it to that request to avoid loading
 * the data multiple times.
 *
 * Returns: %TRUE iff there is already a request pending
 */
static gboolean
ensure_request (StTextureCache        *cache,
                const char            *key,
                StTextureCachePolicy   policy,
                AsyncTextureLoadData **request,
                ClutterActor          *texture)
{
  CoglTexture *texdata;
  AsyncTextureLoadData *pending;
  gboolean had_pending;

  texdata = g_hash_table_lookup (cache->priv->keyed_cache, key);

  if (texdata != NULL)
    {
      /* We had this cached already, just set the texture and we're done. */
      set_texture_cogl_texture (CLUTTER_TEXTURE (texture), texdata);
      return TRUE;
    }

  pending = g_hash_table_lookup (cache->priv->outstanding_requests, key);
  had_pending = pending != NULL;

  if (pending == NULL)
    {
      /* Not cached and no pending request, create it */
      *request = g_new0 (AsyncTextureLoadData, 1);
      if (policy != ST_TEXTURE_CACHE_POLICY_NONE)
        g_hash_table_insert (cache->priv->outstanding_requests, g_strdup (key), *request);
    }
  else
   *request = pending;

  /* Regardless of whether there was a pending request, prepend our texture here. */
  (*request)->textures = g_slist_prepend ((*request)->textures, g_object_ref (texture));

  return had_pending;
}

/**
 * st_texture_cache_load_gicon:
 * @cache: The texture cache instance
 * @theme_node: (nullable): The #StThemeNode to use for colors, or NULL
 *                            if the icon must not be recolored
 * @icon: the #GIcon to load
 * @size: Size of themed
 * @scale: Scale factor of display
 *
 * This method returns a new #ClutterActor for a given #GIcon. If the
 * icon isn't loaded already, the texture will be filled
 * asynchronously.
 *
 * Return Value: (transfer none): A new #ClutterActor for the icon, or %NULL if not found
 */
ClutterActor *
st_texture_cache_load_gicon (StTextureCache    *cache,
                             StThemeNode       *theme_node,
                             GIcon             *icon,
                             gint               size,
                             gint               scale)
{
  AsyncTextureLoadData *request;
  ClutterActor *texture;
  char *gicon_string;
  char *key;
  GtkIconTheme *theme;
  GtkIconInfo *info;
  StTextureCachePolicy policy;
  StIconColors *colors = NULL;
  StIconStyle icon_style = ST_ICON_STYLE_REQUESTED;
  GtkIconLookupFlags lookup_flags;

  if (theme_node)
    {
      colors = st_theme_node_get_icon_colors (theme_node);
      icon_style = st_theme_node_get_icon_style (theme_node);
    }

  /* Do theme lookups in the main thread to avoid thread-unsafety */
  theme = cache->priv->icon_theme;

  lookup_flags = GTK_ICON_LOOKUP_USE_BUILTIN;

  if (icon_style == ST_ICON_STYLE_REGULAR)
    lookup_flags |= GTK_ICON_LOOKUP_FORCE_REGULAR;
  else if (icon_style == ST_ICON_STYLE_SYMBOLIC)
    lookup_flags |= GTK_ICON_LOOKUP_FORCE_SYMBOLIC;

  if (clutter_get_default_text_direction () == CLUTTER_TEXT_DIRECTION_RTL)
    lookup_flags |= GTK_ICON_LOOKUP_DIR_RTL;
  else
    lookup_flags |= GTK_ICON_LOOKUP_DIR_LTR;

  info = gtk_icon_theme_lookup_by_gicon_for_scale (theme, icon, size, scale, lookup_flags);
  if (info == NULL)
    return NULL;

  gicon_string = g_icon_to_string (icon);
  /* A return value of NULL indicates that the icon can not be serialized,
   * so don't have a unique identifier for it as a cache key, and thus can't
   * be cached. If it is cachable, we hardcode a policy of FOREVER here for
   * now; we should actually blow this away on icon theme changes probably */
  policy = gicon_string != NULL ? ST_TEXTURE_CACHE_POLICY_FOREVER
                                : ST_TEXTURE_CACHE_POLICY_NONE;
  if (colors)
    {
      /* This raises some doubts about the practice of using string keys */
      key = g_strdup_printf (CACHE_PREFIX_ICON "%s,size=%d,scale=%d,style=%d,colors=%2x%2x%2x%2x,%2x%2x%2x%2x,%2x%2x%2x%2x,%2x%2x%2x%2x",
                             gicon_string, size, scale, icon_style,
                             colors->foreground.red, colors->foreground.blue, colors->foreground.green, colors->foreground.alpha,
                             colors->warning.red, colors->warning.blue, colors->warning.green, colors->warning.alpha,
                             colors->error.red, colors->error.blue, colors->error.green, colors->error.alpha,
                             colors->success.red, colors->success.blue, colors->success.green, colors->success.alpha);
    }
  else
    {
      key = g_strdup_printf (CACHE_PREFIX_ICON "%s,size=%d,scale=%d,style=%d",
                             gicon_string, size, scale, icon_style);
    }
  g_free (gicon_string);

  texture = (ClutterActor *) create_default_texture ();
  clutter_actor_set_size (texture, size * scale, size * scale);

  if (ensure_request (cache, key, policy, &request, texture))
    {
      /* If there's an outstanding request, we've just added ourselves to it */
      g_object_unref (info);
      g_free (key);
    }
  else
    {
      /* Else, make a new request */

      request->cache = cache;
      /* Transfer ownership of key */
      request->key = key;
      request->policy = policy;
      request->colors = colors ? st_icon_colors_ref (colors) : NULL;
      request->icon_info = info;
      request->width = request->height = size;
      request->scale = scale;

      load_texture_async (cache, request);
    }

  return CLUTTER_ACTOR (texture);
}

static ClutterActor *
load_from_pixbuf (GdkPixbuf *pixbuf)
{
  ClutterTexture *texture;
  CoglTexture *texdata;
  int width = gdk_pixbuf_get_width (pixbuf);
  int height = gdk_pixbuf_get_height (pixbuf);

  texture = create_default_texture ();

  clutter_actor_set_size (CLUTTER_ACTOR (texture), width, height);

  texdata = pixbuf_to_cogl_texture (pixbuf);

  set_texture_cogl_texture (texture, texdata);

  cogl_object_unref (texdata);
  return CLUTTER_ACTOR (texture);
}

static void
file_changed_cb (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other,
                 GFileMonitorEvent  event_type,
                 gpointer           user_data)
{
  StTextureCache *cache = user_data;
  char *key;
  guint file_hash;

  if (event_type != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    return;

  file_hash = g_file_hash (file);

  key = g_strdup_printf (CACHE_PREFIX_FILE "%u", file_hash);
  g_hash_table_remove (cache->priv->keyed_cache, key);
  g_free (key);

  key = g_strdup_printf (CACHE_PREFIX_FILE_FOR_CAIRO "%u", file_hash);
  g_hash_table_remove (cache->priv->keyed_surface_cache, key);
  g_free (key);

  g_signal_emit (cache, signals[TEXTURE_FILE_CHANGED], 0, file);
}

static void
ensure_monitor_for_file (StTextureCache *cache,
                         GFile          *file)
{
  StTextureCachePrivate *priv = cache->priv;

  /* No point in trying to monitor files that are part of a
   * GResource, since it does not support file monitoring.
   */
  if (g_file_has_uri_scheme (file, "resource"))
    return;

  if (g_hash_table_lookup (priv->file_monitors, file) == NULL)
    {
      GFileMonitor *monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE,
                                                   NULL, NULL);
      g_signal_connect (monitor, "changed",
                        G_CALLBACK (file_changed_cb), cache);
      g_hash_table_insert (priv->file_monitors, g_object_ref (file), monitor);
    }
}

typedef struct {
  GFile *gfile;
  gint   grid_width, grid_height;
  gint   scale_factor;
  ClutterActor *actor;
  GFunc load_callback;
  gpointer load_callback_data;
} AsyncImageData;

static void
on_data_destroy (gpointer data)
{
  AsyncImageData *d = (AsyncImageData *)data;
  g_object_unref (d->gfile);
  g_object_unref (d->actor);
  g_free (d);
}

static void
on_sliced_image_loaded (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
  GObject *cache = source_object;
  AsyncImageData *data = (AsyncImageData *)user_data;
  GTask *task = G_TASK (res);
  GList *list, *pixbufs;

  if (g_task_had_error (task))
    return;

  pixbufs = g_task_propagate_pointer (task, NULL);

  for (list = pixbufs; list; list = list->next)
    {
      ClutterActor *actor = load_from_pixbuf (GDK_PIXBUF (list->data));
      clutter_actor_hide (actor);
      clutter_actor_add_child (data->actor, actor);
    }

  g_list_free_full (pixbufs, g_object_unref);

  if (data->load_callback != NULL)
    data->load_callback (cache, data->load_callback_data);
}

static void
free_glist_unref_gobjects (gpointer p)
{
  g_list_free_full (p, g_object_unref);
}

static void
on_loader_size_prepared (GdkPixbufLoader *loader,
                         gint width,
                         gint height,
                         gpointer user_data)
{
  AsyncImageData *data = user_data;
  gdk_pixbuf_loader_set_size (loader,
                              width * data->scale_factor,
                              height * data->scale_factor);
}

static void
load_sliced_image (GTask        *result,
                   gpointer      object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  AsyncImageData *data;
  GList *res = NULL;
  GdkPixbuf *pix;
  gint width, height, y, x;
  GdkPixbufLoader *loader;
  GError *error = NULL;
  gchar *buffer = NULL;
  gsize length;

  g_assert (!cancellable);

  data = task_data;
  g_assert (data);

  loader = gdk_pixbuf_loader_new ();
  g_signal_connect (loader, "size-prepared", G_CALLBACK (on_loader_size_prepared), data);

  if (!g_file_load_contents (data->gfile, NULL, &buffer, &length, NULL, &error))
    {
      g_warning ("Failed to open sliced image: %s", error->message);
      goto out;
    }

  if (!gdk_pixbuf_loader_write (loader, (const guchar *) buffer, length, &error))
    {
      g_warning ("Failed to load image: %s", error->message);
      goto out;
    }

  if (!gdk_pixbuf_loader_close (loader, NULL))
    goto out;

  pix = gdk_pixbuf_loader_get_pixbuf (loader);
  width = gdk_pixbuf_get_width (pix);
  height = gdk_pixbuf_get_height (pix);
  for (y = 0; y < height; y += data->grid_height * data->scale_factor)
    {
      for (x = 0; x < width; x += data->grid_width * data->scale_factor)
        {
          GdkPixbuf *pixbuf = gdk_pixbuf_new_subpixbuf (pix, x, y,
                                                        data->grid_width * data->scale_factor,
                                                        data->grid_height * data->scale_factor);
          g_assert (pixbuf != NULL);
          res = g_list_append (res, pixbuf);
        }
    }

 out:
  /* We don't need the original pixbuf anymore, which is owned by the loader,
   * though the subpixbufs will hold a reference. */
  g_object_unref (loader);
  g_free (buffer);
  g_clear_pointer (&error, g_error_free);
  g_task_return_pointer (result, res, free_glist_unref_gobjects);
}

/**
 * st_texture_cache_load_sliced_image:
 * @cache: A #StTextureCache
 * @file: A #GFile
 * @grid_width: Width in pixels
 * @grid_height: Height in pixels
 * @scale: Scale factor of the display
 * @load_callback: (scope async) (nullable): Function called when the image is loaded, or %NULL
 * @user_data: Data to pass to the load callback
 *
 * This function reads a single image file which contains multiple images internally.
 * The image file will be divided using @grid_width and @grid_height;
 * note that the dimensions of the image loaded from @path 
 * should be a multiple of the specified grid dimensions.
 *
 * Returns: (transfer none): A new #ClutterActor
 */
ClutterActor *
st_texture_cache_load_sliced_image (StTextureCache *cache,
                                    GFile          *file,
                                    gint            grid_width,
                                    gint            grid_height,
                                    gint            scale,
                                    GFunc           load_callback,
                                    gpointer        user_data)
{
  AsyncImageData *data;
  GTask *result;
  ClutterActor *actor = clutter_actor_new ();

  data = g_new0 (AsyncImageData, 1);
  data->grid_width = grid_width;
  data->grid_height = grid_height;
  data->scale_factor = scale;
  data->gfile = g_object_ref (file);
  data->actor = actor;
  data->load_callback = load_callback;
  data->load_callback_data = user_data;
  g_object_ref (G_OBJECT (actor));

  result = g_task_new (cache, NULL, on_sliced_image_loaded, data);
  g_task_set_task_data (result, data, on_data_destroy);
  g_task_run_in_thread (result, load_sliced_image);

  g_object_unref (result);

  return actor;
}

/**
 * st_texture_cache_load_file_async:
 * @cache: The texture cache instance
 * @file: a #GFile of the image file from which to create a pixbuf
 * @available_width: available width for the image, can be -1 if not limited
 * @available_height: available height for the image, can be -1 if not limited
 * @scale: scale factor of the display
 *
 * Asynchronously load an image.   Initially, the returned texture will have a natural
 * size of zero.  At some later point, either the image will be loaded successfully
 * and at that point size will be negotiated, or upon an error, no image will be set.
 *
 * Return value: (transfer none): A new #ClutterActor with no image loaded initially.
 */
ClutterActor *
st_texture_cache_load_file_async (StTextureCache *cache,
                                  GFile          *file,
                                  int             available_width,
                                  int             available_height,
                                  int             scale)
{
  ClutterActor *texture;
  AsyncTextureLoadData *request;
  StTextureCachePolicy policy;
  gchar *key;

  key = g_strdup_printf (CACHE_PREFIX_FILE "%u", g_file_hash (file));

  policy = ST_TEXTURE_CACHE_POLICY_NONE; /* XXX */

  texture = (ClutterActor *) create_default_texture ();

  if (ensure_request (cache, key, policy, &request, texture))
    {
      /* If there's an outstanding request, we've just added ourselves to it */
      g_free (key);
    }
  else
    {
      /* Else, make a new request */

      request->cache = cache;
      /* Transfer ownership of key */
      request->key = key;
      request->file = g_object_ref (file);
      request->policy = policy;
      request->width = available_width;
      request->height = available_height;
      request->scale = scale;

      load_texture_async (cache, request);
    }

  ensure_monitor_for_file (cache, file);

  return CLUTTER_ACTOR (texture);
}

static CoglTexture *
st_texture_cache_load_file_sync_to_cogl_texture (StTextureCache *cache,
                                                 StTextureCachePolicy policy,
                                                 GFile          *file,
                                                 int             available_width,
                                                 int             available_height,
                                                 int             scale,
                                                 GError         **error)
{
  CoglTexture *texdata;
  GdkPixbuf *pixbuf;
  char *key;

  key = g_strdup_printf (CACHE_PREFIX_FILE "%u", g_file_hash (file));

  texdata = g_hash_table_lookup (cache->priv->keyed_cache, key);

  if (texdata == NULL)
    {
      pixbuf = impl_load_pixbuf_file (file, available_width, available_height, scale, error);
      if (!pixbuf)
        goto out;

      texdata = pixbuf_to_cogl_texture (pixbuf);
      g_object_unref (pixbuf);

      if (!texdata)
        goto out;

      if (policy == ST_TEXTURE_CACHE_POLICY_FOREVER)
        {
          cogl_object_ref (texdata);
          g_hash_table_insert (cache->priv->keyed_cache, g_strdup (key), texdata);
        }
    }
  else
    cogl_object_ref (texdata);

  ensure_monitor_for_file (cache, file);

out:
  g_free (key);
  return texdata;
}

static cairo_surface_t *
st_texture_cache_load_file_sync_to_cairo_surface (StTextureCache        *cache,
                                                  StTextureCachePolicy   policy,
                                                  GFile                 *file,
                                                  int                    available_width,
                                                  int                    available_height,
                                                  int                    scale,
                                                  GError               **error)
{
  cairo_surface_t *surface;
  GdkPixbuf *pixbuf;
  char *key;

  key = g_strdup_printf (CACHE_PREFIX_FILE_FOR_CAIRO "%u", g_file_hash (file));

  surface = g_hash_table_lookup (cache->priv->keyed_surface_cache, key);

  if (surface == NULL)
    {
      pixbuf = impl_load_pixbuf_file (file, available_width, available_height, scale, error);
      if (!pixbuf)
        goto out;

      surface = pixbuf_to_cairo_surface (pixbuf);
      g_object_unref (pixbuf);

      if (policy == ST_TEXTURE_CACHE_POLICY_FOREVER)
        {
          cairo_surface_reference (surface);
          g_hash_table_insert (cache->priv->keyed_surface_cache,
                               g_strdup (key), surface);
        }
    }
  else
    cairo_surface_reference (surface);

  ensure_monitor_for_file (cache, file);

out:
  g_free (key);
  return surface;
}

/**
 * st_texture_cache_load_file_to_cogl_texture: (skip)
 * @cache: A #StTextureCache
 * @file: A #GFile in supported image format
 * @scale: Scale factor of the display
 *
 * This function synchronously loads the given file path
 * into a COGL texture.  On error, a warning is emitted
 * and %NULL is returned.
 *
 * Returns: (transfer full): a new #CoglTexture
 */
CoglTexture *
st_texture_cache_load_file_to_cogl_texture (StTextureCache *cache,
                                            GFile          *file,
                                            gint            scale)
{
  CoglTexture *texture;
  GError *error = NULL;

  texture = st_texture_cache_load_file_sync_to_cogl_texture (cache, ST_TEXTURE_CACHE_POLICY_FOREVER,
                                                             file, -1, -1, scale, &error);

  if (texture == NULL)
    {
      char *uri = g_file_get_uri (file);
      g_warning ("Failed to load %s: %s", uri, error->message);
      g_clear_error (&error);
      g_free (uri);
    }

  return texture;
}

/**
 * st_texture_cache_load_file_to_cairo_surface:
 * @cache: A #StTextureCache
 * @file: A #GFile in supported image format
 * @scale: Scale factor of the display
 *
 * This function synchronously loads the given file path
 * into a cairo surface.  On error, a warning is emitted
 * and %NULL is returned.
 *
 * Returns: (transfer full): a new #cairo_surface_t
 */
cairo_surface_t *
st_texture_cache_load_file_to_cairo_surface (StTextureCache *cache,
                                             GFile          *file,
                                             gint            scale)
{
  cairo_surface_t *surface;
  GError *error = NULL;

  surface = st_texture_cache_load_file_sync_to_cairo_surface (cache, ST_TEXTURE_CACHE_POLICY_FOREVER,
                                                              file, -1, -1, scale, &error);

  if (surface == NULL)
    {
      char *uri = g_file_get_uri (file);
      g_warning ("Failed to load %s: %s", uri, error->message);
      g_clear_error (&error);
      g_free (uri);
    }

  return surface;
}

static StTextureCache *instance = NULL;

/**
 * st_texture_cache_get_default:
 *
 * Return value: (transfer none): The global texture cache
 */
StTextureCache*
st_texture_cache_get_default (void)
{
  if (instance == NULL)
    instance = g_object_new (ST_TYPE_TEXTURE_CACHE, NULL);
  return instance;
}
