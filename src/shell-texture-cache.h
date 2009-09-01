#ifndef __SHELL_TEXTURE_CACHE_H__
#define __SHELL_TEXTURE_CACHE_H__

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter.h>

#define SHELL_TYPE_TEXTURE_CACHE                 (shell_texture_cache_get_type ())
#define SHELL_TEXTURE_CACHE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_TEXTURE_CACHE, ShellTextureCache))
#define SHELL_TEXTURE_CACHE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_TEXTURE_CACHE, ShellTextureCacheClass))
#define SHELL_IS_TEXTURE_CACHE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_TEXTURE_CACHE))
#define SHELL_IS_TEXTURE_CACHE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_TEXTURE_CACHE))
#define SHELL_TEXTURE_CACHE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_TEXTURE_CACHE, ShellTextureCacheClass))

typedef struct _ShellTextureCache ShellTextureCache;
typedef struct _ShellTextureCacheClass ShellTextureCacheClass;

typedef struct _ShellTextureCachePrivate ShellTextureCachePrivate;

struct _ShellTextureCache
{
  GObject parent;

  ShellTextureCachePrivate *priv;
};

struct _ShellTextureCacheClass
{
  GObjectClass parent_class;

};

typedef enum {
  SHELL_TEXTURE_CACHE_POLICY_NONE,
  SHELL_TEXTURE_CACHE_POLICY_FOREVER
} ShellTextureCachePolicy;

GType shell_texture_cache_get_type (void) G_GNUC_CONST;

ShellTextureCache* shell_texture_cache_get_default (void);

ClutterActor *shell_texture_cache_bind_pixbuf_property (ShellTextureCache *cache,
                                                        GObject           *object,
                                                        const char        *property_name);

ClutterActor *shell_texture_cache_load_icon_name (ShellTextureCache *cache,
                                                  const char        *name,
                                                  gint               size);

ClutterActor *shell_texture_cache_load_gicon (ShellTextureCache *cache,
                                              GIcon             *icon,
                                              gint               size);

ClutterActor *shell_texture_cache_load_thumbnail (ShellTextureCache *cache,
                                                  int                size,
                                                  const char        *uri,
                                                  const char        *mimetype);

ClutterActor *shell_texture_cache_load_recent_thumbnail (ShellTextureCache *cache,
                                                         int                size,
                                                         GtkRecentInfo     *info);

void shell_texture_cache_evict_thumbnail (ShellTextureCache *cache,
                                          const char        *uri);

void shell_texture_cache_evict_recent_thumbnail (ShellTextureCache *cache,
                                                 GtkRecentInfo     *info);

ClutterActor *shell_texture_cache_load_uri_async (ShellTextureCache *cache,
                                                  const gchar       *filename,
                                                  int                available_width,
                                                  int                available_height);

ClutterActor *shell_texture_cache_load_uri_sync (ShellTextureCache *cache,
                                                 ShellTextureCachePolicy policy,
                                                 const gchar       *filename,
                                                 int                available_width,
                                                 int                available_height,
                                                 GError           **error);

gboolean shell_texture_cache_pixbuf_equal (ShellTextureCache *cache, GdkPixbuf *a, GdkPixbuf *b);

#endif /* __SHELL_TEXTURE_CACHE_H__ */
