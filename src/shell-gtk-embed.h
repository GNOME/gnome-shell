/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_GTK_EMBED_H__
#define __SHELL_GTK_EMBED_H__

#include <clutter/clutter.h>

#include "shell-embedded-window.h"

#define SHELL_TYPE_GTK_EMBED (shell_gtk_embed_get_type ())
G_DECLARE_DERIVABLE_TYPE (ShellGtkEmbed, shell_gtk_embed,
                          SHELL, GTK_EMBED, ClutterClone)

struct _ShellGtkEmbedClass
{
    ClutterCloneClass parent_class;
};

ClutterActor *shell_gtk_embed_new (ShellEmbeddedWindow *window);

#endif /* __SHELL_GTK_EMBED_H__ */
