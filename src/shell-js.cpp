/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "shell-js.h"

#include <gio/gio.h>
#include <gjs/gjs.h>
#include <gjs/gjs-module.h>

/**
 * shell_js_add_extension_importer:
 * @target_object_script: JavaScript code evaluating to a target object
 * @target_property: Name of property to use for importer
 * @directory: Source directory:
 * @error: A #GError
 *
 * This function sets a property named @target_property on the object
 * resulting from the evaluation of @target_object_script code, which
 * acts as a GJS importer for directory @directory.
 *
 * Returns: %TRUE on success
 */
gboolean
shell_js_add_extension_importer (const char  *target_object_script,
                                 const char  *target_property,
                                 const char  *directory,
                                 GError     **error)
{
  jsval target_object;
  GList *contexts;
  JSContext *context;
  char *search_path[2] = { 0, 0 };
  gboolean ret = FALSE;

  /* Take the first GjsContext from all of them --
   * we should only ever have one context, so this
   * should be alright. */
  contexts = gjs_context_get_all ();
  context = (JSContext*) gjs_context_get_native_context ((GjsContext*)contexts->data);
  g_list_free_full (contexts, g_object_unref);

  JS_BeginRequest (context);

  /* This is a bit of a hack; ideally we'd be able to pass our target
   * object directly into this function, but introspection doesn't
   * support that at the moment.  Instead evaluate a string to get it. */
  if (!JS_EvaluateScript(context,
                         JS_GetGlobalObject(context),
                         target_object_script,
                         strlen (target_object_script),
                         "<target_object_script>",
                         0,
                         &target_object))
    {
      gjs_log_exception(context);
      g_set_error(error,
                  G_IO_ERROR,
                  G_IO_ERROR_FAILED,
                  "Unable to import %s", target_object_script);
      goto out;
    }

  if (!JSVAL_IS_OBJECT (target_object))
    {
      g_error ("shell_js_add_extension_importer: invalid target object");
      goto out;
    }

  search_path[0] = (char*)directory;
  gjs_define_importer (context, JSVAL_TO_OBJECT (target_object), target_property, (const char **)search_path, FALSE);
  ret = TRUE;

 out:
  JS_EndRequest (context);
  return ret;
}
