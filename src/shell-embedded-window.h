/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_EMBEDDED_WINDOW_H__
#define __SHELL_EMBEDDED_WINDOW_H__

#include <gtk/gtk.h>
#include <clutter/clutter.h>

#define SHELL_TYPE_EMBEDDED_WINDOW (shell_embedded_window_get_type ())
G_DECLARE_DERIVABLE_TYPE (ShellEmbeddedWindow, shell_embedded_window,
                          SHELL, EMBEDDED_WINDOW, GtkWindow)

struct _ShellEmbeddedWindowClass
{
  GtkWindowClass parent_class;
};

GtkWidget *shell_embedded_window_new (void);

#endif /* __SHELL_EMBEDDED_WINDOW_H__ */
