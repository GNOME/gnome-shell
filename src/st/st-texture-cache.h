/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __ST_TEXTURE_CACHE_H__
#define __ST_TEXTURE_CACHE_H__

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter.h>

#define ST_TYPE_TEXTURE_CACHE                 (st_texture_cache_get_type ())
#define ST_TEXTURE_CACHE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), ST_TYPE_TEXTURE_CACHE, StTextureCache))
#define ST_TEXTURE_CACHE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), ST_TYPE_TEXTURE_CACHE, StTextureCacheClass))
#define ST_IS_TEXTURE_CACHE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ST_TYPE_TEXTURE_CACHE))
#define ST_IS_TEXTURE_CACHE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), ST_TYPE_TEXTURE_CACHE))
#define ST_TEXTURE_CACHE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), ST_TYPE_TEXTURE_CACHE, StTextureCacheClass))

typedef struct _StTextureCache StTextureCache;
typedef struct _StTextureCacheClass StTextureCacheClass;

typedef struct _StTextureCachePrivate StTextureCachePrivate;

struct _StTextureCache
{
  GObject parent;

  StTextureCachePrivate *priv;
};

struct _StTextureCacheClass
{
  GObjectClass parent_class;

};

typedef enum {
  ST_TEXTURE_CACHE_POLICY_NONE,
  ST_TEXTURE_CACHE_POLICY_FOREVER
} StTextureCachePolicy;

GType st_texture_cache_get_type (void) G_GNUC_CONST;

StTextureCache* st_texture_cache_get_default (void);

ClutterGroup *
st_texture_cache_load_sliced_image (StTextureCache    *cache,
                                    const gchar       *path,
                                    gint               grid_width,
                                    gint               grid_height);

ClutterActor *st_texture_cache_bind_pixbuf_property (StTextureCache    *cache,
                                                     GObject           *object,
                                                     const char        *property_name);

ClutterActor *st_texture_cache_load_icon_name (StTextureCache *cache,
                                               const char     *name,
                                               gint            size);

ClutterActor *st_texture_cache_load_gicon (StTextureCache *cache,
                                           GIcon          *icon,
                                           gint            size);

ClutterActor *st_texture_cache_load_thumbnail (StTextureCache *cache,
                                               int             size,
                                               const char     *uri,
                                               const char     *mimetype);

ClutterActor *st_texture_cache_load_recent_thumbnail (StTextureCache    *cache,
                                                      int                size,
                                                      GtkRecentInfo     *info);

void st_texture_cache_evict_thumbnail (StTextureCache *cache,
                                       const char     *uri);

void st_texture_cache_evict_recent_thumbnail (StTextureCache *cache,
                                              GtkRecentInfo  *info);

ClutterActor *st_texture_cache_load_uri_async (StTextureCache    *cache,
                                               const gchar       *filename,
                                               int                available_width,
                                               int                available_height);

ClutterActor *st_texture_cache_load_uri_sync (StTextureCache       *cache,
                                              StTextureCachePolicy  policy,
                                              const gchar          *filename,
                                              int                   available_width,
                                              int                   available_height,
                                              GError              **error);

CoglHandle    st_texture_cache_load_file_to_cogl_texture (StTextureCache *cache,
                                                          const gchar    *file_path);

ClutterActor *st_texture_cache_load_file_simple (StTextureCache *cache,
                                                 const gchar    *file_path);

ClutterActor *st_texture_cache_load_from_data (StTextureCache    *cache,
                                               const guchar      *data,
                                               gsize              len,
                                               int                size,
                                               GError           **error);
ClutterActor *st_texture_cache_load_from_raw  (StTextureCache    *cache,
                                               const guchar      *data,
                                               gsize              len,
                                               gboolean           has_alpha,
                                               int                width,
                                               int                height,
                                               int                rowstride,
                                               int                size,
                                               GError           **error);

typedef CoglHandle (*StTextureCacheLoader) (StTextureCache *cache, const char *key, void *data, GError **error);

CoglHandle st_texture_cache_load (StTextureCache       *cache,
                                  const char           *key,
                                  StTextureCachePolicy  policy,
                                  StTextureCacheLoader  load,
                                  void                 *data,
                                  GError              **error);

gboolean st_texture_cache_pixbuf_equal (StTextureCache *cache, GdkPixbuf *a, GdkPixbuf *b);

#endif /* __ST_TEXTURE_CACHE_H__ */
