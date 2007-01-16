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

static gboolean clutter_is_initialized = FALSE;
static gboolean clutter_show_fps       = FALSE;
static gboolean clutter_fatal_warnings = FALSE;
static gchar *clutter_display_name     = NULL;
static gchar *clutter_vblank_name      = NULL;
static int clutter_screen              = 0;

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

typedef struct 
{
  GSource  source;
  Display *display;
  GPollFD  event_poll_fd;
} 
ClutterXEventSource;

typedef void (*ClutterXEventFunc) (XEvent *xev, gpointer user_data);

static ClutterMainContext *ClutterCntx = NULL;

static gboolean  
x_event_prepare (GSource  *source,
		 gint     *timeout)
{
  Display *display = ((ClutterXEventSource*)source)->display;

  *timeout = -1;

  return XPending (display);
}

static gboolean  
x_event_check (GSource *source) 
{
  ClutterXEventSource *display_source = (ClutterXEventSource*)source;
  gboolean         retval;

  if (display_source->event_poll_fd.revents & G_IO_IN)
    retval = XPending (display_source->display);
  else
    retval = FALSE;

  return retval;
}

static gboolean  
x_event_dispatch (GSource    *source,
		  GSourceFunc callback,
		  gpointer    user_data)
{
  Display *display = ((ClutterXEventSource*)source)->display;
  ClutterXEventFunc event_func = (ClutterXEventFunc) callback;
  
  XEvent xev;

  if (XPending (display))
    {
      XNextEvent (display, &xev);

      if (event_func)
	(*event_func) (&xev, user_data);
    }

  return TRUE;
}

static const GSourceFuncs x_event_funcs = {
  x_event_prepare,
  x_event_check,
  x_event_dispatch,
  NULL
};

static void
translate_key_event (ClutterKeyEvent   *event,
		     XEvent            *xevent)
{
  event->type = xevent->xany.type == KeyPress ? CLUTTER_KEY_PRESS
                                              : CLUTTER_KEY_RELEASE;
  event->time = xevent->xkey.time;
  event->modifier_state = xevent->xkey.state; /* FIXME: handle modifiers */
  event->hardware_keycode = xevent->xkey.keycode;
  event->keyval = XKeycodeToKeysym(xevent->xkey.display, 
				   xevent->xkey.keycode,
				   0 );	/* FIXME: index with modifiers */
}

static void
translate_button_event (ClutterButtonEvent   *event,
			XEvent               *xevent)
{
  /* FIXME: catch double click */
  CLUTTER_NOTE (EVENT, " button event at %ix%i",
		xevent->xbutton.x,
		xevent->xbutton.y);

  event->type = xevent->xany.type == ButtonPress ? CLUTTER_BUTTON_PRESS
                                                 : CLUTTER_BUTTON_RELEASE;
  event->time = xevent->xbutton.time;
  event->x = xevent->xbutton.x;
  event->y = xevent->xbutton.y;
  event->modifier_state = xevent->xbutton.state; /* includes button masks */
  event->button = xevent->xbutton.button;
}

static void
translate_motion_event (ClutterMotionEvent   *event,
			XEvent               *xevent)
{
  event->type = CLUTTER_MOTION;
  event->time = xevent->xbutton.time;
  event->x = xevent->xmotion.x;
  event->y = xevent->xmotion.y;
  event->modifier_state = xevent->xmotion.state;
}

static void
clutter_dispatch_x_event (XEvent  *xevent,
			  gpointer data)
{
  ClutterMainContext *ctx = CLUTTER_CONTEXT ();
  ClutterEvent        event;
  ClutterStage       *stage = ctx->stage;
  gboolean            emit_input_event = FALSE;

  switch (xevent->type)
    {
    case Expose:
      {
	XEvent foo_xev;

	/* Cheap compress */
	while (XCheckTypedWindowEvent(ctx->xdpy, 
				      xevent->xexpose.window,
				      Expose, 
				      &foo_xev));

	/* FIXME: need to make stage an 'actor' so can que
         * a paint direct from there rather than hack here...
	 */
	clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
      }
      break;
    case KeyPress:
      translate_key_event ((ClutterKeyEvent *) &event, xevent);
      g_signal_emit_by_name (stage, "key-press-event", &event);
      emit_input_event = TRUE;
      break;
    case KeyRelease:
      translate_key_event ((ClutterKeyEvent *) &event, xevent);
      g_signal_emit_by_name (stage, "key-release-event", &event);
      emit_input_event = TRUE;
      break;
    case ButtonPress:
      translate_button_event ((ClutterButtonEvent *) &event, xevent);
      g_signal_emit_by_name (stage, "button-press-event", &event);
      emit_input_event = TRUE;
      break;
    case ButtonRelease:
      translate_button_event ((ClutterButtonEvent *) &event, xevent);
      g_signal_emit_by_name (stage, "button-release-event", &event);
      emit_input_event = TRUE;
      break;
    case MotionNotify:
      translate_motion_event ((ClutterMotionEvent *) &event, xevent);
      g_signal_emit_by_name (stage, "motion-event", &event);
      emit_input_event = TRUE;
      break;
    }

  if (emit_input_event)
    g_signal_emit_by_name (stage, "input-event", &event);

}

static void
events_init()
{
  ClutterMainContext   *clutter_context;
  GMainContext         *gmain_context;
  int                   connection_number;
  GSource              *source;
  ClutterXEventSource  *display_source;

  clutter_context = clutter_context_get_default ();
  gmain_context = g_main_context_default ();

  g_main_context_ref (gmain_context);

  connection_number = ConnectionNumber (clutter_context->xdpy);
  
  source = g_source_new ((GSourceFuncs *)&x_event_funcs, 
			 sizeof (ClutterXEventSource));

  display_source = (ClutterXEventSource *)source;

  display_source->event_poll_fd.fd     = connection_number;
  display_source->event_poll_fd.events = G_IO_IN;
  display_source->display              = clutter_context->xdpy;
  
  g_source_add_poll (source, &display_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);

  g_source_set_callback (source, 
			 (GSourceFunc) clutter_dispatch_x_event, 
			 NULL  /* no userdata */, NULL);

  g_source_attach (source, gmain_context);
  g_source_unref (source);
}

static gboolean
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
  ClutterColor        stage_color;
  
  static GTimer      *timer = NULL; 
  static guint        timer_n_frames = 0;

  /* FIXME: Should move all this into stage...
  */

  CLUTTER_NOTE (PAINT, " Redraw enter");

  if (clutter_want_fps ())
    {
      if (!timer)
	timer = g_timer_new ();
    }

  clutter_stage_get_color (stage, &stage_color);

  glClearColor(((float) stage_color.red / 0xff * 1.0),
	       ((float) stage_color.green / 0xff * 1.0),
	       ((float) stage_color.blue / 0xff * 1.0),
	       0.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  
  glDisable(GL_LIGHTING); 
  glDisable(GL_DEPTH_TEST);

  clutter_actor_paint (CLUTTER_ACTOR (stage));

  if (clutter_stage_get_xwindow (stage))
    {
      clutter_feature_wait_for_vblank ();
      glXSwapBuffers(ctx->xdpy, clutter_stage_get_xwindow (stage));  
    }
  else
    {
      glXWaitGL();
      CLUTTER_GLERR();
    }


  if (clutter_want_fps ())
    {
      timer_n_frames++;

      if (g_timer_elapsed (timer, NULL) >= 1.0)
	{
	  g_print ("*** FPS: %i ***\n", timer_n_frames);
	  timer_n_frames = 0;
	  g_timer_start (timer);
	}
    }

  CLUTTER_NOTE (PAINT, "Redraw leave");
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
 * clutter_xdisplay:
 *
 * Retrieves the X display that Clutter is using
 *
 * Return value: A pointer to an X Display structure.
 */
Display*
clutter_xdisplay (void)
{
  ClutterMainContext *context = CLUTTER_CONTEXT ();

  return context->xdpy;
}

/**
 * clutter_xscreen:
 *
 * Retrieves the X screen that Clutter is using.
 *
 * Return value: the X screen ID
 */
int
clutter_xscreen (void)
{
  ClutterMainContext *context = CLUTTER_CONTEXT ();

  return context->xscreen;
}

/**
 * clutter_root_xwindow:
 *
 * FIXME
 *
 * Return value: FIXME
 */
Window
clutter_root_xwindow (void)
{
  ClutterMainContext *context = CLUTTER_CONTEXT ();

  return context->xwin_root;
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
  { "display", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &clutter_display_name,
    "X display to use", "DISPLAY" },
  { "screen", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &clutter_screen,
    "X screen to use", "SCREEN" },
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

#if 0
  /* XXX - this shows a warning with newer releases of GLib,
   * as we use GOption in order to get here, and GOption uses
   * the slice allocator and other GLib stuff.  so, either we
   * move the thread init inside clutter_init() directly or
   * we remove this call altogether, and let the applications
   * deal with threading, as they are supposed to do anyway.
   */
  if (!g_thread_supported ())
    g_thread_init (NULL);
#endif

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

  env_string = g_getenv ("DISPLAY");
  if (env_string)
    {
      clutter_display_name = g_strdup (env_string);
      env_string = NULL;
    }

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

  /* either we got this with the DISPLAY envvar or via the
   * --display command line switch; if both failed, then
   *  we'll fail later when we return in clutter_init()
   */
  if (clutter_display_name)
    clutter_context->xdpy = XOpenDisplay (clutter_display_name);

  if (clutter_context->xdpy)
    {
      if (clutter_screen == 0)
        clutter_context->xscreen = DefaultScreen (clutter_context->xdpy);
      else
        {
          Screen *xscreen;

          xscreen = ScreenOfDisplay (clutter_context->xdpy, clutter_screen);
          clutter_context->xscreen = XScreenNumberOfScreen (xscreen);
        }

      clutter_context->xwin_root = RootWindow (clutter_context->xdpy,
                                               clutter_context->xscreen);

      /* we don't need it anymore */
      g_free (clutter_display_name);
    }

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

static gboolean
clutter_parse_args (int    *argc,
                    char ***argv)
{
  GOptionContext *option_context;
  GOptionGroup *clutter_group;
  GError *error = NULL;

  if (clutter_is_initialized)
    return TRUE;

  option_context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (option_context, TRUE);
  g_option_context_set_help_enabled (option_context, FALSE);

  clutter_group = clutter_get_option_group ();
  g_option_context_set_main_group (option_context, clutter_group);
  if (!g_option_context_parse (option_context, argc, argv, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  g_option_context_free (option_context);

  return TRUE;
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

  if (!XInitThreads())
    {
      g_set_error (error, clutter_init_error_quark (),
                   CLUTTER_INIT_ERROR_THREADS,
                   "Unable to initialise the X threading");
      return CLUTTER_INIT_ERROR_THREADS;
    }

  group = clutter_get_option_group ();

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
  if (!clutter_context->xdpy)
    {
      g_set_error (error, clutter_init_error_quark (),
                   CLUTTER_INIT_ERROR_DISPLAY,
                   "Unable to connect to X DISPLAY. You should either "
                   "set the DISPLAY environment variable or use the "
                   "--display command line switch");
      return CLUTTER_INIT_ERROR_DISPLAY;
    }

  stage_error = NULL;
  if (!clutter_stage_init (clutter_context, &stage_error))
    {
      g_propagate_error (error, stage_error);
      return CLUTTER_INIT_ERROR_INTERNAL;
    }

  /* At least GL 1.2 is needed for CLAMP_TO_EDGE */
  if (!is_gl_version_at_least_12 ())
    {
      g_set_error (error, clutter_init_error_quark (),
                   CLUTTER_INIT_ERROR_OPENGL,
                   "Clutter needs at least version 1.2 of OpenGL");
      return CLUTTER_INIT_ERROR_OPENGL;
    }

  events_init ();

  return CLUTTER_INIT_SUCCESS;
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

  if (!XInitThreads())
    return CLUTTER_INIT_ERROR_THREADS;

  clutter_parse_args (argc, argv);

  context = clutter_context_get_default ();
  if (!context->xdpy)
    {
      g_critical ("Unable to connect to X DISPLAY. You should either "
                  "set the DISPLAY environment variable or use the "
                  "--display command line switch");

      return CLUTTER_INIT_ERROR_DISPLAY;
    }

  stage_error = NULL;
  if (!clutter_stage_init (context, &stage_error))
    {
      g_critical (stage_error->message);
      g_error_free (stage_error);
      return CLUTTER_INIT_ERROR_INTERNAL;
    }

  /* At least GL 1.2 is needed for CLAMP_TO_EDGE */
  if (!is_gl_version_at_least_12 ())
    {
      g_critical ("Clutter needs at least version 1.2 of OpenGL");
      return CLUTTER_INIT_ERROR_OPENGL;
    }

  events_init ();

  return 1;
}
