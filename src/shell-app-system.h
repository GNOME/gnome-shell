/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_APP_SYSTEM_H__
#define __SHELL_APP_SYSTEM_H__

#include <gio/gio.h>
#include <clutter/clutter.h>
#include <meta/window.h>

#include "shell-app.h"

#define SHELL_TYPE_APP_SYSTEM                 (shell_app_system_get_type ())
#define SHELL_APP_SYSTEM(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_APP_SYSTEM, ShellAppSystem))
#define SHELL_APP_SYSTEM_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_APP_SYSTEM, ShellAppSystemClass))
#define SHELL_IS_APP_SYSTEM(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_APP_SYSTEM))
#define SHELL_IS_APP_SYSTEM_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_APP_SYSTEM))
#define SHELL_APP_SYSTEM_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_APP_SYSTEM, ShellAppSystemClass))

typedef struct _ShellAppSystem ShellAppSystem;
typedef struct _ShellAppSystemClass ShellAppSystemClass;
typedef struct _ShellAppSystemPrivate ShellAppSystemPrivate;

struct _ShellAppSystem
{
  GObject parent;

  ShellAppSystemPrivate *priv;
};

struct _ShellAppSystemClass
{
  GObjectClass parent_class;

  void (*installed_changed)(ShellAppSystem *appsys, gpointer user_data);
  void (*favorites_changed)(ShellAppSystem *appsys, gpointer user_data);
};

GType           shell_app_system_get_type    (void) G_GNUC_CONST;
ShellAppSystem *shell_app_system_get_default (void);

typedef struct _ShellAppInfo ShellAppInfo;

#define SHELL_TYPE_APP_INFO (shell_app_info_get_type ())
GType           shell_app_info_get_type              (void);

ShellAppInfo   *shell_app_info_ref                   (ShellAppInfo  *info);
void            shell_app_info_unref                 (ShellAppInfo  *info);

const char     *shell_app_info_get_id                (ShellAppInfo  *info);
char           *shell_app_info_get_name              (ShellAppInfo  *info);
char           *shell_app_info_get_description       (ShellAppInfo  *info);
char           *shell_app_info_get_executable        (ShellAppInfo  *info);
char           *shell_app_info_get_desktop_file_path (ShellAppInfo  *info);
GIcon          *shell_app_info_get_icon              (ShellAppInfo  *info);
ClutterActor   *shell_app_info_create_icon_texture   (ShellAppInfo  *info,
                                                      float          size);
char           *shell_app_info_get_section           (ShellAppInfo  *info);
gboolean        shell_app_info_get_is_nodisplay      (ShellAppInfo  *info);
gboolean        shell_app_info_is_transient          (ShellAppInfo  *info);
MetaWindow     *shell_app_info_get_source_window     (ShellAppInfo  *info);

gboolean        shell_app_info_launch                (ShellAppInfo  *info,
                                                      GError       **error);
gboolean        shell_app_info_launch_full           (ShellAppInfo  *info,
                                                      guint          timestamp,
                                                      GList         *uris,
                                                      int            workspace,
                                                      char         **startup_id,
                                                      GError       **error);


GList          *shell_app_system_get_sections              (ShellAppSystem  *system);
GSList         *shell_app_system_get_flattened_apps        (ShellAppSystem  *system);
GSList         *shell_app_system_get_all_settings          (ShellAppSystem  *system);

ShellApp       *shell_app_system_get_app                   (ShellAppSystem  *system,
                                                            const char      *id);
ShellApp       *shell_app_system_get_app_for_path          (ShellAppSystem  *system,
                                                            const char      *desktop_path);
ShellApp       *shell_app_system_get_app_for_window        (ShellAppSystem  *self,
                                                            MetaWindow      *window);
ShellApp       *shell_app_system_lookup_heuristic_basename (ShellAppSystem  *system,
                                                            const char      *id);

ShellAppInfo   *shell_app_system_create_from_window        (ShellAppSystem  *system,
                                                            MetaWindow      *window);

GSList         *shell_app_system_initial_search            (ShellAppSystem  *system,
                                                            gboolean         prefs,
                                                            GSList          *terms);
GSList         *shell_app_system_subsearch                 (ShellAppSystem  *system,
                                                            gboolean         prefs,
                                                            GSList          *previous_results,
                                                            GSList          *terms);

/* internal API */
void _shell_app_system_register_app (ShellAppSystem  *self,
                                     ShellApp        *app);

#endif /* __SHELL_APP_SYSTEM_H__ */
