/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef SHELL_ALT_TAB_HANDLER_H
#define SHELL_ALT_TAB_HANDLER_H

#include <alttabhandler.h>

#define SHELL_TYPE_ALT_TAB_HANDLER            (shell_alt_tab_handler_get_type ())
#define SHELL_ALT_TAB_HANDLER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_ALT_TAB_HANDLER, ShellAltTabHandler))
#define SHELL_ALT_TAB_HANDLER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  SHELL_TYPE_ALT_TAB_HANDLER, ShellAltTabHandlerClass))
#define SHELL_IS_ALT_TAB_HANDLER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_ALT_TAB_HANDLER_TYPE))
#define SHELL_IS_ALT_TAB_HANDLER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  SHELL_TYPE_ALT_TAB_HANDLER))
#define SHELL_ALT_TAB_HANDLER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  SHELL_TYPE_ALT_TAB_HANDLER, ShellAltTabHandlerClass))

typedef struct _ShellAltTabHandler      ShellAltTabHandler;
typedef struct _ShellAltTabHandlerClass ShellAltTabHandlerClass;

struct _ShellAltTabHandler {
  GObject parent_instance;

  GPtrArray *windows;
  int selected;
  gboolean immediate_mode;
};

struct _ShellAltTabHandlerClass {
  GObjectClass parent_class;

};

GType              shell_alt_tab_handler_get_type     (void);

#endif

