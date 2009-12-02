/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "shell-gconf.h"

#include <gconf/gconf-client.h>
#include <string.h>

/**
 * ShellGConf:
 *
 * A wrapper around #GConfClient that cleans up some of its
 * non-gjs-bindable bits and makes a few gnome-shell-specific
 * assumptions.
 *
 * For all #ShellGConf methods that take a GConf key path as an
 * argument, you can pass either a full path (eg,
 * "/desktop/gnome/shell/sidebar/visible"), or just a relative path
 * starting from the root of the gnome-shell GConf key hierarchy (eg,
 * "sidebar/visible").
 */

struct _ShellGConf
{
  GObject parent;

  GConfClient *client;
};

G_DEFINE_TYPE (ShellGConf, shell_gconf, G_TYPE_OBJECT);

/* Signals */
enum
{
  CHANGED,

  LAST_SIGNAL
};

static guint shell_gconf_signals [LAST_SIGNAL] = { 0 };

static void gconf_value_changed (GConfClient *client, const char *key,
                                 GConfValue *new_value, gpointer user_data);

static void
shell_gconf_init (ShellGConf *gconf)
{
  gconf->client = gconf_client_get_default ();
  gconf_client_add_dir (gconf->client, SHELL_GCONF_DIR,
                        GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
  g_signal_connect (gconf->client, "value_changed",
                    G_CALLBACK (gconf_value_changed), gconf);
}

static void
shell_gconf_finalize (GObject *object)
{
  ShellGConf *gconf = SHELL_GCONF (object);

  g_signal_handlers_disconnect_by_func (gconf->client,
                                        gconf_value_changed, gconf);
  g_object_unref (gconf->client);

  G_OBJECT_CLASS (shell_gconf_parent_class)->finalize (object);
}

static void
shell_gconf_class_init (ShellGConfClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = shell_gconf_finalize;

  /**
   * ShellGConf::changed:
   * @gconf: the #ShellGConf
   *
   * Emitted when a key in a watched directory is changed. The signal
   * detail indicates which key changed. Eg, connect to
   * "changed::sidebar/visible" to be notified when "sidebar/visible"
   * changes. For gnome-shell's own GConf keys, the signal detail will
   * be the relative path from the top of the gnome-shell GConf
   * hierarchy ("/desktop/gnome/shell"). If you want to be notified
   * about the value of a non-gnome-shell key, you must first call
   * shell_gconf_watch_directory(), and then use the full GConf key path
   * as the signal detail.
   */
  shell_gconf_signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (ShellGConfClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

/**
 * shell_gconf_get_default:
 *
 * Gets the default #ShellGConf
 *
 * Return value: (transfer none): the default #ShellGConf
 */
ShellGConf *
shell_gconf_get_default (void)
{
  static ShellGConf *gconf = NULL;

  if (!gconf)
    gconf = g_object_new (SHELL_TYPE_GCONF, NULL);

  return gconf;
}

/**
 * shell_gconf_watch_directory:
 * @gconf: a #ShellGConf
 * @directory: the path of a GConf directory to watch for changes in
 *
 * Adds @directory to the list of directories to watch; you must call
 * this before connecting to #ShellGConf::changed for a key outside of
 * the gnome-shell GConf tree.
 */
void
shell_gconf_watch_directory (ShellGConf *gconf, const char *directory)
{
  gconf_client_add_dir (gconf->client, directory,
                        GCONF_CLIENT_PRELOAD_NONE, NULL);
}

static void
gconf_value_changed (GConfClient *client, const char *key,
                     GConfValue *new_value, gpointer user_data)
{
  ShellGConf *gconf = user_data;
  GQuark detail;

  if (g_str_has_prefix (key, SHELL_GCONF_DIR "/"))
    key += strlen (SHELL_GCONF_DIR "/");

  /* This will create a lot of junk quarks, but it's the best we
   * can do with gjs's current callback support.
   */
  detail = g_quark_from_string (key);
  g_signal_emit (gconf, shell_gconf_signals[CHANGED], detail);
}

static char *
resolve_key (const char *key)
{
  if (*key == '/')
    return g_strdup (key);
  else
    return g_build_filename (SHELL_GCONF_DIR, key, NULL);
}


#define SIMPLE_GETTER(NAME, TYPE, GCONF_GETTER)                 \
TYPE                                                            \
NAME (ShellGConf *gconf, const char *key, GError **error)       \
{                                                               \
  char *get_key = resolve_key (key);                            \
  TYPE value;                                                   \
                                                                \
  value = GCONF_GETTER (gconf->client, get_key, error);         \
  g_free (get_key);                                             \
  return value;                                                 \
}

#define LIST_GETTER(NAME, ELEMENT_TYPE)                         \
GSList *                                                        \
NAME (ShellGConf *gconf, const char *key, GError **error)       \
{                                                               \
  char *get_key = resolve_key (key);                            \
  GSList *value;                                                \
                                                                \
  value = gconf_client_get_list (gconf->client, get_key,        \
                                 ELEMENT_TYPE, error);          \
  g_free (get_key);                                             \
  return value;                                                 \
}

/**
 * shell_gconf_get_boolean:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @error: a #GError, which will be set on error
 *
 * Gets the value of @key, which must be boolean-valued.
 *
 * Return value: @key's value. If an error occurs, @error will be set
 * and the return value is undefined.
 **/
SIMPLE_GETTER(shell_gconf_get_boolean, gboolean, gconf_client_get_bool)

/**
 * shell_gconf_get_int:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @error: a #GError, which will be set on error
 *
 * Gets the value of @key, which must be integer-valued.
 *
 * Return value: @key's value. If an error occurs, @error will be set
 * and the return value is undefined.
 **/
SIMPLE_GETTER(shell_gconf_get_int, int, gconf_client_get_int)

/**
 * shell_gconf_get_float:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @error: a #GError, which will be set on error
 *
 * Gets the value of @key, which must be float-valued.
 *
 * Return value: @key's value. If an error occurs, @error will be set
 * and the return value is undefined.
 **/
SIMPLE_GETTER(shell_gconf_get_float, float, gconf_client_get_float)

/**
 * shell_gconf_get_string:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @error: a #GError, which will be set on error
 *
 * Gets the value of @key, which must be string-valued.
 *
 * Return value: (transfer full): @key's value, or %NULL if an error
 * occurs.
 **/
SIMPLE_GETTER(shell_gconf_get_string, char *, gconf_client_get_string)

/**
 * shell_gconf_get_boolean_list:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @error: a #GError, which will be set on error
 *
 * Gets the value of @key, which must be boolean-list-valued.
 *
 * Return value: (element-type gboolean) (transfer full): @key's
 * value, or %NULL if an error occurs.
 **/
LIST_GETTER(shell_gconf_get_boolean_list, GCONF_VALUE_BOOL)

/**
 * shell_gconf_get_int_list:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @error: a #GError, which will be set on error
 *
 * Gets the value of @key, which must be integer-list-valued.
 *
 * Return value: (element-type int) (transfer full): @key's
 * value, or %NULL if an error occurs.
 **/
LIST_GETTER(shell_gconf_get_int_list, GCONF_VALUE_INT)

/**
 * shell_gconf_get_float_list:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @error: a #GError, which will be set on error
 *
 * Gets the value of @key, which must be float-list-valued.
 *
 * Return value: (element-type float) (transfer full): @key's
 * value, or %NULL if an error occurs.
 **/
LIST_GETTER(shell_gconf_get_float_list, GCONF_VALUE_FLOAT)

/**
 * shell_gconf_get_string_list:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @error: a #GError, which will be set on error
 *
 * Gets the value of @key, which must be string-list-valued.
 *
 * Return value: (element-type utf8) (transfer full): @key's
 * value, or %NULL if an error occurs.
 **/
LIST_GETTER(shell_gconf_get_string_list, GCONF_VALUE_STRING)


#define SIMPLE_SETTER(NAME, TYPE, GCONF_SETTER)                         \
void                                                                    \
NAME (ShellGConf *gconf, const char *key, TYPE value, GError **error)   \
{                                                                       \
  char *set_key = resolve_key (key);                                    \
                                                                        \
  GCONF_SETTER (gconf->client, set_key, value, error);                  \
  g_free (set_key);                                                     \
}

#define LIST_SETTER(NAME, ELEMENT_TYPE)                                 \
void                                                                    \
NAME (ShellGConf *gconf, const char *key,                               \
      GSList *value, GError **error)                                    \
{                                                                       \
  char *set_key = resolve_key (key);                                    \
                                                                        \
  gconf_client_set_list (gconf->client, set_key, ELEMENT_TYPE,          \
                         value, error);                                 \
  g_free (set_key);                                                     \
}

/**
 * shell_gconf_set_boolean:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @value: value to set @key to
 * @error: a #GError, which will be set on error
 *
 * Sets the value of @key to @value.
 **/
SIMPLE_SETTER(shell_gconf_set_boolean, gboolean, gconf_client_set_bool)

/**
 * shell_gconf_set_int:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @value: value to set @key to
 * @error: a #GError, which will be set on error
 *
 * Sets the value of @key to @value.
 **/
SIMPLE_SETTER(shell_gconf_set_int, int, gconf_client_set_int)

/**
 * shell_gconf_set_float:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @value: value to set @key to
 * @error: a #GError, which will be set on error
 *
 * Sets the value of @key to @value.
 **/
SIMPLE_SETTER(shell_gconf_set_float, float, gconf_client_set_float)

/**
 * shell_gconf_set_string:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @value: value to set @key to
 * @error: a #GError, which will be set on error
 *
 * Sets the value of @key to @value.
 **/
SIMPLE_SETTER(shell_gconf_set_string, const char *, gconf_client_set_string)

/**
 * shell_gconf_set_boolean_list:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @value: (transfer none) (element-type gboolean): value to set @key to
 * @error: a #GError, which will be set on error
 *
 * Sets the value of @key to @value.
 **/
LIST_SETTER(shell_gconf_set_boolean_list, GCONF_VALUE_BOOL)

/**
 * shell_gconf_set_int_list:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @value: (transfer none): value to set @key to
 * @error: a #GError, which will be set on error
 *
 * Sets the value of @key to @value.
 **/
LIST_SETTER(shell_gconf_set_int_list, GCONF_VALUE_INT)

/**
 * shell_gconf_set_float_list:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @value: (transfer none) (element-type float): value to set @key to
 * @error: a #GError, which will be set on error
 *
 * Sets the value of @key to @value.
 **/
LIST_SETTER(shell_gconf_set_float_list, GCONF_VALUE_FLOAT)

/**
 * shell_gconf_set_string_list:
 * @gconf: a #ShellGConf
 * @key: a GConf key (as described in the #ShellGConf docs)
 * @value: (transfer none) (element-type utf8): value to set @key to
 * @error: a #GError, which will be set on error
 *
 * Sets the value of @key to @value.
 **/
LIST_SETTER(shell_gconf_set_string_list, GCONF_VALUE_STRING)
