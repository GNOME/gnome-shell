/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __SHELL_UTIL_H__
#define __SHELL_UTIL_H__

#include <gio/gio.h>
#include <clutter/clutter.h>
#include <libsoup/soup.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <meta/meta-cursor-tracker.h>
#include <meta/meta-window-actor.h>

G_BEGIN_DECLS

void     shell_util_set_hidden_from_pick       (ClutterActor     *actor,
                                                gboolean          hidden);

void     shell_util_get_transformed_allocation (ClutterActor     *actor,
                                                ClutterActorBox  *box);

int      shell_util_get_week_start             (void);

char    *shell_util_format_date                (const char       *format,
                                                gint64            time_ms);
const char *shell_util_translate_time_string   (const char *str);

char    *shell_util_regex_escape               (const char *str);

gboolean shell_write_string_to_stream          (GOutputStream    *stream,
                                                const char       *str,
                                                GError          **error);

char    *shell_get_file_contents_utf8_sync     (const char       *path,
                                                GError          **error);

gboolean shell_util_wifexited                  (int               status,
                                                int              *exit);

GdkPixbuf *shell_util_create_pixbuf_from_data (const guchar      *data,
                                               gsize              len,
                                               GdkColorspace      colorspace,
                                               gboolean           has_alpha,
                                               int                bits_per_sample,
                                               int                width,
                                               int                height,
                                               int                rowstride);

void    shell_util_cursor_tracker_to_clutter (MetaCursorTracker *tracker,
                                              ClutterTexture    *texture);

gboolean shell_util_need_background_refresh (void);

ClutterContent * shell_util_get_content_for_window_actor (MetaWindowActor *window_actor,
                                                          MetaRectangle   *window_rect);

cairo_surface_t * shell_util_composite_capture_images (ClutterCapture  *captures,
                                                       int              n_captures,
                                                       int              x,
                                                       int              y,
                                                       int              width,
                                                       int              height);

void shell_util_check_cloexec_fds (void);

G_END_DECLS

#endif /* __SHELL_UTIL_H__ */
