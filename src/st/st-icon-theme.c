/* StIconTheme - a loader for icon themes
 * gtk-icon-theme.c Copyright (C) 2002, 2003 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "st-icon-theme.h"
#include "st-icon-cache.h"
#include "st-settings.h"

#define DEFAULT_ICON_THEME "Adwaita"

/**
 * SECTION:sticontheme
 * @Short_description: Looking up icons by name
 * @Title: StIconTheme
 *
 * #StIconTheme provides a facility for looking up icons by name
 * and size. The main reason for using a name rather than simply
 * providing a filename is to allow different icons to be used
 * depending on what “icon theme” is selected
 * by the user. The operation of icon themes on Linux and Unix
 * follows the [Icon Theme Specification](http://www.freedesktop.org/Standards/icon-theme-spec)
 * There is a fallback icon theme, named `hicolor`, where applications
 * should install their icons, but additional icon themes can be installed
 * as operating system vendors and users choose.
 *
 * In many cases, named themes are used indirectly, via #StIcon,
 * rather than directly, but looking up icons directly is also simple.
 * The #StIconTheme object acts as a database of all the icons in the
 * current theme.
 */

#define FALLBACK_ICON_THEME "hicolor"

typedef enum
{
  ICON_THEME_DIR_FIXED,
  ICON_THEME_DIR_SCALABLE,
  ICON_THEME_DIR_THRESHOLD,
  ICON_THEME_DIR_UNTHEMED
} IconThemeDirType;

/* In reverse search order: */
typedef enum
{
  ICON_SUFFIX_NONE = 0,
  ICON_SUFFIX_XPM = 1 << 0,
  ICON_SUFFIX_SVG = 1 << 1,
  ICON_SUFFIX_PNG = 1 << 2,
  HAS_ICON_FILE = 1 << 3,
  ICON_SUFFIX_SYMBOLIC_PNG = 1 << 4
} IconSuffix;

#define INFO_CACHE_LRU_SIZE 32
#if 0
#define DEBUG_CACHE(args) g_print args
#else
#define DEBUG_CACHE(args)
#endif

struct _StIconTheme
{
  GObject parent_instance;

  GHashTable *info_cache;
  GList *info_cache_lru;

  char *current_theme;
  char **search_path;
  int search_path_len;
  GList *resource_paths;

  guint pixbuf_supports_svg : 1;
  guint themes_valid        : 1;
  guint loading_themes      : 1;

  /* A list of all the themes needed to look up icons.
   * In search order, without duplicates
   */
  GList *themes;
  GHashTable *unthemed_icons;

  /* time when we last stat:ed for theme changes */
  int64_t last_stat_time;
  GList *dir_mtimes;

  guint theme_changed_idle;
};

typedef struct {
  char **icon_names;
  int size;
  int scale;
  StIconLookupFlags flags;
} IconInfoKey;

typedef struct _SymbolicPixbufCache SymbolicPixbufCache;

struct _SymbolicPixbufCache {
  GdkPixbuf *pixbuf;
  GdkPixbuf *proxy_pixbuf;
  StIconColors *colors;
  SymbolicPixbufCache *next;
};

struct _StIconInfo
{
  GObject parent_instance;

  /* Information about the source
   */
  IconInfoKey key;
  StIconTheme *in_cache;

  char *filename;
  GFile *icon_file;
  GLoadableIcon *loadable;
  GSList *emblem_infos;

  /* Cache pixbuf (if there is any) */
  GdkPixbuf *cache_pixbuf;

  /* Information about the directory where
   * the source was found
   */
  IconThemeDirType dir_type;
  int dir_size;
  int dir_scale;
  int min_size;
  int max_size;

  /* Parameters influencing the scaled icon
   */
  int desired_size;
  int desired_scale;
  guint forced_size     : 1;
  guint emblems_applied : 1;
  guint is_svg          : 1;
  guint is_resource     : 1;

  /* Cached information if we go ahead and try to load
   * the icon.
   */
  GdkPixbuf *pixbuf;
  GdkPixbuf *proxy_pixbuf;
  GError *load_error;
  gdouble unscaled_scale;
  gdouble scale;

  SymbolicPixbufCache *symbolic_pixbuf_cache;

  int symbolic_width;
  int symbolic_height;
};

typedef struct
{
  char *name;
  char *display_name;
  char *comment;
  char *example;

  /* In search order */
  GList *dirs;
} IconTheme;

typedef struct
{
  IconThemeDirType type;
  GQuark context;

  int size;
  int min_size;
  int max_size;
  int threshold;
  int scale;
  gboolean is_resource;

  char *dir;
  char *subdir;
  int subdir_index;

  StIconCache *cache;

  GHashTable *icons;
} IconThemeDir;

typedef struct
{
  char *svg_filename;
  char *no_svg_filename;
  gboolean is_resource;
} UnthemedIcon;

typedef struct
{
  char *dir;
  time_t mtime;
  StIconCache *cache;
  gboolean exists;
} IconThemeDirMtime;

static void st_icon_theme_finalize (GObject *object);
static void theme_dir_destroy (IconThemeDir *dir);
static void theme_destroy (IconTheme *theme);
static StIconInfo * theme_lookup_icon (IconTheme  *theme,
                                       const char *icon_name,
                                       int         size,
                                       int         scale,
                                       gboolean    allow_svg);
static void theme_list_icons (IconTheme  *theme,
                              GHashTable *icons,
                              GQuark      context);
static gboolean theme_has_icon (IconTheme  *theme,
                                const char *icon_name);
static void theme_list_contexts (IconTheme  *theme,
                                 GHashTable *contexts);
static void theme_subdir_load (StIconTheme *icon_theme,
                               IconTheme   *theme,
                               GKeyFile    *theme_file,
                               char        *subdir);
static void do_theme_change (StIconTheme *icon_theme);
static void blow_themes (StIconTheme *icon_themes);
static gboolean rescan_themes (StIconTheme *icon_themes);
static IconSuffix theme_dir_get_icon_suffix (IconThemeDir     *dir,
                                             const char      *icon_name,
                                             gboolean         *has_icon_file);
static StIconInfo * icon_info_new (IconThemeDirType type,
                                   int              dir_size,
                                   int              dir_scale);
static StIconInfo *st_icon_info_new_for_file (GFile *file,
                                              int     dir_size,
                                              int     dir_scale);
static IconSuffix suffix_from_name (const char *name);
static void remove_from_lru_cache (StIconTheme *icon_theme,
                                   StIconInfo  *icon_info);
static gboolean icon_info_ensure_scale_and_pixbuf (StIconInfo *icon_info);

enum
{
  CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static guint
icon_info_key_hash (gconstpointer _key)
{
  const IconInfoKey *key = _key;
  guint h = 0;
  int i;
  for (i = 0; key->icon_names[i] != NULL; i++)
    h ^= g_str_hash (key->icon_names[i]);

  h ^= key->size * 0x10001;
  h ^= key->scale * 0x1000010;
  h ^= key->flags * 0x100000100;

  return h;
}

static gboolean
icon_info_key_equal (gconstpointer _a,
                     gconstpointer _b)
{
  const IconInfoKey *a = _a;
  const IconInfoKey *b = _b;
  int i;

  if (a->size != b->size)
    return FALSE;

  if (a->scale != b->scale)
    return FALSE;

  if (a->flags != b->flags)
    return FALSE;

  for (i = 0;
       a->icon_names[i] != NULL &&
       b->icon_names[i] != NULL; i++)
    {
      if (strcmp (a->icon_names[i], b->icon_names[i]) != 0)
        return FALSE;
    }

  return a->icon_names[i] == NULL && b->icon_names[i] == NULL;
}

G_DEFINE_TYPE (StIconTheme, st_icon_theme, G_TYPE_OBJECT)

/**
 * st_icon_theme_new:
 *
 * Creates a new icon theme object. Icon theme objects are used
 * to lookup up an icon by name in a particular icon theme.
 *
 * Returns: the newly created #StIconTheme object.
 */
StIconTheme *
st_icon_theme_new (void)
{
  return g_object_new (ST_TYPE_ICON_THEME, NULL);
}

static void
st_icon_theme_class_init (StIconThemeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = st_icon_theme_finalize;

  /**
   * StIconTheme::changed:
   * @icon_theme: the icon theme
   *
   * Emitted when the current icon theme is switched or GTK+ detects
   * that a change has occurred in the contents of the current
   * icon theme.
   */
  signals[CHANGED] = g_signal_new ("changed",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, NULL,
                                   NULL,
                                   G_TYPE_NONE, 0);
}


static void
update_current_theme (StIconTheme *icon_theme)
{
#define theme_changed(_old, _new) \
  ((_old && !_new) || (!_old && _new) || \
   (_old && _new && strcmp (_old, _new) != 0))
  StSettings *settings = st_settings_get ();
  g_autofree char *theme = NULL;
  gboolean changed = FALSE;

  g_object_get (settings, "gtk-icon-theme", &theme, NULL);

  if (theme_changed (icon_theme->current_theme, theme))
    {
      g_free (icon_theme->current_theme);
      icon_theme->current_theme = g_steal_pointer (&theme);
      changed = TRUE;
    }

    if (changed)
      do_theme_change (icon_theme);
#undef theme_changed
}

/* Callback when the icon theme StSetting changes
 */
static void
theme_changed (StSettings  *settings,
               GParamSpec  *pspec,
               StIconTheme *icon_theme)
{
  update_current_theme (icon_theme);
}

/* Checks whether a loader for SVG files has been registered
 * with GdkPixbuf.
 */
static gboolean
pixbuf_supports_svg (void)
{
  GSList *formats;
  GSList *tmp_list;
  static int found_svg = -1;

  if (found_svg != -1)
    return found_svg;

  formats = gdk_pixbuf_get_formats ();

  found_svg = FALSE;
  for (tmp_list = formats; tmp_list && !found_svg; tmp_list = tmp_list->next)
    {
      char **mime_types = gdk_pixbuf_format_get_mime_types (tmp_list->data);
      char **mime_type;

      for (mime_type = mime_types; *mime_type && !found_svg; mime_type++)
        {
          if (strcmp (*mime_type, "image/svg") == 0)
            found_svg = TRUE;
        }

      g_strfreev (mime_types);
    }

  g_slist_free (formats);

  return found_svg;
}

/* The icon info was removed from the icon_info_hash hash table */
static void
icon_info_uncached (StIconInfo *icon_info)
{
  StIconTheme *icon_theme = icon_info->in_cache;

  DEBUG_CACHE (("removing %p (%s %d 0x%x) from cache (icon_them: %p)  (cache size %d)\n",
                icon_info,
                g_strjoinv (",", icon_info->key.icon_names),
                icon_info->key.size, icon_info->key.flags,
                icon_theme,
                icon_theme != NULL ? g_hash_table_size (icon_theme->info_cache) : 0));

  icon_info->in_cache = NULL;

  if (icon_theme != NULL)
    remove_from_lru_cache (icon_theme, icon_info);
}

static void
st_icon_theme_init (StIconTheme *icon_theme)
{
  StSettings *settings;
  const char * const *xdg_data_dirs;
  int i, j;

  icon_theme->info_cache = g_hash_table_new_full (icon_info_key_hash, icon_info_key_equal, NULL,
                                                  (GDestroyNotify)icon_info_uncached);

  xdg_data_dirs = g_get_system_data_dirs ();
  for (i = 0; xdg_data_dirs[i]; i++) ;

  icon_theme->search_path_len = 2 * i + 2;

  icon_theme->search_path = g_new (char *, icon_theme->search_path_len);

  i = 0;
  icon_theme->search_path[i++] = g_build_filename (g_get_user_data_dir (), "icons", NULL);
  icon_theme->search_path[i++] = g_build_filename (g_get_home_dir (), ".icons", NULL);

  for (j = 0; xdg_data_dirs[j]; j++)
    icon_theme->search_path[i++] = g_build_filename (xdg_data_dirs[j], "icons", NULL);

  for (j = 0; xdg_data_dirs[j]; j++)
    icon_theme->search_path[i++] = g_build_filename (xdg_data_dirs[j], "pixmaps", NULL);

  icon_theme->resource_paths = g_list_append (NULL, g_strdup ("/org/gtk/libgtk/icons/"));

  icon_theme->themes_valid = FALSE;
  icon_theme->themes = NULL;
  icon_theme->unthemed_icons = NULL;

  icon_theme->pixbuf_supports_svg = pixbuf_supports_svg ();

  settings = st_settings_get ();
  g_signal_connect_object (settings, "notify::gtk-icon-theme",
                           G_CALLBACK (theme_changed), icon_theme, 0);
  update_current_theme (icon_theme);
}

static void
free_dir_mtime (IconThemeDirMtime *dir_mtime)
{
  if (dir_mtime->cache)
    st_icon_cache_unref (dir_mtime->cache);

  g_free (dir_mtime->dir);
  g_free (dir_mtime);
}

static gboolean
theme_changed_idle (gpointer user_data)
{
  StIconTheme *icon_theme;

  icon_theme = ST_ICON_THEME (user_data);

  g_signal_emit (icon_theme, signals[CHANGED], 0);

  icon_theme->theme_changed_idle = 0;

  return FALSE;
}

static void
queue_theme_changed (StIconTheme *icon_theme)
{
  if (!icon_theme->theme_changed_idle)
    {
      icon_theme->theme_changed_idle = g_idle_add (theme_changed_idle, icon_theme);
      g_source_set_name_by_id (icon_theme->theme_changed_idle, "theme_changed_idle");
    }
}

static void
do_theme_change (StIconTheme *icon_theme)
{
  g_hash_table_remove_all (icon_theme->info_cache);

  if (!icon_theme->themes_valid)
    return;

  g_debug ("change to icon theme \"%s\"", icon_theme->current_theme);
  blow_themes (icon_theme);

  queue_theme_changed (icon_theme);
}

static void
blow_themes (StIconTheme *icon_theme)
{
  if (icon_theme->themes_valid)
    {
      g_list_free_full (icon_theme->themes, (GDestroyNotify) theme_destroy);
      g_list_free_full (icon_theme->dir_mtimes, (GDestroyNotify) free_dir_mtime);
      g_hash_table_destroy (icon_theme->unthemed_icons);
    }
  icon_theme->themes = NULL;
  icon_theme->unthemed_icons = NULL;
  icon_theme->dir_mtimes = NULL;
  icon_theme->themes_valid = FALSE;
}

static void
st_icon_theme_finalize (GObject *object)
{
  StIconTheme *icon_theme;
  int i;

  icon_theme = ST_ICON_THEME (object);

  g_hash_table_destroy (icon_theme->info_cache);
  g_assert (icon_theme->info_cache_lru == NULL);

  g_clear_handle_id (&icon_theme->theme_changed_idle, g_source_remove);

  g_free (icon_theme->current_theme);

  for (i = 0; i < icon_theme->search_path_len; i++)
    g_free (icon_theme->search_path[i]);
  g_free (icon_theme->search_path);

  g_list_free_full (icon_theme->resource_paths, g_free);

  blow_themes (icon_theme);

  G_OBJECT_CLASS (st_icon_theme_parent_class)->finalize (object);
}

/**
 * st_icon_theme_set_search_path:
 * @icon_theme: a #StIconTheme
 * @path: (array length=n_elements) (element-type filename): array of
 *     directories that are searched for icon themes
 * @n_elements: number of elements in @path.
 *
 * Sets the search path for the icon theme object. When looking
 * for an icon theme, GTK+ will search for a subdirectory of
 * one or more of the directories in @path with the same name
 * as the icon theme containing an index.theme file. (Themes from
 * multiple of the path elements are combined to allow themes to be
 * extended by adding icons in the user’s home directory.)
 *
 * In addition if an icon found isn’t found either in the current
 * icon theme or the default icon theme, and an image file with
 * the right name is found directly in one of the elements of
 * @path, then that image will be used for the icon name.
 * (This is legacy feature, and new icons should be put
 * into the fallback icon theme, which is called hicolor,
 * rather than directly on the icon path.)
 */
void
st_icon_theme_set_search_path (StIconTheme *icon_theme,
                               const char  *path[],
                               int          n_elements)
{
  int i;

  g_return_if_fail (ST_IS_ICON_THEME (icon_theme));

  for (i = 0; i < icon_theme->search_path_len; i++)
    g_free (icon_theme->search_path[i]);

  g_free (icon_theme->search_path);

  icon_theme->search_path = g_new (char *, n_elements);
  icon_theme->search_path_len = n_elements;

  for (i = 0; i < icon_theme->search_path_len; i++)
    icon_theme->search_path[i] = g_strdup (path[i]);

  do_theme_change (icon_theme);
}

/**
 * st_icon_theme_get_search_path:
 * @icon_theme: a #StIconTheme
 * @path: (allow-none) (array length=n_elements) (element-type filename) (out):
 *     location to store a list of icon theme path directories or %NULL.
 *     The stored value should be freed with g_strfreev().
 * @n_elements: location to store number of elements in @path, or %NULL
 *
 * Gets the current search path. See st_icon_theme_set_search_path().
 */
void
st_icon_theme_get_search_path (StIconTheme   *icon_theme,
                               char         **path[],
                               int           *n_elements)
{
  int i;

  g_return_if_fail (ST_IS_ICON_THEME (icon_theme));

  if (n_elements)
    *n_elements = icon_theme->search_path_len;

  if (path)
    {
      *path = g_new (char *, icon_theme->search_path_len + 1);
      for (i = 0; i < icon_theme->search_path_len; i++)
        (*path)[i] = g_strdup (icon_theme->search_path[i]);
      (*path)[i] = NULL;
    }
}

/**
 * st_icon_theme_append_search_path:
 * @icon_theme: a #StIconTheme
 * @path: (type filename): directory name to append to the icon path
 *
 * Appends a directory to the search path.
 * See st_icon_theme_set_search_path().
 */
void
st_icon_theme_append_search_path (StIconTheme *icon_theme,
                                  const char  *path)
{
  g_return_if_fail (ST_IS_ICON_THEME (icon_theme));
  g_return_if_fail (path != NULL);

  icon_theme->search_path_len++;

  icon_theme->search_path = g_renew (char *, icon_theme->search_path, icon_theme->search_path_len);
  icon_theme->search_path[icon_theme->search_path_len-1] = g_strdup (path);

  do_theme_change (icon_theme);
}

/**
 * st_icon_theme_prepend_search_path:
 * @icon_theme: a #StIconTheme
 * @path: (type filename): directory name to prepend to the icon path
 *
 * Prepends a directory to the search path.
 * See st_icon_theme_set_search_path().
 */
void
st_icon_theme_prepend_search_path (StIconTheme *icon_theme,
                                   const char  *path)
{
  int i;

  g_return_if_fail (ST_IS_ICON_THEME (icon_theme));
  g_return_if_fail (path != NULL);

  icon_theme->search_path_len++;
  icon_theme->search_path = g_renew (char *, icon_theme->search_path, icon_theme->search_path_len);

  for (i = icon_theme->search_path_len - 1; i > 0; i--)
    icon_theme->search_path[i] = icon_theme->search_path[i - 1];

  icon_theme->search_path[0] = g_strdup (path);

  do_theme_change (icon_theme);
}

/**
 * st_icon_theme_add_resource_path:
 * @icon_theme: a #StIconTheme
 * @path: a resource path
 *
 * Adds a resource path that will be looked at when looking
 * for icons, similar to search paths.
 *
 * This function should be used to make application-specific icons
 * available as part of the icon theme.
 *
 * The resources are considered as part of the hicolor icon theme
 * and must be located in subdirectories that are defined in the
 * hicolor icon theme, such as `@path/16x16/actions/run.png`.
 * Icons that are directly placed in the resource path instead
 * of a subdirectory are also considered as ultimate fallback.
 */
void
st_icon_theme_add_resource_path (StIconTheme *icon_theme,
                                 const char  *path)
{
  g_return_if_fail (ST_IS_ICON_THEME (icon_theme));
  g_return_if_fail (path != NULL);

  icon_theme->resource_paths = g_list_append (icon_theme->resource_paths, g_strdup (path));

  do_theme_change (icon_theme);
}

static const char builtin_hicolor_index[] =
"[Icon Theme]\n"
"Name=Hicolor\n"
"Hidden=True\n"
"Directories=16x16/actions,16x16/status,22x22/actions,24x24/actions,24x24/status,32x32/actions,32x32/status,48x48/status,64x64/actions\n"
"[16x16/actions]\n"
"Size=16\n"
"Type=Threshold\n"
"[16x16/status]\n"
"Size=16\n"
"Type=Threshold\n"
"[22x22/actions]\n"
"Size=22\n"
"Type=Threshold\n"
"[24x24/actions]\n"
"Size=24\n"
"Type=Threshold\n"
"[24x24/status]\n"
"Size=24\n"
"Type=Threshold\n"
"[32x32/actions]\n"
"Size=32\n"
"Type=Threshold\n"
"[32x32/status]\n"
"Size=32\n"
"Type=Threshold\n"
"[48x48/status]\n"
"Size=48\n"
"Type=Threshold\n"
"[64x64/actions]\n"
"Size=64\n"
"Type=Threshold\n";

static void
insert_theme (StIconTheme *icon_theme,
              const char  *theme_name)
{
  int i;
  GList *l;
  char **dirs;
  char **scaled_dirs;
  char **themes;
  IconTheme *theme = NULL;
  char *path;
  GKeyFile *theme_file;
  GError *error = NULL;
  IconThemeDirMtime *dir_mtime;
  GStatBuf stat_buf;

  for (l = icon_theme->themes; l != NULL; l = l->next)
    {
      theme = l->data;
      if (strcmp (theme->name, theme_name) == 0)
        return;
    }

  for (i = 0; i < icon_theme->search_path_len; i++)
    {
      path = g_build_filename (icon_theme->search_path[i],
                               theme_name,
                               NULL);
      dir_mtime = g_new (IconThemeDirMtime, 1);
      dir_mtime->cache = NULL;
      dir_mtime->dir = path;
      if (g_stat (path, &stat_buf) == 0 && S_ISDIR (stat_buf.st_mode)) {
        dir_mtime->mtime = stat_buf.st_mtime;
        dir_mtime->exists = TRUE;
      } else {
        dir_mtime->mtime = 0;
        dir_mtime->exists = FALSE;
      }

      icon_theme->dir_mtimes = g_list_prepend (icon_theme->dir_mtimes, dir_mtime);
    }

  theme_file = NULL;
  for (i = 0; i < icon_theme->search_path_len && !theme_file; i++)
    {
      path = g_build_filename (icon_theme->search_path[i],
                               theme_name,
                               "index.theme",
                               NULL);
      if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
        {
          theme_file = g_key_file_new ();
          g_key_file_set_list_separator (theme_file, ',');
          if (!g_key_file_load_from_file (theme_file, path, 0, &error))
            {
              g_key_file_free (theme_file);
              theme_file = NULL;
              g_error_free (error);
              error = NULL;
            }
        }
      g_free (path);
    }

  if (theme_file || strcmp (theme_name, FALLBACK_ICON_THEME) == 0)
    {
      theme = g_new0 (IconTheme, 1);
      theme->name = g_strdup (theme_name);
      icon_theme->themes = g_list_prepend (icon_theme->themes, theme);
      if (!theme_file)
        {
          theme_file = g_key_file_new ();
          g_key_file_set_list_separator (theme_file, ',');
          g_key_file_load_from_data (theme_file, builtin_hicolor_index, -1, 0, NULL);
        }
    }

  if (theme_file == NULL)
    return;

  theme->display_name =
    g_key_file_get_locale_string (theme_file, "Icon Theme", "Name", NULL, NULL);
  if (!theme->display_name)
    g_warning ("Theme file for %s has no name", theme_name);

  dirs = g_key_file_get_string_list (theme_file, "Icon Theme", "Directories", NULL, NULL);
  if (!dirs)
    {
      g_warning ("Theme file for %s has no directories", theme_name);
      icon_theme->themes = g_list_remove (icon_theme->themes, theme);
      g_free (theme->name);
      g_free (theme->display_name);
      g_free (theme);
      g_key_file_free (theme_file);
      return;
    }

  scaled_dirs = g_key_file_get_string_list (theme_file, "Icon Theme", "ScaledDirectories", NULL, NULL);

  theme->comment =
    g_key_file_get_locale_string (theme_file,
                                  "Icon Theme", "Comment",
                                  NULL, NULL);
  theme->example =
    g_key_file_get_string (theme_file,
                           "Icon Theme", "Example",
                           NULL);

  theme->dirs = NULL;
  for (i = 0; dirs[i] != NULL; i++)
    theme_subdir_load (icon_theme, theme, theme_file, dirs[i]);

  if (scaled_dirs)
    {
      for (i = 0; scaled_dirs[i] != NULL; i++)
        theme_subdir_load (icon_theme, theme, theme_file, scaled_dirs[i]);
    }
  g_strfreev (dirs);
  g_strfreev (scaled_dirs);

  theme->dirs = g_list_reverse (theme->dirs);

  themes = g_key_file_get_string_list (theme_file,
                                       "Icon Theme",
                                       "Inherits",
                                       NULL,
                                       NULL);
  if (themes)
    {
      for (i = 0; themes[i] != NULL; i++)
        insert_theme (icon_theme, themes[i]);

      g_strfreev (themes);
    }

  g_key_file_free (theme_file);
}

static void
free_unthemed_icon (UnthemedIcon *unthemed_icon)
{
  g_free (unthemed_icon->svg_filename);
  g_free (unthemed_icon->no_svg_filename);
  g_free (unthemed_icon);
}

static char *
strip_suffix (const char *filename)
{
  const char *dot;

  if (g_str_has_suffix (filename, ".symbolic.png"))
    return g_strndup (filename, strlen(filename)-13);

  dot = strrchr (filename, '.');

  if (dot == NULL)
    return g_strdup (filename);

  return g_strndup (filename, dot - filename);
}

static void
add_unthemed_icon (StIconTheme *icon_theme,
                   const char  *dir,
                   const char  *file,
                   gboolean     is_resource)
{
  IconSuffix new_suffix, old_suffix;
  char *abs_file;
  char *base_name;
  UnthemedIcon *unthemed_icon;

  new_suffix = suffix_from_name (file);

  if (new_suffix == ICON_SUFFIX_NONE)
    return;

  abs_file = g_build_filename (dir, file, NULL);
  base_name = strip_suffix (file);

  unthemed_icon = g_hash_table_lookup (icon_theme->unthemed_icons, base_name);

  if (unthemed_icon)
    {
      if (new_suffix == ICON_SUFFIX_SVG)
        {
          if (unthemed_icon->svg_filename)
            g_free (abs_file);
          else
            unthemed_icon->svg_filename = abs_file;
        }
      else
        {
          if (unthemed_icon->no_svg_filename)
            {
              old_suffix = suffix_from_name (unthemed_icon->no_svg_filename);
              if (new_suffix > old_suffix)
                {
                  g_free (unthemed_icon->no_svg_filename);
                  unthemed_icon->no_svg_filename = abs_file;
                }
              else
                g_free (abs_file);
            }
          else
            unthemed_icon->no_svg_filename = abs_file;
        }

      g_free (base_name);
    }
  else
    {
      unthemed_icon = g_new0 (UnthemedIcon, 1);

      unthemed_icon->is_resource = is_resource;

      if (new_suffix == ICON_SUFFIX_SVG)
        unthemed_icon->svg_filename = abs_file;
      else
        unthemed_icon->no_svg_filename = abs_file;

      /* takes ownership of base_name */
      g_hash_table_replace (icon_theme->unthemed_icons, base_name, unthemed_icon);
    }
}

static void
load_themes (StIconTheme *icon_theme)
{
  GDir *gdir;
  int base;
  char *dir;
  const char *file;
  IconThemeDirMtime *dir_mtime;
  GStatBuf stat_buf;
  GList *d;

  if (icon_theme->current_theme)
    insert_theme (icon_theme, icon_theme->current_theme);

  /* Always look in the Adwaita, gnome and hicolor icon themes.
   * Looking in hicolor is mandated by the spec, looking in Adwaita
   * and gnome is a pragmatic solution to prevent missing icons in
   * GTK+ applications when run under, e.g. KDE.
   */
  insert_theme (icon_theme, DEFAULT_ICON_THEME);
  insert_theme (icon_theme, "gnome");
  insert_theme (icon_theme, FALLBACK_ICON_THEME);
  icon_theme->themes = g_list_reverse (icon_theme->themes);


  icon_theme->unthemed_icons = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                g_free, (GDestroyNotify)free_unthemed_icon);

  for (base = 0; base < icon_theme->search_path_len; base++)
    {
      dir = icon_theme->search_path[base];

      dir_mtime = g_new (IconThemeDirMtime, 1);
      icon_theme->dir_mtimes = g_list_prepend (icon_theme->dir_mtimes, dir_mtime);

      dir_mtime->dir = g_strdup (dir);
      dir_mtime->mtime = 0;
      dir_mtime->exists = FALSE;
      dir_mtime->cache = NULL;

      if (g_stat (dir, &stat_buf) != 0 || !S_ISDIR (stat_buf.st_mode))
        continue;
      dir_mtime->mtime = stat_buf.st_mtime;
      dir_mtime->exists = TRUE;

      dir_mtime->cache = st_icon_cache_new_for_path (dir);
      if (dir_mtime->cache != NULL)
        continue;

      gdir = g_dir_open (dir, 0, NULL);
      if (gdir == NULL)
        continue;

      while ((file = g_dir_read_name (gdir)))
        add_unthemed_icon (icon_theme, dir, file, FALSE);

      g_dir_close (gdir);
    }
  icon_theme->dir_mtimes = g_list_reverse (icon_theme->dir_mtimes);

  for (d = icon_theme->resource_paths; d; d = d->next)
    {
      char **children;
      int i;

      dir = d->data;
      children = g_resources_enumerate_children (dir, 0, NULL);
      if (!children)
        continue;

      for (i = 0; children[i]; i++)
        add_unthemed_icon (icon_theme, dir, children[i], TRUE);

      g_strfreev (children);
    }

  icon_theme->themes_valid = TRUE;

  icon_theme->last_stat_time = g_get_monotonic_time ();
}

static void
ensure_valid_themes (StIconTheme *icon_theme)
{
  gboolean was_valid = icon_theme->themes_valid;

  if (icon_theme->loading_themes)
    return;
  icon_theme->loading_themes = TRUE;

  if (icon_theme->themes_valid)
    {
      int64_t time = g_get_monotonic_time ();

      if (ABS (time - icon_theme->last_stat_time) > 5 * G_TIME_SPAN_SECOND &&
          rescan_themes (icon_theme))
        {
          g_hash_table_remove_all (icon_theme->info_cache);
          blow_themes (icon_theme);
        }
    }

  if (!icon_theme->themes_valid)
    {
      load_themes (icon_theme);

      if (was_valid)
        queue_theme_changed (icon_theme);
    }

  icon_theme->loading_themes = FALSE;
}

/* The LRU cache is a short list of IconInfos that are kept
 * alive even though their IconInfo would otherwise have
 * been freed, so that we can avoid reloading these
 * constantly.
 * We put infos on the lru list when nothing otherwise
 * references the info. So, when we get a cache hit
 * we remove it from the list, and when the proxy
 * pixmap is released we put it on the list.
 */
static void
ensure_lru_cache_space (StIconTheme *icon_theme)
{
  GList *l;

  /* Remove last item if LRU full */
  l = g_list_nth (icon_theme->info_cache_lru, INFO_CACHE_LRU_SIZE - 1);
  if (l)
    {
      StIconInfo *icon_info = l->data;

      DEBUG_CACHE (("removing (due to out of space) %p (%s %d 0x%x) from LRU cache (cache size %d)\n",
                    icon_info,
                    g_strjoinv (",", icon_info->key.icon_names),
                    icon_info->key.size, icon_info->key.flags,
                    g_list_length (icon_theme->info_cache_lru)));

      icon_theme->info_cache_lru = g_list_delete_link (icon_theme->info_cache_lru, l);
      g_object_unref (icon_info);
    }
}

static void
add_to_lru_cache (StIconTheme *icon_theme,
                  StIconInfo  *icon_info)
{
  DEBUG_CACHE (("adding  %p (%s %d 0x%x) to LRU cache (cache size %d)\n",
                icon_info,
                g_strjoinv (",", icon_info->key.icon_names),
                icon_info->key.size, icon_info->key.flags,
                g_list_length (icon_theme->info_cache_lru)));

  g_assert (g_list_find (icon_theme->info_cache_lru, icon_info) == NULL);

  ensure_lru_cache_space (icon_theme);
  /* prepend new info to LRU */
  icon_theme->info_cache_lru = g_list_prepend (icon_theme->info_cache_lru,
                                         g_object_ref (icon_info));
}

static void
ensure_in_lru_cache (StIconTheme *icon_theme,
                     StIconInfo  *icon_info)
{
  GList *l;

  l = g_list_find (icon_theme->info_cache_lru, icon_info);
  if (l)
    {
      /* Move to front of LRU if already in it */
      icon_theme->info_cache_lru = g_list_remove_link (icon_theme->info_cache_lru, l);
      icon_theme->info_cache_lru = g_list_concat (l, icon_theme->info_cache_lru);
    }
  else
    add_to_lru_cache (icon_theme, icon_info);
}

static void
remove_from_lru_cache (StIconTheme *icon_theme,
                       StIconInfo  *icon_info)
{
  if (g_list_find (icon_theme->info_cache_lru, icon_info))
    {
      DEBUG_CACHE (("removing %p (%s %d 0x%x) from LRU cache (cache size %d)\n",
                    icon_info,
                    g_strjoinv (",", icon_info->key.icon_names),
                    icon_info->key.size, icon_info->key.flags,
                    g_list_length (icon_theme->info_cache_lru)));

      icon_theme->info_cache_lru = g_list_remove (icon_theme->info_cache_lru, icon_info);
      g_object_unref (icon_info);
    }
}

static SymbolicPixbufCache *
symbolic_pixbuf_cache_new (GdkPixbuf           *pixbuf,
                           StIconColors        *colors,
                           SymbolicPixbufCache *next)
{
  SymbolicPixbufCache *cache;

  cache = g_new0 (SymbolicPixbufCache, 1);
  cache->pixbuf = g_object_ref (pixbuf);
  if (colors)
    cache->colors = st_icon_colors_ref (colors);
  cache->next = next;
  return cache;
}

static SymbolicPixbufCache *
symbolic_pixbuf_cache_matches (SymbolicPixbufCache *cache,
                               StIconColors        *colors)
{
  while (cache != NULL)
    {
      if (st_icon_colors_equal (colors, cache->colors))
        return cache;

      cache = cache->next;
    }

  return NULL;
}

static void
symbolic_pixbuf_cache_free (SymbolicPixbufCache *cache)
{
  SymbolicPixbufCache *next;

  while (cache != NULL)
    {
      next = cache->next;
      g_object_unref (cache->pixbuf);
      g_clear_pointer (&cache->colors, st_icon_colors_unref);
      g_free (cache);

      cache = next;
    }
}

static gboolean
icon_name_is_symbolic (const char *icon_name)
{
  return g_str_has_suffix (icon_name, "-symbolic")
      || g_str_has_suffix (icon_name, "-symbolic-ltr")
      || g_str_has_suffix (icon_name, "-symbolic-rtl");
}

static gboolean
icon_uri_is_symbolic (const char *icon_name)
{
  return g_str_has_suffix (icon_name, "-symbolic.svg")
      || g_str_has_suffix (icon_name, "-symbolic-ltr.svg")
      || g_str_has_suffix (icon_name, "-symbolic-rtl.svg")
      || g_str_has_suffix (icon_name, ".symbolic.png");
}

static StIconInfo *
real_choose_icon (StIconTheme       *icon_theme,
                  const char        *icon_names[],
                  int                size,
                  int                scale,
                  StIconLookupFlags  flags)
{
  GList *l;
  StIconInfo *icon_info = NULL;
  StIconInfo *unscaled_icon_info;
  UnthemedIcon *unthemed_icon = NULL;
  const char *icon_name = NULL;
  gboolean allow_svg;
  IconTheme *theme = NULL;
  int i;
  IconInfoKey key;

  ensure_valid_themes (icon_theme);

  key.icon_names = (char **)icon_names;
  key.size = size;
  key.scale = scale;
  key.flags = flags;

  icon_info = g_hash_table_lookup (icon_theme->info_cache, &key);
  if (icon_info != NULL)
    {
      DEBUG_CACHE (("cache hit %p (%s %d 0x%x) (cache size %d)\n",
                    icon_info,
                    g_strjoinv (",", icon_info->key.icon_names),
                    icon_info->key.size, icon_info->key.flags,
                    g_hash_table_size (icon_theme->info_cache)));

      icon_info = g_object_ref (icon_info);
      remove_from_lru_cache (icon_theme, icon_info);

      return icon_info;
    }

  if (flags & ST_ICON_LOOKUP_NO_SVG)
    allow_svg = FALSE;
  else if (flags & ST_ICON_LOOKUP_FORCE_SVG)
    allow_svg = TRUE;
  else
    allow_svg = icon_theme->pixbuf_supports_svg;

  /* For symbolic icons, do a search in all registered themes first;
   * a theme that inherits them from a parent theme might provide
   * an alternative full-color version, but still expect the symbolic icon
   * to show up instead.
   *
   * In other words: We prefer symbolic icons in inherited themes over
   * generic icons in the theme.
   */
  for (l = icon_theme->themes; l; l = l->next)
    {
      theme = l->data;
      for (i = 0; icon_names[i] && icon_name_is_symbolic (icon_names[i]); i++)
        {
          icon_name = icon_names[i];
          icon_info = theme_lookup_icon (theme, icon_name, size, scale, allow_svg);
          if (icon_info)
            goto out;
        }
    }

  for (l = icon_theme->themes; l; l = l->next)
    {
      theme = l->data;

      for (i = 0; icon_names[i]; i++)
        {
          icon_name = icon_names[i];
          icon_info = theme_lookup_icon (theme, icon_name, size, scale, allow_svg);
          if (icon_info)
            goto out;
        }
    }

  theme = NULL;

  for (i = 0; icon_names[i]; i++)
    {
      unthemed_icon = g_hash_table_lookup (icon_theme->unthemed_icons, icon_names[i]);
      if (unthemed_icon)
        break;
    }

  if (unthemed_icon)
    {
      icon_info = icon_info_new (ICON_THEME_DIR_UNTHEMED, size, 1);

      /* A SVG icon, when allowed, beats out a XPM icon, but not a PNG icon */
      if (allow_svg &&
          unthemed_icon->svg_filename &&
          (!unthemed_icon->no_svg_filename ||
           suffix_from_name (unthemed_icon->no_svg_filename) < ICON_SUFFIX_PNG))
        icon_info->filename = g_strdup (unthemed_icon->svg_filename);
      else if (unthemed_icon->no_svg_filename)
        icon_info->filename = g_strdup (unthemed_icon->no_svg_filename);
      else
        {
          static gboolean warned_once = FALSE;

          if (!warned_once)
            {
              g_warning ("Found an icon but could not load it. "
                         "Most likely gdk-pixbuf does not provide SVG support.");
              warned_once = TRUE;
            }

          g_clear_object (&icon_info);
          goto out;
        }

      if (unthemed_icon->is_resource)
        {
          g_autofree char *uri = NULL;
          uri = g_strconcat ("resource://", icon_info->filename, NULL);
          icon_info->icon_file = g_file_new_for_uri (uri);
        }
      else
        icon_info->icon_file = g_file_new_for_path (icon_info->filename);

      icon_info->is_svg = suffix_from_name (icon_info->filename) == ICON_SUFFIX_SVG;
      icon_info->is_resource = unthemed_icon->is_resource;
    }

 out:
  if (icon_info)
    {
      icon_info->desired_size = size;
      icon_info->desired_scale = scale;
      icon_info->forced_size = (flags & ST_ICON_LOOKUP_FORCE_SIZE) != 0;

      /* In case we're not scaling the icon we want to reuse the exact same
       * size as a scale==1 lookup would be, rather than not scaling at all
       * and causing a different layout
       */
      icon_info->unscaled_scale = 1.0;
      if (scale != 1 && !icon_info->forced_size && theme != NULL)
        {
          unscaled_icon_info = theme_lookup_icon (theme, icon_name, size, 1, allow_svg);
          if (unscaled_icon_info)
            {
              icon_info->unscaled_scale =
                (gdouble) unscaled_icon_info->dir_size * scale / (icon_info->dir_size * icon_info->dir_scale);
              g_object_unref (unscaled_icon_info);
            }
        }

      icon_info->key.icon_names = g_strdupv ((char **)icon_names);
      icon_info->key.size = size;
      icon_info->key.scale = scale;
      icon_info->key.flags = flags;
      icon_info->in_cache = icon_theme;
      DEBUG_CACHE (("adding %p (%s %d 0x%x) to cache (cache size %d)\n",
                    icon_info,
                    g_strjoinv (",", icon_info->key.icon_names),
                    icon_info->key.size, icon_info->key.flags,
                    g_hash_table_size (icon_theme->info_cache)));
     g_hash_table_insert (icon_theme->info_cache, &icon_info->key, icon_info);
    }
  else
    {
      static gboolean check_for_default_theme = TRUE;
      gboolean found = FALSE;

      if (check_for_default_theme)
        {
          check_for_default_theme = FALSE;

          for (i = 0; !found && i < icon_theme->search_path_len; i++)
            {
              g_autofree char *default_theme_path = NULL;

              default_theme_path = g_build_filename (icon_theme->search_path[i],
                                                     FALLBACK_ICON_THEME,
                                                     "index.theme",
                                                     NULL);
              found = g_file_test (default_theme_path, G_FILE_TEST_IS_REGULAR);
            }

          if (!found)
            {
              g_warning ("Could not find the icon '%s'. The '%s' theme\n"
                         "was not found either, perhaps you need to install it.\n"
                         "You can get a copy from:\n"
                         "\t%s",
                         icon_names[0], FALLBACK_ICON_THEME, "http://icon-theme.freedesktop.org/releases");
            }
        }
    }

  return icon_info;
}

static void
icon_name_list_add_icon (GPtrArray  *icons,
                         const char *dir_suffix,
                         char       *icon_name)
{
  if (dir_suffix)
    g_ptr_array_add (icons, g_strconcat (icon_name, dir_suffix, NULL));
  g_ptr_array_add (icons, icon_name);
}

static StIconInfo *
choose_icon (StIconTheme       *icon_theme,
             const char        *icon_names[],
             int                size,
             int                scale,
             StIconLookupFlags  flags)
{
  gboolean has_regular = FALSE, has_symbolic = FALSE;
  StIconInfo *icon_info;
  GPtrArray *new_names;
  const char *dir_suffix;
  guint i;

  if (flags & ST_ICON_LOOKUP_DIR_LTR)
    dir_suffix = "-ltr";
  else if (flags & ST_ICON_LOOKUP_DIR_RTL)
    dir_suffix = "-rtl";
  else
    dir_suffix = NULL;

  for (i = 0; icon_names[i]; i++)
    {
      if (icon_name_is_symbolic (icon_names[i]))
        has_symbolic = TRUE;
      else
        has_regular = TRUE;
    }

  if ((flags & ST_ICON_LOOKUP_FORCE_REGULAR) && has_symbolic)
    {
      new_names = g_ptr_array_new_with_free_func (g_free);
      for (i = 0; icon_names[i]; i++)
        {
          if (icon_name_is_symbolic (icon_names[i]))
            icon_name_list_add_icon (new_names, dir_suffix, g_strndup (icon_names[i], strlen (icon_names[i]) - strlen ("-symbolic")));
          else
            icon_name_list_add_icon (new_names, dir_suffix, g_strdup (icon_names[i]));
        }
      for (i = 0; icon_names[i]; i++)
        {
          if (icon_name_is_symbolic (icon_names[i]))
            icon_name_list_add_icon (new_names, dir_suffix, g_strdup (icon_names[i]));
        }
      g_ptr_array_add (new_names, NULL);

      icon_info = real_choose_icon (icon_theme,
                                    (const char **) new_names->pdata,
                                    size,
                                    scale,
                                    flags & ~(ST_ICON_LOOKUP_FORCE_REGULAR | ST_ICON_LOOKUP_FORCE_SYMBOLIC));

      g_ptr_array_free (new_names, TRUE);
    }
  else if ((flags & ST_ICON_LOOKUP_FORCE_SYMBOLIC) && has_regular)
    {
      new_names = g_ptr_array_new_with_free_func (g_free);
      for (i = 0; icon_names[i]; i++)
        {
          if (!icon_name_is_symbolic (icon_names[i]))
            icon_name_list_add_icon (new_names, dir_suffix, g_strconcat (icon_names[i], "-symbolic", NULL));
          else
            icon_name_list_add_icon (new_names, dir_suffix, g_strdup (icon_names[i]));
        }
      for (i = 0; icon_names[i]; i++)
        {
          if (!icon_name_is_symbolic (icon_names[i]))
            icon_name_list_add_icon (new_names, dir_suffix, g_strdup (icon_names[i]));
        }
      g_ptr_array_add (new_names, NULL);

      icon_info = real_choose_icon (icon_theme,
                                    (const char **) new_names->pdata,
                                    size,
                                    scale,
                                    flags & ~(ST_ICON_LOOKUP_FORCE_REGULAR | ST_ICON_LOOKUP_FORCE_SYMBOLIC));

      g_ptr_array_free (new_names, TRUE);
    }
  else if (dir_suffix)
    {
      new_names = g_ptr_array_new_with_free_func (g_free);
      for (i = 0; icon_names[i]; i++)
        {
          icon_name_list_add_icon (new_names, dir_suffix, g_strdup (icon_names[i]));
        }
      g_ptr_array_add (new_names, NULL);

      icon_info = real_choose_icon (icon_theme,
                                    (const char **) new_names->pdata,
                                    size,
                                    scale,
                                    flags & ~(ST_ICON_LOOKUP_FORCE_REGULAR | ST_ICON_LOOKUP_FORCE_SYMBOLIC));

      g_ptr_array_free (new_names, TRUE);
    }
  else
    {
      icon_info = real_choose_icon (icon_theme,
                                    icon_names,
                                    size,
                                    scale,
                                    flags & ~(ST_ICON_LOOKUP_FORCE_REGULAR | ST_ICON_LOOKUP_FORCE_SYMBOLIC));
    }

  return icon_info;
}

/**
 * st_icon_theme_lookup_icon:
 * @icon_theme: a #StIconTheme
 * @icon_name: the name of the icon to lookup
 * @size: desired icon size
 * @flags: flags modifying the behavior of the icon lookup
 *
 * Looks up a named icon and returns a #StIconInfo containing
 * information such as the filename of the icon. The icon
 * can then be rendered into a pixbuf using
 * st_icon_info_load_icon(). (st_icon_theme_load_icon()
 * combines these two steps if all you need is the pixbuf.)
 *
 * When rendering on displays with high pixel densities you should not
 * use a @size multiplied by the scaling factor returned by functions
 * like gdk_window_get_scale_factor(). Instead, you should use
 * st_icon_theme_lookup_icon_for_scale(), as the assets loaded
 * for a given scaling factor may be different.
 *
 * Returns: (nullable) (transfer full): a #StIconInfo object
 *     containing information about the icon, or %NULL if the
 *     icon wasn’t found.
 */
StIconInfo *
st_icon_theme_lookup_icon (StIconTheme       *icon_theme,
                           const char        *icon_name,
                           int                size,
                           StIconLookupFlags  flags)
{
  g_return_val_if_fail (ST_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (icon_name != NULL, NULL);
  g_return_val_if_fail ((flags & ST_ICON_LOOKUP_NO_SVG) == 0 ||
                        (flags & ST_ICON_LOOKUP_FORCE_SVG) == 0, NULL);

  g_debug ("looking up icon %s", icon_name);

  return st_icon_theme_lookup_icon_for_scale (icon_theme, icon_name,
                                               size, 1, flags);
}

/**
 * st_icon_theme_lookup_icon_for_scale:
 * @icon_theme: a #StIconTheme
 * @icon_name: the name of the icon to lookup
 * @size: desired icon size
 * @scale: the desired scale
 * @flags: flags modifying the behavior of the icon lookup
 *
 * Looks up a named icon for a particular window scale and returns a
 * #StIconInfo containing information such as the filename of the
 * icon. The icon can then be rendered into a pixbuf using
 * st_icon_info_load_icon(). (st_icon_theme_load_icon() combines
 * these two steps if all you need is the pixbuf.)
 *
 * Returns: (nullable) (transfer full): a #StIconInfo object
 *     containing information about the icon, or %NULL if the
 *     icon wasn’t found.
 */
StIconInfo *
st_icon_theme_lookup_icon_for_scale (StIconTheme       *icon_theme,
                                     const char        *icon_name,
                                     int                size,
                                     int                scale,
                                     StIconLookupFlags  flags)
{
  StIconInfo *info;

  g_return_val_if_fail (ST_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (icon_name != NULL, NULL);
  g_return_val_if_fail ((flags & ST_ICON_LOOKUP_NO_SVG) == 0 ||
                        (flags & ST_ICON_LOOKUP_FORCE_SVG) == 0, NULL);
  g_return_val_if_fail (scale >= 1, NULL);

  g_debug ("looking up icon %s for scale %d", icon_name, scale);

  if (flags & ST_ICON_LOOKUP_GENERIC_FALLBACK)
    {
      char **names, **nonsymbolic_names;
      int dashes, i;
      char *p, *nonsymbolic_icon_name;
      gboolean is_symbolic;

      is_symbolic = icon_name_is_symbolic (icon_name);
      if (is_symbolic)
        nonsymbolic_icon_name = g_strndup (icon_name, strlen (icon_name) - strlen ("-symbolic"));
      else
        nonsymbolic_icon_name = g_strdup (icon_name);

      dashes = 0;
      for (p = (char *) nonsymbolic_icon_name; *p; p++)
        if (*p == '-')
          dashes++;

      nonsymbolic_names = g_new (char *, dashes + 2);
      nonsymbolic_names[0] = nonsymbolic_icon_name;

      for (i = 1; i <= dashes; i++)
        {
          nonsymbolic_names[i] = g_strdup (nonsymbolic_names[i - 1]);
          p = strrchr (nonsymbolic_names[i], '-');
          *p = '\0';
        }
      nonsymbolic_names[dashes + 1] = NULL;

      if (is_symbolic)
        {
          names = g_new (char *, 2 * dashes + 3);
          for (i = 0; nonsymbolic_names[i] != NULL; i++)
            {
              names[i] = g_strconcat (nonsymbolic_names[i], "-symbolic", NULL);
              names[dashes + 1 + i] = nonsymbolic_names[i];
            }

          names[dashes + 1 + i] = NULL;
          g_free (nonsymbolic_names);
        }
      else
        {
          names = nonsymbolic_names;
        }

      info = choose_icon (icon_theme, (const char **) names, size, scale, flags);

      g_strfreev (names);
    }
  else
    {
      const char *names[2];

      names[0] = icon_name;
      names[1] = NULL;

      info = choose_icon (icon_theme, names, size, scale, flags);
    }

  return info;
}

/**
 * st_icon_theme_choose_icon:
 * @icon_theme: a #StIconTheme
 * @icon_names: (array zero-terminated=1): %NULL-terminated array of
 *     icon names to lookup
 * @size: desired icon size
 * @flags: flags modifying the behavior of the icon lookup
 *
 * Looks up a named icon and returns a #StIconInfo containing
 * information such as the filename of the icon. The icon
 * can then be rendered into a pixbuf using
 * st_icon_info_load_icon(). (st_icon_theme_load_icon()
 * combines these two steps if all you need is the pixbuf.)
 *
 * If @icon_names contains more than one name, this function
 * tries them all in the given order before falling back to
 * inherited icon themes.
 *
 * Returns: (nullable) (transfer full): a #StIconInfo object
 * containing information about the icon, or %NULL if the icon wasn’t
 * found.
 */
StIconInfo *
st_icon_theme_choose_icon (StIconTheme       *icon_theme,
                           const char        *icon_names[],
                           int                size,
                           StIconLookupFlags  flags)
{
  g_return_val_if_fail (ST_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (icon_names != NULL, NULL);
  g_return_val_if_fail ((flags & ST_ICON_LOOKUP_NO_SVG) == 0 ||
                        (flags & ST_ICON_LOOKUP_FORCE_SVG) == 0, NULL);
  g_warn_if_fail ((flags & ST_ICON_LOOKUP_GENERIC_FALLBACK) == 0);

  return choose_icon (icon_theme, icon_names, size, 1, flags);
}

/**
 * st_icon_theme_choose_icon_for_scale:
 * @icon_theme: a #StIconTheme
 * @icon_names: (array zero-terminated=1): %NULL-terminated
 *     array of icon names to lookup
 * @size: desired icon size
 * @scale: desired scale
 * @flags: flags modifying the behavior of the icon lookup
 *
 * Looks up a named icon for a particular window scale and returns
 * a #StIconInfo containing information such as the filename of the
 * icon. The icon can then be rendered into a pixbuf using
 * st_icon_info_load_icon(). (st_icon_theme_load_icon()
 * combines these two steps if all you need is the pixbuf.)
 *
 * If @icon_names contains more than one name, this function
 * tries them all in the given order before falling back to
 * inherited icon themes.
 *
 * Returns: (nullable) (transfer full): a #StIconInfo object
 *     containing information about the icon, or %NULL if the
 *     icon wasn’t found.
 */
StIconInfo *
st_icon_theme_choose_icon_for_scale (StIconTheme       *icon_theme,
                                     const char        *icon_names[],
                                     int                size,
                                     int                scale,
                                     StIconLookupFlags  flags)
{
  g_return_val_if_fail (ST_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (icon_names != NULL, NULL);
  g_return_val_if_fail ((flags & ST_ICON_LOOKUP_NO_SVG) == 0 ||
                        (flags & ST_ICON_LOOKUP_FORCE_SVG) == 0, NULL);
  g_return_val_if_fail (scale >= 1, NULL);
  g_warn_if_fail ((flags & ST_ICON_LOOKUP_GENERIC_FALLBACK) == 0);

  return choose_icon (icon_theme, icon_names, size, scale, flags);
}


/* Error quark */
GQuark
st_icon_theme_error_quark (void)
{
  return g_quark_from_static_string ("gtk-icon-theme-error-quark");
}

/**
 * st_icon_theme_load_icon:
 * @icon_theme: a #StIconTheme
 * @icon_name: the name of the icon to lookup
 * @size: the desired icon size. The resulting icon may not be
 *     exactly this size; see st_icon_info_load_icon().
 * @flags: flags modifying the behavior of the icon lookup
 * @error: (allow-none): Location to store error information on failure,
 *     or %NULL.
 *
 * Looks up an icon in an icon theme, scales it to the given size
 * and renders it into a pixbuf. This is a convenience function;
 * if more details about the icon are needed, use
 * st_icon_theme_lookup_icon() followed by st_icon_info_load_icon().
 *
 * Note that you probably want to listen for icon theme changes and
 * update the icon. This is usually done by connecting to the
 * GtkWidget::style-set signal. If for some reason you do not want to
 * update the icon when the icon theme changes, you should consider
 * using gdk_pixbuf_copy() to make a private copy of the pixbuf
 * returned by this function. Otherwise GTK+ may need to keep the old
 * icon theme loaded, which would be a waste of memory.
 *
 * Returns: (nullable) (transfer full): the rendered icon; this may be
 *     a newly created icon or a new reference to an internal icon, so
 *     you must not modify the icon. Use g_object_unref() to release
 *     your reference to the icon. %NULL if the icon isn’t found.
 */
GdkPixbuf *
st_icon_theme_load_icon (StIconTheme       *icon_theme,
                         const char        *icon_name,
                         int                size,
                         StIconLookupFlags  flags,
                         GError            **error)
{
  g_return_val_if_fail (ST_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (icon_name != NULL, NULL);
  g_return_val_if_fail ((flags & ST_ICON_LOOKUP_NO_SVG) == 0 ||
                        (flags & ST_ICON_LOOKUP_FORCE_SVG) == 0, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return st_icon_theme_load_icon_for_scale (icon_theme, icon_name,
                                             size, 1, flags, error);
}

/**
 * st_icon_theme_load_icon_for_scale:
 * @icon_theme: a #StIconTheme
 * @icon_name: the name of the icon to lookup
 * @size: the desired icon size. The resulting icon may not be
 *     exactly this size; see st_icon_info_load_icon().
 * @scale: desired scale
 * @flags: flags modifying the behavior of the icon lookup
 * @error: (allow-none): Location to store error information on failure,
 *     or %NULL.
 *
 * Looks up an icon in an icon theme for a particular window scale,
 * scales it to the given size and renders it into a pixbuf. This is a
 * convenience function; if more details about the icon are needed,
 * use st_icon_theme_lookup_icon() followed by
 * st_icon_info_load_icon().
 *
 * Note that you probably want to listen for icon theme changes and
 * update the icon. This is usually done by connecting to the
 * GtkWidget::style-set signal. If for some reason you do not want to
 * update the icon when the icon theme changes, you should consider
 * using gdk_pixbuf_copy() to make a private copy of the pixbuf
 * returned by this function. Otherwise GTK+ may need to keep the old
 * icon theme loaded, which would be a waste of memory.
 *
 * Returns: (nullable) (transfer full): the rendered icon; this may be
 *     a newly created icon or a new reference to an internal icon, so
 *     you must not modify the icon. Use g_object_unref() to release
 *     your reference to the icon. %NULL if the icon isn’t found.
 */
GdkPixbuf *
st_icon_theme_load_icon_for_scale (StIconTheme       *icon_theme,
                                   const char        *icon_name,
                                   int                size,
                                   int                scale,
                                   StIconLookupFlags  flags,
                                   GError            **error)
{
  StIconInfo *icon_info;
  GdkPixbuf *pixbuf = NULL;

  g_return_val_if_fail (ST_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (icon_name != NULL, NULL);
  g_return_val_if_fail ((flags & ST_ICON_LOOKUP_NO_SVG) == 0 ||
                        (flags & ST_ICON_LOOKUP_FORCE_SVG) == 0, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (scale >= 1, NULL);

  icon_info = st_icon_theme_lookup_icon_for_scale (icon_theme, icon_name, size, scale, flags);
  if (!icon_info)
    {
      g_set_error (error, ST_ICON_THEME_ERROR,  ST_ICON_THEME_NOT_FOUND,
                   _("Icon '%s' not present in theme %s"), icon_name, icon_theme->current_theme);
      return NULL;
    }

  pixbuf = st_icon_info_load_icon (icon_info, error);
  g_prefix_error (error, "Failed to load %s: ", icon_info->filename);
  g_object_unref (icon_info);

  return pixbuf;
}

/**
 * st_icon_theme_has_icon:
 * @icon_theme: a #StIconTheme
 * @icon_name: the name of an icon
 *
 * Checks whether an icon theme includes an icon
 * for a particular name.
 *
 * Returns: %TRUE if @icon_theme includes an
 *  icon for @icon_name.
 */
gboolean
st_icon_theme_has_icon (StIconTheme *icon_theme,
                        const char  *icon_name)
{
  GList *l;

  g_return_val_if_fail (ST_IS_ICON_THEME (icon_theme), FALSE);
  g_return_val_if_fail (icon_name != NULL, FALSE);

  ensure_valid_themes (icon_theme);

  for (l = icon_theme->dir_mtimes; l; l = l->next)
    {
      IconThemeDirMtime *dir_mtime = l->data;
      StIconCache *cache = dir_mtime->cache;

      if (cache && st_icon_cache_has_icon (cache, icon_name))
        return TRUE;
    }

  for (l = icon_theme->themes; l; l = l->next)
    {
      if (theme_has_icon (l->data, icon_name))
        return TRUE;
    }

  return FALSE;
}

static void
add_size (gpointer key,
          gpointer value,
          gpointer user_data)
{
  int **res_p = user_data;

  **res_p = GPOINTER_TO_INT (key);

  (*res_p)++;
}

/**
 * st_icon_theme_get_icon_sizes:
 * @icon_theme: a #StIconTheme
 * @icon_name: the name of an icon
 *
 * Returns an array of integers describing the sizes at which
 * the icon is available without scaling. A size of -1 means
 * that the icon is available in a scalable format. The array
 * is zero-terminated.
 *
 * Returns: (array zero-terminated=1) (transfer full): An newly
 * allocated array describing the sizes at which the icon is
 * available. The array should be freed with g_free() when it is no
 * longer needed.
 */
int *
st_icon_theme_get_icon_sizes (StIconTheme *icon_theme,
                              const char  *icon_name)
{
  GList *l, *d;
  GHashTable *sizes;
  int *result, *r;
  guint suffix;

  g_return_val_if_fail (ST_IS_ICON_THEME (icon_theme), NULL);

  ensure_valid_themes (icon_theme);

  sizes = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (l = icon_theme->themes; l; l = l->next)
    {
      IconTheme *theme = l->data;
      for (d = theme->dirs; d; d = d->next)
        {
          IconThemeDir *dir = d->data;

          if (dir->type != ICON_THEME_DIR_SCALABLE && g_hash_table_lookup_extended (sizes, GINT_TO_POINTER (dir->size), NULL, NULL))
            continue;

          suffix = theme_dir_get_icon_suffix (dir, icon_name, NULL);
          if (suffix != ICON_SUFFIX_NONE)
            {
              if (suffix == ICON_SUFFIX_SVG)
                g_hash_table_insert (sizes, GINT_TO_POINTER (-1), NULL);
              else
                g_hash_table_insert (sizes, GINT_TO_POINTER (dir->size), NULL);
            }
        }
    }

  r = result = g_new0 (int, g_hash_table_size (sizes) + 1);

  g_hash_table_foreach (sizes, add_size, &r);
  g_hash_table_destroy (sizes);

  return result;
}

static void
add_key_to_hash (gpointer key,
                 gpointer value,
                 gpointer user_data)
{
  GHashTable *hash = user_data;

  g_hash_table_insert (hash, key, NULL);
}

static void
add_key_to_list (gpointer key,
                 gpointer value,
                 gpointer user_data)
{
  GList **list = user_data;

  *list = g_list_prepend (*list, g_strdup (key));
}

/**
 * st_icon_theme_list_icons:
 * @icon_theme: a #StIconTheme
 * @context: (allow-none): a string identifying a particular type of
 *           icon, or %NULL to list all icons.
 *
 * Lists the icons in the current icon theme. Only a subset
 * of the icons can be listed by providing a context string.
 * The set of values for the context string is system dependent,
 * but will typically include such values as “Applications” and
 * “MimeTypes”. Contexts are explained in the
 * [Icon Theme Specification](http://www.freedesktop.org/wiki/Specifications/icon-theme-spec).
 * The standard contexts are listed in the
 * [Icon Naming Specification](http://www.freedesktop.org/wiki/Specifications/icon-naming-spec).
 * Also see st_icon_theme_list_contexts().
 *
 * Returns: (element-type utf8) (transfer full): a #GList list
 *     holding the names of all the icons in the theme. You must
 *     first free each element in the list with g_free(), then
 *     free the list itself with g_list_free().
 */
GList *
st_icon_theme_list_icons (StIconTheme *icon_theme,
                          const char  *context)
{
  GHashTable *icons;
  GList *list, *l;
  GQuark context_quark;

  ensure_valid_themes (icon_theme);

  if (context)
    {
      context_quark = g_quark_try_string (context);

      if (!context_quark)
        return NULL;
    }
  else
    context_quark = 0;

  icons = g_hash_table_new (g_str_hash, g_str_equal);

  l = icon_theme->themes;
  while (l != NULL)
    {
      theme_list_icons (l->data, icons, context_quark);
      l = l->next;
    }

  if (context_quark == 0)
    g_hash_table_foreach (icon_theme->unthemed_icons,
                          add_key_to_hash,
                          icons);

  list = NULL;

  g_hash_table_foreach (icons,
                        add_key_to_list,
                        &list);

  g_hash_table_destroy (icons);

  return list;
}

/**
 * st_icon_theme_list_contexts:
 * @icon_theme: a #StIconTheme
 *
 * Gets the list of contexts available within the current
 * hierarchy of icon themes.
 * See st_icon_theme_list_icons() for details about contexts.
 *
 * Returns: (element-type utf8) (transfer full): a #GList list
 *     holding the names of all the contexts in the theme. You must first
 *     free each element in the list with g_free(), then free the list
 *     itself with g_list_free().
 */
GList *
st_icon_theme_list_contexts (StIconTheme *icon_theme)
{
  GHashTable *contexts;
  GList *list, *l;

  ensure_valid_themes (icon_theme);

  contexts = g_hash_table_new (g_str_hash, g_str_equal);

  l = icon_theme->themes;
  while (l != NULL)
    {
      theme_list_contexts (l->data, contexts);
      l = l->next;
    }

  list = NULL;

  g_hash_table_foreach (contexts,
                        add_key_to_list,
                        &list);

  g_hash_table_destroy (contexts);

  return list;
}

static gboolean
rescan_themes (StIconTheme *icon_theme)
{
  IconThemeDirMtime *dir_mtime;
  GList *d;
  int stat_res;
  GStatBuf stat_buf;

  for (d = icon_theme->dir_mtimes; d != NULL; d = d->next)
    {
      dir_mtime = d->data;

      stat_res = g_stat (dir_mtime->dir, &stat_buf);

      /* dir mtime didn't change */
      if (stat_res == 0 && dir_mtime->exists &&
          S_ISDIR (stat_buf.st_mode) &&
          dir_mtime->mtime == stat_buf.st_mtime)
        continue;
      /* didn't exist before, and still doesn't */
      if (!dir_mtime->exists &&
          (stat_res != 0 || !S_ISDIR (stat_buf.st_mode)))
        continue;

      return TRUE;
    }

  icon_theme->last_stat_time = g_get_monotonic_time ();

  return FALSE;
}

/**
 * st_icon_theme_rescan_if_needed:
 * @icon_theme: a #StIconTheme
 *
 * Checks to see if the icon theme has changed; if it has, any
 * currently cached information is discarded and will be reloaded
 * next time @icon_theme is accessed.
 *
 * Returns: %TRUE if the icon theme has changed and needed
 *     to be reloaded.
 */
gboolean
st_icon_theme_rescan_if_needed (StIconTheme *icon_theme)
{
  gboolean retval;

  g_return_val_if_fail (ST_IS_ICON_THEME (icon_theme), FALSE);

  retval = rescan_themes (icon_theme);
  if (retval)
      do_theme_change (icon_theme);

  return retval;
}

static void
theme_destroy (IconTheme *theme)
{
  g_free (theme->display_name);
  g_free (theme->comment);
  g_free (theme->name);
  g_free (theme->example);

  g_list_free_full (theme->dirs, (GDestroyNotify) theme_dir_destroy);

  g_free (theme);
}

static void
theme_dir_destroy (IconThemeDir *dir)
{
  if (dir->cache)
    st_icon_cache_unref (dir->cache);

  if (dir->icons)
    g_hash_table_destroy (dir->icons);

  g_free (dir->dir);
  g_free (dir->subdir);
  g_free (dir);
}

static int
theme_dir_size_difference (IconThemeDir *dir,
                           int           size,
                           int           scale)
{
  int scaled_size, scaled_dir_size;
  int min, max;

  scaled_size = size * scale;
  scaled_dir_size = dir->size * dir->scale;

  switch (dir->type)
    {
    case ICON_THEME_DIR_FIXED:
      return abs (scaled_size - scaled_dir_size);
      break;
    case ICON_THEME_DIR_SCALABLE:
      if (scaled_size < (dir->min_size * dir->scale))
        return (dir->min_size * dir->scale) - scaled_size;
      if (size > (dir->max_size * dir->scale))
        return scaled_size - (dir->max_size * dir->scale);
      return 0;
      break;
    case ICON_THEME_DIR_THRESHOLD:
      min = (dir->size - dir->threshold) * dir->scale;
      max = (dir->size + dir->threshold) * dir->scale;
      if (scaled_size < min)
        return min - scaled_size;
      if (scaled_size > max)
        return scaled_size - max;
      return 0;
      break;
    case ICON_THEME_DIR_UNTHEMED:
      g_assert_not_reached ();
      break;
    }
  g_assert_not_reached ();
  return 1000;
}

static const char *
string_from_suffix (IconSuffix suffix)
{
  switch (suffix)
    {
    case ICON_SUFFIX_XPM:
      return ".xpm";
    case ICON_SUFFIX_SVG:
      return ".svg";
    case ICON_SUFFIX_PNG:
      return ".png";
    case ICON_SUFFIX_SYMBOLIC_PNG:
      return ".symbolic.png";
    default:
      g_assert_not_reached();
    }
  return NULL;
}

static IconSuffix
suffix_from_name (const char *name)
{
  IconSuffix retval = ICON_SUFFIX_NONE;

  if (name != NULL)
    {
      if (g_str_has_suffix (name, ".symbolic.png"))
        retval = ICON_SUFFIX_SYMBOLIC_PNG;
      else if (g_str_has_suffix (name, ".png"))
        retval = ICON_SUFFIX_PNG;
      else if (g_str_has_suffix (name, ".svg"))
        retval = ICON_SUFFIX_SVG;
      else if (g_str_has_suffix (name, ".xpm"))
        retval = ICON_SUFFIX_XPM;
    }

  return retval;
}

static IconSuffix
best_suffix (IconSuffix suffix,
             gboolean   allow_svg)
{
  if ((suffix & ICON_SUFFIX_SYMBOLIC_PNG) != 0)
    return ICON_SUFFIX_SYMBOLIC_PNG;
  else if ((suffix & ICON_SUFFIX_PNG) != 0)
    return ICON_SUFFIX_PNG;
  else if (allow_svg && ((suffix & ICON_SUFFIX_SVG) != 0))
    return ICON_SUFFIX_SVG;
  else if ((suffix & ICON_SUFFIX_XPM) != 0)
    return ICON_SUFFIX_XPM;
  else
    return ICON_SUFFIX_NONE;
}

static IconSuffix
theme_dir_get_icon_suffix (IconThemeDir *dir,
                           const char   *icon_name,
                           gboolean     *has_icon_file)
{
  IconSuffix suffix, symbolic_suffix;

  if (dir->cache)
    {
      suffix = (IconSuffix)st_icon_cache_get_icon_flags (dir->cache,
                                                         icon_name,
                                                         dir->subdir_index);

      if (icon_name_is_symbolic (icon_name))
        {
          /* Look for foo-symbolic.symbolic.png, as the cache only stores the ".png" suffix */
          char *icon_name_with_prefix = g_strconcat (icon_name, ".symbolic", NULL);
          symbolic_suffix = (IconSuffix)st_icon_cache_get_icon_flags (dir->cache,
                                                                      icon_name_with_prefix,
                                                                      dir->subdir_index);
          g_free (icon_name_with_prefix);

          if (symbolic_suffix & ICON_SUFFIX_PNG)
            suffix = ICON_SUFFIX_SYMBOLIC_PNG;
        }

      if (has_icon_file)
        *has_icon_file = suffix & HAS_ICON_FILE;

      suffix = suffix & ~HAS_ICON_FILE;
    }
  else
    suffix = GPOINTER_TO_UINT (g_hash_table_lookup (dir->icons, icon_name));

  g_debug ("get icon suffix%s: %u", dir->cache ? " (cached)" : "", suffix);

  return suffix;
}

/* returns TRUE if dir_a is a better match */
static gboolean
compare_dir_matches (IconThemeDir *dir_a, int difference_a,
                     IconThemeDir *dir_b, int difference_b,
                     int requested_size,
                     int requested_scale)
{
  int diff_a;
  int diff_b;

  if (difference_a == 0)
    {
      if (difference_b != 0)
        return TRUE;

      /* a and b both exact matches */
    }
  else
    {
      /* If scaling, *always* prefer downscaling */
      if (dir_a->size >= requested_size &&
          dir_b->size < requested_size)
        return TRUE;

      if (dir_a->size < requested_size &&
          dir_b->size >= requested_size)
        return FALSE;

      /* Otherwise prefer the closest match */

      if (difference_a < difference_b)
        return TRUE;

      if (difference_a > difference_b)
        return FALSE;

      /* same pixel difference */
    }

  if (dir_a->scale == requested_scale &&
      dir_b->scale != requested_scale)
    return TRUE;

  if (dir_a->scale != requested_scale &&
      dir_b->scale == requested_scale)
    return FALSE;

  /* a and b both match the scale */

  if (dir_a->type != ICON_THEME_DIR_SCALABLE &&
      dir_b->type == ICON_THEME_DIR_SCALABLE)
    return TRUE;

  if (dir_a->type == ICON_THEME_DIR_SCALABLE &&
      dir_b->type != ICON_THEME_DIR_SCALABLE)
    return FALSE;

  /* a and b both are scalable */

  diff_a = abs (requested_size * requested_scale - dir_a->size * dir_a->scale);
  diff_b = abs (requested_size * requested_scale - dir_b->size * dir_b->scale);

  return diff_a <= diff_b;
}

static StIconInfo *
theme_lookup_icon (IconTheme  *theme,
                   const char *icon_name,
                   int         size,
                   int         scale,
                   gboolean    allow_svg)
{
  GList *dirs, *l;
  IconThemeDir *dir, *min_dir;
  char *file;
  int min_difference, difference;
  IconSuffix suffix;

  min_difference = G_MAXINT;
  min_dir = NULL;

  dirs = theme->dirs;

  l = dirs;
  while (l != NULL)
    {
      dir = l->data;

      g_debug ("look up icon dir %s", dir->dir);
      suffix = theme_dir_get_icon_suffix (dir, icon_name, NULL);
      if (best_suffix (suffix, allow_svg) != ICON_SUFFIX_NONE)
        {
          difference = theme_dir_size_difference (dir, size, scale);
          if (min_dir == NULL ||
              compare_dir_matches (dir, difference,
                                   min_dir, min_difference,
                                   size, scale))
            {
              min_dir = dir;
              min_difference = difference;
            }
        }

      l = l->next;
    }

  if (min_dir)
    {
      StIconInfo *icon_info;
      gboolean has_icon_file = FALSE;

      icon_info = icon_info_new (min_dir->type, min_dir->size, min_dir->scale);
      icon_info->min_size = min_dir->min_size;
      icon_info->max_size = min_dir->max_size;

      suffix = theme_dir_get_icon_suffix (min_dir, icon_name, &has_icon_file);
      suffix = best_suffix (suffix, allow_svg);
      g_assert (suffix != ICON_SUFFIX_NONE);

      if (min_dir->dir)
        {
          file = g_strconcat (icon_name, string_from_suffix (suffix), NULL);
          icon_info->filename = g_build_filename (min_dir->dir, file, NULL);

          if (min_dir->is_resource)
            {
              g_autofree char *uri = NULL;
              uri = g_strconcat ("resource://", icon_info->filename, NULL);
              icon_info->icon_file = g_file_new_for_uri (uri);
            }
          else
            icon_info->icon_file = g_file_new_for_path (icon_info->filename);

          icon_info->is_svg = suffix == ICON_SUFFIX_SVG;
          icon_info->is_resource = min_dir->is_resource;
          g_free (file);
        }
      else
        {
          icon_info->filename = NULL;
          icon_info->icon_file = NULL;
        }

      if (min_dir->cache)
        {
          icon_info->cache_pixbuf = st_icon_cache_get_icon (min_dir->cache, icon_name,
                                                            min_dir->subdir_index);
        }

      return icon_info;
    }

  return NULL;
}

static void
theme_list_icons (IconTheme  *theme,
                  GHashTable *icons,
                  GQuark      context)
{
  GList *l = theme->dirs;
  IconThemeDir *dir;

  while (l != NULL)
    {
      dir = l->data;

      if (context == dir->context ||
          context == 0)
        {
          if (dir->cache)
            st_icon_cache_add_icons (dir->cache, dir->subdir, icons);
          else
            g_hash_table_foreach (dir->icons, add_key_to_hash, icons);
        }
      l = l->next;
    }
}

static gboolean
theme_has_icon (IconTheme  *theme,
                const char *icon_name)
{
  GList *l;

  for (l = theme->dirs; l; l = l->next)
    {
      IconThemeDir *dir = l->data;

      if (dir->cache)
        {
          if (st_icon_cache_has_icon (dir->cache, icon_name))
            return TRUE;
        }
      else
        {
          if (g_hash_table_lookup (dir->icons, icon_name) != NULL)
            return TRUE;
        }
    }

  return FALSE;
}

static void
theme_list_contexts (IconTheme  *theme,
                     GHashTable *contexts)
{
  GList *l = theme->dirs;
  IconThemeDir *dir;
  const char *context;

  while (l != NULL)
    {
      dir = l->data;

      /* The "Context" key can be unset */
      if (dir->context != 0)
        {
          context = g_quark_to_string (dir->context);
          g_hash_table_replace (contexts, (gpointer) context, NULL);
        }

      l = l->next;
    }
}

static gboolean
scan_directory (StIconTheme  *icon_theme,
                IconThemeDir *dir,
                char         *full_dir)
{
  GDir *gdir;
  const char *name;

  g_debug ("scanning directory %s", full_dir);

  gdir = g_dir_open (full_dir, 0, NULL);

  if (gdir == NULL)
    return FALSE;

  dir->icons = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  while ((name = g_dir_read_name (gdir)))
    {
      char *base_name;
      IconSuffix suffix, hash_suffix;

      suffix = suffix_from_name (name);
      if (suffix == ICON_SUFFIX_NONE)
        continue;

      base_name = strip_suffix (name);

      hash_suffix = GPOINTER_TO_INT (g_hash_table_lookup (dir->icons, base_name));
      /* takes ownership of base_name */
      g_hash_table_replace (dir->icons, base_name, GUINT_TO_POINTER (hash_suffix|suffix));
    }

  g_dir_close (gdir);

  return g_hash_table_size (dir->icons) > 0;
}

static gboolean
scan_resources (StIconTheme  *icon_theme,
                IconThemeDir *dir,
                char         *full_dir)
{
  int i;
  char **children;

  g_debug ("scanning resources %s", full_dir);

  children = g_resources_enumerate_children (full_dir, 0, NULL);
  if (!children)
    return FALSE;

  dir->icons = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (i = 0; children[i]; i++)
    {
      char *base_name;
      IconSuffix suffix, hash_suffix;

      suffix = suffix_from_name (children[i]);
      if (suffix == ICON_SUFFIX_NONE)
        continue;

      base_name = strip_suffix (children[i]);

      hash_suffix = GPOINTER_TO_INT (g_hash_table_lookup (dir->icons, base_name));
      /* takes ownership of base_name */
      g_hash_table_replace (dir->icons, base_name, GUINT_TO_POINTER (hash_suffix|suffix));
    }
  g_strfreev (children);

  return g_hash_table_size (dir->icons) > 0;
}

static void
theme_subdir_load (StIconTheme *icon_theme,
                   IconTheme   *theme,
                   GKeyFile    *theme_file,
                   char        *subdir)
{
  GList *d;
  g_autofree char *type_string = NULL;
  IconThemeDir *dir;
  IconThemeDirType type;
  g_autofree char *context_string = NULL;
  GQuark context;
  int size;
  int min_size;
  int max_size;
  int threshold;
  char *full_dir;
  GError *error = NULL;
  IconThemeDirMtime *dir_mtime;
  int scale;
  gboolean has_icons;

  size = g_key_file_get_integer (theme_file, subdir, "Size", &error);
  if (error)
    {
      g_error_free (error);
      g_warning ("Theme directory %s of theme %s has no size field\n",
                 subdir, theme->name);
      return;
    }

  type = ICON_THEME_DIR_THRESHOLD;
  type_string = g_key_file_get_string (theme_file, subdir, "Type", NULL);
  if (type_string)
    {
      if (strcmp (type_string, "Fixed") == 0)
        type = ICON_THEME_DIR_FIXED;
      else if (strcmp (type_string, "Scalable") == 0)
        type = ICON_THEME_DIR_SCALABLE;
      else if (strcmp (type_string, "Threshold") == 0)
        type = ICON_THEME_DIR_THRESHOLD;
    }

  context = 0;
  context_string = g_key_file_get_string (theme_file, subdir, "Context", NULL);
  if (context_string)
    context = g_quark_from_string (context_string);

  if (g_key_file_has_key (theme_file, subdir, "MaxSize", NULL))
    max_size = g_key_file_get_integer (theme_file, subdir, "MaxSize", NULL);
  else
    max_size = size;

  if (g_key_file_has_key (theme_file, subdir, "MinSize", NULL))
    min_size = g_key_file_get_integer (theme_file, subdir, "MinSize", NULL);
  else
    min_size = size;

  if (g_key_file_has_key (theme_file, subdir, "Threshold", NULL))
    threshold = g_key_file_get_integer (theme_file, subdir, "Threshold", NULL);
  else
    threshold = 2;

  if (g_key_file_has_key (theme_file, subdir, "Scale", NULL))
    scale = g_key_file_get_integer (theme_file, subdir, "Scale", NULL);
  else
    scale = 1;

  for (d = icon_theme->dir_mtimes; d; d = d->next)
    {
      dir_mtime = (IconThemeDirMtime *)d->data;

      if (!dir_mtime->exists)
        continue; /* directory doesn't exist */

      full_dir = g_build_filename (dir_mtime->dir, subdir, NULL);

      /* First, see if we have a cache for the directory */
      if (dir_mtime->cache != NULL || g_file_test (full_dir, G_FILE_TEST_IS_DIR))
        {
          if (dir_mtime->cache == NULL)
            {
              /* This will return NULL if the cache doesn't exist or is outdated */
              dir_mtime->cache = st_icon_cache_new_for_path (dir_mtime->dir);
            }

          dir = g_new0 (IconThemeDir, 1);
          dir->type = type;
          dir->is_resource = FALSE;
          dir->context = context;
          dir->size = size;
          dir->min_size = min_size;
          dir->max_size = max_size;
          dir->threshold = threshold;
          dir->dir = full_dir;
          dir->subdir = g_strdup (subdir);
          dir->scale = scale;

          if (dir_mtime->cache != NULL)
            {
              dir->cache = st_icon_cache_ref (dir_mtime->cache);
              dir->subdir_index = st_icon_cache_get_directory_index (dir->cache, dir->subdir);
              has_icons = st_icon_cache_has_icons (dir->cache, dir->subdir);
            }
          else
            {
              dir->cache = NULL;
              dir->subdir_index = -1;
              has_icons = scan_directory (icon_theme, dir, full_dir);
            }

          if (has_icons)
            theme->dirs = g_list_prepend (theme->dirs, dir);
          else
            theme_dir_destroy (dir);
        }
      else
        g_free (full_dir);
    }

  if (strcmp (theme->name, FALLBACK_ICON_THEME) == 0)
    {
      for (d = icon_theme->resource_paths; d; d = d->next)
        {
          /* Force a trailing / here, to avoid extra copies in GResource */
          full_dir = g_build_filename ((const char *)d->data, subdir, " ", NULL);
          full_dir[strlen (full_dir) - 1] = '\0';
          dir = g_new0 (IconThemeDir, 1);
          dir->type = type;
          dir->is_resource = TRUE;
          dir->context = context;
          dir->size = size;
          dir->min_size = min_size;
          dir->max_size = max_size;
          dir->threshold = threshold;
          dir->dir = full_dir;
          dir->subdir = g_strdup (subdir);
          dir->scale = scale;
          dir->cache = NULL;
          dir->subdir_index = -1;

          if (scan_resources (icon_theme, dir, full_dir))
            theme->dirs = g_list_prepend (theme->dirs, dir);
          else
            theme_dir_destroy (dir);
        }
    }
}

/*
 * StIconInfo
 */

G_DEFINE_TYPE (StIconInfo, st_icon_info, G_TYPE_OBJECT)

static void
st_icon_info_init (StIconInfo *icon_info)
{
  icon_info->scale = -1.;
}

static StIconInfo *
icon_info_new (IconThemeDirType type,
               int              dir_size,
               int              dir_scale)
{
  StIconInfo *icon_info;

  icon_info = g_object_new (ST_TYPE_ICON_INFO, NULL);

  icon_info->dir_type = type;
  icon_info->dir_size = dir_size;
  icon_info->dir_scale = dir_scale;
  icon_info->unscaled_scale = 1.0;
  icon_info->is_svg = FALSE;
  icon_info->is_resource = FALSE;

  return icon_info;
}

/* This only copies whatever is needed to load the pixbuf,
 * so that we can do a load in a thread without affecting
 * the original IconInfo from the thread.
 */
static StIconInfo *
icon_info_dup (StIconInfo *icon_info)
{
  StIconInfo *dup;
  GSList *l;

  dup = icon_info_new (icon_info->dir_type, icon_info->dir_size, icon_info->dir_scale);

  dup->filename = g_strdup (icon_info->filename);
  dup->is_svg = icon_info->is_svg;

  if (icon_info->icon_file)
    dup->icon_file = g_object_ref (icon_info->icon_file);
  if (icon_info->loadable)
    dup->loadable = g_object_ref (icon_info->loadable);
  if (icon_info->pixbuf)
    dup->pixbuf = g_object_ref (icon_info->pixbuf);

  for (l = icon_info->emblem_infos; l != NULL; l = l->next)
    {
      dup->emblem_infos =
        g_slist_append (dup->emblem_infos,
                        icon_info_dup (l->data));
    }

  if (icon_info->cache_pixbuf)
    dup->cache_pixbuf = g_object_ref (icon_info->cache_pixbuf);

  dup->scale = icon_info->scale;
  dup->unscaled_scale = icon_info->unscaled_scale;
  dup->desired_size = icon_info->desired_size;
  dup->desired_scale = icon_info->desired_scale;
  dup->forced_size = icon_info->forced_size;
  dup->emblems_applied = icon_info->emblems_applied;
  dup->is_resource = icon_info->is_resource;
  dup->min_size = icon_info->min_size;
  dup->max_size = icon_info->max_size;
  dup->symbolic_width = icon_info->symbolic_width;
  dup->symbolic_height = icon_info->symbolic_height;

  return dup;
}

static void
st_icon_info_finalize (GObject *object)
{
  StIconInfo *icon_info = (StIconInfo *) object;

  if (icon_info->in_cache)
    g_hash_table_remove (icon_info->in_cache->info_cache, &icon_info->key);

  g_strfreev (icon_info->key.icon_names);

  g_free (icon_info->filename);
  g_clear_object (&icon_info->icon_file);

  g_clear_object (&icon_info->loadable);
  g_slist_free_full (icon_info->emblem_infos, (GDestroyNotify) g_object_unref);
  g_clear_object (&icon_info->pixbuf);
  g_clear_object (&icon_info->proxy_pixbuf);
  g_clear_object (&icon_info->cache_pixbuf);
  g_clear_error (&icon_info->load_error);

  symbolic_pixbuf_cache_free (icon_info->symbolic_pixbuf_cache);

  G_OBJECT_CLASS (st_icon_info_parent_class)->finalize (object);
}

static void
st_icon_info_class_init (StIconInfoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = st_icon_info_finalize;
}

/**
 * st_icon_info_get_base_size:
 * @icon_info: a #StIconInfo
 *
 * Gets the base size for the icon. The base size
 * is a size for the icon that was specified by
 * the icon theme creator. This may be different
 * than the actual size of image; an example of
 * this is small emblem icons that can be attached
 * to a larger icon. These icons will be given
 * the same base size as the larger icons to which
 * they are attached.
 *
 * Note that for scaled icons the base size does
 * not include the base scale.
 *
 * Returns: the base size, or 0, if no base
 *     size is known for the icon.
 */
int
st_icon_info_get_base_size (StIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, 0);

  return icon_info->dir_size;
}

/**
 * st_icon_info_get_base_scale:
 * @icon_info: a #StIconInfo
 *
 * Gets the base scale for the icon. The base scale is a scale
 * for the icon that was specified by the icon theme creator.
 * For instance an icon drawn for a high-dpi screen with window
 * scale 2 for a base size of 32 will be 64 pixels tall and have
 * a base scale of 2.
 *
 * Returns: the base scale
 */
int
st_icon_info_get_base_scale (StIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, 0);

  return icon_info->dir_scale;
}

/**
 * st_icon_info_get_filename:
 * @icon_info: a #StIconInfo
 *
 * Gets the filename for the icon.
 *
 * Returns: (nullable) (type filename): the filename for the icon, or %NULL.
 *     The return value is owned by GTK+ and should not be modified
 *     or freed.
 */
const char *
st_icon_info_get_filename (StIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, NULL);

  return icon_info->filename;
}

/**
 * st_icon_info_is_symbolic:
 * @icon_info: a #StIconInfo
 *
 * Checks if the icon is symbolic or not. This currently uses only
 * the file name and not the file contents for determining this.
 * This behaviour may change in the future.
 *
 * Returns: %TRUE if the icon is symbolic, %FALSE otherwise
 */
gboolean
st_icon_info_is_symbolic (StIconInfo *icon_info)
{
  g_autofree char *icon_uri = NULL;
  gboolean is_symbolic;

  g_return_val_if_fail (ST_IS_ICON_INFO (icon_info), FALSE);

  if (icon_info->icon_file)
    icon_uri = g_file_get_uri (icon_info->icon_file);

  is_symbolic = (icon_uri != NULL) && (icon_uri_is_symbolic (icon_uri));

  return is_symbolic;
}

static GdkPixbuf *
load_from_stream (GdkPixbufLoader  *loader,
                  GInputStream     *stream,
                  GCancellable     *cancellable,
                  GError          **error)
{
  GdkPixbuf *pixbuf;
  gssize n_read;
  guchar buffer[65536];
  gboolean res;

  res = TRUE;
  while (1)
    {
      n_read = g_input_stream_read (stream, buffer, sizeof (buffer), cancellable, error);
      if (n_read < 0)
        {
          res = FALSE;
          error = NULL; /* Ignore further errors */
          break;
        }

      if (n_read == 0)
        break;

      if (!gdk_pixbuf_loader_write (loader, buffer, n_read, error))
        {
          res = FALSE;
          error = NULL;
          break;
        }
    }

  if (!gdk_pixbuf_loader_close (loader, error))
    {
      res = FALSE;
      error = NULL;
    }

  pixbuf = NULL;

  if (res)
    {
      pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
      if (pixbuf)
        g_object_ref (pixbuf);
    }

  return pixbuf;
}

static void
size_prepared_cb (GdkPixbufLoader *loader,
                  int              width,
                  int              height,
                  gpointer         data)
{
  gdouble *scale = data;

  width = MAX (*scale * width, 1);
  height = MAX (*scale * height, 1);

  gdk_pixbuf_loader_set_size (loader, width, height);
}

/* Like gdk_pixbuf_new_from_stream_at_scale, but
 * load the image at its original size times the
 * given scale.
 */
static GdkPixbuf *
_gdk_pixbuf_new_from_stream_scaled (GInputStream  *stream,
                                    double         scale,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;

  loader = gdk_pixbuf_loader_new ();

  g_signal_connect (loader, "size-prepared",
                    G_CALLBACK (size_prepared_cb), &scale);

  pixbuf = load_from_stream (loader, stream, cancellable, error);

  g_object_unref (loader);

  return pixbuf;
}

/* Like gdk_pixbuf_new_from_resource_at_scale, but
 * load the image at its original size times the
 * given scale.
 */
static GdkPixbuf *
_gdk_pixbuf_new_from_resource_scaled (const char  *resource_path,
                                      double       scale,
                                      GError     **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;

  stream = g_resources_open_stream (resource_path, 0, error);
  if (stream == NULL)
    return NULL;

  pixbuf = _gdk_pixbuf_new_from_stream_scaled (stream, scale, NULL, error);
  g_object_unref (stream);

  return pixbuf;
}

static GdkPixbuf *
apply_emblems_to_pixbuf (GdkPixbuf  *pixbuf,
                         StIconInfo *info)
{
  GdkPixbuf *icon = NULL;
  int w, h, pos;
  GSList *l;

  if (info->emblem_infos == NULL)
    return NULL;

  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);

  for (l = info->emblem_infos, pos = 0; l; l = l->next, pos++)
    {
      StIconInfo *emblem_info = l->data;

      if (icon_info_ensure_scale_and_pixbuf (emblem_info))
        {
          GdkPixbuf *emblem = emblem_info->pixbuf;
          int ew, eh;
          int x = 0, y = 0; /* silence compiler */
          gdouble scale;

          ew = gdk_pixbuf_get_width (emblem);
          eh = gdk_pixbuf_get_height (emblem);
          if (ew >= w)
            {
              scale = 0.75;
              ew = ew * 0.75;
              eh = eh * 0.75;
            }
          else
            scale = 1.0;

          switch (pos % 4)
            {
            case 0:
              x = w - ew;
              y = h - eh;
              break;
            case 1:
              x = w - ew;
              y = 0;
              break;
            case 2:
              x = 0;
              y = h - eh;
              break;
            case 3:
              x = 0;
              y = 0;
              break;
            }

          if (icon == NULL)
            {
              icon = gdk_pixbuf_copy (pixbuf);
              if (icon == NULL)
                break;
            }

          gdk_pixbuf_composite (emblem, icon, x, y, ew, eh, x, y,
                                scale, scale, GDK_INTERP_BILINEAR, 255);
       }
   }

  return icon;
}

/* Combine the icon with all emblems, the first emblem is placed
 * in the southeast corner. Scale emblems to be at most 3/4 of the
 * size of the icon itself.
 */
static void
apply_emblems (StIconInfo *info)
{
  GdkPixbuf *icon;

  if (info->emblems_applied)
    return;

  icon = apply_emblems_to_pixbuf (info->pixbuf, info);

  if (icon)
    {
      g_object_unref (info->pixbuf);
      info->pixbuf = icon;
      info->emblems_applied = TRUE;
    }
}

/* If this returns TRUE, its safe to call icon_info_ensure_scale_and_pixbuf
 * without blocking
 */
static gboolean
icon_info_get_pixbuf_ready (StIconInfo *icon_info)
{
  if (icon_info->pixbuf &&
      (icon_info->emblem_infos == NULL || icon_info->emblems_applied))
    return TRUE;

  if (icon_info->load_error)
    return TRUE;

  return FALSE;
}

/* This function contains the complicated logic for deciding
 * on the size at which to load the icon and loading it at
 * that size.
 */
static gboolean
icon_info_ensure_scale_and_pixbuf (StIconInfo *icon_info)
{
  int image_width, image_height, image_size;
  int scaled_desired_size;
  GdkPixbuf *source_pixbuf;
  gdouble dir_scale;

  if (icon_info->pixbuf)
    {
      apply_emblems (icon_info);
      return TRUE;
    }

  if (icon_info->load_error)
    return FALSE;

  if (icon_info->icon_file && !icon_info->loadable)
    icon_info->loadable = G_LOADABLE_ICON (g_file_icon_new (icon_info->icon_file));

  scaled_desired_size = icon_info->desired_size * icon_info->desired_scale;

  dir_scale = icon_info->dir_scale;

  /* In many cases, the scale can be determined without actual access
   * to the icon file. This is generally true when we have a size
   * for the directory where the icon is; the image size doesn't
   * matter in that case.
   */
  if (icon_info->forced_size ||
      icon_info->dir_type == ICON_THEME_DIR_UNTHEMED)
    icon_info->scale = -1;
  else if (icon_info->dir_type == ICON_THEME_DIR_FIXED ||
           icon_info->dir_type == ICON_THEME_DIR_THRESHOLD)
    icon_info->scale = icon_info->unscaled_scale;
  else if (icon_info->dir_type == ICON_THEME_DIR_SCALABLE)
    {
      /* For svg icons, treat scalable directories as if they had
       * a Scale=<desired_scale> entry. In particular, this means
       * spinners that are restriced to size 32 will loaded at size
       * up to 64 with Scale=2.
       */
      if (icon_info->is_svg)
        dir_scale = icon_info->desired_scale;

      if (scaled_desired_size < icon_info->min_size * dir_scale)
        icon_info->scale = (gdouble) icon_info->min_size / (gdouble) icon_info->dir_size;
      else if (scaled_desired_size > icon_info->max_size * dir_scale)
        icon_info->scale = (gdouble) icon_info->max_size / (gdouble) icon_info->dir_size;
      else
        icon_info->scale = (gdouble) scaled_desired_size / (icon_info->dir_size * dir_scale);
    }

  /* At this point, we need to actually get the icon; either from the
   * builtin image or by loading the file
   */
  source_pixbuf = NULL;
  if (icon_info->cache_pixbuf)
    source_pixbuf = g_object_ref (icon_info->cache_pixbuf);
  else if (icon_info->is_resource)
    {
      if (icon_info->is_svg)
        {
          int size;

          if (icon_info->forced_size || icon_info->dir_type == ICON_THEME_DIR_UNTHEMED)
            size = scaled_desired_size;
          else
            size = icon_info->dir_size * dir_scale * icon_info->scale;

          if (size == 0)
            source_pixbuf = _gdk_pixbuf_new_from_resource_scaled (icon_info->filename,
                                                                  icon_info->desired_scale,
                                                                  &icon_info->load_error);
          else
            source_pixbuf = gdk_pixbuf_new_from_resource_at_scale (icon_info->filename,
                                                                   size, size, TRUE,
                                                                   &icon_info->load_error);
        }
      else
        source_pixbuf = gdk_pixbuf_new_from_resource (icon_info->filename,
                                                      &icon_info->load_error);
    }
  else
    {
      GInputStream *stream;

      /* TODO: We should have a load_at_scale */
      stream = g_loadable_icon_load (icon_info->loadable,
                                     scaled_desired_size,
                                     NULL, NULL,
                                     &icon_info->load_error);
      if (stream)
        {
          /* SVG icons are a special case - we just immediately scale them
           * to the desired size
           */
          if (icon_info->is_svg)
            {
              int size;

              if (icon_info->forced_size || icon_info->dir_type == ICON_THEME_DIR_UNTHEMED)
                size = scaled_desired_size;
              else
                size = icon_info->dir_size * dir_scale * icon_info->scale;
              if (size == 0)
                source_pixbuf = _gdk_pixbuf_new_from_stream_scaled (stream,
                                                                    icon_info->desired_scale,
                                                                    NULL,
                                                                    &icon_info->load_error);
              else
                source_pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream,
                                                                     size, size,
                                                                     TRUE, NULL,
                                                                     &icon_info->load_error);
            }
          else
            source_pixbuf = gdk_pixbuf_new_from_stream (stream,
                                                        NULL,
                                                        &icon_info->load_error);
          g_object_unref (stream);
        }
    }

  if (!source_pixbuf)
    {
      static gboolean warn_about_load_failure = TRUE;

      if (warn_about_load_failure)
        {
          g_autofree char *path = NULL;

          if (icon_info->is_resource)
            path = g_strdup (icon_info->filename);
          else if (G_IS_FILE (icon_info->loadable))
            path = g_file_get_path (G_FILE (icon_info->loadable));
          else
            path = g_strdup ("icon theme");

          g_warning ("Could not load a pixbuf from %s.\n"
                     "This may indicate that pixbuf loaders or the mime database could not be found.",
                     path);

          warn_about_load_failure = FALSE;
        }

      return FALSE;
    }

  /* Do scale calculations that depend on the image size
   */
  image_width = gdk_pixbuf_get_width (source_pixbuf);
  image_height = gdk_pixbuf_get_height (source_pixbuf);
  image_size = MAX (image_width, image_height);

  if (icon_info->is_svg)
    icon_info->scale = image_size / 1000.;
  else if (icon_info->scale < 0.0)
    {
      if (image_size > 0 && scaled_desired_size > 0)
        icon_info->scale = (gdouble)scaled_desired_size / (gdouble)image_size;
      else
        icon_info->scale = 1.0;

      if (icon_info->dir_type == ICON_THEME_DIR_UNTHEMED &&
          !icon_info->forced_size)
        icon_info->scale = MIN (icon_info->scale, 1.0);
    }

  if (icon_info->is_svg)
    icon_info->pixbuf = source_pixbuf;
  else if (icon_info->scale == 1.0)
    icon_info->pixbuf = source_pixbuf;
  else
    {
      icon_info->pixbuf = gdk_pixbuf_scale_simple (source_pixbuf,
                                                   MAX (1, 0.5 + image_width * icon_info->scale),
                                                   MAX (1, 0.5 + image_height * icon_info->scale),
                                                   GDK_INTERP_BILINEAR);
      g_object_unref (source_pixbuf);
    }

  apply_emblems (icon_info);

  return TRUE;
}

static void
proxy_pixbuf_destroy (guchar *pixels, gpointer data)
{
  StIconInfo *icon_info = data;
  StIconTheme *icon_theme = icon_info->in_cache;

  g_assert (icon_info->proxy_pixbuf != NULL);
  icon_info->proxy_pixbuf = NULL;

  /* Keep it alive a bit longer */
  if (icon_theme != NULL)
    ensure_in_lru_cache (icon_theme, icon_info);

  g_object_unref (icon_info);
}

/**
 * st_icon_info_load_icon:
 * @icon_info: a #StIconInfo from st_icon_theme_lookup_icon()
 * @error: (allow-none): location to store error information on failure,
 *     or %NULL.
 *
 * Renders an icon previously looked up in an icon theme using
 * st_icon_theme_lookup_icon(); the size will be based on the size
 * passed to st_icon_theme_lookup_icon(). Note that the resulting
 * pixbuf may not be exactly this size; an icon theme may have icons
 * that differ slightly from their nominal sizes, and in addition GTK+
 * will avoid scaling icons that it considers sufficiently close to the
 * requested size or for which the source image would have to be scaled
 * up too far. (This maintains sharpness.). This behaviour can be changed
 * by passing the %ST_ICON_LOOKUP_FORCE_SIZE flag when obtaining
 * the #StIconInfo. If this flag has been specified, the pixbuf
 * returned by this function will be scaled to the exact size.
 *
 * Returns: (transfer full): the rendered icon; this may be a newly
 *     created icon or a new reference to an internal icon, so you must
 *     not modify the icon. Use g_object_unref() to release your reference
 *     to the icon.
 */
GdkPixbuf *
st_icon_info_load_icon (StIconInfo  *icon_info,
                        GError     **error)
{
  g_return_val_if_fail (icon_info != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!icon_info_ensure_scale_and_pixbuf (icon_info))
    {
      if (icon_info->load_error)
        {
          if (error)
            *error = g_error_copy (icon_info->load_error);
        }
      else
        {
          g_set_error_literal (error,
                               ST_ICON_THEME_ERROR,
                               ST_ICON_THEME_NOT_FOUND,
                               _("Failed to load icon"));
        }

      return NULL;
    }

  /* Instead of returning the pixbuf directly we return a proxy
   * to it that we don't own (but that shares the data with the
   * one we own). This way we can know when it is freed and ensure
   * the IconInfo is alive (and thus cached) while the pixbuf is
   * still alive.
   */
  if (icon_info->proxy_pixbuf != NULL)
    return g_object_ref (icon_info->proxy_pixbuf);

  icon_info->proxy_pixbuf =
    gdk_pixbuf_new_from_data (gdk_pixbuf_get_pixels (icon_info->pixbuf),
                              gdk_pixbuf_get_colorspace (icon_info->pixbuf),
                              gdk_pixbuf_get_has_alpha (icon_info->pixbuf),
                              gdk_pixbuf_get_bits_per_sample (icon_info->pixbuf),
                              gdk_pixbuf_get_width (icon_info->pixbuf),
                              gdk_pixbuf_get_height (icon_info->pixbuf),
                              gdk_pixbuf_get_rowstride (icon_info->pixbuf),
                              proxy_pixbuf_destroy,
                              g_object_ref (icon_info));

  return icon_info->proxy_pixbuf;
}

static void
load_icon_thread  (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  StIconInfo *dup = task_data;

  (void)icon_info_ensure_scale_and_pixbuf (dup);
  g_task_return_pointer (task, NULL, NULL);
}

/**
 * st_icon_info_load_icon_async:
 * @icon_info: a #StIconInfo from st_icon_theme_lookup_icon()
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the
 *     request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously load, render and scale an icon previously looked up
 * from the icon theme using st_icon_theme_lookup_icon().
 *
 * For more details, see st_icon_info_load_icon() which is the synchronous
 * version of this call.
 */
void
st_icon_info_load_icon_async (StIconInfo          *icon_info,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  GTask *task;
  GdkPixbuf *pixbuf;
  StIconInfo *dup;
  GError *error = NULL;

  task = g_task_new (icon_info, cancellable, callback, user_data);

  if (icon_info_get_pixbuf_ready (icon_info))
    {
      pixbuf = st_icon_info_load_icon (icon_info, &error);
      if (pixbuf == NULL)
        g_task_return_error (task, error);
      else
        g_task_return_pointer (task, pixbuf, g_object_unref);
      g_object_unref (task);
    }
  else
    {
      dup = icon_info_dup (icon_info);
      g_task_set_task_data (task, dup, g_object_unref);
      g_task_run_in_thread (task, load_icon_thread);
      g_object_unref (task);
    }
}

/**
 * st_icon_info_load_icon_finish:
 * @icon_info: a #StIconInfo from st_icon_theme_lookup_icon()
 * @res: a #GAsyncResult
 * @error: (allow-none): location to store error information on failure,
 *     or %NULL.
 *
 * Finishes an async icon load, see st_icon_info_load_icon_async().
 *
 * Returns: (transfer full): the rendered icon; this may be a newly
 *     created icon or a new reference to an internal icon, so you must
 *     not modify the icon. Use g_object_unref() to release your reference
 *     to the icon.
 */
GdkPixbuf *
st_icon_info_load_icon_finish (StIconInfo    *icon_info,
                               GAsyncResult  *result,
                               GError       **error)
{
  GTask *task = G_TASK (result);
  StIconInfo *dup;

  g_return_val_if_fail (g_task_is_valid (result, icon_info), NULL);

  dup = g_task_get_task_data (task);
  if (dup == NULL || g_task_had_error (task))
    return g_task_propagate_pointer (task, error);

  /* We ran the thread and it was not cancelled */

  /* Check if someone else updated the icon_info in between */
  if (!icon_info_get_pixbuf_ready (icon_info))
    {
      /* If not, copy results from dup back to icon_info */
      icon_info->emblems_applied = dup->emblems_applied;
      icon_info->scale = dup->scale;
      g_clear_object (&icon_info->pixbuf);
      if (dup->pixbuf)
        icon_info->pixbuf = g_object_ref (dup->pixbuf);
      g_clear_error (&icon_info->load_error);
      if (dup->load_error)
        icon_info->load_error = g_error_copy (dup->load_error);
    }

  g_assert (icon_info_get_pixbuf_ready (icon_info));

  /* This is now guaranteed to not block */
  return st_icon_info_load_icon (icon_info, error);
}

static void
proxy_symbolic_pixbuf_destroy (guchar   *pixels,
                               gpointer  data)
{
  StIconInfo *icon_info = data;
  StIconTheme *icon_theme = icon_info->in_cache;
  SymbolicPixbufCache *symbolic_cache;

  for (symbolic_cache = icon_info->symbolic_pixbuf_cache;
       symbolic_cache != NULL;
       symbolic_cache = symbolic_cache->next)
    {
      if (symbolic_cache->proxy_pixbuf != NULL &&
          gdk_pixbuf_get_pixels (symbolic_cache->proxy_pixbuf) == pixels)
        break;
    }

  g_assert (symbolic_cache != NULL);
  g_assert (symbolic_cache->proxy_pixbuf != NULL);

  symbolic_cache->proxy_pixbuf = NULL;

  /* Keep it alive a bit longer */
  if (icon_theme != NULL)
    ensure_in_lru_cache (icon_theme, icon_info);

  g_object_unref (icon_info);
}

static GdkPixbuf *
symbolic_cache_get_proxy (SymbolicPixbufCache *symbolic_cache,
                          StIconInfo          *icon_info)
{
  if (symbolic_cache->proxy_pixbuf)
    return g_object_ref (symbolic_cache->proxy_pixbuf);

  symbolic_cache->proxy_pixbuf =
    gdk_pixbuf_new_from_data (gdk_pixbuf_get_pixels (symbolic_cache->pixbuf),
                              gdk_pixbuf_get_colorspace (symbolic_cache->pixbuf),
                              gdk_pixbuf_get_has_alpha (symbolic_cache->pixbuf),
                              gdk_pixbuf_get_bits_per_sample (symbolic_cache->pixbuf),
                              gdk_pixbuf_get_width (symbolic_cache->pixbuf),
                              gdk_pixbuf_get_height (symbolic_cache->pixbuf),
                              gdk_pixbuf_get_rowstride (symbolic_cache->pixbuf),
                              proxy_symbolic_pixbuf_destroy,
                              g_object_ref (icon_info));

  return symbolic_cache->proxy_pixbuf;
}

static char *
color_to_string_noalpha (const ClutterColor *color)
{
  return g_strdup_printf ("rgb(%d,%d,%d)",
                          color->red,
                          color->green,
                          color->blue);
}

static void
color_to_pixel(const ClutterColor *color,
               uint8_t             pixel[4])
{
  pixel[0] = color->red;
  pixel[1] = color->green;
  pixel[2] = color->blue;
  pixel[3] = 255;
}

static GdkPixbuf *
color_symbolic_pixbuf (GdkPixbuf    *symbolic,
                       StIconColors *colors)
{
  int width, height, x, y, src_stride, dst_stride;
  guchar *src_data, *dst_data;
  guchar *src_row, *dst_row;
  int alpha;
  GdkPixbuf *colored;
  uint8_t fg_pixel[4], success_pixel[4], warning_pixel[4], error_pixel[4];

  alpha = colors->foreground.alpha;

  color_to_pixel (&colors->foreground, fg_pixel);
  color_to_pixel (&colors->success, success_pixel);
  color_to_pixel (&colors->warning, warning_pixel);
  color_to_pixel (&colors->error, error_pixel);

  width = gdk_pixbuf_get_width (symbolic);
  height = gdk_pixbuf_get_height (symbolic);

  colored = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);

  src_stride = gdk_pixbuf_get_rowstride (symbolic);
  src_data = gdk_pixbuf_get_pixels (symbolic);

  dst_data = gdk_pixbuf_get_pixels (colored);
  dst_stride = gdk_pixbuf_get_rowstride (colored);

  for (y = 0; y < height; y++)
    {
      src_row = src_data + src_stride * y;
      dst_row = dst_data + dst_stride * y;
      for (x = 0; x < width; x++)
        {
          guint r, g, b, a;
          int c1, c2, c3, c4;

          a = src_row[3];
          dst_row[3] = a * alpha / 255;

          if (a == 0)
            {
              dst_row[0] = 0;
              dst_row[1] = 0;
              dst_row[2] = 0;
            }
          else
            {
              c2 = src_row[0];
              c3 = src_row[1];
              c4 = src_row[2];

              if (c2 == 0 && c3 == 0 && c4 == 0)
                {
                  dst_row[0] = fg_pixel[0];
                  dst_row[1] = fg_pixel[1];
                  dst_row[2] = fg_pixel[2];
                }
              else
                {
                  c1 = 255 - c2 - c3 - c4;

                  r = fg_pixel[0] * c1 + success_pixel[0] * c2 +  warning_pixel[0] * c3 +  error_pixel[0] * c4;
                  g = fg_pixel[1] * c1 + success_pixel[1] * c2 +  warning_pixel[1] * c3 +  error_pixel[1] * c4;
                  b = fg_pixel[2] * c1 + success_pixel[2] * c2 +  warning_pixel[2] * c3 +  error_pixel[2] * c4;

                  dst_row[0] = r / 255;
                  dst_row[1] = g / 255;
                  dst_row[2] = b / 255;
                }
            }

          src_row += 4;
          dst_row += 4;
        }
    }

  return colored;
}

static GdkPixbuf *
st_icon_info_load_symbolic_png (StIconInfo    *icon_info,
                                StIconColors  *colors,
                                GError       **error)
{
  if (!icon_info_ensure_scale_and_pixbuf (icon_info))
    {
      if (icon_info->load_error)
        {
          if (error)
            *error = g_error_copy (icon_info->load_error);
        }
      else
        {
          g_set_error_literal (error,
                               ST_ICON_THEME_ERROR,
                               ST_ICON_THEME_NOT_FOUND,
                               _("Failed to load icon"));
        }

      return NULL;
    }

  return color_symbolic_pixbuf (icon_info->pixbuf, colors);
}

static GdkPixbuf *
st_icon_info_load_symbolic_svg (StIconInfo    *icon_info,
                                StIconColors  *colors,
                                GError       **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;
  g_autofree char *css_fg = NULL;
  g_autofree char *css_success = NULL;
  g_autofree char *css_warning = NULL;
  g_autofree char *css_error = NULL;
  g_autofree char *width = NULL;
  g_autofree char *height = NULL;
  g_autofree char *file_data = NULL;
  g_autofree char *escaped_file_data = NULL;
  char *data;
  gsize file_len;
  int symbolic_size;
  double alpha;
  char alphastr[G_ASCII_DTOSTR_BUF_SIZE];

  alpha = colors->foreground.alpha / 255.;

  css_fg = color_to_string_noalpha (&colors->foreground);

  css_warning = color_to_string_noalpha (&colors->warning);
  css_error = color_to_string_noalpha (&colors->error);
  css_success = color_to_string_noalpha (&colors->success);

  if (!g_file_load_contents (icon_info->icon_file, NULL, &file_data, &file_len, NULL, error))
    return NULL;

  if (!icon_info_ensure_scale_and_pixbuf (icon_info))
    {
      g_propagate_error (error, icon_info->load_error);
      icon_info->load_error = NULL;
      return NULL;
    }

  if (icon_info->symbolic_width == 0 ||
      icon_info->symbolic_height == 0)
    {
      /* Fetch size from the original icon */
      stream = g_memory_input_stream_new_from_data (file_data, file_len, NULL);
      pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, error);
      g_object_unref (stream);

      if (!pixbuf)
        return NULL;

      icon_info->symbolic_width = gdk_pixbuf_get_width (pixbuf);
      icon_info->symbolic_height = gdk_pixbuf_get_height (pixbuf);
      g_object_unref (pixbuf);
    }

  symbolic_size = MAX (icon_info->symbolic_width, icon_info->symbolic_height);

  if (icon_info->dir_type == ICON_THEME_DIR_UNTHEMED)
    g_debug ("Symbolic icon %s is not in an icon theme directory",
             icon_info->key.icon_names ? icon_info->key.icon_names[0] : icon_info->filename);
  else if (icon_info->dir_size * icon_info->dir_scale != symbolic_size)
    g_debug ("Symbolic icon %s of size %d is in an icon theme directory of size %d",
             icon_info->key.icon_names ? icon_info->key.icon_names[0] : icon_info->filename,
             symbolic_size,
             icon_info->dir_size * icon_info->dir_scale);

  width = g_strdup_printf ("%d", icon_info->symbolic_width);
  height = g_strdup_printf ("%d", icon_info->symbolic_height);

  escaped_file_data = g_base64_encode ((guchar *) file_data, file_len);

  g_ascii_dtostr (alphastr, G_ASCII_DTOSTR_BUF_SIZE, CLAMP (alpha, 0, 1));

  data = g_strconcat ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
                      "<svg version=\"1.1\"\n"
                      "     xmlns=\"http://www.w3.org/2000/svg\"\n"
                      "     xmlns:xi=\"http://www.w3.org/2001/XInclude\"\n"
                      "     width=\"", width, "\"\n"
                      "     height=\"", height, "\">\n"
                      "  <style type=\"text/css\">\n"
                      "    rect,path,ellipse,circle,polygon {\n"
                      "      fill: ", css_fg," !important;\n"
                      "    }\n"
                      "    .warning {\n"
                      "      fill: ", css_warning, " !important;\n"
                      "    }\n"
                      "    .error {\n"
                      "      fill: ", css_error ," !important;\n"
                      "    }\n"
                      "    .success {\n"
                      "      fill: ", css_success, " !important;\n"
                      "    }\n"
                      "  </style>\n"
                      "  <g opacity=\"", alphastr, "\" ><xi:include href=\"data:text/xml;base64,", escaped_file_data, "\"/></g>\n"
                      "</svg>",
                      NULL);

  stream = g_memory_input_stream_new_from_data (data, -1, g_free);
  pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream,
                                                gdk_pixbuf_get_width (icon_info->pixbuf),
                                                gdk_pixbuf_get_height (icon_info->pixbuf),
                                                TRUE,
                                                NULL,
                                                error);
  g_object_unref (stream);

  return pixbuf;
}


static GdkPixbuf *
st_icon_info_load_symbolic_internal (StIconInfo     *icon_info,
				     StIconColors  *colors,
				     gboolean       use_cache,
				     GError       **error)
{
  GdkPixbuf *pixbuf;
  SymbolicPixbufCache *symbolic_cache;
  g_autofree char *icon_uri = NULL;

  if (use_cache)
    {
      symbolic_cache = symbolic_pixbuf_cache_matches (icon_info->symbolic_pixbuf_cache, colors);
      if (symbolic_cache)
        return symbolic_cache_get_proxy (symbolic_cache, icon_info);
    }

  /* css_fg can't possibly have failed, otherwise
   * that would mean we have a broken style
   */
  g_return_val_if_fail (colors != NULL, NULL);

  icon_uri = g_file_get_uri (icon_info->icon_file);
  if (g_str_has_suffix (icon_uri, ".symbolic.png"))
    pixbuf = st_icon_info_load_symbolic_png (icon_info, colors, error);
  else
    pixbuf = st_icon_info_load_symbolic_svg (icon_info, colors, error);

  if (pixbuf != NULL)
    {
      GdkPixbuf *icon;

      icon = apply_emblems_to_pixbuf (pixbuf, icon_info);
      if (icon != NULL)
        {
          g_object_unref (pixbuf);
          pixbuf = icon;
        }

      if (use_cache)
        {
          icon_info->symbolic_pixbuf_cache =
            symbolic_pixbuf_cache_new (pixbuf, colors, icon_info->symbolic_pixbuf_cache);
          g_object_unref (pixbuf);
          return symbolic_cache_get_proxy (icon_info->symbolic_pixbuf_cache, icon_info);
        }
      else
        return pixbuf;
    }

  return NULL;
}

/**
 * st_icon_info_load_symbolic:
 * @icon_info: a #StIconInfo
 * @colors: a #StIconColors representing the foreground, warning and error colors
 * @was_symbolic: (out) (allow-none): a #gboolean, returns whether the
 *     loaded icon was a symbolic one and whether the @fg color was
 *     applied to it.
 * @error: (allow-none): location to store error information on failure,
 *     or %NULL.
 *
 * Loads an icon, modifying it to match the system colours for the foreground,
 * success, warning and error colors provided. If the icon is not a symbolic
 * one, the function will return the result from st_icon_info_load_icon().
 *
 * This allows loading symbolic icons that will match the system theme.
 *
 * Unless you are implementing a widget, you will want to use
 * g_themed_icon_new_with_default_fallbacks() to load the icon.
 *
 * As implementation details, the icon loaded needs to be of SVG type,
 * contain the “symbolic” term as the last component of the icon name,
 * and use the “fg”, “success”, “warning” and “error” CSS styles in the
 * SVG file itself.
 *
 * See the [Symbolic Icons Specification](http://www.freedesktop.org/wiki/SymbolicIcons)
 * for more information about symbolic icons.
 *
 * Returns: (transfer full): a #GdkPixbuf representing the loaded icon
 */
GdkPixbuf *
st_icon_info_load_symbolic (StIconInfo    *icon_info,
                            StIconColors  *colors,
                            gboolean      *was_symbolic,
                            GError       **error)
{
  gboolean is_symbolic;

  g_return_val_if_fail (icon_info != NULL, NULL);
  g_return_val_if_fail (colors != NULL, NULL);

  is_symbolic = st_icon_info_is_symbolic (icon_info);

  if (was_symbolic)
    *was_symbolic = is_symbolic;

  if (!is_symbolic)
    return st_icon_info_load_icon (icon_info, error);

  return st_icon_info_load_symbolic_internal (icon_info,
                                              colors,
                                              TRUE,
                                              error);
}

typedef struct {
  gboolean is_symbolic;
  StIconInfo *dup;
  StIconColors *colors;
} AsyncSymbolicData;

static void
async_symbolic_data_free (AsyncSymbolicData *data)
{
  if (data->dup)
    g_object_unref (data->dup);
  g_clear_pointer (&data->colors, st_icon_colors_unref);
  g_free (data);
}

static void
async_load_no_symbolic_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  StIconInfo *icon_info = ST_ICON_INFO (source_object);
  GTask *task = user_data;
  GError *error = NULL;
  GdkPixbuf *pixbuf;

  pixbuf = st_icon_info_load_icon_finish (icon_info, res, &error);
  if (pixbuf == NULL)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, pixbuf, g_object_unref);
  g_object_unref (task);
}

static void
load_symbolic_icon_thread (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  AsyncSymbolicData *data = task_data;
  GError *error;
  GdkPixbuf *pixbuf;

  error = NULL;
  pixbuf = st_icon_info_load_symbolic_internal (data->dup,
                                                data->colors,
                                                FALSE,
                                                &error);
  if (pixbuf == NULL)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, pixbuf, g_object_unref);
}

/**
 * st_icon_info_load_symbolic_async:
 * @icon_info: a #StIconInfo from st_icon_theme_lookup_icon()
 * @colors: an #StIconColors representing the foreground, error and
 *     success colors of the icon
 * @cancellable: (allow-none): optional #GCancellable object,
 *     %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the
 *     request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously load, render and scale a symbolic icon previously looked up
 * from the icon theme using st_icon_theme_lookup_icon().
 *
 * For more details, see st_icon_info_load_symbolic() which is the synchronous
 * version of this call.
 */
void
st_icon_info_load_symbolic_async (StIconInfo          *icon_info,
                                  StIconColors        *colors,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GTask *task;
  AsyncSymbolicData *data;
  SymbolicPixbufCache *symbolic_cache;
  GdkPixbuf *pixbuf;

  g_return_if_fail (icon_info != NULL);
  g_return_if_fail (colors != NULL);

  task = g_task_new (icon_info, cancellable, callback, user_data);

  data = g_new0 (AsyncSymbolicData, 1);
  g_task_set_task_data (task, data, (GDestroyNotify) async_symbolic_data_free);

  data->is_symbolic = st_icon_info_is_symbolic (icon_info);

  if (!data->is_symbolic)
    {
      st_icon_info_load_icon_async (icon_info, cancellable, async_load_no_symbolic_cb, g_object_ref (task));
    }
  else
    {
      symbolic_cache = symbolic_pixbuf_cache_matches (icon_info->symbolic_pixbuf_cache, colors);
      if (symbolic_cache)
        {
          pixbuf = symbolic_cache_get_proxy (symbolic_cache, icon_info);
          g_task_return_pointer (task, pixbuf, g_object_unref);
        }
      else
        {
          data->dup = icon_info_dup (icon_info);
          data->colors = st_icon_colors_ref (colors);
          g_task_run_in_thread (task, load_symbolic_icon_thread);
        }
    }
  g_object_unref (task);
}

/**
 * st_icon_info_load_symbolic_finish:
 * @icon_info: a #StIconInfo from st_icon_theme_lookup_icon()
 * @res: a #GAsyncResult
 * @was_symbolic: (out) (allow-none): a #gboolean, returns whether the
 *     loaded icon was a symbolic one and whether the @fg color was
 *     applied to it.
 * @error: (allow-none): location to store error information on failure,
 *     or %NULL.
 *
 * Finishes an async icon load, see st_icon_info_load_symbolic_async().
 *
 * Returns: (transfer full): the rendered icon; this may be a newly
 *     created icon or a new reference to an internal icon, so you must
 *     not modify the icon. Use g_object_unref() to release your reference
 *     to the icon.
 */
GdkPixbuf *
st_icon_info_load_symbolic_finish (StIconInfo    *icon_info,
                                   GAsyncResult  *result,
                                   gboolean      *was_symbolic,
                                   GError       **error)
{
  GTask *task = G_TASK (result);
  AsyncSymbolicData *data = g_task_get_task_data (task);
  SymbolicPixbufCache *symbolic_cache;
  GdkPixbuf *pixbuf;

  if (was_symbolic)
    *was_symbolic = data->is_symbolic;

  if (data->dup && !g_task_had_error (task))
    {
      pixbuf = g_task_propagate_pointer (task, NULL);

      g_assert (pixbuf != NULL); /* we checked for !had_error above */

      symbolic_cache = symbolic_pixbuf_cache_matches (icon_info->symbolic_pixbuf_cache,
                                                      data->colors);

      if (symbolic_cache == NULL)
        {
          symbolic_cache = icon_info->symbolic_pixbuf_cache =
            symbolic_pixbuf_cache_new (pixbuf,
                                       data->colors,
                                       icon_info->symbolic_pixbuf_cache);
        }

      g_object_unref (pixbuf);

      return symbolic_cache_get_proxy (symbolic_cache, icon_info);
    }

  return g_task_propagate_pointer (task, error);
}

/**
 * st_icon_theme_lookup_by_gicon:
 * @icon_theme: a #StIconTheme
 * @icon: the #GIcon to look up
 * @size: desired icon size
 * @flags: flags modifying the behavior of the icon lookup
 *
 * Looks up an icon and returns a #StIconInfo containing information
 * such as the filename of the icon. The icon can then be rendered
 * into a pixbuf using st_icon_info_load_icon().
 *
 * When rendering on displays with high pixel densities you should not
 * use a @size multiplied by the scaling factor returned by functions
 * like gdk_window_get_scale_factor(). Instead, you should use
 * st_icon_theme_lookup_by_gicon_for_scale(), as the assets loaded
 * for a given scaling factor may be different.
 *
 * Returns: (nullable) (transfer full): a #StIconInfo containing
 *     information about the icon, or %NULL if the icon wasn’t
 *     found. Unref with g_object_unref()
 */
StIconInfo *
st_icon_theme_lookup_by_gicon (StIconTheme       *icon_theme,
                               GIcon             *icon,
                               int                size,
                               StIconLookupFlags  flags)
{
  return st_icon_theme_lookup_by_gicon_for_scale (icon_theme, icon,
                                                   size, 1, flags);
}


/**
 * st_icon_theme_lookup_by_gicon_for_scale:
 * @icon_theme: a #StIconTheme
 * @icon: the #GIcon to look up
 * @size: desired icon size
 * @scale: the desired scale
 * @flags: flags modifying the behavior of the icon lookup
 *
 * Looks up an icon and returns a #StIconInfo containing information
 * such as the filename of the icon. The icon can then be rendered into
 * a pixbuf using st_icon_info_load_icon().
 *
 * Returns: (nullable) (transfer full): a #StIconInfo containing
 *     information about the icon, or %NULL if the icon wasn’t
 *     found. Unref with g_object_unref()
 */
StIconInfo *
st_icon_theme_lookup_by_gicon_for_scale (StIconTheme       *icon_theme,
                                         GIcon             *icon,
                                         int                size,
                                         int                scale,
                                         StIconLookupFlags  flags)
{
  StIconInfo *info;

  g_return_val_if_fail (ST_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (G_IS_ICON (icon), NULL);
  g_warn_if_fail ((flags & ST_ICON_LOOKUP_GENERIC_FALLBACK) == 0);

  if (GDK_IS_PIXBUF (icon))
    {
      GdkPixbuf *pixbuf;

      pixbuf = GDK_PIXBUF (icon);

      if ((flags & ST_ICON_LOOKUP_FORCE_SIZE) != 0)
        {
          int width, height, max;
          gdouble pixbuf_scale;
          GdkPixbuf *scaled;

          width = gdk_pixbuf_get_width (pixbuf);
          height = gdk_pixbuf_get_height (pixbuf);
          max = MAX (width, height);
          pixbuf_scale = (gdouble) size * scale / (gdouble) max;

          scaled = gdk_pixbuf_scale_simple (pixbuf,
                                            0.5 + width * pixbuf_scale,
                                            0.5 + height * pixbuf_scale,
                                            GDK_INTERP_BILINEAR);

          info = st_icon_info_new_for_pixbuf (icon_theme, scaled);

          g_object_unref (scaled);
        }
      else
        {
          info = st_icon_info_new_for_pixbuf (icon_theme, pixbuf);
        }

      return info;
    }
  else if (G_IS_FILE_ICON (icon))
    {
      GFile *file = g_file_icon_get_file (G_FILE_ICON (icon));

      info = st_icon_info_new_for_file (file, size, scale);
      info->forced_size = (flags & ST_ICON_LOOKUP_FORCE_SIZE) != 0;

      return info;
    }
  else if (G_IS_LOADABLE_ICON (icon))
    {
      info = icon_info_new (ICON_THEME_DIR_UNTHEMED, size, 1);
      info->loadable = G_LOADABLE_ICON (g_object_ref (icon));
      info->is_svg = FALSE;
      info->desired_size = size;
      info->desired_scale = scale;
      info->forced_size = (flags & ST_ICON_LOOKUP_FORCE_SIZE) != 0;

      return info;
    }
  else if (G_IS_THEMED_ICON (icon))
    {
      const char **names;

      names = (const char **)g_themed_icon_get_names (G_THEMED_ICON (icon));
      info = st_icon_theme_choose_icon_for_scale (icon_theme, names, size, scale, flags);

      return info;
    }
  else if (G_IS_EMBLEMED_ICON (icon))
    {
      GIcon *base, *emblem;
      GList *list, *l;
      StIconInfo *base_info, *emblem_info;

      base = g_emblemed_icon_get_icon (G_EMBLEMED_ICON (icon));
      base_info = st_icon_theme_lookup_by_gicon_for_scale (icon_theme, base, size, scale, flags);
      if (base_info)
        {
          info = icon_info_dup (base_info);
          g_object_unref (base_info);

          list = g_emblemed_icon_get_emblems (G_EMBLEMED_ICON (icon));
          for (l = list; l; l = l->next)
            {
              emblem = g_emblem_get_icon (G_EMBLEM (l->data));
              /* always force size for emblems */
              emblem_info = st_icon_theme_lookup_by_gicon_for_scale (icon_theme, emblem, size / 2, scale, flags | ST_ICON_LOOKUP_FORCE_SIZE);
              if (emblem_info)
                info->emblem_infos = g_slist_prepend (info->emblem_infos, emblem_info);
            }

          return info;
        }
      else
        return NULL;
    }

  return NULL;
}

/**
 * st_icon_info_new_for_pixbuf:
 * @icon_theme: a #StIconTheme
 * @pixbuf: the pixbuf to wrap in a #StIconInfo
 *
 * Creates a #StIconInfo for a #GdkPixbuf.
 *
 * Returns: (transfer full): a #StIconInfo
 */
StIconInfo *
st_icon_info_new_for_pixbuf (StIconTheme *icon_theme,
                             GdkPixbuf   *pixbuf)
{
  StIconInfo *info;

  g_return_val_if_fail (ST_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  info = icon_info_new (ICON_THEME_DIR_UNTHEMED, 0, 1);
  info->pixbuf = g_object_ref (pixbuf);
  info->scale = 1.0;

  return info;
}

static StIconInfo *
st_icon_info_new_for_file (GFile *file,
                           int    size,
                           int    scale)
{
  StIconInfo *info;

  info = icon_info_new (ICON_THEME_DIR_UNTHEMED, size, 1);
  info->loadable = G_LOADABLE_ICON (g_file_icon_new (file));
  info->icon_file = g_object_ref (file);
  info->is_resource = g_file_has_uri_scheme (file, "resource");

  if (info->is_resource)
    {
      g_autofree char *uri = NULL;

      uri = g_file_get_uri (file);
      info->filename = g_strdup (uri + 11); /* resource:// */
    }
  else
    {
      info->filename = g_file_get_path (file);
    }

  info->is_svg = suffix_from_name (info->filename) == ICON_SUFFIX_SVG;

 info->desired_size = size;
 info->desired_scale = scale;
 info->forced_size = FALSE;

 return info;
}
