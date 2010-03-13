/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "st-texture-cache.h"
#include <gtk/gtk.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-desktop-thumbnail.h>
#include <string.h>
#include <glib.h>

#define CACHE_PREFIX_GICON "gicon:"
#define CACHE_PREFIX_URI "uri:"
#define CACHE_PREFIX_THUMBNAIL_URI "thumbnail-uri:"
#define CACHE_PREFIX_RAW_CHECKSUM "raw-checksum:"
#define CACHE_PREFIX_COMPRESSED_CHECKSUM "compressed-checksum:"

struct _StTextureCachePrivate
{
  /* Things that were loaded with a cache policy != NONE */
  GHashTable *keyed_cache; /* char * -> CoglTexture* */
  /* Presently this is used to de-duplicate requests for GIcons,
   * it could in theory be extended to async URL loading and other
   * cases too.
   */
  GHashTable *outstanding_requests; /* char * -> AsyncTextureLoadData * */
  GnomeDesktopThumbnailFactory *thumbnails;
};

static void st_texture_cache_dispose (GObject *object);
static void st_texture_cache_finalize (GObject *object);

G_DEFINE_TYPE(StTextureCache, st_texture_cache, G_TYPE_OBJECT);

/* We want to preserve the aspect ratio by default, also the default
 * material for an empty texture is full opacity white, which we
 * definitely don't want.  Skip that by setting 0 opacity.
 */
static ClutterTexture *
create_default_texture (StTextureCache *self)
{
  ClutterTexture * texture = CLUTTER_TEXTURE (clutter_texture_new ());
  g_object_set (texture, "keep-aspect-ratio", TRUE, "opacity", 0, NULL);
  return texture;
}

/* Reverse the opacity we added while loading */
static void
set_texture_cogl_texture (ClutterTexture *clutter_texture, CoglHandle cogl_texture)
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
}

static void
st_texture_cache_init (StTextureCache *self)
{
  self->priv = g_new0 (StTextureCachePrivate, 1);
  self->priv->keyed_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   g_free, cogl_handle_unref);
  self->priv->outstanding_requests = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                            g_free, NULL);
  self->priv->thumbnails = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);
}

static void
st_texture_cache_dispose (GObject *object)
{
  StTextureCache *self = (StTextureCache*)object;

  if (self->priv->keyed_cache)
    g_hash_table_destroy (self->priv->keyed_cache);
  self->priv->keyed_cache = NULL;

  if (self->priv->outstanding_requests)
    g_hash_table_destroy (self->priv->outstanding_requests);
  self->priv->outstanding_requests = NULL;

  if (self->priv->thumbnails)
    g_object_unref (self->priv->thumbnails);
  self->priv->thumbnails = NULL;

  G_OBJECT_CLASS (st_texture_cache_parent_class)->dispose (object);
}

static void
st_texture_cache_finalize (GObject *object)
{
  G_OBJECT_CLASS (st_texture_cache_parent_class)->finalize (object);
}

typedef struct {
  StTextureCache *cache;
  char *uri;
  char *mimetype;
  gboolean thumbnail;
  GIcon *icon;
  GtkRecentInfo *recent_info;
  GtkIconInfo *icon_info;
  gint width;
  gint height;
  gpointer user_data;
} AsyncIconLookupData;

static gboolean
compute_pixbuf_scale (gint      width,
                      gint      height,
                      gint      available_width,
                      gint      available_height,
                      gint     *new_width,
                      gint     *new_height)
{
  int scaled_width, scaled_height;

  if (width == 0 || height == 0)
    return FALSE;

  if (available_width >= 0 && available_height >= 0)
    {
      // This should keep the aspect ratio of the image intact, because if
      // available_width < (available_height * width) / height
      // than
      // (available_width * height) / width < available_height
      // So we are guaranteed to either scale the image to have an available_width
      // for width and height scaled accordingly OR have the available_height
      // for height and width scaled accordingly, whichever scaling results
      // in the image that can fit both available dimensions.
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

  // Scale the image only if that will not increase its original dimensions.
  if (scaled_width > 0 && scaled_height > 0 && scaled_width < width && scaled_height < height)
    {
      *new_width = scaled_width;
      *new_height = scaled_height;
      return TRUE;
    }
  return FALSE;
}

static GdkPixbuf *
impl_load_pixbuf_gicon (GIcon       *icon,
                        GtkIconInfo *info,
                        int          size,
                        GError     **error)
{
  int scaled_width, scaled_height;
  GdkPixbuf *pixbuf = gtk_icon_info_load_icon (info, error);
  int width, height;

  if (!pixbuf)
    return NULL;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  if (compute_pixbuf_scale (width,
                            height,
                            size, size,
                            &scaled_width, &scaled_height))
    {
      GdkPixbuf *scaled = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
      g_object_unref (pixbuf);
      pixbuf = scaled;
    }
  return pixbuf;
}

// A private structure for keeping width and height.
typedef struct {
  int width;
  int height;
} Dimensions;

static void
icon_lookup_data_destroy (gpointer p)
{
  AsyncIconLookupData *data = p;

  if (data->icon)
    {
      g_object_unref (data->icon);
      gtk_icon_info_free (data->icon_info);
    }
  else if (data->uri)
    g_free (data->uri);
  if (data->mimetype)
    g_free (data->mimetype);
  if (data->recent_info)
    gtk_recent_info_unref (data->recent_info);

  g_free (data);
}

/**
 * on_image_size_prepared:
 *
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
  int scaled_width;
  int scaled_height;

  if (compute_pixbuf_scale (width, height, available_width, available_height,
                            &scaled_width, &scaled_height))
    gdk_pixbuf_loader_set_size (pixbuf_loader, scaled_width, scaled_height);
}

static GdkPixbuf *
impl_load_pixbuf_data (const guchar   *data,
                       gsize           size,
                       int             available_width,
                       int             available_height,
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

  // There is currently no way to tell if the pixbuf will need to be rotated before it is loaded,
  // so we only check that once it is loaded, and reload it again if it needs to be rotated in order
  // to use the available width and height correctly.
  // http://bugzilla.gnome.org/show_bug.cgi?id=579003
  if (width_before_rotation != width_after_rotation)
    {
      g_object_unref (pixbuf_loader);
      g_object_unref (rotated_pixbuf);
      rotated_pixbuf = NULL;

      pixbuf_loader = gdk_pixbuf_loader_new ();

      // We know that the image will later be rotated, so we reverse the available dimensions.
      available_dimensions.width = available_height;
      available_dimensions.height = available_width;
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
impl_load_pixbuf_file (const char     *uri,
                       int             available_width,
                       int             available_height,
                       GError        **error)
{
  GdkPixbuf *pixbuf = NULL;
  GFile *file;
  char *contents = NULL;
  gsize size;

  file = g_file_new_for_uri (uri);
  if (g_file_load_contents (file, NULL, &contents, &size, NULL, error))
    {
      pixbuf = impl_load_pixbuf_data ((const guchar *) contents, size,
                                      available_width, available_height,
                                      error);
    }

  g_object_unref (file);
  g_free (contents);

  return pixbuf;
}

static GdkPixbuf *
impl_load_thumbnail (StTextureCache    *cache,
                     const char        *uri,
                     const char        *mime_type,
                     guint              size,
                     GError           **error)
{
  GnomeDesktopThumbnailFactory *thumbnail_factory;
  GdkPixbuf *pixbuf = NULL;
  GFile *file;
  GFileInfo *file_info;
  GTimeVal mtime_g;
  time_t mtime = 0;
  char *existing_thumbnail;

  file = g_file_new_for_uri (uri);
  file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NONE, NULL, NULL);
  g_object_unref (file);
  if (file_info)
    {
      g_file_info_get_modification_time (file_info, &mtime_g);
      g_object_unref (file_info);
      mtime = (time_t) mtime_g.tv_sec;
    }

  thumbnail_factory = cache->priv->thumbnails;

  existing_thumbnail = gnome_desktop_thumbnail_factory_lookup (thumbnail_factory, uri, mtime);

  if (existing_thumbnail != NULL)
    {
      pixbuf = gdk_pixbuf_new_from_file_at_size (existing_thumbnail, size, size, error);
      g_free (existing_thumbnail);
    }
  else if (gnome_desktop_thumbnail_factory_has_valid_failed_thumbnail (thumbnail_factory, uri, mtime))
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Has failed thumbnail");
  else if (gnome_desktop_thumbnail_factory_can_thumbnail (thumbnail_factory, uri, mime_type, mtime))
    {
      pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (thumbnail_factory, uri, mime_type);
      if (pixbuf)
        {
          // we need to save the thumbnail so that we don't need to generate it again in the future
          gnome_desktop_thumbnail_factory_save_thumbnail (thumbnail_factory, pixbuf, uri, mtime);
        }
      else
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to generate thumbnail");
          gnome_desktop_thumbnail_factory_create_failed_thumbnail (thumbnail_factory, uri, mtime);
        }
     }
   return pixbuf;
}

static GIcon *
icon_for_mimetype (const char *mimetype)
{
  char *content_type;
  GIcon *icon;

  content_type = g_content_type_from_mime_type (mimetype);
  if (!content_type)
    return NULL;

  icon = g_content_type_get_icon (content_type);
  g_free (content_type);
  return icon;
}

static void
load_pixbuf_thread (GSimpleAsyncResult *result,
                    GObject *object,
                    GCancellable *cancellable)
{
  GdkPixbuf *pixbuf;
  AsyncIconLookupData *data;
  GError *error = NULL;

  data = g_object_get_data (G_OBJECT (result), "load_pixbuf_async");
  g_assert (data != NULL);

  if (data->thumbnail)
    {
      const char *uri;
      const char *mimetype;

      if (data->recent_info)
        {
          uri = gtk_recent_info_get_uri (data->recent_info);
          mimetype = gtk_recent_info_get_mime_type (data->recent_info);
        }
      else
        {
          uri = data->uri;
          mimetype = data->mimetype;
        }
      pixbuf = impl_load_thumbnail (data->cache, uri, mimetype, data->width, &error);
    }
  else if (data->uri)
    pixbuf = impl_load_pixbuf_file (data->uri, data->width, data->height, &error);
  else if (data->icon)
    pixbuf = impl_load_pixbuf_gicon (data->icon, data->icon_info, data->width, &error);
  else
    g_assert_not_reached ();

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      return;
    }

  if (pixbuf)
    g_simple_async_result_set_op_res_gpointer (result, g_object_ref (pixbuf),
                                               g_object_unref);
}

/**
 * load_icon_pixbuf_async:
 *
 * Asynchronously load the #GdkPixbuf associated with a #GIcon.  Currently
 * the #GtkIconInfo must have already been provided.
 */
static void
load_icon_pixbuf_async (StTextureCache       *cache,
                        GIcon                *icon,
                        GtkIconInfo          *icon_info,
                        gint                  size,
                        GCancellable         *cancellable,
                        GAsyncReadyCallback   callback,
                        gpointer              user_data)
{
  GSimpleAsyncResult *result;
  AsyncIconLookupData *data;

  data = g_new0 (AsyncIconLookupData, 1);
  data->cache = cache;
  data->icon = g_object_ref (icon);
  data->icon_info = gtk_icon_info_copy (icon_info);
  data->width = data->height = size;
  data->user_data = user_data;

  result = g_simple_async_result_new (G_OBJECT (cache), callback, user_data, load_icon_pixbuf_async);

  g_object_set_data_full (G_OBJECT (result), "load_pixbuf_async", data, icon_lookup_data_destroy);
  g_simple_async_result_run_in_thread (result, load_pixbuf_thread, G_PRIORITY_DEFAULT, cancellable);

  g_object_unref (result);
}

static void
load_uri_pixbuf_async (StTextureCache     *cache,
                       const char         *uri,
                       guint               width,
                       guint               height,
                       GCancellable       *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer            user_data)
{
  GSimpleAsyncResult *result;
  AsyncIconLookupData *data;

  data = g_new0 (AsyncIconLookupData, 1);
  data->cache = cache;
  data->uri = g_strdup (uri);
  data->width = width;
  data->height = height;
  data->user_data = user_data;

  result = g_simple_async_result_new (G_OBJECT (cache), callback, user_data, load_uri_pixbuf_async);

  g_object_set_data_full (G_OBJECT (result), "load_pixbuf_async", data, icon_lookup_data_destroy);
  g_simple_async_result_run_in_thread (result, load_pixbuf_thread, G_PRIORITY_DEFAULT, cancellable);

  g_object_unref (result);
}

static void
load_thumbnail_async (StTextureCache     *cache,
                      const char         *uri,
                      const char         *mimetype,
                      guint               size,
                      GCancellable       *cancellable,
                      GAsyncReadyCallback callback,
                      gpointer            user_data)
{
  GSimpleAsyncResult *result;
  AsyncIconLookupData *data;

  data = g_new0 (AsyncIconLookupData, 1);
  data->cache = cache;
  data->uri = g_strdup (uri);
  data->mimetype = g_strdup (mimetype);
  data->thumbnail = TRUE;
  data->width = size;
  data->height = size;
  data->user_data = user_data;

  result = g_simple_async_result_new (G_OBJECT (cache), callback, user_data, load_thumbnail_async);

  g_object_set_data_full (G_OBJECT (result), "load_pixbuf_async", data, icon_lookup_data_destroy);
  g_simple_async_result_run_in_thread (result, load_pixbuf_thread, G_PRIORITY_DEFAULT, cancellable);

  g_object_unref (result);
}

static void
load_recent_thumbnail_async (StTextureCache     *cache,
                             GtkRecentInfo      *info,
                             guint               size,
                             GCancellable       *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer            user_data)
{
  GSimpleAsyncResult *result;
  AsyncIconLookupData *data;

  data = g_new0 (AsyncIconLookupData, 1);
  data->cache = cache;
  data->thumbnail = TRUE;
  data->recent_info = gtk_recent_info_ref (info);
  data->width = size;
  data->height = size;
  data->user_data = user_data;

  result = g_simple_async_result_new (G_OBJECT (cache), callback, user_data, load_recent_thumbnail_async);

  g_object_set_data_full (G_OBJECT (result), "load_pixbuf_async", data, icon_lookup_data_destroy);
  g_simple_async_result_run_in_thread (result, load_pixbuf_thread, G_PRIORITY_DEFAULT, cancellable);

  g_object_unref (result);
}

static GdkPixbuf *
load_pixbuf_async_finish (StTextureCache *cache, GAsyncResult *result, GError **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;
  return g_simple_async_result_get_op_res_gpointer (simple);
}

typedef struct {
  StTextureCachePolicy policy;
  char *key;
  char *uri;
  gboolean thumbnail;
  char *mimetype;
  GtkRecentInfo *recent_info;
  char *checksum;
  GIcon *icon;
  GtkIconInfo *icon_info;
  guint width;
  guint height;
  GSList *textures;
} AsyncTextureLoadData;

static CoglHandle
pixbuf_to_cogl_handle (GdkPixbuf *pixbuf)
{
  return cogl_texture_new_from_data (gdk_pixbuf_get_width (pixbuf),
                                     gdk_pixbuf_get_height (pixbuf),
                                     COGL_TEXTURE_NONE,
                                     gdk_pixbuf_get_has_alpha (pixbuf) ? COGL_PIXEL_FORMAT_RGBA_8888 : COGL_PIXEL_FORMAT_RGB_888,
                                     COGL_PIXEL_FORMAT_ANY,
                                     gdk_pixbuf_get_rowstride (pixbuf),
                                     gdk_pixbuf_get_pixels (pixbuf));
}

static GdkPixbuf *
load_pixbuf_fallback(AsyncTextureLoadData *data)
{
  GdkPixbuf *pixbuf = NULL;

  if (data->thumbnail)
    {

      GtkIconTheme *theme = gtk_icon_theme_get_default ();

      if (data->recent_info)
          pixbuf = gtk_recent_info_get_icon (data->recent_info, data->width);
      else
        {
          GIcon *icon = icon_for_mimetype (data->mimetype);
          if (icon != NULL)
            {
              GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon (theme,
                                                                       icon,
                                                                       data->width,
                                                                       GTK_ICON_LOOKUP_USE_BUILTIN);
              g_object_unref (icon);
              if (icon_info != NULL)
                pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
            }
        }

      if (pixbuf == NULL)
        pixbuf = gtk_icon_theme_load_icon (theme,
                                           "gtk-file",
                                           data->width,
                                           GTK_ICON_LOOKUP_USE_BUILTIN,
                                           NULL);
    }
  /* Maybe we could need a fallback for outher image types? */

  return pixbuf;
}

static void
on_pixbuf_loaded (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GSList *iter;
  StTextureCache *cache;
  AsyncTextureLoadData *data;
  GdkPixbuf *pixbuf;
  GError *error = NULL;
  CoglHandle texdata = NULL;

  data = user_data;
  cache = ST_TEXTURE_CACHE (source);

  g_hash_table_remove (cache->priv->outstanding_requests, data->key);

  pixbuf = load_pixbuf_async_finish (cache, result, &error);
  if (pixbuf == NULL)
    pixbuf = load_pixbuf_fallback (data);
  if (pixbuf == NULL)
    goto out;

  texdata = pixbuf_to_cogl_handle (pixbuf);

  g_object_unref (pixbuf);

  if (data->policy != ST_TEXTURE_CACHE_POLICY_NONE)
    {
      gpointer orig_key, value;

      if (!g_hash_table_lookup_extended (cache->priv->keyed_cache, data->key,
                                         &orig_key, &value))
        {
          cogl_handle_ref (texdata);
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
    cogl_handle_unref (texdata);
  g_free (data->key);

  if (data->icon)
    {
      gtk_icon_info_free (data->icon_info);
      g_object_unref (data->icon);
    }
  else if (data->uri)
    g_free (data->uri);

  if (data->recent_info)
    gtk_recent_info_unref (data->recent_info);
  if (data->mimetype)
    g_free (data->mimetype);

  /* Alternatively we could weakref and just do nothing if the texture
     is destroyed */
  for (iter = data->textures; iter; iter = iter->next)
    {
      ClutterTexture *texture = iter->data;
      g_object_unref (texture);
    }

  g_clear_error (&error);
  g_free (data);
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
  GdkPixbuf *pixbuf;
  CoglHandle texdata;

  g_object_get (bind->source, propname, &pixbuf, NULL);

  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  if (pixbuf != NULL)
    {
      texdata = pixbuf_to_cogl_handle (pixbuf);
      g_object_unref (pixbuf);

      clutter_texture_set_cogl_texture (bind->texture, texdata);
      cogl_handle_unref (texdata);

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
 * st_texture_cache_bind_pixbuf_property:
 * @cache:
 * @object: A #GObject with a property @property_name of type #GdkPixbuf
 * @property_name: Name of a property
 *
 * Create a #ClutterTexture which tracks the #GdkPixbuf value of a GObject property
 * named by @property_name.  Unlike other methods in StTextureCache, the underlying
 * CoglHandle is not shared by default with other invocations to this method.
 *
 * If the source object is destroyed, the texture will continue to show the last
 * value of the property.
 *
 * Return value: (transfer none): A new #ClutterActor
 */
ClutterActor *
st_texture_cache_bind_pixbuf_property (StTextureCache    *cache,
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
 * st_texture_cache_load:
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
CoglHandle
st_texture_cache_load (StTextureCache       *cache,
                       const char           *key,
                       StTextureCachePolicy  policy,
                       StTextureCacheLoader  load,
                       void                 *data,
                       GError              **error)
{
  CoglHandle texture;

  texture = g_hash_table_lookup (cache->priv->keyed_cache, key);
  if (!texture)
    {
      texture = load (cache, key, data, error);
      if (texture)
        g_hash_table_insert (cache->priv->keyed_cache, g_strdup (key), texture);
      else
        return COGL_INVALID_HANDLE;
    }
  cogl_handle_ref (texture);
  return texture;
}

/**
 * create_texture_and_ensure_request:
 * @cache:
 * @key: A cache key
 * @size: Size in pixels
 * @request: (out): If no request is outstanding, one will be created and returned here
 * @texture: (out): A new texture, also added to the request
 *
 * Check for any outstanding load for the data represented by @key.  If there
 * is already a request pending, append it to that request to avoid loading
 * the data multiple times.
 *
 * Returns: %TRUE iff there is already a request pending
 */
static gboolean
create_texture_and_ensure_request (StTextureCache        *cache,
                                   const char            *key,
                                   guint                  size,
                                   AsyncTextureLoadData **request,
                                   ClutterActor         **texture)
{
  CoglHandle texdata;
  AsyncTextureLoadData *pending;
  gboolean had_pending;

  *texture = (ClutterActor *) create_default_texture (cache);
  clutter_actor_set_size (*texture, size, size);

  texdata = g_hash_table_lookup (cache->priv->keyed_cache, key);

  if (texdata != NULL)
    {
      /* We had this cached already, just set the texture and we're done. */
      set_texture_cogl_texture (CLUTTER_TEXTURE (*texture), texdata);
      return TRUE;
    }

  pending = g_hash_table_lookup (cache->priv->outstanding_requests, key);
  had_pending = pending != NULL;

  if (pending == NULL)
    {
      /* Not cached and no pending request, create it */
      *request = g_new0 (AsyncTextureLoadData, 1);
      (*request)->key = g_strdup (key);
      g_hash_table_insert (cache->priv->outstanding_requests, g_strdup (key), *request);
    }
  else
   *request = pending;

  /* Regardless of whether there was a pending request, prepend our texture here. */
  (*request)->textures = g_slist_prepend ((*request)->textures, g_object_ref (*texture));

  return had_pending;
}

/**
 * st_texture_cache_load_gicon:
 *
 * This method returns a new #ClutterClone for a given #GIcon.  If the
 * icon isn't loaded already, the texture will be filled asynchronously.
 *
 * Return Value: (transfer none): A new #ClutterActor for the icon
 */
ClutterActor *
st_texture_cache_load_gicon (StTextureCache    *cache,
                             GIcon             *icon,
                             gint               size)
{
  AsyncTextureLoadData *request;
  ClutterActor *texture;
  char *gicon_string;
  char *key;
  GtkIconTheme *theme;
  GtkIconInfo *info;

  gicon_string = g_icon_to_string (icon);
  key = g_strdup_printf (CACHE_PREFIX_GICON "icon=%s,size=%d", gicon_string, size);
  g_free (gicon_string);

  if (create_texture_and_ensure_request (cache, key, size, &request, &texture))
    {
      g_free (key);
      return texture;
    }

  /* Do theme lookups in the main thread to avoid thread-unsafety */
  theme = gtk_icon_theme_get_default ();

  info = gtk_icon_theme_lookup_by_gicon (theme, icon, size, GTK_ICON_LOOKUP_USE_BUILTIN);
  if (info != NULL)
    {
      /* hardcoded here for now; we should actually blow this away on
       * icon theme changes probably */
      request->key = g_strdup (key);
      request->policy = ST_TEXTURE_CACHE_POLICY_FOREVER;
      request->icon = g_object_ref (icon);
      request->icon_info = info;
      request->width = request->height = size;

      load_icon_pixbuf_async (cache, icon, info, size, NULL, on_pixbuf_loaded, request);
    }
  else
    {
      /* Blah; we failed to find the icon, but we've added our texture to the outstanding
       * requests.  In that case, just undo what create_texture_lookup_status did.
       */
       g_slist_foreach (request->textures, (GFunc) g_object_unref, NULL);
       g_slist_free (request->textures);
       g_free (request);
       g_hash_table_remove (cache->priv->outstanding_requests, key);
    }

  g_free (key);
  return CLUTTER_ACTOR (texture);
}

/**
 * st_texture_cache_load_icon_name:
 * @cache: The texture cache instance
 * @name: Name of a themed icon
 * @size: Size of themed
 *
 * Load a themed icon into a texture.
 *
 * Return Value: (transfer none): A new #ClutterTexture for the icon
 */
ClutterActor *
st_texture_cache_load_icon_name (StTextureCache    *cache,
                                 const char        *name,
                                 gint               size)
{
  ClutterActor *texture;
  GIcon *themed;

  themed = g_themed_icon_new (name);
  texture = st_texture_cache_load_gicon (cache, themed, size);
  g_object_unref (themed);

  return CLUTTER_ACTOR (texture);
}

/**
 * st_texture_cache_load_uri_async:
 *
 * @cache: The texture cache instance
 * @uri: uri of the image file from which to create a pixbuf
 * @available_width: available width for the image, can be -1 if not limited
 * @available_height: available height for the image, can be -1 if not limited
 *
 * Asynchronously load an image.   Initially, the returned texture will have a natural
 * size of zero.  At some later point, either the image will be loaded successfully
 * and at that point size will be negotiated, or upon an error, no image will be set.
 *
 * Return value: (transfer none): A new #ClutterActor with no image loaded initially.
 */
ClutterActor *
st_texture_cache_load_uri_async (StTextureCache *cache,
                                 const gchar    *uri,
                                 int             available_width,
                                 int             available_height)
{
  ClutterTexture *texture;
  AsyncTextureLoadData *data;

  texture = create_default_texture (cache);

  data = g_new0 (AsyncTextureLoadData, 1);
  data->key = g_strconcat (CACHE_PREFIX_URI, uri, NULL);
  data->policy = ST_TEXTURE_CACHE_POLICY_NONE;
  data->uri = g_strdup (uri);
  data->width = available_width;
  data->height = available_height;
  data->textures = g_slist_prepend (data->textures, g_object_ref (texture));
  load_uri_pixbuf_async (cache, uri, available_width, available_height, NULL, on_pixbuf_loaded, data);

  return CLUTTER_ACTOR (texture);
}

static CoglHandle
st_texture_cache_load_uri_sync_to_cogl_texture (StTextureCache *cache,
                                                StTextureCachePolicy policy,
                                                const gchar    *uri,
                                                int             available_width,
                                                int             available_height,
                                                GError         **error)
{
  CoglHandle texdata;
  GdkPixbuf *pixbuf;
  char *key;

  key = g_strconcat (CACHE_PREFIX_URI, uri, NULL);

  texdata = g_hash_table_lookup (cache->priv->keyed_cache, key);

  if (texdata == NULL)
    {
      pixbuf = impl_load_pixbuf_file (uri, available_width, available_height, error);
      if (!pixbuf)
        goto out;

      texdata = pixbuf_to_cogl_handle (pixbuf);
      g_object_unref (pixbuf);

      if (policy == ST_TEXTURE_CACHE_POLICY_FOREVER)
        {
          cogl_handle_ref (texdata);
          g_hash_table_insert (cache->priv->keyed_cache, g_strdup (key), texdata);
        }
    }
  else
    cogl_handle_ref (texdata);

out:
  g_free (key);
  return texdata;
}

/**
 * st_texture_cache_load_uri_sync:
 *
 * @cache: The texture cache instance
 * @policy: Requested lifecycle of cached data
 * @uri: uri of the image file from which to create a pixbuf
 * @available_width: available width for the image, can be -1 if not limited
 * @available_height: available height for the image, can be -1 if not limited
 * @error: Return location for error
 *
 * Synchronously load an image from a uri.  The image is scaled down to fit the
 * available width and height imensions, but the image is never scaled up beyond
 * its actual size. The pixbuf is rotated according to the associated orientation
 * setting.
 *
 * Return value: (transfer none): A new #ClutterActor with the image file loaded if it was
 *               generated succesfully, %NULL otherwise
 */
ClutterActor *
st_texture_cache_load_uri_sync (StTextureCache *cache,
                                StTextureCachePolicy policy,
                                const gchar       *uri,
                                int                available_width,
                                int                available_height,
                                GError            **error)
{
  CoglHandle texdata;
  ClutterTexture *texture;

  texdata = st_texture_cache_load_uri_sync_to_cogl_texture (cache, policy, uri, available_width, available_height, error);

  if (texdata == COGL_INVALID_HANDLE)
    return NULL;

  texture = create_default_texture (cache);
  set_texture_cogl_texture (texture, texdata);
  cogl_handle_unref (texdata);

  return CLUTTER_ACTOR (texture);
}

/**
 * st_texture_cache_load_file_to_cogl_texture:
 * @cache: A #StTextureCache
 * @file_path: Path to a file in supported image format
 *
 * This function synchronously loads the given file path
 * into a COGL texture.  On error, a warning is emitted
 * and %COGL_INVALID_HANDLE is returned.
 */
CoglHandle
st_texture_cache_load_file_to_cogl_texture (StTextureCache *cache,
                                            const gchar    *file_path)
{
  CoglHandle texture;
  GFile *file;
  char *uri;
  GError *error = NULL;

  file = g_file_new_for_path (file_path);
  uri = g_file_get_uri (file);

  texture = st_texture_cache_load_uri_sync_to_cogl_texture (cache, ST_TEXTURE_CACHE_POLICY_FOREVER,
                                                            uri, -1, -1, &error);
  g_object_unref (file);
  g_free (uri);

  if (texture == NULL)
    {
      g_warning ("Failed to load %s: %s", file_path, error->message);
      g_clear_error (&error);
      return COGL_INVALID_HANDLE;
    }
  return texture;
}

/**
 * st_texture_cache_load_file_simple:
 * @cache: A #StTextureCache
 * @file_path: Filesystem path
 *
 * Synchronously load an image into a texture.  The texture will be cached
 * indefinitely.  On error, this function returns an empty texture and prints a warning.
 *
 * Returns: (transfer none): A new #ClutterTexture
 */
ClutterActor *
st_texture_cache_load_file_simple (StTextureCache *cache,
                                   const gchar    *file_path)
{
  GFile *file;
  char *uri;
  ClutterActor *texture;
  GError *error = NULL;

  file = g_file_new_for_path (file_path);
  uri = g_file_get_uri (file);

  texture = st_texture_cache_load_uri_sync (cache, ST_TEXTURE_CACHE_POLICY_FOREVER,
                                            uri, -1, -1, &error);
  if (texture == NULL)
    {
      g_warning ("Failed to load %s: %s", file_path, error->message);
      g_clear_error (&error);
      texture = clutter_texture_new ();
    }
  return texture;
}

/**
 * st_texture_cache_load_from_data:
 * @cache: The texture cache instance
 * @data: Image data in PNG, GIF, etc format
 * @len: length of @data
 * @size: Size in pixels to use for the resulting texture
 * @error: Return location for error
 *
 * Synchronously creates an image from @data. The image is scaled down
 * to fit the available width and height dimensions, but the image is
 * never scaled up beyond its actual size. The pixbuf is rotated
 * according to the associated orientation setting.
 *
 * Return value: (transfer none): A new #ClutterActor with the image data loaded if it was
 *               generated succesfully, %NULL otherwise
 */
ClutterActor *
st_texture_cache_load_from_data (StTextureCache    *cache,
                                 const guchar      *data,
                                 gsize              len,
                                 int                size,
                                 GError           **error)
{
  ClutterTexture *texture;
  CoglHandle texdata;
  GdkPixbuf *pixbuf;
  char *key;
  char *checksum;

  texture = create_default_texture (cache);
  clutter_actor_set_size (CLUTTER_ACTOR (texture), size, size);

  checksum = g_compute_checksum_for_data (G_CHECKSUM_SHA1, data, len);
  key = g_strdup_printf (CACHE_PREFIX_COMPRESSED_CHECKSUM "checksum=%s,size=%d", checksum, size);
  g_free (checksum);

  texdata = g_hash_table_lookup (cache->priv->keyed_cache, key);
  if (texdata == NULL)
    {
      pixbuf = impl_load_pixbuf_data (data, len, size, size, error);
      if (!pixbuf)
        {
          g_object_unref (texture);
          g_free (key);
          return NULL;
        }

      texdata = pixbuf_to_cogl_handle (pixbuf);
      g_object_unref (pixbuf);

      set_texture_cogl_texture (texture, texdata);

      g_hash_table_insert (cache->priv->keyed_cache, g_strdup (key), texdata);
    }

  g_free (key);

  set_texture_cogl_texture (texture, texdata);
  return CLUTTER_ACTOR (texture);
}

/**
 * st_texture_cache_load_from_raw:
 * @cache: a #StTextureCache
 * @data: raw pixel data
 * @len: the length of @data
 * @has_alpha: whether @data includes an alpha channel
 * @width: width in pixels of @data
 * @height: width in pixels of @data
 * @rowstride: rowstride of @data
 * @size: size of icon to return
 *
 * Creates (or retrieves from cache) an icon based on raw pixel data.
 *
 * Return value: (transfer none): a new #ClutterActor displaying a
 * pixbuf created from @data and the other parameters.
 **/
ClutterActor *
st_texture_cache_load_from_raw (StTextureCache    *cache,
                                const guchar      *data,
                                gsize              len,
                                gboolean           has_alpha,
                                int                width,
                                int                height,
                                int                rowstride,
                                int                size,
                                GError           **error)
{
  ClutterTexture *texture;
  CoglHandle texdata;
  char *key;
  char *checksum;

  texture = create_default_texture (cache);
  clutter_actor_set_size (CLUTTER_ACTOR (texture), size, size);

  /* In theory, two images of different size could have the same
   * pixel data. We ignore that theory.
   */
  checksum = g_compute_checksum_for_data (G_CHECKSUM_SHA1, data, len);
  key = g_strdup_printf (CACHE_PREFIX_RAW_CHECKSUM "checksum=%s,size=%d", checksum, size);
  g_free (checksum);

  texdata = g_hash_table_lookup (cache->priv->keyed_cache, key);
  if (texdata == NULL)
    {
      texdata = cogl_texture_new_from_data (width, height, COGL_TEXTURE_NONE,
                                            has_alpha ? COGL_PIXEL_FORMAT_RGBA_8888 : COGL_PIXEL_FORMAT_RGB_888,
                                            COGL_PIXEL_FORMAT_ANY,
                                            rowstride, data);
      g_hash_table_insert (cache->priv->keyed_cache, g_strdup (key), texdata);
    }

  g_free (key);

  set_texture_cogl_texture (texture, texdata);
  return CLUTTER_ACTOR (texture);
}

/**
 * st_texture_cache_load_thumbnail:
 * @cache:
 * @size: Size in pixels to use for thumbnail
 * @uri: Source URI
 * @mimetype: Source mime type
 *
 * Asynchronously load a thumbnail image of a URI into a texture.  The
 * returned texture object will be a new instance; however, its texture data
 * may be shared with other objects.  This implies the texture data is cached.
 *
 * The current caching policy is permanent; to uncache, you must explicitly
 * call st_texture_cache_unref_thumbnail().
 *
 * Returns: (transfer none): A new #ClutterActor
 */
ClutterActor *
st_texture_cache_load_thumbnail (StTextureCache    *cache,
                                 int                size,
                                 const char        *uri,
                                 const char        *mimetype)
{
  ClutterTexture *texture;
  AsyncTextureLoadData *data;
  char *key;
  CoglHandle texdata;

  /* Don't attempt to load thumbnails for non-local URIs */
  if (!g_str_has_prefix (uri, "file://"))
    {
      GIcon *icon = icon_for_mimetype (mimetype);
      return st_texture_cache_load_gicon (cache, icon, size);
    }

  texture = create_default_texture (cache);
  clutter_actor_set_size (CLUTTER_ACTOR (texture), size, size);

  key = g_strdup_printf (CACHE_PREFIX_THUMBNAIL_URI "uri=%s,size=%d", uri, size);

  texdata = g_hash_table_lookup (cache->priv->keyed_cache, key);
  if (!texdata)
    {
      data = g_new0 (AsyncTextureLoadData, 1);
      data->key = g_strdup (key);
      data->policy = ST_TEXTURE_CACHE_POLICY_FOREVER;
      data->uri = g_strdup (uri);
      data->mimetype = g_strdup (mimetype);
      data->thumbnail = TRUE;
      data->width = size;
      data->height = size;
      data->textures = g_slist_prepend (data->textures, g_object_ref (texture));
      load_thumbnail_async (cache, uri, mimetype, size, NULL, on_pixbuf_loaded, data);
    }
  else
    {
      set_texture_cogl_texture (texture, texdata);
    }

  g_free (key);
  return CLUTTER_ACTOR (texture);
}

static GIcon *
icon_for_recent (GtkRecentInfo *info)
{
  const char *mimetype;

  mimetype = gtk_recent_info_get_mime_type (info);
  if (!mimetype)
    {
      return g_themed_icon_new (GTK_STOCK_FILE);
    }

  return icon_for_mimetype (mimetype);
}

/**
 * st_texture_cache_load_recent_thumbnail:
 * @cache:
 * @size: Size in pixels to use for thumbnail
 * @info: Recent item info
 *
 * Asynchronously load a thumbnail image of a #GtkRecentInfo into a texture.  The
 * returned texture object will be a new instance; however, its texture data
 * may be shared with other objects.  This implies the texture data is cached.
 *
 * The current caching policy is permanent; to uncache, you must explicitly
 * call st_texture_cache_unref_recent_thumbnail().
 *
 * Returns: (transfer none): A new #ClutterActor
 */
ClutterActor *
st_texture_cache_load_recent_thumbnail (StTextureCache    *cache,
                                        int                size,
                                        GtkRecentInfo     *info)
{
  ClutterTexture *texture;
  AsyncTextureLoadData *data;
  char *key;
  CoglHandle texdata;
  const char *uri;

  uri = gtk_recent_info_get_uri (info);

  /* Don't attempt to load thumbnails for non-local URIs */
  if (!g_str_has_prefix (uri, "file://"))
    {
      GIcon *icon = icon_for_recent (info);
      return st_texture_cache_load_gicon (cache, icon, size);
    }

  texture = CLUTTER_TEXTURE (clutter_texture_new ());
  clutter_actor_set_size (CLUTTER_ACTOR (texture), size, size);

  key = g_strdup_printf (CACHE_PREFIX_THUMBNAIL_URI "uri=%s,size=%d", uri, size);

  texdata = g_hash_table_lookup (cache->priv->keyed_cache, key);
  if (!texdata)
    {
      data = g_new0 (AsyncTextureLoadData, 1);
      data->key = g_strdup (key);
      data->policy = ST_TEXTURE_CACHE_POLICY_FOREVER;
      data->thumbnail = TRUE;
      data->recent_info = gtk_recent_info_ref (info);
      data->width = size;
      data->height = size;
      data->textures = g_slist_prepend (data->textures, g_object_ref (texture));
      load_recent_thumbnail_async (cache, info, size, NULL, on_pixbuf_loaded, data);
    }
  else
    {
      set_texture_cogl_texture (texture, texdata);
    }

  g_free (key);
  return CLUTTER_ACTOR (texture);
}

/**
 * st_texture_cache_evict_thumbnail:
 * @cache:
 * @uri: Source URI
 *
 * Removes all references added by st_texture_cache_load_thumbnail() function
 * created for the given URI.
 */
void
st_texture_cache_evict_thumbnail (StTextureCache    *cache,
                                  const char        *uri)
{
  char *target_key;

  target_key = g_strconcat (CACHE_PREFIX_THUMBNAIL_URI, uri, NULL);
  g_hash_table_remove (cache->priv->keyed_cache, target_key);
  g_free (target_key);
}

/**
 * st_texture_cache_evict_recent_thumbnail:
 * @cache:
 * @info: A recent info
 *
 * Removes all references added by st_texture_cache_load_recent_thumbnail() function
 * for the URI associated with the given @info.
 */
void
st_texture_cache_evict_recent_thumbnail (StTextureCache *cache,
                                         GtkRecentInfo  *info)
{
  st_texture_cache_evict_thumbnail (cache, gtk_recent_info_get_uri (info));
}

static size_t
pixbuf_byte_size (GdkPixbuf *pixbuf)
{
  /* This bit translated from gtk+/gdk-pixbuf/gdk-pixbuf.c:gdk_pixbuf_copy.  The comment
   * there was:
   *
   * Calculate a semi-exact size.  Here we copy with full rowstrides;
   * maybe we should copy each row individually with the minimum
   * rowstride?
   */
  return (gdk_pixbuf_get_height (pixbuf) - 1) * gdk_pixbuf_get_rowstride (pixbuf) +
    + gdk_pixbuf_get_width (pixbuf) * ((gdk_pixbuf_get_n_channels (pixbuf)* gdk_pixbuf_get_bits_per_sample (pixbuf) + 7) / 8);
}

/**
 * st_texture_cache_pixbuf_equal:
 *
 * Returns: %TRUE iff the given pixbufs are bytewise-equal
 */
gboolean
st_texture_cache_pixbuf_equal (StTextureCache *cache, GdkPixbuf *a, GdkPixbuf *b)
{
  size_t size_a = pixbuf_byte_size (a);
  size_t size_b = pixbuf_byte_size (b);
  if (size_a != size_b)
    return FALSE;
  return memcmp (gdk_pixbuf_get_pixels (a), gdk_pixbuf_get_pixels (b), size_a) == 0;
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
