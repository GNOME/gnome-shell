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

#include "cogl.h"

/* main context */
static ClutterMainContext *ClutterCntx = NULL;

/* main lock and locking/unlocking functions */
static GMutex *clutter_threads_mutex    = NULL;
static GCallback clutter_threads_lock   = NULL;
static GCallback clutter_threads_unlock = NULL;

static gboolean clutter_is_initialized = FALSE;
static gboolean clutter_show_fps       = FALSE;
static gboolean clutter_fatal_warnings = FALSE;

static guint clutter_main_loop_level = 0;
static GSList *main_loops = NULL;

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
  { "scheduler", CLUTTER_DEBUG_SCHEDULER },
};
#endif /* CLUTTER_ENABLE_DEBUG */

/**
 * clutter_get_show_fps:
 *
 * Returns whether Clutter should print out the frames per second on the
 * console. You can enable this setting either using the
 * <literal>CLUTTER_SHOW_FPS</literal> environment variable or passing
 * the <literal>--clutter-show-fps</literal> command line argument. *
 *
 * Return value: %TRUE if Clutter should show the FPS.
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
 * Forces a redraw of the entire stage. Applications should never use this
 * function, but queue a redraw using clutter_actor_queue_redraw().
 */
void
clutter_redraw (void)
{
  ClutterMainContext *ctx;
  ClutterActor       *stage;
  static GTimer      *timer = NULL; 
  static guint        timer_n_frames = 0;
  
  ctx  = clutter_context_get_default ();

  stage = _clutter_backend_get_stage (ctx->backend);

  CLUTTER_TIMESTAMP (SCHEDULER, "Redraw start");

  CLUTTER_NOTE (PAINT, " Redraw enter");

  /* Setup FPS count */
  if (clutter_get_show_fps ())
    {
      if (!timer)
	timer = g_timer_new ();
    }

  /* The below cant go in stage paint as base actor_paint will get
   * called before the below (and break picking etc)
  */
  if (CLUTTER_PRIVATE_FLAGS (stage) & CLUTTER_ACTOR_SYNC_MATRICES)
    {
      ClutterPerspective perspective;

      clutter_stage_get_perspectivex (CLUTTER_STAGE (stage), &perspective);

      cogl_setup_viewport (clutter_actor_get_width (stage),
			   clutter_actor_get_height (stage),
			   perspective.fovy,
			   perspective.aspect,
			   perspective.z_near,
			   perspective.z_far);

      CLUTTER_UNSET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_SYNC_MATRICES);
    }

  /* Call through ti the actual backend to do the painting down from  
   * the stage. It will likely need to swap buffers, vblank sync etc
   * which will be windowing system dependant.
  */
  _clutter_backend_redraw (ctx->backend);

  /* Complete FPS info */
  if (clutter_get_show_fps ())
    {
      timer_n_frames++;

      if (g_timer_elapsed (timer, NULL) >= 1.0)
	{
	  g_print ("*** FPS: %i ***\n", timer_n_frames);
	  timer_n_frames = 0;
	  g_timer_start (timer);
	}
    }

  CLUTTER_NOTE (PAINT, " Redraw leave");

  CLUTTER_TIMESTAMP (SCHEDULER, "Redraw finish");
}

void
clutter_enable_motion_events (gboolean enable)
{
  ClutterMainContext  *context = clutter_context_get_default ();

  context->motion_events_per_actor = enable;
}

gboolean
clutter_get_motion_events_enabled (void)
{
  ClutterMainContext  *context = clutter_context_get_default ();

  return context->motion_events_per_actor;
}


/** 
 * clutter_do_event
 * @event: a #ClutterEvent.
 *
 * Processes an event. This function should never be called by applications.
 *
 * Since: 0.4
 */
void
clutter_do_event (ClutterEvent *event)
{
  ClutterMainContext  *context;
  ClutterBackend      *backend;
  ClutterActor        *stage;
  static ClutterActor *motion_last_actor = NULL; 

  context = clutter_context_get_default ();
  backend = context->backend;
  stage = _clutter_backend_get_stage (backend);
  if (!stage)
    return;

  CLUTTER_TIMESTAMP (EVENT, "Event received");

  /* TODO: 
   *
  */

  switch (event->type)
    {
    case CLUTTER_NOTHING:
      break;

    case CLUTTER_DESTROY_NOTIFY:
    case CLUTTER_DELETE:
      /* FIXME: handle delete working in stage */
      if (clutter_stage_event (CLUTTER_STAGE (stage), event))
        clutter_main_quit ();
      break;
    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
      {
	ClutterActor *actor = NULL;

	actor = clutter_stage_get_key_focus (CLUTTER_STAGE(stage));

	g_return_if_fail (actor != NULL);

	/* FIXME: should we ref ? */
	event->key.source = actor;

	/* bubble up */
	do
	  {
	    clutter_actor_event (actor, event);
	    actor = clutter_actor_get_parent (actor);
	  }
	while (actor != NULL);
      }
      break;
    case CLUTTER_MOTION:
      if (context->motion_events_per_actor == FALSE)
	{
	  /* Only stage gets motion events */
	  event->motion.source = stage;
	  clutter_actor_event (stage, event);
	  break;
	}
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_2BUTTON_PRESS:
    case CLUTTER_3BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
    case CLUTTER_SCROLL:
      {
	ClutterActor *actor;
	gint          x,y;

	clutter_event_get_coords (event, &x, &y);

	/* Map the event to a reactive actor */
	actor = _clutter_do_pick (CLUTTER_STAGE (stage), 
				  x, y, 
				  CLUTTER_PICK_REACTIVE);

	CLUTTER_NOTE (EVENT, "Reactive event received at %i, %i - actor: %p", 
		      x, y, actor);

	if (event->type == CLUTTER_SCROLL)
	  event->scroll.source = actor;
	else
	  event->button.source = actor;

	/* Motion enter leave events */
	if (event->type == CLUTTER_MOTION)
	  {
	    if (motion_last_actor != actor)
	      {
		if (motion_last_actor)
		  ;		/* FIXME: leave_notify to motion_last_actor */
		if (actor)
		  ;             /* FIXME: Enter notify to actor */
	      }
	    motion_last_actor = actor;
	  }

	/* Send the event to the actor and all parents always the 
	 * stage.  
         *
	 * FIXME: for an optimisation should check if there are
	 * actually any reactive actors and avoid the pick all togeather
	 * (signalling just the stage). Should be big help for gles.
	 */
	while (actor)
	  {
	    if (clutter_actor_is_reactive (actor) ||
                clutter_actor_get_parent (actor) == NULL /* STAGE */ )
	      {
		CLUTTER_NOTE (EVENT, "forwarding event to reactive actor");
		clutter_actor_event (actor, event);
	      }

	    actor = clutter_actor_get_parent (actor);
	  }
      }
      break;
    case CLUTTER_STAGE_STATE:
      /* fullscreen / focus - forward to stage */
      clutter_stage_event (CLUTTER_STAGE(stage), event);
      break;
    case CLUTTER_CLIENT_MESSAGE:
      break;
    }
}

ClutterActor*  
_clutter_do_pick (ClutterStage   *stage,
		  gint            x,
		  gint            y,
		  ClutterPickMode mode)
{
  ClutterMainContext *context;
  guchar              pixel[4];
  GLint               viewport[4];
  ClutterColor        white = { 0xff, 0xff, 0xff, 0xff };
  guint32             id;
  gint                r,g,b;

  context = clutter_context_get_default ();

  cogl_paint_init (&white);
  cogl_enable (0);

  /* Render the entire scence in pick mode - just single colored silhouette's  
   * are drawn offscreen (as we never swap buffers)
  */
  context->pick_mode = mode;
  clutter_actor_paint (CLUTTER_ACTOR (stage));
  context->pick_mode = CLUTTER_PICK_NONE;

  /* Calls should work under both GL and GLES, note GLES needs RGBA */
  glGetIntegerv(GL_VIEWPORT, viewport);
  glReadPixels(x, viewport[3] - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

  if (pixel[0] == 0xff && pixel[1] == 0xff && pixel[2] == 0xff)
    return CLUTTER_ACTOR(stage);

  cogl_get_bitmasks (&r, &g, &b, NULL);

  /* Decode color back into an ID, taking into account fb depth */
  id = pixel[2]>>(8-b) | pixel[1]<<b>>(8-g) | pixel[0]<<(g+b)>>(8-r);

  return clutter_container_find_child_by_id (CLUTTER_CONTAINER (stage), id);
}


/**
 * clutter_main_quit:
 *
 * Terminates the Clutter mainloop.
 */
void
clutter_main_quit (void)
{
  g_return_if_fail (main_loops != NULL);

  g_main_loop_quit (main_loops->data);
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
  return clutter_main_loop_level;
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

  CLUTTER_MARK ();

  clutter_main_loop_level++;

  loop = g_main_loop_new (NULL, TRUE);
  main_loops = g_slist_prepend (main_loops, loop);

  if (g_main_loop_is_running (main_loops->data))
    {
      clutter_threads_leave ();
      g_main_loop_run (loop);
      clutter_threads_enter ();
    }

  main_loops = g_slist_remove (main_loops, loop);

  g_main_loop_unref (loop);

  clutter_main_loop_level--;

  if (clutter_main_loop_level == 0)
    {
      /* this will take care of destroying the stage */
      g_object_unref (context->backend);
      context->backend = NULL;

      g_free (context);
    }

  CLUTTER_MARK ();
}

static void
clutter_threads_impl_lock (void)
{
  if (clutter_threads_mutex)
    g_mutex_lock (clutter_threads_mutex);
}

static void
clutter_threads_impl_unlock (void)
{
  if (clutter_threads_mutex)
    g_mutex_unlock (clutter_threads_mutex);
}

/**
 * clutter_threads_init:
 *
 * Initialises the Clutter threading mechanism, so that Clutter API can be
 * called by multiple threads, using clutter_threads_enter() and
 * clutter_threads_leave() to mark the critical sections.
 *
 * You must call g_thread_init() before this function.
 *
 * This function must be called before clutter_init().
 *
 * Since: 0.4
 */
void
clutter_threads_init (void)
{
  if (!g_thread_supported ())
    g_error ("g_thread_init() must be called before clutter_threads_init()");

  clutter_threads_mutex = g_mutex_new ();

  if (!clutter_threads_lock)
    clutter_threads_lock = clutter_threads_impl_lock;

  if (!clutter_threads_unlock)
    clutter_threads_unlock = clutter_threads_impl_unlock;
}

/**
 * clutter_threads_set_lock_functions:
 * @enter_fn: function called when aquiring the Clutter main lock
 * @leave_fn: function called when releasing the Clutter main lock
 *
 * Allows the application to replace the standard method that
 * Clutter uses to protect its data structures. Normally, Clutter
 * creates a single #GMutex that is locked by clutter_threads_enter(),
 * and released by clutter_threads_leave(); using this function an
 * application provides, instead, a function @enter_fn that is
 * called by clutter_threads_enter() and a function @leave_fn that is
 * called by clutter_threads_leave().
 *
 * The functions must provide at least same locking functionality
 * as the default implementation, but can also do extra application
 * specific processing.
 *
 * As an example, consider an application that has its own recursive
 * lock that when held, holds the Clutter lock as well. When Clutter
 * unlocks the Clutter lock when entering a recursive main loop, the
 * application must temporarily release its lock as well.
 *
 * Most threaded Clutter apps won't need to use this method.
 *
 * This method must be called before clutter_threads_init(), and cannot
 * be called multiple times.
 *
 * Since: 0.4
 */
void
clutter_threads_set_lock_functions (GCallback enter_fn,
                                    GCallback leave_fn)
{
  g_return_if_fail (clutter_threads_lock == NULL &&
                    clutter_threads_unlock == NULL);

  clutter_threads_lock = enter_fn;
  clutter_threads_unlock = leave_fn;
}

typedef struct
{
  GSourceFunc func;
  gpointer data;
  GDestroyNotify notify;
} ClutterThreadsDispatch;

static gboolean
clutter_threads_dispatch (gpointer data)
{
  ClutterThreadsDispatch *dispatch = data;
  gboolean ret = FALSE;

  clutter_threads_enter ();

  if (!g_source_is_destroyed (g_main_current_source ()))
    ret = dispatch->func (dispatch->data);

  clutter_threads_leave ();

  return ret;
}

static void
clutter_threads_dispatch_free (gpointer data)
{
  ClutterThreadsDispatch *dispatch = data;

  /* XXX - we cannot hold the thread lock here because the main loop
   * might destroy a source while still in the dispatcher function; so
   * knowing whether the lock is being held or not is not known a priori.
   *
   * see bug: http://bugzilla.gnome.org/show_bug.cgi?id=459555
   */
  if (dispatch->notify)
    dispatch->notify (dispatch->data);

  g_slice_free (ClutterThreadsDispatch, dispatch);
}

/**
 * clutter_threads_add_idle_full:
 * @priority: the priority of the timeout source. Typically this will be in the
 *    range between #G_PRIORITY_DEFAULT_IDLE and #G_PRIORITY_HIGH_IDLE
 * @func: function to call
 * @data: data to pass to the function
 * @notify: functio to call when the idle source is removed
 *
 * Adds a function to be called whenever there are no higher priority
 * events pending.  If the function returns %FALSE it is automatically
 * removed from the list of event sources and will not be called again.
 *
 * This variant of g_idle_add_full() calls @function with the Clutter lock
 * held. It can be thought of a MT-safe version for Clutter actors for the 
 * following use case, where you have to worry about idle_callback()
 * running in thread A and accessing @self after it has been finalized
 * in thread B:
 *
 * <informalexample><programlisting>
 * static gboolean
 * idle_callback (gpointer data)
 * {
 *    // clutter_threads_enter(); would be needed for g_idle_add()
 *
 *    SomeActor *self = data;
 *    /<!-- -->* do stuff with self *<!-- -->/
 *
 *    self->idle_id = 0;
 *
 *    // clutter_threads_leave(); would be needed for g_idle_add()
 *    return FALSE;
 * }
 * static void
 * some_actor_do_stuff_later (SomeActor *self)
 * {
 *    self->idle_id = clutter_threads_add_idle (idle_callback, self)
 *    // using g_idle_add() here would require thread protection in the callback
 * }
 *
 * static void
 * some_actor_finalize (GObject *object)
 * {
 *    SomeActor *self = SOME_ACTOR (object);
 *    if (self->idle_id)
 *      g_source_remove (self->idle_id);
 *    G_OBJECT_CLASS (parent_class)->finalize (object);
 * }
 * </programlisting></informalexample>
 *
 * Return value: the ID (greater than 0) of the event source.
 *
 * Since: 0.4
 */
guint
clutter_threads_add_idle_full (gint           priority,
                               GSourceFunc    func,
                               gpointer       data,
                               GDestroyNotify notify)
{
  ClutterThreadsDispatch *dispatch;

  g_return_val_if_fail (func != NULL, 0);

  dispatch = g_slice_new (ClutterThreadsDispatch);
  dispatch->func = func;
  dispatch->data = data;
  dispatch->notify = notify;

  return g_idle_add_full (priority,
                          clutter_threads_dispatch, dispatch,
                          clutter_threads_dispatch_free);
}

/**
 * clutter_threads_add_idle:
 * @func: function to call
 * @data: data to pass to the function
 *
 * Simple wrapper around clutter_threads_add_idle_full()
 *
 * Return value: the ID (greater than 0) of the event source.
 *
 * Since: 0.4
 */
guint
clutter_threads_add_idle (GSourceFunc func,
                          gpointer    data)
{
  g_return_val_if_fail (func != NULL, 0);

  return clutter_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE,
                                        func, data,
                                        NULL);
}

/**
 * clutter_threads_add_timeout_full:
 * @priority: the priority of the timeout source. Typically this will be in the
 *            range between #G_PRIORITY_DEFAULT and #G_PRIORITY_HIGH.
 * @interval: the time between calls to the function, in milliseconds
 * @func: function to call
 * @data: data to pass to the function
 * @notify: function to call when the timeout source is removed
 *
 * Sets a function to be called at regular intervals holding the Clutter lock,
 * with the given priority.  The function is called repeatedly until it 
 * returns %FALSE, at which point the timeout is automatically destroyed 
 * and the function will not be called again.  The @notify function is
 * called when the timeout is destroyed.  The first call to the
 * function will be at the end of the first @interval.
 *
 * Note that timeout functions may be delayed, due to the processing of other
 * event sources. Thus they should not be relied on for precise timing.
 * After each call to the timeout function, the time of the next
 * timeout is recalculated based on the current time and the given interval
 * (it does not try to 'catch up' time lost in delays).
 *
 * This variant of g_timeout_add_full() can be thought of a MT-safe version 
 * for Clutter actors. See also clutter_threads_add_idle_full().
 *
 * Return value: the ID (greater than 0) of the event source.
 *
 * Since: 0.4
 */
guint
clutter_threads_add_timeout_full (gint           priority,
                                  guint          interval,
                                  GSourceFunc    func,
                                  gpointer       data,
                                  GDestroyNotify notify)
{
  ClutterThreadsDispatch *dispatch;

  g_return_val_if_fail (func != NULL, 0);

  dispatch = g_slice_new (ClutterThreadsDispatch);
  dispatch->func = func;
  dispatch->data = data;
  dispatch->notify = notify;

  return g_timeout_add_full (priority,
                             interval,
                             clutter_threads_dispatch, dispatch,
                             clutter_threads_dispatch_free);
}

/**
 * clutter_threads_add_timeout:
 * @interval: the time between calls to the function, in milliseconds
 * @func: function to call
 * @data: data to pass to the function
 *
 * Simple wrapper around clutter_threads_add_timeout_full().
 *
 * Return value: the ID (greater than 0) of the event source.
 *
 * Since: 0.4
 */
guint
clutter_threads_add_timeout (guint       interval,
                             GSourceFunc func,
                             gpointer    data)
{
  g_return_val_if_fail (func != NULL, 0);

  return clutter_threads_add_timeout_full (G_PRIORITY_DEFAULT,
                                           interval,
                                           func, data,
                                           NULL);
}

/**
 * clutter_threads_enter:
 *
 * Locks the Clutter thread lock.
 *
 * Since: 0.4
 */
void
clutter_threads_enter (void)
{
  if (clutter_threads_lock)
    (* clutter_threads_lock) ();
}

/**
 * clutter_threads_leave:
 *
 * Unlocks the Clutter thread lock.
 *
 * Since: 0.4
 */
void
clutter_threads_leave (void)
{
  if (clutter_threads_unlock)
    (* clutter_threads_unlock) ();
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
  if (G_UNLIKELY(!ClutterCntx))
    {
      ClutterMainContext *ctx;

      ctx = g_new0 (ClutterMainContext, 1);
      ctx->backend = g_object_new (_clutter_backend_impl_get_type (), NULL);

      ctx->is_initialized = FALSE;
#ifdef CLUTTER_ENABLE_DEBUG
      ctx->timer          =  g_timer_new ();
      g_timer_start (ctx->timer);
#endif
      ClutterCntx = ctx;
    }

  return ClutterCntx;
}

/**
 * clutter_get_timestamp:
 *
 * Returns the approximate number of microseconds passed since clutter was
 * intialised.
 *
 * Return value: Number of microseconds since clutter_init() was called.
 */
gulong
clutter_get_timestamp (void)
{
#ifdef CLUTTER_ENABLE_DEBUG
  ClutterMainContext *ctx;
  gdouble seconds;

  ctx = clutter_context_get_default ();

  /* FIXME: may need a custom timer for embedded setups */
  seconds = g_timer_elapsed (ctx->timer, NULL);

  return (gulong)(seconds / 1.0e-6);
#else
  return 0;
#endif
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

  clutter_base_init ();
  
  if (argc && *argc > 0 && *argv)
    g_set_prgname ((*argv)[0]);

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

  clutter_stage_set_title (CLUTTER_STAGE(clutter_stage_get_default()),
			   g_get_prgname ());

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

  clutter_base_init ();
  
  if (argc && *argc > 0 && *argv)
    g_set_prgname ((*argv)[0]);


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

  clutter_stage_set_title (CLUTTER_STAGE(clutter_stage_get_default()), 
			   g_get_prgname());

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
      GType foo; /* Quiet gcc */

      initialised = TRUE;

      /* initialise GLib type system */
      g_type_init ();

      /* CLUTTER_TYPE_ACTOR */
      foo = clutter_actor_get_type ();
    }
}
