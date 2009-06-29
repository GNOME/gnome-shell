/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef SHELL_GCONF_H
#define SHELL_GCONF_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ShellGConf      ShellGConf;
typedef struct _ShellGConfClass ShellGConfClass;

#define SHELL_TYPE_GCONF              (shell_gconf_get_type ())
#define SHELL_GCONF(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), SHELL_TYPE_GCONF, ShellGConf))
#define SHELL_GCONF_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_GCONF, ShellGConfClass))
#define SHELL_IS_GCONF(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), SHELL_TYPE_GCONF))
#define SHELL_IS_GCONF_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_GCONF))
#define SHELL_GCONF_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_GCONF, ShellGConfClass))

struct _ShellGConfClass
{
  GObjectClass parent_class;

  /* signals */
  void (*changed) (ShellGConf *gconf);
};

#define SHELL_GCONF_DIR "/desktop/gnome/shell"

GType       shell_gconf_get_type        (void) G_GNUC_CONST;
ShellGConf *shell_gconf_get_default     (void);

void        shell_gconf_watch_directory (ShellGConf  *gconf,
                                         const char  *directory);

gboolean    shell_gconf_get_boolean      (ShellGConf  *gconf,
                                          const char  *key,
                                          GError     **error);
int         shell_gconf_get_int          (ShellGConf  *gconf,
                                          const char  *key,
                                          GError     **error);
float       shell_gconf_get_float        (ShellGConf  *gconf,
                                          const char  *key,
                                          GError     **error);
char       *shell_gconf_get_string       (ShellGConf  *gconf,
                                          const char  *key,
                                          GError     **error);
GSList     *shell_gconf_get_boolean_list (ShellGConf  *gconf,
                                          const char  *key,
                                          GError     **error);
GSList     *shell_gconf_get_int_list     (ShellGConf  *gconf,
                                          const char  *key,
                                          GError     **error);
GSList     *shell_gconf_get_float_list   (ShellGConf  *gconf,
                                          const char  *key,
                                          GError     **error);
GSList     *shell_gconf_get_string_list  (ShellGConf  *gconf,
                                          const char  *key,
                                          GError     **error);

void        shell_gconf_set_boolean      (ShellGConf  *gconf,
                                          const char  *key,
                                          gboolean     value,
                                          GError     **error);
void        shell_gconf_set_int          (ShellGConf  *gconf,
                                          const char  *key,
                                          int          value,
                                          GError     **error);
void        shell_gconf_set_float        (ShellGConf  *gconf,
                                          const char  *key,
                                          float        value,
                                          GError     **error);
void        shell_gconf_set_string       (ShellGConf  *gconf,
                                          const char  *key,
                                          const char  *value,
                                          GError     **error);
void        shell_gconf_set_boolean_list (ShellGConf  *gconf,
                                          const char  *key,
                                          GSList      *value,
                                          GError     **error);
void        shell_gconf_set_int_list     (ShellGConf  *gconf,
                                          const char  *key,
                                          GSList      *value,
                                          GError     **error);
void        shell_gconf_set_float_list   (ShellGConf  *gconf,
                                          const char  *key,
                                          GSList      *value,
                                          GError     **error);
void        shell_gconf_set_string_list  (ShellGConf  *gconf,
                                          const char  *key,
                                          GSList      *value,
                                          GError     **error);

#endif

