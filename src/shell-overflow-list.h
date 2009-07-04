/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_OVERFLOW_LIST_H__
#define __SHELL_OVERFLOW_LIST_H__

#include <glib-object.h>
#include <glib.h>

#include <clutter/clutter.h>

G_BEGIN_DECLS

typedef struct _ShellOverflowList ShellOverflowList;
typedef struct _ShellOverflowListClass ShellOverflowListClass;
typedef struct _ShellOverflowListPrivate ShellOverflowListPrivate;

#define SHELL_TYPE_OVERFLOW_LIST              (shell_overflow_list_get_type ())
#define SHELL_OVERFLOW_LIST(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), SHELL_TYPE_OVERFLOW_LIST, ShellOverflowList))
#define SHELL_OVERFLOW_LIST_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_OVERFLOW_LIST, ShellOverflowListClass))
#define SHELL_IS_OVERFLOW_LIST(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), SHELL_TYPE_OVERFLOW_LIST))
#define SHELL_IS_OVERFLOW_LIST_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_OVERFLOW_LIST))
#define SHELL_OVERFLOW_LIST_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_OVERFLOW_LIST, ShellOverflowListClass))

struct _ShellOverflowList
{
  ClutterGroup parent_instance;

  ShellOverflowListPrivate *priv;
};

struct _ShellOverflowListClass
{
  ClutterGroupClass parent_class;

  ShellOverflowListPrivate *priv;
};

GType shell_overflow_list_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __SHELL_OVERFLOW_LIST_H__ */
