#include "shell-texture-cache.h"
#include "shell-global.h"
#include <gtk/gtk.h>

typedef struct
{
  GIcon *icon;
  guint size;
} CacheKey;

struct _ShellTextureCachePrivate
{
  GHashTable *gicon_cache; /* CacheKey -> CoglTexture* */
};

static void shell_texture_cache_dispose (GObject *object);
static void shell_texture_cache_finalize (GObject *object);

G_DEFINE_TYPE(ShellTextureCache, shell_texture_cache, G_TYPE_OBJECT);

static guint
cache_key_hash (gconstpointer a)
{
  CacheKey *akey = (CacheKey *)a;

  if (akey->icon)
    return g_icon_hash (akey->icon) + 31*akey->size;
  g_assert_not_reached ();
}

static gboolean
cache_key_equal (gconstpointer a,
                 gconstpointer b)
{
  CacheKey *akey = (CacheKey*)a;
  CacheKey *bkey = (CacheKey*)b;

  if (akey->size != bkey->size)
    return FALSE;
  if (akey->icon && bkey->icon)
    return g_icon_equal (akey->icon, bkey->icon);
  g_assert_not_reached ();
}

static void
cache_key_destroy (gpointer a)
{
  CacheKey *akey = (CacheKey*)a;
  if (akey->icon)
    g_object_unref (akey->icon);
  g_free (akey);
}

static void
shell_texture_cache_class_init (ShellTextureCacheClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->dispose = shell_texture_cache_dispose;
  gobject_class->finalize = shell_texture_cache_finalize;
}

static void
shell_texture_cache_init (ShellTextureCache *self)
{
  self->priv = g_new0 (ShellTextureCachePrivate, 1);
  self->priv->gicon_cache = g_hash_table_new_full (cache_key_hash, cache_key_equal,
                                                   cache_key_destroy, cogl_handle_unref);
}

static void
shell_texture_cache_dispose (GObject *object)
{
  ShellTextureCache *self = (ShellTextureCache*)object;

  if (self->priv->gicon_cache)
    g_hash_table_destroy (self->priv->gicon_cache);
  self->priv->gicon_cache = NULL;

  G_OBJECT_CLASS (shell_texture_cache_parent_class)->dispose (object);
}

static void
shell_texture_cache_finalize (GObject *object)
{
  G_OBJECT_CLASS (shell_texture_cache_parent_class)->finalize (object);
}

ShellTextureCache*
shell_texture_cache_new ()
{
  return SHELL_TEXTURE_CACHE (g_object_new (SHELL_TYPE_TEXTURE_CACHE,
				            NULL));
}

typedef struct {
  char *uri;
  GIcon *icon;
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
  if (scaled_width >= 0 && scaled_height >= 0 && scaled_width < width && scaled_height < height)
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
impl_load_pixbuf_file (const char     *uri,
                       int             available_width,
                       int             available_height,
                       GError        **error)
{
  GdkPixbufLoader *pixbuf_loader = NULL;
  GdkPixbuf *rotated_pixbuf = NULL;
  GdkPixbuf *pixbuf;
  GFile *file = NULL;
  char *contents = NULL;
  gsize size;
  gboolean success;
  Dimensions available_dimensions;
  int width_before_rotation, width_after_rotation;

  file = g_file_new_for_uri (uri);

  success = g_file_load_contents (file, NULL, &contents, &size, NULL, error);

  if (!success)
    {
      goto out;
    }

  pixbuf_loader = gdk_pixbuf_loader_new ();

  available_dimensions.width = available_width;
  available_dimensions.height = available_height;
  g_signal_connect (pixbuf_loader, "size-prepared",
                    G_CALLBACK (on_image_size_prepared), &available_dimensions);

  success = gdk_pixbuf_loader_write (pixbuf_loader,
                                     (const guchar *) contents,
                                     size,
                                     error);
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

      success = gdk_pixbuf_loader_write (pixbuf_loader,
                                         (const guchar *) contents,
                                         size,
                                         error);
      if (!success)
        goto out;

      success = gdk_pixbuf_loader_close (pixbuf_loader, error);
      if (!success)
        goto out;

      pixbuf = gdk_pixbuf_loader_get_pixbuf (pixbuf_loader);

      rotated_pixbuf = gdk_pixbuf_apply_embedded_orientation (pixbuf);
    }

out:
  g_free (contents);
  if (file)
    g_object_unref (file);
  if (pixbuf_loader)
    g_object_unref (pixbuf_loader);
  return rotated_pixbuf;
}

static void
load_pixbuf_thread (GSimpleAsyncResult *result,
                    GObject *object,
                    GCancellable *cancellable)
{
  GdkPixbuf *pixbuf;
  AsyncIconLookupData *data;
  GError *error = NULL;

  data = g_simple_async_result_get_op_res_gpointer (result);

  if (data->uri)
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


  g_simple_async_result_set_op_res_gpointer (result, g_object_ref (pixbuf),
                                             g_object_unref);
}

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

  g_free (data);
}

/**
 * load_icon_pixbuf_async:
 *
 * Asynchronously load the #GdkPixbuf associated with a #GIcon.  Currently
 * the #GtkIconInfo must have already been provided.
 */
static void
load_icon_pixbuf_async (ShellTextureCache    *cache,
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
  data->icon = g_object_ref (icon);
  data->icon_info = gtk_icon_info_copy (icon_info);
  data->width = data->height = size;
  data->user_data = user_data;

  result = g_simple_async_result_new (G_OBJECT (cache), callback, user_data, load_icon_pixbuf_async);

  g_simple_async_result_set_op_res_gpointer (result, data, icon_lookup_data_destroy);
  g_simple_async_result_run_in_thread (result, load_pixbuf_thread, G_PRIORITY_DEFAULT, cancellable);
}

static void
load_uri_pixbuf_async (ShellTextureCache *cache,
                       const char *uri,
                       guint width,
                       guint height,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer user_data)
{
  GSimpleAsyncResult *result;
  AsyncIconLookupData *data;

  data = g_new0 (AsyncIconLookupData, 1);
  data->uri = g_strdup (uri);
  data->width = width;
  data->height = height;
  data->user_data = user_data;

  result = g_simple_async_result_new (G_OBJECT (cache), callback, user_data, load_uri_pixbuf_async);

  g_simple_async_result_set_op_res_gpointer (result, data, icon_lookup_data_destroy);
  g_simple_async_result_run_in_thread (result, load_pixbuf_thread, G_PRIORITY_DEFAULT, cancellable);
}

static GdkPixbuf *
load_pixbuf_async_finish (ShellTextureCache *cache, GAsyncResult *result, GError **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;
  return g_simple_async_result_get_op_res_gpointer (simple);
}

typedef struct {
  char *uri;
  GIcon *icon;
  GtkIconInfo *icon_info;
  guint width;
  guint height;
  ClutterTexture *texture;
} AsyncTextureLoadData;

static CoglHandle
pixbuf_to_cogl_handle (GdkPixbuf *pixbuf)
{
  return cogl_texture_new_from_data (gdk_pixbuf_get_width (pixbuf),
                                     gdk_pixbuf_get_height (pixbuf),
                                     63, /* taken from clutter-texture.c default */
                                     COGL_TEXTURE_AUTO_MIPMAP,
                                     gdk_pixbuf_get_has_alpha (pixbuf) ? COGL_PIXEL_FORMAT_RGBA_8888 : COGL_PIXEL_FORMAT_RGB_888,
                                     COGL_PIXEL_FORMAT_ANY,
                                     gdk_pixbuf_get_rowstride (pixbuf),
                                     gdk_pixbuf_get_pixels (pixbuf));
}

static void
on_pixbuf_loaded (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  ShellTextureCache *cache;
  AsyncTextureLoadData *data;
  GdkPixbuf *pixbuf;
  GError *error = NULL;
  CoglHandle texdata;
  CacheKey *key;

  data = user_data;
  cache = SHELL_TEXTURE_CACHE (source);
  pixbuf = load_pixbuf_async_finish (cache, result, &error);
  if (pixbuf == NULL)
    {
      /* TODO - we need a "broken image" display of some sort */
      goto out;
    }

  texdata = pixbuf_to_cogl_handle (pixbuf);

  if (data->icon)
    {
      gpointer orig_key, value;

      key = g_new0 (CacheKey, 1);
      key->icon = g_object_ref (data->icon);
      key->size = data->width;

      if (!g_hash_table_lookup_extended (cache->priv->gicon_cache, key,
                                         &orig_key, &value))
        g_hash_table_insert (cache->priv->gicon_cache, key,
                             texdata);
      else
        cache_key_destroy (key);
    }

  clutter_texture_set_cogl_texture (data->texture, texdata);

out:
  if (data->icon)
    {
      gtk_icon_info_free (data->icon_info);
      g_object_unref (data->icon);
    }
  else if (data->uri)
    g_free (data->uri);
  /* Alternatively we could weakref and just do nothing if the texture
     is destroyed */
  g_object_unref (data->texture);

  g_free (data);
}

/**
 * shell_texture_cache_load_gicon:
 *
 * This method returns a new #ClutterClone for a given #GIcon.  If the
 * icon isn't loaded already, the texture will be filled asynchronously.
 *
 * Return Value: (transfer none): A new #ClutterActor for the icon
 */
ClutterActor *
shell_texture_cache_load_gicon (ShellTextureCache *cache,
                                GIcon             *icon,
                                gint               size)
{
  ClutterTexture *texture;
  CoglHandle texdata;
  CacheKey key;

  texture = CLUTTER_TEXTURE (clutter_texture_new ());
  clutter_actor_set_size (CLUTTER_ACTOR (texture), size, size);

  key.icon = icon;
  key.size = size;
  texdata = g_hash_table_lookup (cache->priv->gicon_cache, &key);

  if (texdata == NULL)
    {
      GtkIconTheme *theme;
      GtkIconInfo *info;

      /* Do theme lookups in the main thread to avoid thread-unsafety */
      theme = gtk_icon_theme_get_default ();

      info = gtk_icon_theme_lookup_by_gicon (theme, icon, size, GTK_ICON_LOOKUP_USE_BUILTIN);
      if (info != NULL)
        {
          AsyncTextureLoadData *data;
          data = g_new0 (AsyncTextureLoadData, 1);

          data->icon = icon;
          data->icon_info = info;
          data->texture = g_object_ref (texture);
          data->width = data->height = size;
          load_icon_pixbuf_async (cache, icon, info, size, NULL, on_pixbuf_loaded, data);
        }
    }
  else
    {
      clutter_texture_set_cogl_texture (texture, texdata);
    }

  return CLUTTER_ACTOR (texture);
}

/**
 * shell_texture_cache_load_uri:
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
shell_texture_cache_load_uri_async (ShellTextureCache *cache,
                                    const gchar *uri,
                                    int available_width,
                                    int available_height)
{
  ClutterTexture *texture;
  AsyncTextureLoadData *data;

  texture = CLUTTER_TEXTURE (clutter_texture_new ());

  data = g_new0 (AsyncTextureLoadData, 1);
  data->uri = g_strdup (uri);
  data->width = available_width;
  data->height = available_height;
  data->texture = g_object_ref (texture);
  load_uri_pixbuf_async (cache, uri, available_width, available_height, NULL, on_pixbuf_loaded, data);

  return CLUTTER_ACTOR (texture);
}

/**
 * shell_texture_cache_load_uri_sync:
 *
 * @cache: The texture cache instance
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
shell_texture_cache_load_uri_sync (ShellTextureCache *cache,
                                   const gchar       *uri,
                                   int                available_width,
                                   int                available_height,
                                   GError            **error)
{
  ClutterTexture *texture;
  GdkPixbuf *pixbuf;
  CoglHandle texdata;

  pixbuf = impl_load_pixbuf_file (uri, available_width, available_height, error);
  if (!pixbuf)
    return NULL;

  texture = CLUTTER_TEXTURE (clutter_texture_new ());
  texdata = pixbuf_to_cogl_handle (pixbuf);
  clutter_texture_set_cogl_texture (texture, texdata);

  return CLUTTER_ACTOR (texture);
}

static ShellTextureCache *instance = NULL;

/**
 * shell_texture_cache_get_default:
 *
 * Return value: (transfer none): The global texture cache
 */
ShellTextureCache*
shell_texture_cache_get_default ()
{
  if (instance == NULL)
    instance = g_object_new (SHELL_TYPE_TEXTURE_CACHE, NULL);
  return instance;
}
