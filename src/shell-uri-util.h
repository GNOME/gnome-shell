/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __SHELL_URI_UTIL_H__
#define __SHELL_URI_UTIL_H__

#include <gio/gio.h>

G_BEGIN_DECLS

char *shell_util_get_label_for_uri (const char *text_uri);
GIcon *shell_util_get_icon_for_uri (const char *text_uri);

G_END_DECLS

#endif /* __SHELL_URI_UTIL_H__ */
