/* StIconTheme - a loader for icon themes
 * gtk-icon-loader.h Copyright (C) 2002, 2003 Red Hat, Inc.
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

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#pragma once

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "st-icon-colors.h"

G_BEGIN_DECLS

#define ST_TYPE_ICON_INFO              (st_icon_info_get_type ())
G_DECLARE_FINAL_TYPE (StIconInfo, st_icon_info, ST, ICON_INFO, GObject)

#define ST_TYPE_ICON_THEME             (st_icon_theme_get_type ())
G_DECLARE_FINAL_TYPE (StIconTheme, st_icon_theme, ST, ICON_THEME, GObject)

/**
 * StIconLookupFlags:
 * @ST_ICON_LOOKUP_NO_SVG: Never get SVG icons, even if gdk-pixbuf
 *   supports them. Cannot be used together with %ST_ICON_LOOKUP_FORCE_SVG.
 * @ST_ICON_LOOKUP_FORCE_SVG: Get SVG icons, even if gdk-pixbuf
 *   doesn’t support them.
 *   Cannot be used together with %ST_ICON_LOOKUP_NO_SVG.
 * @ST_ICON_LOOKUP_GENERIC_FALLBACK: Try to shorten icon name at '-'
 *   characters before looking at inherited themes. This flag is only
 *   supported in functions that take a single icon name. For more general
 *   fallback, see st_icon_theme_choose_icon().
 * @ST_ICON_LOOKUP_FORCE_SIZE: Always get the icon scaled to the
 *   requested size.
 * @ST_ICON_LOOKUP_FORCE_REGULAR: Try to always load regular icons, even
 *   when symbolic icon names are given.
 * @ST_ICON_LOOKUP_FORCE_SYMBOLIC: Try to always load symbolic icons, even
 *   when regular icon names are given.
 * @ST_ICON_LOOKUP_DIR_LTR: Try to load a variant of the icon for left-to-right
 *   text direction.
 * @ST_ICON_LOOKUP_DIR_RTL: Try to load a variant of the icon for right-to-left
 *   text direction.
 *
 * Used to specify options for st_icon_theme_lookup_icon()
 */
typedef enum
{
  ST_ICON_LOOKUP_NO_SVG           = 1 << 0,
  ST_ICON_LOOKUP_FORCE_SVG        = 1 << 1,
  ST_ICON_LOOKUP_GENERIC_FALLBACK = 1 << 2,
  ST_ICON_LOOKUP_FORCE_SIZE       = 1 << 3,
  ST_ICON_LOOKUP_FORCE_REGULAR    = 1 << 4,
  ST_ICON_LOOKUP_FORCE_SYMBOLIC   = 1 << 5,
  ST_ICON_LOOKUP_DIR_LTR          = 1 << 6,
  ST_ICON_LOOKUP_DIR_RTL          = 1 << 7
} StIconLookupFlags;

/**
 * ST_ICON_THEME_ERROR:
 *
 * The #GQuark used for #StIconThemeError errors.
 */
#define ST_ICON_THEME_ERROR st_icon_theme_error_quark ()

/**
 * StIconThemeError:
 * @ST_ICON_THEME_NOT_FOUND: The icon specified does not exist in the theme
 * @ST_ICON_THEME_FAILED: An unspecified error occurred.
 *
 * Error codes for StIconTheme operations.
 **/
typedef enum {
  ST_ICON_THEME_NOT_FOUND,
  ST_ICON_THEME_FAILED
} StIconThemeError;

GQuark st_icon_theme_error_quark (void);

StIconTheme *st_icon_theme_new (void);

void st_icon_theme_set_search_path (StIconTheme *icon_theme,
                                    const char  *path[],
                                    int          n_elements);

void st_icon_theme_get_search_path (StIconTheme  *icon_theme,
                                    char        **path[],
                                    int          *n_elements);

void st_icon_theme_append_search_path (StIconTheme *icon_theme,
                                       const char  *path);

void st_icon_theme_prepend_search_path (StIconTheme *icon_theme,
                                        const char  *path);

void st_icon_theme_add_resource_path (StIconTheme *icon_theme,
                                      const char  *path);

gboolean st_icon_theme_has_icon (StIconTheme *icon_theme,
                                 const char  *icon_name);

int * st_icon_theme_get_icon_sizes (StIconTheme *icon_theme,
                                    const char  *icon_name);

StIconInfo * st_icon_theme_lookup_icon (StIconTheme       *icon_theme,
                                        const char        *icon_name,
                                        int                size,
                                        StIconLookupFlags  flags);

StIconInfo * st_icon_theme_lookup_icon_for_scale (StIconTheme       *icon_theme,
                                                  const char        *icon_name,
                                                  int                size,
                                                  int                scale,
                                                  StIconLookupFlags  flags);

StIconInfo * st_icon_theme_choose_icon (StIconTheme       *icon_theme,
                                        const char        *icon_names[],
                                        int                size,
                                        StIconLookupFlags  flags);

StIconInfo * st_icon_theme_choose_icon_for_scale (StIconTheme       *icon_theme,
                                                  const char        *icon_names[],
                                                  int                size,
                                                  int                scale,
                                                  StIconLookupFlags  flags);

GdkPixbuf * st_icon_theme_load_icon (StIconTheme        *icon_theme,
                                     const char         *icon_name,
                                     int                 size,
                                     StIconLookupFlags   flags,
                                     GError            **error);

GdkPixbuf * st_icon_theme_load_icon_for_scale (StIconTheme       *icon_theme,
                                               const char        *icon_name,
                                               int                size,
                                               int                scale,
                                               StIconLookupFlags  flags,
                                               GError            **error);

StIconInfo * st_icon_theme_lookup_by_gicon (StIconTheme       *icon_theme,
                                            GIcon              *icon,
                                            int                size,
                                            StIconLookupFlags  flags);

StIconInfo * st_icon_theme_lookup_by_gicon_for_scale (StIconTheme       *icon_theme,
                                                      GIcon              *icon,
                                                      int                size,
                                                      int                scale,
                                                      StIconLookupFlags  flags);


GList * st_icon_theme_list_icons (StIconTheme *icon_theme,
                                  const char  *context);

GList * st_icon_theme_list_contexts (StIconTheme *icon_theme);

gboolean st_icon_theme_rescan_if_needed (StIconTheme *icon_theme);

StIconInfo * st_icon_info_new_for_pixbuf (StIconTheme *icon_theme,
                                          GdkPixbuf   *pixbuf);

int st_icon_info_get_base_size (StIconInfo *icon_info);

int st_icon_info_get_base_scale (StIconInfo *icon_info);

const char * st_icon_info_get_filename (StIconInfo *icon_info);

gboolean st_icon_info_is_symbolic (StIconInfo *icon_info);

GdkPixbuf * st_icon_info_load_icon (StIconInfo  *icon_info,
                                    GError     **error);

void st_icon_info_load_icon_async (StIconInfo          *icon_info,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data);

GdkPixbuf * st_icon_info_load_icon_finish (StIconInfo    *icon_info,
                                           GAsyncResult  *res,
                                           GError       **error);

GdkPixbuf * st_icon_info_load_symbolic (StIconInfo    *icon_info,
                                        StIconColors  *colors,
                                        gboolean      *was_symbolic,
                                        GError       **error);

void st_icon_info_load_symbolic_async (StIconInfo           *icon_info,
                                       StIconColors         *colors,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);

GdkPixbuf * st_icon_info_load_symbolic_finish (StIconInfo    *icon_info,
                                               GAsyncResult  *res,
                                               gboolean      *was_symbolic,
                                               GError       **error);
G_END_DECLS
