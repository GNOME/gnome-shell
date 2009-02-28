#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>

#include "cogl-debug.h"

#ifdef COGL_ENABLE_DEBUG
static const GDebugKey cogl_debug_keys[] = {
  { "misc", COGL_DEBUG_MISC },
  { "texture", COGL_DEBUG_TEXTURE },
  { "material", COGL_DEBUG_MATERIAL },
  { "shader", COGL_DEBUG_SHADER },
  { "offscreen", COGL_DEBUG_OFFSCREEN },
  { "draw", COGL_DEBUG_DRAW },
  { "pango", COGL_DEBUG_PANGO }
};

static const gint n_cogl_debug_keys = G_N_ELEMENTS (cogl_debug_keys);
#endif /* COGL_ENABLE_DEBUG */

guint cogl_debug_flags = 0;

#ifdef COGL_ENABLE_DEBUG
static gboolean
cogl_arg_debug_cb (const char *key,
                   const char *value,
                   gpointer    user_data)
{
  cogl_debug_flags |=
    g_parse_debug_string (value,
                          cogl_debug_keys,
                          n_cogl_debug_keys);
  return TRUE;
}

static gboolean
cogl_arg_no_debug_cb (const char *key,
                         const char *value,
                         gpointer    user_data)
{
  cogl_debug_flags &=
    ~g_parse_debug_string (value,
                           cogl_debug_keys,
                           n_cogl_debug_keys);
  return TRUE;
}
#endif /* CLUTTER_ENABLE_DEBUG */

static GOptionEntry cogl_args[] = {
#ifdef COGL_ENABLE_DEBUG
  { "cogl-debug", 0, 0, G_OPTION_ARG_CALLBACK, cogl_arg_debug_cb,
    N_("COGL debugging flags to set"), "FLAGS" },
  { "cogl-no-debug", 0, 0, G_OPTION_ARG_CALLBACK, cogl_arg_no_debug_cb,
    N_("COGL debugging flags to unset"), "FLAGS" },
#endif /* COGL_ENABLE_DEBUG */
  { NULL, },
};

static gboolean
pre_parse_hook (GOptionContext  *context,
                GOptionGroup    *group,
                gpointer         data,
                GError         **error)
{
  const char *env_string;

#ifdef COGL_ENABLE_DEBUG
  env_string = g_getenv ("COGL_DEBUG");
  if (env_string != NULL)
    {
      cogl_debug_flags =
        g_parse_debug_string (env_string,
                              cogl_debug_keys,
                              n_cogl_debug_keys);
      env_string = NULL;
    }
#endif /* COGL_ENABLE_DEBUG */

  return TRUE;
}

GOptionGroup *
cogl_get_option_group (void)
{
  GOptionGroup *group;

  group = g_option_group_new ("cogl",
                              _("COGL Options"),
                              _("Show COGL options"),
                              NULL, NULL);

  g_option_group_set_parse_hooks (group, pre_parse_hook, NULL);
  g_option_group_add_entries (group, cogl_args);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);

  return group;
}
