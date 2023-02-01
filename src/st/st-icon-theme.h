/* GtkIconTheme - a loader for icon themes
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

#ifndef __GTK_ICON_THEME_H__
#define __GTK_ICON_THEME_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GTK_TYPE_ICON_INFO              (gtk_icon_info_get_type ())
G_DECLARE_FINAL_TYPE (GtkIconInfo, gtk_icon_info, GTK, ICON_INFO, GObject)

#define GTK_TYPE_ICON_THEME             (gtk_icon_theme_get_type ())
G_DECLARE_FINAL_TYPE (GtkIconTheme, gtk_icon_theme, GTK, ICON_THEME, GOBject)

/**
 * GtkIconLookupFlags:
 * @GTK_ICON_LOOKUP_NO_SVG: Never get SVG icons, even if gdk-pixbuf
 *   supports them. Cannot be used together with %GTK_ICON_LOOKUP_FORCE_SVG.
 * @GTK_ICON_LOOKUP_FORCE_SVG: Get SVG icons, even if gdk-pixbuf
 *   doesn’t support them.
 *   Cannot be used together with %GTK_ICON_LOOKUP_NO_SVG.
 * @GTK_ICON_LOOKUP_GENERIC_FALLBACK: Try to shorten icon name at '-'
 *   characters before looking at inherited themes. This flag is only
 *   supported in functions that take a single icon name. For more general
 *   fallback, see gtk_icon_theme_choose_icon(). Since 2.12.
 * @GTK_ICON_LOOKUP_FORCE_SIZE: Always get the icon scaled to the
 *   requested size. Since 2.14.
 * @GTK_ICON_LOOKUP_FORCE_REGULAR: Try to always load regular icons, even
 *   when symbolic icon names are given. Since 3.14.
 * @GTK_ICON_LOOKUP_FORCE_SYMBOLIC: Try to always load symbolic icons, even
 *   when regular icon names are given. Since 3.14.
 * @GTK_ICON_LOOKUP_DIR_LTR: Try to load a variant of the icon for left-to-right
 *   text direction. Since 3.14.
 * @GTK_ICON_LOOKUP_DIR_RTL: Try to load a variant of the icon for right-to-left
 *   text direction. Since 3.14.
 *
 * Used to specify options for gtk_icon_theme_lookup_icon()
 */
typedef enum
{
  GTK_ICON_LOOKUP_NO_SVG           = 1 << 0,
  GTK_ICON_LOOKUP_FORCE_SVG        = 1 << 1,
  GTK_ICON_LOOKUP_GENERIC_FALLBACK = 1 << 2,
  GTK_ICON_LOOKUP_FORCE_SIZE       = 1 << 3,
  GTK_ICON_LOOKUP_FORCE_REGULAR    = 1 << 4,
  GTK_ICON_LOOKUP_FORCE_SYMBOLIC   = 1 << 5,
  GTK_ICON_LOOKUP_DIR_LTR          = 1 << 6,
  GTK_ICON_LOOKUP_DIR_RTL          = 1 << 7
} GtkIconLookupFlags;

/**
 * GTK_ICON_THEME_ERROR:
 *
 * The #GQuark used for #GtkIconThemeError errors.
 */
#define GTK_ICON_THEME_ERROR gtk_icon_theme_error_quark ()

/**
 * GtkIconThemeError:
 * @GTK_ICON_THEME_NOT_FOUND: The icon specified does not exist in the theme
 * @GTK_ICON_THEME_FAILED: An unspecified error occurred.
 *
 * Error codes for GtkIconTheme operations.
 **/
typedef enum {
  GTK_ICON_THEME_NOT_FOUND,
  GTK_ICON_THEME_FAILED
} GtkIconThemeError;

GDK_AVAILABLE_IN_ALL
GQuark gtk_icon_theme_error_quark (void);

GDK_AVAILABLE_IN_ALL
GtkIconTheme *gtk_icon_theme_new                   (void);

GDK_AVAILABLE_IN_ALL
void          gtk_icon_theme_set_search_path       (GtkIconTheme                *icon_theme,
                                                    const char                  *path[],
                                                    int                          n_elements);
GDK_AVAILABLE_IN_ALL
void          gtk_icon_theme_get_search_path       (GtkIconTheme                *icon_theme,
                                                    char                       **path[],
                                                    int                         *n_elements);
GDK_AVAILABLE_IN_ALL
void          gtk_icon_theme_append_search_path    (GtkIconTheme                *icon_theme,
                                                    const char                  *path);
GDK_AVAILABLE_IN_ALL
void          gtk_icon_theme_prepend_search_path   (GtkIconTheme                *icon_theme,
                                                    const char                  *path);

GDK_AVAILABLE_IN_3_14
void          gtk_icon_theme_add_resource_path     (GtkIconTheme                *icon_theme,
                                                    const char                  *path);

GDK_AVAILABLE_IN_ALL
gboolean      gtk_icon_theme_has_icon              (GtkIconTheme                *icon_theme,
                                                    const char                  *icon_name);
GDK_AVAILABLE_IN_ALL
int          *gtk_icon_theme_get_icon_sizes        (GtkIconTheme                *icon_theme,
                                                    const char                  *icon_name);
GDK_AVAILABLE_IN_ALL
GtkIconInfo * gtk_icon_theme_lookup_icon           (GtkIconTheme                *icon_theme,
                                                    const char                  *icon_name,
                                                    int                          size,
                                                    GtkIconLookupFlags           flags);
GDK_AVAILABLE_IN_3_10
GtkIconInfo * gtk_icon_theme_lookup_icon_for_scale (GtkIconTheme                *icon_theme,
                                                    const char                  *icon_name,
                                                    int                          size,
                                                    int                          scale,
                                                    GtkIconLookupFlags           flags);

GDK_AVAILABLE_IN_ALL
GtkIconInfo * gtk_icon_theme_choose_icon           (GtkIconTheme                *icon_theme,
                                                    const char                  *icon_names[],
                                                    int                          size,
                                                    GtkIconLookupFlags           flags);
GDK_AVAILABLE_IN_3_10
GtkIconInfo * gtk_icon_theme_choose_icon_for_scale (GtkIconTheme                *icon_theme,
                                                    const char                  *icon_names[],
                                                    int                          size,
                                                    int                          scale,
                                                    GtkIconLookupFlags           flags);
GDK_AVAILABLE_IN_ALL
GdkPixbuf *   gtk_icon_theme_load_icon             (GtkIconTheme                *icon_theme,
                                                    const char                  *icon_name,
                                                    int                          size,
                                                    GtkIconLookupFlags           flags,
                                                    GError                     **error);
GDK_AVAILABLE_IN_3_10
GdkPixbuf *   gtk_icon_theme_load_icon_for_scale   (GtkIconTheme                *icon_theme,
                                                    const char                  *icon_name,
                                                    int                          size,
                                                    int                          scale,
                                                    GtkIconLookupFlags           flags,
                                                    GError                     **error);

GDK_AVAILABLE_IN_ALL
GtkIconInfo * gtk_icon_theme_lookup_by_gicon       (GtkIconTheme                *icon_theme,
                                                    GIcon                       *icon,
                                                    int                          size,
                                                    GtkIconLookupFlags           flags);
GDK_AVAILABLE_IN_3_10
GtkIconInfo * gtk_icon_theme_lookup_by_gicon_for_scale (GtkIconTheme             *icon_theme,
                                                        GIcon                    *icon,
                                                        int                       size,
                                                        int                       scale,
                                                        GtkIconLookupFlags        flags);


GDK_AVAILABLE_IN_ALL
GList *       gtk_icon_theme_list_icons            (GtkIconTheme                *icon_theme,
                                                    const char                  *context);
GDK_AVAILABLE_IN_ALL
GList *       gtk_icon_theme_list_contexts         (GtkIconTheme                *icon_theme);

GDK_AVAILABLE_IN_ALL
gboolean      gtk_icon_theme_rescan_if_needed      (GtkIconTheme                *icon_theme);

GDK_AVAILABLE_IN_ALL
GtkIconInfo *         gtk_icon_info_new_for_pixbuf     (GtkIconTheme  *icon_theme,
                                                        GdkPixbuf     *pixbuf);

GDK_AVAILABLE_IN_ALL
int                  gtk_icon_info_get_base_size      (GtkIconInfo   *icon_info);
GDK_AVAILABLE_IN_3_10
int                  gtk_icon_info_get_base_scale     (GtkIconInfo   *icon_info);
GDK_AVAILABLE_IN_ALL
const char *         gtk_icon_info_get_filename       (GtkIconInfo   *icon_info);
GDK_AVAILABLE_IN_3_12
gboolean              gtk_icon_info_is_symbolic        (GtkIconInfo   *icon_info);
GDK_AVAILABLE_IN_ALL
GdkPixbuf *           gtk_icon_info_load_icon          (GtkIconInfo   *icon_info,
                                                        GError       **error);
GDK_AVAILABLE_IN_3_8
void                  gtk_icon_info_load_icon_async   (GtkIconInfo          *icon_info,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
GDK_AVAILABLE_IN_3_8
GdkPixbuf *           gtk_icon_info_load_icon_finish  (GtkIconInfo          *icon_info,
                                                       GAsyncResult         *res,
                                                       GError              **error);
GDK_AVAILABLE_IN_ALL
GdkPixbuf *           gtk_icon_info_load_symbolic      (GtkIconInfo   *icon_info,
                                                        const GdkRGBA *fg,
                                                        const GdkRGBA *success_color,
                                                        const GdkRGBA *warning_color,
                                                        const GdkRGBA *error_color,
                                                        gboolean      *was_symbolic,
                                                        GError       **error);
GDK_AVAILABLE_IN_3_8
void                  gtk_icon_info_load_symbolic_async (GtkIconInfo   *icon_info,
                                                         const GdkRGBA *fg,
                                                         const GdkRGBA *success_color,
                                                         const GdkRGBA *warning_color,
                                                         const GdkRGBA *error_color,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
GDK_AVAILABLE_IN_3_8
GdkPixbuf *           gtk_icon_info_load_symbolic_finish (GtkIconInfo   *icon_info,
                                                          GAsyncResult         *res,
                                                          gboolean      *was_symbolic,
                                                          GError       **error);
G_END_DECLS

#endif /* __GTK_ICON_THEME_H__ */
