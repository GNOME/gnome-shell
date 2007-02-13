/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/**
 * SECTION:clutter-main
 * @short_description: Various 'global' clutter functions.
 *
 * Functions to retrieve various global Clutter resources and other utility
 * functions for mainloops, events and threads
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "clutter-main.h"
#include "clutter-feature.h"
#include "clutter-actor.h"
#include "clutter-stage.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-version.h" 	/* For flavour define */

#ifdef CLUTTER_FLAVOUR_GLX
#include <clutter/clutter-backend-glx.h>
#endif

static ClutterMainContext *ClutterCntx = NULL;

static gboolean clutter_is_initialized = FALSE;
static gboolean clutter_show_fps       = FALSE;
static gboolean clutter_fatal_warnings = FALSE;
static gchar *clutter_vblank_name      = NULL;

guint clutter_debug_flags = 0;  /* global clutter debug flag */

#ifdef CLUTTER_ENABLE_DEBUG
static const GDebugKey clutter_debug_keys[] = {
  { "misc", CLUTTER_DEBUG_MISC },
  { "actor", CLUTTER_DEBUG_ACTOR },
  { "texture", CLUTTER_DEBUG_TEXTURE },
  { "event", CLUTTER_DEBUG_EVENT },
  { "paint", CLUTTER_DEBUG_PAINT },
  { "gl", CLUTTER_DEBUG_GL },
  { "alpha", CLUTTER_DEBUG_ALPHA },
  { "behaviour", CLUTTER_DEBUG_BEHAVIOUR },
  { "pango", CLUTTER_DEBUG_PANGO },
};
#endif /* CLUTTER_ENABLE_DEBUG */


gboolean
clutter_want_fps (void)
{
  return clutter_show_fps;
}

const gchar *
clutter_vblank_method (void)
{
  return clutter_vblank_name;
}

/**
 * clutter_redraw:
 *
 * FIXME
 */
void
clutter_redraw (void)
{
  ClutterMainContext *ctx = CLUTTER_CONTEXT();
  ClutterStage       *stage = ctx->stage;

  clutter_actor_paint (CLUTTER_ACTOR(stage));
}

/**
 * clutter_main_quit:
 *
 * Terminates the Clutter mainloop.
 */
void
clutter_main_quit (void)
{
  ClutterMainContext *context = CLUTTER_CONTEXT ();

  g_return_if_fail (context->main_loops != NULL);

  g_main_loop_quit (context->main_loops->data);
}

/**
 * clutter_main_level:
 *
 * Retrieves the depth of the Clutter mainloop.
 *
 * Return value: The level of the mainloop.
 */
gint
clutter_main_level (void)
{
  ClutterMainContext *context = CLUTTER_CONTEXT ();

  return context->main_loop_level;
}

/**
 * clutter_main:
 *
 * Starts the Clutter mainloop.
 */
void
clutter_main (void)
{
  ClutterMainContext *context = CLUTTER_CONTEXT ();
  GMainLoop *loop;

  if (!clutter_is_initialized)
    {
      g_warning ("Called clutter_main() but Clutter wasn't initialised.  "
		 "You must call clutter_init() first.");
      return;
    }

  context->main_loop_level++;

  loop = g_main_loop_new (NULL, TRUE);
  context->main_loops = g_slist_prepend (context->main_loops, loop);

  if (g_main_loop_is_running (context->main_loops->data))
    {
      g_main_loop_run (loop);
    }

  context->main_loops = g_slist_remove (context->main_loops, loop);

  g_main_loop_unref (loop);

  context->main_loop_level--;

  if (context->main_loop_level == 0)
    {
      clutter_actor_destroy (CLUTTER_ACTOR (context->stage));
      g_free (context);
    }
}

/**
 * clutter_threads_enter:
 *
 * Locks the Clutter thread lock.
 */
void
clutter_threads_enter(void)
{
  ClutterMainContext *context = CLUTTER_CONTEXT ();
  
  g_mutex_lock (context->gl_lock);
}

/**
 * clutter_threads_leave:
 *
 * Unlocks the Clutter thread lock.
 */
void
clutter_threads_leave (void)
{
  ClutterMainContext *context = CLUTTER_CONTEXT ();
  
  g_mutex_unlock (context->gl_lock);
}


/**
 * clutter_want_debug:
 * 
 * Check if clutter has debugging turned on.
 *
 * Return value: TRUE if debugging is turned on, FALSE otherwise.
 */
gboolean
clutter_want_debug (void)
{
  return clutter_debug_flags != 0;
}

ClutterMainContext*
clutter_context_get_default (void)
{
  if (!ClutterCntx)
    {
      ClutterMainContext *ctx;

      ctx = g_new0 (ClutterMainContext, 1);
      ctx->is_initialized = FALSE;

      ClutterCntx = ctx;
    }

  return ClutterCntx;
}

static gboolean 
is_gl_version_at_least_12 (void)
{     
#define NON_VENDOR_VERSION_MAX_LEN 32
  gchar        non_vendor_version[NON_VENDOR_VERSION_MAX_LEN];
  const gchar *version;
  gint         i = 0;

  version = (const gchar*) glGetString (GL_VERSION);

  while ( ((version[i] <= '9' && version[i] >= '0') || version[i] == '.') 
	  && i < NON_VENDOR_VERSION_MAX_LEN)
    {
      non_vendor_version[i] = version[i];
      i++;
    }

  non_vendor_version[i] = '\0';

  if (strstr (non_vendor_version, "1.0") == NULL
      && strstr (non_vendor_version, "1.0") == NULL)
    return TRUE;

  return FALSE;
}


#ifdef CLUTTER_ENABLE_DEBUG
static gboolean
clutter_arg_debug_cb (const char *key,
                      const char *value,
                      gpointer    user_data)
{
  clutter_debug_flags |=
    g_parse_debug_string (value,
                          clutter_debug_keys,
                          G_N_ELEMENTS (clutter_debug_keys));
  return TRUE;
}

static gboolean
clutter_arg_no_debug_cb (const char *key,
                         const char *value,
                         gpointer    user_data)
{
  clutter_debug_flags &=
    ~g_parse_debug_string (value,
                           clutter_debug_keys,
                           G_N_ELEMENTS (clutter_debug_keys));
  return TRUE;
}
#endif /* CLUTTER_ENABLE_DEBUG */

static GOptionEntry clutter_args[] = {
  { "clutter-show-fps", 0, 0, G_OPTION_ARG_NONE, &clutter_show_fps,
    "Show frames per second", NULL },
  { "clutter-vblank", 0, 0, G_OPTION_ARG_STRING, &clutter_vblank_name,
    "VBlank method to be used (none, dri or glx)", "METHOD" },
  { "g-fatal-warnings", 0, 0, G_OPTION_ARG_NONE, &clutter_fatal_warnings,
    "Make all warnings fatal", NULL },
#ifdef CLUTTER_ENABLE_DEBUG
  { "clutter-debug", 0, 0, G_OPTION_ARG_CALLBACK, clutter_arg_debug_cb,
    "Clutter debugging flags to set", "FLAGS" },
  { "clutter-no-debug", 0, 0, G_OPTION_ARG_CALLBACK, clutter_arg_no_debug_cb,
    "Clutter debugging flags to unset", "FLAGS" },
#endif /* CLUTTER_ENABLE_DEBUG */
  { NULL, },
};

/* pre_parse_hook: initialise variables depending on environment
 * variables; these variables might be overridden by the command
 * line arguments that are going to be parsed after.
 */
static gboolean
pre_parse_hook (GOptionContext  *context,
                GOptionGroup    *group,
                gpointer         data,
                GError         **error)
{
  const char *env_string;

  if (clutter_is_initialized)
    return TRUE;

  g_type_init ();

#ifdef CLUTTER_ENABLE_DEBUG
  env_string = g_getenv ("CLUTTER_DEBUG");
  if (env_string != NULL)
    {
      clutter_debug_flags =
        g_parse_debug_string (env_string,
                              clutter_debug_keys,
                              G_N_ELEMENTS (clutter_debug_keys));
      env_string = NULL;
    }
#endif /* CLUTTER_ENABLE_DEBUG */

  env_string = g_getenv ("CLUTTER_VBLANK");
  if (env_string)
    {
      clutter_vblank_name = g_strdup (env_string);
      env_string = NULL;
    }

  env_string = g_getenv ("CLUTTER_SHOW_FPS");
  if (env_string)
    clutter_show_fps = TRUE;

  return TRUE;
}

/* post_parse_hook: initialise the context and data structures
 * and opens the X display
 */
static gboolean
post_parse_hook (GOptionContext  *context,
                 GOptionGroup    *group,
                 gpointer         data,
                 GError         **error)
{
  ClutterMainContext *clutter_context;

  if (clutter_is_initialized)
    return TRUE;

  if (clutter_fatal_warnings)
    {
      GLogLevelFlags fatal_mask;

      fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
      fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
      g_log_set_always_fatal (fatal_mask);
    }

  clutter_context = clutter_context_get_default ();
  clutter_context->main_loops = NULL;
  clutter_context->main_loop_level = 0;

  clutter_context->font_map = PANGO_FT2_FONT_MAP (pango_ft2_font_map_new ());
  pango_ft2_font_map_set_resolution (clutter_context->font_map, 96.0, 96.0);

  clutter_context->gl_lock = g_mutex_new ();

  clutter_is_initialized = TRUE;
  
  return TRUE;
}

/**
 * clutter_get_option_group:
 *
 * Returns a #GOptionGroup for the command line arguments recognized
 * by Clutter. You should add this group to your #GOptionContext with
 * g_option_context_add_group(), if you are using g_option_context_parse()
 * to parse your commandline arguments.
 *
 * Return value: a GOptionGroup for the commandline arguments
 *   recognized by Clutter
 *
 * Since: 0.2
 */
GOptionGroup *
clutter_get_option_group (void)
{
  GOptionGroup *group;

  group = g_option_group_new ("clutter",
                              "Clutter Options",
                              "Show Clutter Options",
                              NULL,
                              NULL);
  g_option_group_set_parse_hooks (group, pre_parse_hook, post_parse_hook);
  g_option_group_add_entries (group, clutter_args);

  return group;
}

GQuark
clutter_init_error_quark (void)
{
  return g_quark_from_static_string ("clutter-init-error-quark");
}

static gboolean
clutter_stage_init (ClutterMainContext  *context,
                    GError             **error)
{
  context->stage = CLUTTER_STAGE (clutter_stage_get_default ());
  if (!CLUTTER_IS_STAGE (context->stage))
    {
      g_set_error (error, clutter_init_error_quark (),
                   CLUTTER_INIT_ERROR_INTERNAL,
                   "Unable to create the main stage");
      return FALSE;
    }

  g_object_ref_sink (context->stage);

  /* Realize to get context */
  clutter_actor_realize (CLUTTER_ACTOR (context->stage));
  if (!CLUTTER_ACTOR_IS_REALIZED (CLUTTER_ACTOR (context->stage)))
    {
      g_set_error (error, clutter_init_error_quark (),
                   CLUTTER_INIT_ERROR_INTERNAL,
                   "Unable to realize the main stage");
      return FALSE;
    }

  return TRUE;
}

/**
 * clutter_init_with_args:
 * @argc: a pointer to the number of command line arguments
 * @argv: a pointer to the array of comman line arguments
 * @parameter_string: a string which is displayed in the
 *   first line of <option>--help</option> output, after
 *   <literal><replaceable>programname</replaceable> [OPTION...]</literal>
 * @entries: a %NULL terminated array of #GOptionEntry<!-- -->s
 *   describing the options of your program
 * @translation_domain: a translation domain to use for translating
 *   the <option>--help</option> output for the options in @entries
 *   with gettext(), or %NULL
 * @error: a return location for a #GError
 *
 * This function does the same work as clutter_init(). Additionally,
 * it allows you to add your own command line options, and it
 * automatically generates nicely formatted <option>--help</option>
 * output. Note that your program will be terminated after writing
 * out the help output. Also note that, in case of error, the
 * error message will be placed inside @error instead of being
 * printed on the display.
 *
 * Return value: %CLUTTER_INIT_SUCCESS if Clutter has been successfully
 *   initialised, or other values or #ClutterInitError in case of
 *   error.
 *
 * Since: 0.2
 */
ClutterInitError
clutter_init_with_args (int            *argc,
                        char         ***argv,
                        char           *parameter_string,
                        GOptionEntry   *entries,
                        char           *translation_domain,
                        GError        **error)
{
  ClutterMainContext *clutter_context;
  GOptionContext *context;
  GOptionGroup *group;
  gboolean res;
  GError *stage_error;

  if (clutter_is_initialized)
    return CLUTTER_INIT_SUCCESS;

  if (!g_thread_supported ())
    g_thread_init (NULL);

  group   = clutter_get_option_group ();
  context = g_option_context_new (parameter_string);

  clutter_backend_init (context);

  g_option_context_add_group (context, group);

  if (entries)
    g_option_context_add_main_entries (context, entries, translation_domain);

  res = g_option_context_parse (context, argc, argv, error);
  g_option_context_free (context);

  /* if res is FALSE, the error is filled for
   * us by g_option_context_parse()
   */
  if (!res)
    return CLUTTER_INIT_ERROR_INTERNAL;

  clutter_context = clutter_context_get_default ();

  stage_error = NULL;
  if (!clutter_stage_init (clutter_context, &stage_error))
    {
      g_propagate_error (error, stage_error);
      return CLUTTER_INIT_ERROR_INTERNAL;
    }

  return CLUTTER_INIT_SUCCESS;
}

static gboolean
clutter_parse_args (int    *argc,
                    char ***argv)
{
  GOptionContext *option_context;
  GOptionGroup   *clutter_group;
  GError         *error = NULL;
  gboolean        ret = TRUE;

  if (clutter_is_initialized)
    return TRUE;

  option_context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (option_context, TRUE);
  g_option_context_set_help_enabled (option_context, FALSE); 

  /* Initiate any command line options from the backend */
  clutter_backend_init (option_context);

  clutter_group = clutter_get_option_group ();

  g_option_context_set_main_group (option_context, clutter_group);

  if (!g_option_context_parse (option_context, argc, argv, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      ret = FALSE;
    }

  g_option_context_free (option_context);

  return ret;
}

/**
 * clutter_init:
 * @argc: The number of arguments in @argv
 * @argv: A pointer to an array of arguments.
 *
 * It will initialise everything needed to operate with Clutter and
 * parses some standard command line options. @argc and @argv are
 * adjusted accordingly so your own code will never see those standard
 * arguments.
 *
 * Return value: 1 on success, < 0 on failure.
 */
ClutterInitError
clutter_init (int    *argc,
              char ***argv)
{
  ClutterMainContext *context;
  GError *stage_error;

  if (clutter_is_initialized)
    return CLUTTER_INIT_SUCCESS;

  if (!g_thread_supported ())
    g_thread_init (NULL);

  if (clutter_parse_args (argc, argv) == FALSE)
    return CLUTTER_INIT_ERROR_INTERNAL;

  context = clutter_context_get_default ();

  stage_error = NULL;
  if (!clutter_stage_init (context, &stage_error))
    {
      g_critical (stage_error->message);
      g_error_free (stage_error);
      return CLUTTER_INIT_ERROR_INTERNAL;
    }

#if 0
  /* FIXME: move to backend */
  /* At least GL 1.2 is needed for CLAMP_TO_EDGE */
  if (!is_gl_version_at_least_12 ())
    {
      g_critical ("Clutter needs at least version 1.2 of OpenGL");
      return CLUTTER_INIT_ERROR_OPENGL;
    }
#endif

  return 1;
}
