/* Metacity Session Management */

/* 
 * Copyright (C) 2001 Havoc Pennington (some code in here from
 * libgnomeui, (C) Tom Tromey, Carsten Schaar)
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  */

#include "session.h"

#ifndef HAVE_SM
void
meta_session_init (const char *previous_id)
{
  meta_verbose ("Compiled without session management support\n");
}
#else /* HAVE_SM */

#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"
#include "util.h"
#include "display.h"
#include "workspace.h"

static void ice_io_error_handler (IceConn connection);

static void new_ice_connection (IceConn connection, IcePointer client_data, 
				Bool opening, IcePointer *watch_data);

static void save_state         (void);
static void load_state         (const char *previous_id);

/* This is called when data is available on an ICE connection.  */
static gboolean
process_ice_messages (GIOChannel *channel,
                      GIOCondition condition,
                      gpointer client_data)
{
  IceConn connection = (IceConn) client_data;
  IceProcessMessagesStatus status;

  /* This blocks infinitely sometimes. I don't know what
   * to do about it. Checking "condition" just breaks
   * session management.
   */
  status = IceProcessMessages (connection, NULL, NULL);

  if (status == IceProcessMessagesIOError)
    {
#if 0
      IcePointer context = IceGetConnectionContext (connection);
#endif
      
      /* We were disconnected */
      IceSetShutdownNegotiation (connection, False);
      IceCloseConnection (connection);
    }

  return TRUE;
}

/* This is called when a new ICE connection is made.  It arranges for
   the ICE connection to be handled via the event loop.  */
static void
new_ice_connection (IceConn connection, IcePointer client_data, Bool opening,
		    IcePointer *watch_data)
{
  guint input_id;

  if (opening)
    {
      /* Make sure we don't pass on these file descriptors to any
       * exec'ed children
       */
      GIOChannel *channel;
      
      fcntl (IceConnectionNumber(connection),F_SETFD,
             fcntl(IceConnectionNumber(connection),F_GETFD,0) | FD_CLOEXEC);

      channel = g_io_channel_unix_new (IceConnectionNumber (connection));
      
      input_id = g_io_add_watch (channel,
                                 G_IO_IN | G_IO_ERR,
                                 process_ice_messages,
                                 connection);

      g_io_channel_unref (channel);
      
      *watch_data = (IcePointer) GUINT_TO_POINTER (input_id);
    }
  else 
    {
      input_id = GPOINTER_TO_UINT ((gpointer) *watch_data);

      g_source_remove (input_id);
    }
}

static IceIOErrorHandler gnome_ice_installed_handler;

/* We call any handler installed before (or after) gnome_ice_init but 
   avoid calling the default libICE handler which does an exit() */
static void
ice_io_error_handler (IceConn connection)
{
    if (gnome_ice_installed_handler)
      (*gnome_ice_installed_handler) (connection);
}    

static void
ice_init (void)
{
  static gboolean ice_initted = FALSE;

  if (! ice_initted)
    {
      IceIOErrorHandler default_handler;

      gnome_ice_installed_handler = IceSetIOErrorHandler (NULL);
      default_handler = IceSetIOErrorHandler (ice_io_error_handler);

      if (gnome_ice_installed_handler == default_handler)
	gnome_ice_installed_handler = NULL;

      IceAddConnectionWatch (new_ice_connection, NULL);

      ice_initted = TRUE;
    }
}

typedef enum
{
  STATE_DISCONNECTED,
  STATE_IDLE,
  STATE_SAVING_PHASE_1,
  STATE_WAITING_FOR_PHASE_2,
  STATE_SAVING_PHASE_2,
  STATE_FROZEN,
  STATE_REGISTERING
} ClientState;

static void save_phase_2_callback       (SmcConn   smc_conn,
                                         SmPointer client_data);
static void interact_callback           (SmcConn   smc_conn,
                                         SmPointer client_data);
static void shutdown_cancelled_callback (SmcConn   smc_conn,
                                         SmPointer client_data);
static void save_complete_callback      (SmcConn   smc_conn,
                                         SmPointer client_data);
static void die_callback                (SmcConn   smc_conn,
                                         SmPointer client_data);
static void save_yourself_callback      (SmcConn   smc_conn,
                                         SmPointer client_data,
                                         int       save_style,
                                         Bool      shutdown,
                                         int       interact_style,
                                         Bool      fast);
static void set_clone_restart_commands  (void);

static gchar *client_id = NULL;
static gpointer session_connection = NULL;
static ClientState current_state = STATE_DISCONNECTED;

void
meta_session_init (const char *previous_id)
{
  /* Some code here from twm */
  char buf[256];
  unsigned long mask;
  SmcCallbacks callbacks;

  meta_verbose ("Initializing session with session ID '%s'\n",
                previous_id ? previous_id : "(none)");

  if (previous_id)
    load_state (previous_id);
  
  ice_init ();
  
  mask = SmcSaveYourselfProcMask | SmcDieProcMask |
    SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask;
  
  callbacks.save_yourself.callback = save_yourself_callback;
  callbacks.save_yourself.client_data = NULL;
  
  callbacks.die.callback = die_callback;
  callbacks.die.client_data = NULL;
  
  callbacks.save_complete.callback = save_complete_callback;
  callbacks.save_complete.client_data = NULL;
  
  callbacks.shutdown_cancelled.callback = shutdown_cancelled_callback;
  callbacks.shutdown_cancelled.client_data = NULL;

  session_connection =
    SmcOpenConnection (NULL, /* use SESSION_MANAGER env */
                       NULL, /* means use existing ICE connection */
                       SmProtoMajor,
                       SmProtoMinor,
                       mask,
                       &callbacks,
                       previous_id,
                       &client_id,
                       255, buf);
  
  if (session_connection == NULL)
    {
      meta_warning ("Failed to open connection to session manager: %s\n", buf);
      return;
    }
  else
    {
      if (client_id == NULL)
        meta_bug ("Session manager gave us a NULL client ID?");
      meta_verbose ("Obtained session ID '%s'\n", client_id);
    }

  if (previous_id && strcmp (previous_id, client_id) == 0)
    current_state = STATE_IDLE;
  else
    current_state = STATE_REGISTERING;
  
  {
    SmProp prop1, prop2, prop3, prop4, prop5, *props[5];
    SmPropValue prop1val, prop2val, prop3val, prop4val, prop5val;
    char pid[32];
    char hint = SmRestartIfRunning;
    
    prop1.name = SmProgram;
    prop1.type = SmARRAY8;
    prop1.num_vals = 1;
    prop1.vals = &prop1val;
    prop1val.value = "metacity";
    prop1val.length = strlen ("metacity");

    /* twm sets getuid() for this, but the SM spec plainly
     * says pw_name, twm is on crack
     */
    prop2.name = SmUserID;
    prop2.type = SmARRAY8;
    prop2.num_vals = 1;
    prop2.vals = &prop2val;
    prop2val.value = g_get_user_name ();
    prop2val.length = strlen (prop2val.value);
	
    prop3.name = SmRestartStyleHint;
    prop3.type = SmCARD8;
    prop3.num_vals = 1;
    prop3.vals = &prop3val;
    prop3val.value = &hint;
    prop3val.length = 1;

    sprintf (pid, "%d", getpid ());
    prop4.name = SmProcessID;
    prop4.type = SmARRAY8;
    prop4.num_vals = 1;
    prop4.vals = &prop4val;
    prop4val.value = pid;
    prop4val.length = strlen (prop4val.value);    

    /* Always start in home directory */
    prop5.name = SmCurrentDirectory;
    prop5.type = SmARRAY8;
    prop5.num_vals = 1;
    prop5.vals = &prop5val;
    prop5val.value = g_get_home_dir ();
    prop5val.length = strlen (prop5val.value);
    
    props[0] = &prop1;
    props[1] = &prop2;
    props[2] = &prop3;
    props[3] = &prop4;
    props[4] = &prop5;
    
    SmcSetProperties (session_connection, 5, props);
  }

  set_clone_restart_commands ();
}

static void
disconnect (void)
{
  SmcCloseConnection (session_connection, 0, NULL);
  session_connection = NULL;
  current_state = STATE_DISCONNECTED;
}

static void
save_yourself_possibly_done (gboolean shutdown,
                             gboolean successful)
{
  if (current_state == STATE_SAVING_PHASE_1)
    {
      Status status;
      
      status = SmcRequestSaveYourselfPhase2 (session_connection,
                                             save_phase_2_callback,
                                             GINT_TO_POINTER (shutdown));

      if (status)
        current_state = STATE_WAITING_FOR_PHASE_2;
    }

  if (current_state == STATE_SAVING_PHASE_1 ||
      current_state == STATE_SAVING_PHASE_2)
    {
      SmcSaveYourselfDone (session_connection,
                           successful);
      
      if (shutdown)
        current_state = STATE_FROZEN;
      else
        current_state = STATE_IDLE;
    }
}


static void 
save_phase_2_callback (SmcConn smc_conn, SmPointer client_data)
{
  gboolean shutdown;

  shutdown = GPOINTER_TO_INT (client_data);
  
  current_state = STATE_SAVING_PHASE_2;

  save_state ();
  
  save_yourself_possibly_done (shutdown, TRUE);
}

static void
save_yourself_callback (SmcConn   smc_conn,
                        SmPointer client_data,
                        int       save_style,
                        Bool      shutdown,
                        int       interact_style,
                        Bool      fast)
{
  gboolean successful;

  successful = TRUE;
  
  /* The first SaveYourself after registering for the first time
   * is a special case (SM specs 7.2).
   *
   * This SaveYourself seems to be included in the protocol to
   * ask the client to specify its initial SmProperties since 
   * there is little point saving a copy of the initial state.
   *
   * A bug in xsm means that it does not send us a SaveComplete 
   * in response to this initial SaveYourself. Therefore, we 
   * must not set a grab because it would never be released.
   * Indeed, even telling the app that this SaveYourself has
   * arrived is hazardous as the app may take its own steps
   * to freeze its WM state while waiting for the SaveComplete.
   *
   * Fortunately, we have already set the SmProperties during
   * gnome_client_connect so there is little lost in simply
   * returning immediately.
   *
   * Apps which really want to save their initial states can 
   * do so safely using gnome_client_save_yourself_request.
   */

  if (current_state == STATE_REGISTERING)
    {
      current_state = STATE_IDLE;

      /* Double check that this is a section 7.2 SaveYourself: */
      
      if (save_style == SmSaveLocal && 
	  interact_style == SmInteractStyleNone &&
	  !shutdown && !fast)
	{
	  /* The protocol requires this even if xsm ignores it. */
	  SmcSaveYourselfDone (session_connection, successful);
	  return;
	}
    }

  current_state = STATE_SAVING_PHASE_1;

  set_clone_restart_commands ();

  save_yourself_possibly_done (shutdown, successful);
}


static void
die_callback (SmcConn smc_conn, SmPointer client_data)
{
  meta_verbose ("Exiting at request of session manager\n");
  disconnect ();
  meta_quit (META_EXIT_SUCCESS);
}

static void
save_complete_callback (SmcConn smc_conn, SmPointer client_data)
{
  /* nothing */
}

static void
shutdown_cancelled_callback (SmcConn smc_conn, SmPointer client_data)
{
  /* nothing */
}

static void 
interact_callback (SmcConn smc_conn, SmPointer client_data)
{
  /* nothing */
}

static void
set_clone_restart_commands (void)
{
  char *restartv[10];
  char *clonev[10];
  char *discardv[10];
  int i;
  SmProp prop1, prop2, prop3, *props[3];
  char *session_file;

  session_file = g_strconcat (g_get_home_dir (),
                              ".metacity/sessions/",
                              client_id,
                              NULL);
  
  /* Restart (use same client ID) */
  
  prop1.name = SmRestartCommand;
  prop1.type = SmLISTofARRAY8;
  
  i = 0;
  restartv[i] = "metacity";
  ++i;
  restartv[i] = "--sm-client-id";
  ++i;
  restartv[i] = client_id;
  ++i;
  restartv[i] = NULL;

  prop1.vals = g_new (SmPropValue, i);
  i = 0;
  while (restartv[i])
    {
      prop1.vals[i].value = restartv[i];
      prop1.vals[i].length = strlen (restartv[i]);
      ++i;
    }
  prop1.num_vals = i;

  /* Clone (no client ID) */
  
  i = 0;
  clonev[i] = "metacity";
  ++i;
  clonev[i] = NULL;

  prop2.name = SmCloneCommand;
  prop2.type = SmLISTofARRAY8;
  
  prop2.vals = g_new (SmPropValue, i);
  i = 0;
  while (clonev[i])
    {
      prop2.vals[i].value = clonev[i];
      prop2.vals[i].length = strlen (clonev[i]);
      ++i;
    }
  prop2.num_vals = i;

  /* Discard */
  
  i = 0;
  discardv[i] = "rm";
  ++i;
  discardv[i] = "-f";
  ++i;
  discardv[i] = session_file;

  prop3.name = SmCloneCommand;
  prop3.type = SmLISTofARRAY8;
  
  prop3.vals = g_new (SmPropValue, i);
  i = 0;
  while (clonev[i])
    {
      prop3.vals[i].value = discardv[i];
      prop3.vals[i].length = strlen (discardv[i]);
      ++i;
    }
  prop3.num_vals = i;

  
  props[0] = &prop1;
  props[1] = &prop2;
  props[2] = &prop3;
  
  SmcSetProperties (session_connection, 3, props);

  g_free (session_file);
}

/* The remaining code in this file actually loads/saves the session,
 * while the code above this comment handles chatting with the
 * session manager.
 */

static void
save_state (void)
{
  char *metacity_dir;
  char *session_dir;
  char *session_file;
  FILE *outfile;
  GSList *displays;
  GSList *display_iter;
  
  g_assert (client_id);

  outfile = NULL;
  
  metacity_dir = g_strconcat (g_get_home_dir (), "/.metacity",
                              NULL);
  
  session_dir = g_strconcat (metacity_dir, "/sessions",
                             NULL);

  /* Assuming client ID is a workable filename. */
  session_file = g_strconcat (session_dir, "/",
                              client_id,
                              NULL);


  if (mkdir (metacity_dir, 0700) < 0 &&
      errno != EEXIST)
    {
      meta_warning (_("Could not create directory '%s': %s\n"),
                    metacity_dir, g_strerror (errno));
    }

  if (mkdir (session_dir, 0700) < 0 &&
      errno != EEXIST)
    {
      meta_warning (_("Could not create directory '%s': %s\n"),
                    session_dir, g_strerror (errno));
    }

  meta_verbose ("Saving session to '%s'\n", session_file);
  
  outfile = fopen (session_file, "w");

  if (outfile == NULL)
    {
      meta_warning (_("Could not open session file '%s' for writing: %s\n"),
                    session_file, g_strerror (errno));
      goto out;
    }

  /* The file format is:
   * <metacity_session id="foo">
   *   <window id="bar" class="XTerm" name="xterm" title="/foo/bar" role="blah">
   *     <workspace>2</workspace>
   *     <workspace>4</workspace>
   *     <sticky/>
   *     <geometry x="100" y="100" width="200" height="200" gravity="northwest"/>
   *   </window>
   * </metacity_session>
   */

  /* FIXME we are putting non-UTF-8 in here. */
  
  fprintf (outfile, "<metacity_session id=\"%s\">\n",
           client_id);
  
  displays = meta_displays_list ();
  display_iter = displays;
  while (display_iter != NULL)
    {
      GSList *windows;
      GSList *tmp;

      windows = meta_display_list_windows (display_iter->data);
      tmp = windows;
      while (tmp != NULL)
        {
          MetaWindow *window;

          window = tmp->data;

          if (window->sm_client_id)
            {
              meta_verbose ("Saving session managed window %s, client ID '%s'\n",
                            window->desc, window->sm_client_id);

              fprintf (outfile,
                       "  <window id=\"%s\" class=\"%s\" name=\"%s\" title=\"%s\" role=\"%s\">\n",
                       window->sm_client_id,
                       window->res_class ? window->res_class : "",
                       window->res_name ? window->res_name : "",
                       window->title ? window->title : "",
                       window->role ? window->role : "");

              /* Sticky */
              if (window->on_all_workspaces)
                fputs ("    <sticky/>\n", outfile);

              /* Workspaces we're on */
              {
                GSList *w;
                w = window->workspaces;
                while (w != NULL)
                  {
                    int n;
                    n = meta_workspace_screen_index (w->data);
                    fprintf (outfile,
                             "<workspace>%d</workspace>\n", n);

                    w = w->next;
                  }
              }
              
              fputs ("  </window>\n", outfile);
            }
          else
            {
              meta_verbose ("Not saving window '%s', not session managed\n",
                            window->desc);
            }
          
          tmp = tmp->next;
        }
      
      g_slist_free (windows);

      display_iter = display_iter->next;
    }
  /* don't need to free displays */
  displays = NULL;
  
  fputs ("</metacity_session>\n", outfile);
  
 out:
  if (outfile)
    {
      if (fclose (outfile) != 0)
        {
          meta_warning (_("Error writing session file '%s': %s\n"),
                        session_file, g_strerror (errno));
        }
    }
  
  g_free (metacity_dir);
  g_free (session_dir);
  g_free (session_file);
}

static void
load_state (const char *previous_id)
{


}

void
meta_window_lookup_saved_state (MetaWindow *window,
                                MetaWindowSessionInfo *info)
{
  

}

#endif /* HAVE_SM */
