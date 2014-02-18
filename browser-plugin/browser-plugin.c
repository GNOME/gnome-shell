/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2011 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Jasper St. Pierre <jstpierre@mecheye.net>
 *      Giovanni Campagna <scampa.giovanni@gmail.com>
 */

#include <string.h>

#define XP_UNIX 1

#include "npapi/npapi.h"
#include "npapi/npruntime.h"
#include "npapi/npfunctions.h"

#include <glib.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>

#define ORIGIN "extensions.gnome.org"
#define PLUGIN_NAME "Gnome Shell Integration"
#define PLUGIN_DESCRIPTION "This plugin provides integration with Gnome Shell " \
      "for live extension enabling and disabling. " \
      "It can be used only by extensions.gnome.org"
#define PLUGIN_MIME_STRING "application/x-gnome-shell-integration::Gnome Shell Integration Dummy Content-Type";

#define PLUGIN_API_VERSION 5

#define EXTENSION_DISABLE_VERSION_CHECK_KEY "disable-extension-version-validation"

typedef struct {
  GDBusProxy *proxy;
} PluginData;

static NPNetscapeFuncs funcs;

static inline gchar *
get_string_property (NPP         instance,
                     NPObject   *obj,
                     const char *name)
{
  NPVariant result = { NPVariantType_Void };
  NPString result_str;
  gchar *result_copy;

  result_copy = NULL;

  if (!funcs.getproperty (instance, obj,
                          funcs.getstringidentifier (name),
                          &result))
    goto out;

  if (!NPVARIANT_IS_STRING (result))
    goto out;

  result_str = NPVARIANT_TO_STRING (result);
  result_copy = g_strndup (result_str.UTF8Characters, result_str.UTF8Length);

 out:
  funcs.releasevariantvalue (&result);
  return result_copy;
}

static gboolean
check_origin_and_protocol (NPP instance)
{
  gboolean ret = FALSE;
  NPError error;
  NPObject *window = NULL;
  NPVariant document = { NPVariantType_Void };
  NPVariant location = { NPVariantType_Void };
  gchar *hostname = NULL;
  gchar *protocol = NULL;

  error = funcs.getvalue (instance, NPNVWindowNPObject, &window);
  if (error != NPERR_NO_ERROR)
    goto out;

  if (!funcs.getproperty (instance, window,
                          funcs.getstringidentifier ("document"),
                          &document))
    goto out;

  if (!NPVARIANT_IS_OBJECT (document))
    goto out;

  if (!funcs.getproperty (instance, NPVARIANT_TO_OBJECT (document),
                          funcs.getstringidentifier ("location"),
                          &location))
    goto out;

  if (!NPVARIANT_IS_OBJECT (location))
    goto out;

  hostname = get_string_property (instance,
                                  NPVARIANT_TO_OBJECT (location),
                                  "hostname");

  if (g_strcmp0 (hostname, ORIGIN))
    {
      g_debug ("origin does not match, is %s",
               hostname);

      goto out;
    }

  protocol = get_string_property (instance,
                                  NPVARIANT_TO_OBJECT (location),
                                  "protocol");

  if (g_strcmp0 (protocol, "https:") != 0)
    {
      g_debug ("protocol does not match, is %s",
               protocol);

      goto out;
    }

  ret = TRUE;

 out:
  g_free (protocol);
  g_free (hostname);

  funcs.releasevariantvalue (&location);
  funcs.releasevariantvalue (&document);

  if (window != NULL)
    funcs.releaseobject (window);
  return ret;
}

/* =============== public entry points =================== */

NPError
NP_Initialize(NPNetscapeFuncs *pfuncs, NPPluginFuncs *plugin)
{
  /* global initialization routine, called once when plugin
     is loaded */

  g_debug ("plugin loaded");

  memcpy (&funcs, pfuncs, sizeof (funcs));

  plugin->size = sizeof(NPPluginFuncs);
  plugin->newp = NPP_New;
  plugin->destroy = NPP_Destroy;
  plugin->getvalue = NPP_GetValue;
  plugin->setwindow = NPP_SetWindow;

  return NPERR_NO_ERROR;
}

NPError
NP_Shutdown(void)
{
  return NPERR_NO_ERROR;
}

const char*
NP_GetMIMEDescription(void)
{
  return PLUGIN_MIME_STRING;
}

NPError
NP_GetValue(void         *instance,
            NPPVariable   variable,
            void         *value)
{
  switch (variable) {
  case NPPVpluginNameString:
    *(char**)value = PLUGIN_NAME;
    break;
  case NPPVpluginDescriptionString:
    *(char**)value = PLUGIN_DESCRIPTION;
    break;
  default:
    ;
  }

  return NPERR_NO_ERROR;
}

NPError
NPP_New(NPMIMEType    mimetype,
        NPP           instance,
        uint16_t      mode,
        int16_t       argc,
        char        **argn,
        char        **argv,
        NPSavedData  *saved)
{
  /* instance initialization function */
  PluginData *data;
  GError *error = NULL;

  g_debug ("plugin created");

  if (!check_origin_and_protocol (instance))
    return NPERR_GENERIC_ERROR;

  data = g_slice_new (PluginData);
  instance->pdata = data;

  data->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                               G_DBUS_PROXY_FLAGS_NONE,
                                               NULL, /* interface info */
                                               "org.gnome.Shell",
                                               "/org/gnome/Shell",
                                               "org.gnome.Shell.Extensions",
                                               NULL, /* GCancellable */
                                               &error);
  if (!data->proxy)
    {
      /* ignore error if the shell is not running, otherwise warn */
      if (error->domain != G_DBUS_ERROR ||
          error->code != G_DBUS_ERROR_NAME_HAS_NO_OWNER)
        {
          g_warning ("Failed to set up Shell proxy: %s", error->message);
        }
      g_clear_error (&error);
      return NPERR_GENERIC_ERROR;
    }

  g_debug ("plugin created successfully");

  return NPERR_NO_ERROR;
}

NPError
NPP_Destroy(NPP           instance,
	    NPSavedData **saved)
{
  /* instance finalization function */

  PluginData *data = instance->pdata;

  g_debug ("plugin destroyed");

  g_object_unref (data->proxy);

  g_slice_free (PluginData, data);

  return NPERR_NO_ERROR;
}

/* =================== scripting interface =================== */

typedef struct {
  NPObject     parent;
  NPP          instance;
  GDBusProxy  *proxy;
  GSettings   *settings;
  NPObject    *listener;
  NPObject    *restart_listener;
  gint         signal_id;
  guint        watch_name_id;
} PluginObject;

static void
on_shell_signal (GDBusProxy *proxy,
                 gchar      *sender_name,
                 gchar      *signal_name,
                 GVariant   *parameters,
                 gpointer    user_data)
{
  PluginObject *obj = user_data;

  if (strcmp (signal_name, "ExtensionStatusChanged") == 0)
    {
      gchar *uuid;
      gint32 status;
      gchar *error;
      NPVariant args[3];
      NPVariant result = { NPVariantType_Void };

      g_variant_get (parameters, "(sis)", &uuid, &status, &error);
      STRINGZ_TO_NPVARIANT (uuid, args[0]);
      INT32_TO_NPVARIANT (status, args[1]);
      STRINGZ_TO_NPVARIANT (error, args[2]);

      funcs.invokeDefault (obj->instance, obj->listener,
			   args, 3, &result);

      funcs.releasevariantvalue (&result);
      g_free (uuid);
      g_free (error);
    }
}

static void
on_shell_appeared (GDBusConnection *connection,
                   const gchar     *name,
                   const gchar     *name_owner,
                   gpointer         user_data)
{
  PluginObject *obj = (PluginObject*) user_data;

  if (obj->restart_listener)
    {
      NPVariant result = { NPVariantType_Void };

      funcs.invokeDefault (obj->instance, obj->restart_listener,
                           NULL, 0, &result);

      funcs.releasevariantvalue (&result);
    }
}

#define SHELL_SCHEMA "org.gnome.shell"
#define ENABLED_EXTENSIONS_KEY "enabled-extensions"

static NPObject *
plugin_object_allocate (NPP      instance,
                        NPClass *klass)
{
  PluginData *data = instance->pdata;
  PluginObject *obj = g_slice_new0 (PluginObject);

  obj->instance = instance;
  obj->proxy = g_object_ref (data->proxy);
  obj->settings = g_settings_new (SHELL_SCHEMA);
  obj->signal_id = g_signal_connect (obj->proxy, "g-signal",
                                     G_CALLBACK (on_shell_signal), obj);

  obj->watch_name_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                         "org.gnome.Shell",
                                         G_BUS_NAME_WATCHER_FLAGS_NONE,
                                         on_shell_appeared,
                                         NULL,
                                         obj,
                                         NULL);

  g_debug ("plugin object created");

  return (NPObject*)obj;
}

static void
plugin_object_deallocate (NPObject *npobj)
{
  PluginObject *obj = (PluginObject*)npobj;

  g_signal_handler_disconnect (obj->proxy, obj->signal_id);
  g_object_unref (obj->proxy);

  if (obj->listener)
    funcs.releaseobject (obj->listener);

  if (obj->watch_name_id)
    g_bus_unwatch_name (obj->watch_name_id);

  g_debug ("plugin object destroyed");

  g_slice_free (PluginObject, obj);
}

static inline gboolean
uuid_is_valid (NPString string)
{
  gsize i;

  for (i = 0; i < string.UTF8Length; i++)
    {
      gchar c = string.UTF8Characters[i];
      if (c < 32 || c >= 127)
        return FALSE;

      switch (c)
        {
        case '&':
        case '<':
        case '>':
        case '/':
        case '\\':
          return FALSE;
        default:
          break;
        }
    }
  return TRUE;
}

static gboolean
jsonify_variant (GVariant  *variant,
                 NPVariant *result)
{
  gboolean ret;
  GVariant *real_value;
  JsonNode *root;
  JsonGenerator *generator;
  gsize json_length;
  gchar *json;
  gchar *buffer;

  ret = TRUE;

  /* DBus methods can return multiple values,
   * but we're only interested in the first. */
  g_variant_get (variant, "(@*)", &real_value);

  root = json_gvariant_serialize (real_value);

  generator = json_generator_new ();
  json_generator_set_root (generator, root);
  json = json_generator_to_data (generator, &json_length);

  buffer = funcs.memalloc (json_length + 1);
  if (!buffer)
    {
      ret = FALSE;
      goto out;
    }

  strcpy (buffer, json);

  STRINGN_TO_NPVARIANT (buffer, json_length, *result);

 out:
  g_variant_unref (variant);
  g_variant_unref (real_value);
  json_node_free (root);
  g_free (json);

  return ret;
}

static gboolean
parse_args (const gchar     *format_str,
            uint32_t         argc,
            const NPVariant *argv,
            ...)
{
  va_list args;
  gsize i;
  gboolean ret = FALSE;

  if (strlen (format_str) != argc)
    return FALSE;

  va_start (args, argv);

  for (i = 0; format_str[i]; i++)
    {
      gpointer arg_location;
      const NPVariant arg = argv[i];

      arg_location = va_arg (args, gpointer);

      switch (format_str[i])
        {
        case 'u':
          {
            NPString string;

            if (!NPVARIANT_IS_STRING (arg))
              goto out;

            string = NPVARIANT_TO_STRING (arg);

            if (!uuid_is_valid (string))
              goto out;

            *(gchar **) arg_location = g_strndup (string.UTF8Characters, string.UTF8Length);
          }
          break;

        case 'b':
          if (!NPVARIANT_IS_BOOLEAN (arg))
            goto out;

          *(gboolean *) arg_location = NPVARIANT_TO_BOOLEAN (arg);
          break;

        case 'o':
          if (!NPVARIANT_IS_OBJECT (arg))
            goto out;

          *(NPObject **) arg_location = NPVARIANT_TO_OBJECT (arg);
        }
    }

  ret = TRUE;

 out:
  va_end (args);

  return ret;
}

static gboolean
plugin_list_extensions (PluginObject    *obj,
                        uint32_t         argc,
                        const NPVariant *args,
                        NPVariant       *result)
{
  GError *error = NULL;
  GVariant *res;

  res = g_dbus_proxy_call_sync (obj->proxy,
                                "ListExtensions",
                                NULL, /* parameters */
                                G_DBUS_CALL_FLAGS_NONE,
                                -1, /* timeout */
                                NULL, /* cancellable */
                                &error);

  if (!res)
    {
      g_warning ("Failed to retrieve extension list: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  return jsonify_variant (res, result);
}

static gboolean
plugin_enable_extension (PluginObject    *obj,
                         uint32_t         argc,
                         const NPVariant *argv,
                         NPVariant       *result)
{
  gboolean ret;
  gchar *uuid;
  gboolean enabled;
  gsize length;
  gchar **uuids;
  const gchar **new_uuids;

  if (!parse_args ("ub", argc, argv, &uuid, &enabled))
    return FALSE;

  uuids = g_settings_get_strv (obj->settings, ENABLED_EXTENSIONS_KEY);
  length = g_strv_length (uuids);

  if (enabled)
    {
      new_uuids = g_new (const gchar *, length + 2); /* New key, NULL */
      memcpy (new_uuids, uuids, length * sizeof (*new_uuids));
      new_uuids[length] = uuid;
      new_uuids[length + 1] = NULL;
    }
  else
    {
      gsize i = 0, j = 0;
      new_uuids = g_new (const gchar *, length);
      for (i = 0; i < length; i ++)
        {
          if (g_str_equal (uuids[i], uuid))
            continue;

          new_uuids[j] = uuids[i];
          j++;
        }

      new_uuids[j] = NULL;
    }

  ret = g_settings_set_strv (obj->settings,
                             ENABLED_EXTENSIONS_KEY,
                             new_uuids);

  g_strfreev (uuids);
  g_free (new_uuids);
  g_free (uuid);

  return ret;
}

typedef struct _AsyncClosure AsyncClosure;

struct _AsyncClosure {
  PluginObject *obj;
  NPObject *callback;
  NPObject *errback;
};

static void
install_extension_cb (GObject      *proxy,
                      GAsyncResult *async_res,
                      gpointer      user_data)
{
  AsyncClosure *async_closure = (AsyncClosure *) user_data;
  GError *error = NULL;
  GVariant *res;
  NPVariant args[1];
  NPVariant result = { NPVariantType_Void };
  NPObject *callback;

  res = g_dbus_proxy_call_finish (G_DBUS_PROXY (proxy), async_res, &error);

  if (res == NULL)
    {
      if (g_dbus_error_is_remote_error (error))
        g_dbus_error_strip_remote_error (error);
      STRINGZ_TO_NPVARIANT (error->message, args[0]);
      callback = async_closure->errback;
    }
  else
    {
      char *string_result;
      g_variant_get (res, "(&s)", &string_result);
      STRINGZ_TO_NPVARIANT (string_result, args[0]);
      callback = async_closure->callback;
    }

  funcs.invokeDefault (async_closure->obj->instance,
                       callback, args, 1, &result);

  funcs.releasevariantvalue (&result);

  funcs.releaseobject (async_closure->callback);
  funcs.releaseobject (async_closure->errback);
  g_slice_free (AsyncClosure, async_closure);
}

static gboolean
plugin_install_extension (PluginObject    *obj,
                          uint32_t         argc,
                          const NPVariant *argv,
                          NPVariant       *result)
{
  gchar *uuid;
  NPObject *callback, *errback;
  AsyncClosure *async_closure;

  if (!parse_args ("uoo", argc, argv, &uuid, &callback, &errback))
    return FALSE;

  async_closure = g_slice_new (AsyncClosure);
  async_closure->obj = obj;
  async_closure->callback = funcs.retainobject (callback);
  async_closure->errback = funcs.retainobject (errback);

  g_dbus_proxy_call (obj->proxy,
                     "InstallRemoteExtension",
                     g_variant_new ("(s)", uuid),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, /* timeout */
                     NULL, /* cancellable */
                     install_extension_cb,
                     async_closure);

  g_free (uuid);

  return TRUE;
}

static gboolean
plugin_uninstall_extension (PluginObject    *obj,
                            uint32_t         argc,
                            const NPVariant *argv,
                            NPVariant       *result)
{
  GError *error = NULL;
  GVariant *res;
  gchar *uuid;

  if (!parse_args ("u", argc, argv, &uuid))
    return FALSE;

  res = g_dbus_proxy_call_sync (obj->proxy,
                                "UninstallExtension",
                                g_variant_new ("(s)", uuid),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1, /* timeout */
                                NULL, /* cancellable */
                                &error);

  g_free (uuid);

  if (!res)
    {
      g_warning ("Failed to uninstall extension: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  return jsonify_variant (res, result);
}

static gboolean
plugin_get_info (PluginObject    *obj,
                 uint32_t         argc,
                 const NPVariant *argv,
                 NPVariant       *result)
{
  GError *error = NULL;
  GVariant *res;
  gchar *uuid;

  if (!parse_args ("u", argc, argv, &uuid))
    return FALSE;

  res = g_dbus_proxy_call_sync (obj->proxy,
                                "GetExtensionInfo",
                                g_variant_new ("(s)", uuid),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1, /* timeout */
                                NULL, /* cancellable */
                                &error);

  g_free (uuid);

  if (!res)
    {
      g_warning ("Failed to retrieve extension metadata: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  return jsonify_variant (res, result);
}

static gboolean
plugin_get_errors (PluginObject    *obj,
                   uint32_t         argc,
                   const NPVariant *argv,
                   NPVariant       *result)
{
  GError *error = NULL;
  GVariant *res;
  gchar *uuid;

  if (!parse_args ("u", argc, argv, &uuid))
    return FALSE;

  res = g_dbus_proxy_call_sync (obj->proxy,
                                "GetExtensionErrors",
                                g_variant_new ("(s)", uuid),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1, /* timeout */
                                NULL, /* cancellable */
                                &error);

  if (!res)
    {
      g_warning ("Failed to retrieve errors: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  return jsonify_variant (res, result);
}

static gboolean
plugin_launch_extension_prefs (PluginObject    *obj,
                               uint32_t         argc,
                               const NPVariant *argv,
                               NPVariant       *result)
{
  gchar *uuid;

  if (!parse_args ("u", argc, argv, &uuid))
    return FALSE;

  g_dbus_proxy_call (obj->proxy,
                     "LaunchExtensionPrefs",
                     g_variant_new ("(s)", uuid),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, /* timeout */
                     NULL, /* cancellable */
                     NULL, /* callback */
                     NULL /* user_data */);

  return TRUE;
}

static int
plugin_get_api_version (PluginObject  *obj,
                        NPVariant     *result)
{
  INT32_TO_NPVARIANT (PLUGIN_API_VERSION, *result);
  return TRUE;
}

static gboolean
plugin_get_shell_version (PluginObject  *obj,
                          NPVariant     *result)
{
  GVariant *res;
  const gchar *version;
  gsize length;
  gchar *buffer;
  gboolean ret;

  ret = TRUE;

  res = g_dbus_proxy_get_cached_property (obj->proxy,
                                          "ShellVersion");

  if (res == NULL)
    {
      g_warning ("Failed to grab shell version.");
      version = "-1";
    }
  else
    {
      g_variant_get (res, "&s", &version);
    }

  length = strlen (version);
  buffer = funcs.memalloc (length + 1);
  if (!buffer)
    {
      ret = FALSE;
      goto out;
    }
  strcpy (buffer, version);

  STRINGN_TO_NPVARIANT (buffer, length, *result);

 out:
  if (res)
    g_variant_unref (res);
  return ret;
}

static gboolean
plugin_get_version_validation_enabled (PluginObject  *obj,
                                       NPVariant     *result)
{
  gboolean is_enabled = !g_settings_get_boolean (obj->settings, EXTENSION_DISABLE_VERSION_CHECK_KEY);
  BOOLEAN_TO_NPVARIANT(is_enabled, *result);

  return TRUE;
}

#define METHODS                                 \
  METHOD (list_extensions)                      \
  METHOD (get_info)                             \
  METHOD (enable_extension)                     \
  METHOD (install_extension)                    \
  METHOD (uninstall_extension)                  \
  METHOD (get_errors)                           \
  METHOD (launch_extension_prefs)               \
  /* */

#define METHOD(x)                               \
  static NPIdentifier x##_id;
METHODS
#undef METHOD

static NPIdentifier api_version_id;
static NPIdentifier shell_version_id;
static NPIdentifier onextension_changed_id;
static NPIdentifier onrestart_id;
static NPIdentifier version_validation_enabled_id;


static bool
plugin_object_has_method (NPObject     *npobj,
                          NPIdentifier  name)
{
#define METHOD(x) (name == (x##_id)) ||
  /* expands to (name == list_extensions_id) || FALSE; */
  return METHODS FALSE;
#undef METHOD
}

static bool
plugin_object_invoke (NPObject        *npobj,
                      NPIdentifier     name,
                      const NPVariant *argv,
                      uint32_t         argc,
                      NPVariant       *result)
{
  PluginObject *obj;

  g_debug ("invoking plugin object method");

  obj = (PluginObject*) npobj;

  VOID_TO_NPVARIANT (*result);

#define METHOD(x)                                       \
  if (name == x##_id)                                   \
    return plugin_##x (obj, argc, argv, result);
METHODS
#undef METHOD

  return FALSE;
}

static bool
plugin_object_has_property (NPObject     *npobj,
                            NPIdentifier  name)
{
  return (name == onextension_changed_id ||
          name == onrestart_id ||
          name == api_version_id ||
          name == shell_version_id ||
          name == version_validation_enabled_id);
}

static bool
plugin_object_get_property (NPObject     *npobj,
                            NPIdentifier  name,
                            NPVariant    *result)
{
  PluginObject *obj;

  if (!plugin_object_has_property (npobj, name))
    return FALSE;

  obj = (PluginObject*) npobj;
  if (name == api_version_id)
    return plugin_get_api_version (obj, result);
  else if (name == shell_version_id)
    return plugin_get_shell_version (obj, result);
  else if (name == version_validation_enabled_id)
    return plugin_get_version_validation_enabled (obj, result);
  else if (name == onextension_changed_id)
    {
      if (obj->listener)
        OBJECT_TO_NPVARIANT (obj->listener, *result);
      else
        NULL_TO_NPVARIANT (*result);
    }
  else if (name == onrestart_id)
    {
      if (obj->restart_listener)
        OBJECT_TO_NPVARIANT (obj->restart_listener, *result);
      else
        NULL_TO_NPVARIANT (*result);
    }

  return TRUE;
}

static bool
plugin_object_set_callback (NPObject        **listener,
                            const NPVariant  *value)
{
  if (!NPVARIANT_IS_OBJECT (*value) && !NPVARIANT_IS_NULL (*value))
    return FALSE;

  if (*listener)
    funcs.releaseobject (*listener);
  *listener = NULL;

  if (NPVARIANT_IS_OBJECT (*value))
    {
      *listener = NPVARIANT_TO_OBJECT (*value);
      funcs.retainobject (*listener);
    }

  return TRUE;
}

static bool
plugin_object_set_property (NPObject        *npobj,
                            NPIdentifier     name,
                            const NPVariant *value)
{
  PluginObject *obj;

  obj = (PluginObject *)npobj;

  if (name == onextension_changed_id)
    return plugin_object_set_callback (&obj->listener, value);

  if (name == onrestart_id)
    return plugin_object_set_callback (&obj->restart_listener, value);

  return FALSE;
}

static NPClass plugin_class = {
  NP_CLASS_STRUCT_VERSION,
  plugin_object_allocate,
  plugin_object_deallocate,
  NULL, /* invalidate */
  plugin_object_has_method,
  plugin_object_invoke,
  NULL, /* invoke default */
  plugin_object_has_property,
  plugin_object_get_property,
  plugin_object_set_property,
  NULL, /* remove property */
  NULL, /* enumerate */
  NULL, /* construct */
};

static void
init_methods_and_properties (void)
{
  /* this is the JS public API; it is manipulated through NPIdentifiers for speed */
  api_version_id = funcs.getstringidentifier ("apiVersion");
  shell_version_id = funcs.getstringidentifier ("shellVersion");
  version_validation_enabled_id = funcs.getstringidentifier ("versionValidationEnabled");

  get_info_id = funcs.getstringidentifier ("getExtensionInfo");
  list_extensions_id = funcs.getstringidentifier ("listExtensions");
  enable_extension_id = funcs.getstringidentifier ("setExtensionEnabled");
  install_extension_id = funcs.getstringidentifier ("installExtension");
  uninstall_extension_id = funcs.getstringidentifier ("uninstallExtension");
  get_errors_id = funcs.getstringidentifier ("getExtensionErrors");
  launch_extension_prefs_id = funcs.getstringidentifier ("launchExtensionPrefs");

  onrestart_id = funcs.getstringidentifier ("onshellrestart");
  onextension_changed_id = funcs.getstringidentifier ("onchange");
}

NPError
NPP_GetValue(NPP          instance,
	     NPPVariable  variable,
	     void        *value)
{
  g_debug ("NPP_GetValue called");

  switch (variable) {
  case NPPVpluginScriptableNPObject:
    g_debug ("creating scriptable object");
    init_methods_and_properties ();

    *(NPObject**)value = funcs.createobject (instance, &plugin_class);
    break;

  case NPPVpluginNeedsXEmbed:
    *(bool *)value = TRUE;
    break;

  default:
    ;
  }

  return NPERR_NO_ERROR;
}

/* Opera tries to call NPP_SetWindow without checking the
 * NULL pointer beforehand. */
NPError
NPP_SetWindow(NPP          instance,
              NPWindow    *window)
{
  return NPERR_NO_ERROR;
}
