/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __SHELL_UTIL_H__
#define __SHELL_UTIL_H__

#include <gio/gio.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

char *shell_util_get_label_for_uri (const char *text_uri);
GIcon *shell_util_get_icon_for_uri (const char *text_uri);
GIcon *shell_util_icon_from_string (const char *string, GError **error);
void   shell_util_set_hidden_from_pick (ClutterActor *actor, gboolean hidden);

void shell_util_get_transformed_allocation (ClutterActor    *actor,
                                            ClutterActorBox *box);

char *shell_util_format_date (const char *format,
                              gint64      time_ms);

G_END_DECLS

#endif /* __SHELL_UTIL_H__ */
