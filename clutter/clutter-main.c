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

#include "clutter-event.h"
#include "clutter-backend.h"
#include "clutter-main.h"
#include "clutter-feature.h"
#include "clutter-actor.h"
#include "clutter-stage.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-version.h" 	/* For flavour define */

static ClutterMainContext *ClutterCntx = NULL;

static gboolean clutter_is_initialized = FALSE;
static gboolean clutter_show_fps       = FALSE;
static gboolean clutter_fatal_warnings = FALSE;

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
  { "backend", CLUTTER_DEBUG_BACKEND },
};
#endif /* CLUTTER_ENABLE_DEBUG */

/**
 * clutter_get_show_fps:
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.4
 */
gboolean
clutter_get_show_fps (void)
{
  return clutter_show_fps;
}


/**
 * clutter_redraw:
 *
 * FIXME
 */
void
clutter_redraw (void)
{
  ClutterMainContext *ctx;
  
  ctx = clutter_context_get_default ();
  if (ctx->backend)
    clutter_actor_paint (_clutter_backend_get_stage (ctx->backend));
}

/** 
 * clutter_do_event
 *
 * This function should never be called by applications.
 */
void
clutter_do_event (ClutterEvent *event)
{
  ClutterMainContext *context;
  ClutterBackend *backend;
  ClutterActor *stage;

  context = clutter_context_get_default ();
  backend = context->backend;
  stage = _clutter_backend_get_stage (backend);
  if (!stage)
    return;

  switch (event->type)
    {
    case CLUTTER_NOTHING:
      break;

    case CLUTTER_DESTROY_NOTIFY:
    case CLUTTER_DELETE:
      if (clutter_stage_event (CLUTTER_STAGE (stage), event))
        clutter_main_quit ();
      break;
    
    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
    case CLUTTER_MOTION:
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_2BUTTON_PRESS:
    case CLUTTER_3BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
    case CLUTTER_SCROLL:
      clutter_stage_event (CLUTTER_STAGE (stage), event);
      break;

    case CLUTTER_STAGE_STATE:
    case CLUTTER_CLIENT_MESSAGE:
      break;
    }
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

  if (g_main_loop_is_running (context->main_loops->data))
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
      /* FIXME - add thread locking around this call */
      g_main_loop_run (loop);
    }

  context->main_loops = g_slist_remove (context->main_loops, loop);

  g_main_loop_unref (loop);

  context->main_loop_level--;

  if (context->main_loop_level == 0)
    {
      /* this will take care of destroying the stage */
      g_object_unref (context->backend);
      context->backend = NULL;

      g_free (context);
    }
}

/**
 * clutter_threads_enter:
 *
 * Locks the Clutter thread lock.
 */
void
clutter_threads_enter (void)
{
  ClutterMainContext *context = CLUTTER_CONTEXT ();
  
  if (context->gl_lock)
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
  
  if (context->gl_lock)
    g_mutex_unlock (context->gl_lock);
}


/**
 * clutter_get_debug_enabled:
 * 
 * Check if clutter has debugging turned on.
 *
 * Return value: TRUE if debugging is turned on, FALSE otherwise.
 */
gboolean
clutter_get_debug_enabled (void)
{
#ifdef CLUTTER_ENABLE_DEBUG
  return clutter_debug_flags != 0;
#else
  return FALSE;
#endif
}

ClutterMainContext*
clutter_context_get_default (void)
{
  if (!ClutterCntx)
    {
      ClutterMainContext *ctx;

      ctx = g_new0 (ClutterMainContext, 1);
      ctx->backend = g_object_new (_clutter_backend_impl_get_type (), NULL);

      ctx->is_initialized = FALSE;

      ClutterCntx = ctx;
    }

  return ClutterCntx;
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
  ClutterMainContext *clutter_context;
  ClutterBackend *backend;
  const char *env_string;

  if (clutter_is_initialized)
    return TRUE;

  clutter_context = clutter_context_get_default ();
  clutter_context->main_loops = NULL;
  clutter_context->main_loop_level = 0;

  clutter_context->font_map = PANGO_FT2_FONT_MAP (pango_ft2_font_map_new ());
  pango_ft2_font_map_set_resolution (clutter_context->font_map, 96.0, 96.0);

  backend = clutter_context->backend;
  g_assert (CLUTTER_IS_BACKEND (backend));

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


  env_string = g_getenv ("CLUTTER_SHOW_FPS");
  if (env_string)
    clutter_show_fps = TRUE;

  if (CLUTTER_BACKEND_GET_CLASS (backend)->pre_parse)
    return CLUTTER_BACKEND_GET_CLASS (backend)->pre_parse (backend, error);

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
  ClutterBackend *backend;
  gboolean retval = FALSE;

  if (clutter_is_initialized)
    return TRUE;

  clutter_context = clutter_context_get_default ();
  backend = clutter_context->backend;
  g_assert (CLUTTER_IS_BACKEND (backend));

  if (clutter_fatal_warnings)
    {
      GLogLevelFlags fatal_mask;

      fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
      fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
      g_log_set_always_fatal (fatal_mask);
    }

  if (CLUTTER_BACKEND_GET_CLASS (backend)->post_parse)
    retval = CLUTTER_BACKEND_GET_CLASS (backend)->post_parse (backend, error);
  else
    retval = TRUE;

  clutter_is_initialized = retval;
  
  return retval;
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
  ClutterMainContext *context;
  GOptionGroup *group;

  context = clutter_context_get_default ();

  group = g_option_group_new ("clutter",
                              "Clutter Options",
                              "Show Clutter Options",
                              NULL,
                              NULL);
  
  g_option_group_set_parse_hooks (group, pre_parse_hook, post_parse_hook);
  g_option_group_add_entries (group, clutter_args);
  
  /* add backend-specific options */
  _clutter_backend_add_options (context->backend, group);

  return group;
}

GQuark
clutter_init_error_quark (void)
{
  return g_quark_from_static_string ("clutter-init-error-quark");
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

  g_type_init ();

  group   = clutter_get_option_group ();
  context = g_option_context_new (parameter_string);

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
  if (!_clutter_backend_init_stage (clutter_context->backend, &stage_error))
    {
      g_propagate_error (error, stage_error);
      return CLUTTER_INIT_ERROR_INTERNAL;
    }

  _clutter_backend_init_events (clutter_context->backend);

  _clutter_feature_init ();

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

  g_type_init ();

  /* parse_args will trigger backend creation and things like
   * DISPLAY connection etc.
  */
  if (clutter_parse_args (argc, argv) == FALSE)
    {
      CLUTTER_NOTE (MISC, "failed to parse arguments.");
      return CLUTTER_INIT_ERROR_INTERNAL;
    }

  /* Note, creates backend if not already existing (though parse args will
   * have likely created it)  
   */
  context = clutter_context_get_default ();

  /* Stage will give us a GL Context etc */
  stage_error = NULL;
  if (!_clutter_backend_init_stage (context->backend, &stage_error))
    {
      CLUTTER_NOTE (MISC, "stage failed to initialise.");
      g_critical (stage_error->message);
      g_error_free (stage_error);
      return CLUTTER_INIT_ERROR_INTERNAL;
    }

  /* Initiate event collection */
  _clutter_backend_init_events (context->backend);

  /* finally features - will call to backend and cogl */
  _clutter_feature_init ();

  return CLUTTER_INIT_SUCCESS;
}

gboolean
_clutter_boolean_accumulator (GSignalInvocationHint *ihint,
                              GValue                *return_accu,
                              const GValue          *handler_return,
                              gpointer               dummy)
{
  gboolean continue_emission;
  gboolean signal_handled;
      
  signal_handled = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, signal_handled);
  continue_emission = !signal_handled;

  return continue_emission;
}

void
clutter_base_init (void)
{
  static gboolean initialised = FALSE;

  if (!initialised)
    {
      initialised = TRUE;

      /* initialise GLib type system */
      g_type_init ();
      clutter_actor_get_type ();
    }
}
