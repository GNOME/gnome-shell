/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Session Management */

/* 
 * Copyright (C) 2001 Havoc Pennington (some code in here from
 * libgnomeui, (C) Tom Tromey, Carsten Schaar)
 * Copyright (C) 2004, 2005 Elijah Newren
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
 * 02111-1307, USA.
 */

#include <config.h>

#include "session.h"
#include <X11/Xatom.h>

#include <time.h>

#ifndef HAVE_SM
void
meta_session_init (const char *client_id,
                   const char *save_file)
{
  meta_topic (META_DEBUG_SM, "Compiled without session management support\n");
}

void
meta_session_shutdown (void)
{
  /* nothing */
}

const MetaWindowSessionInfo*
meta_window_lookup_saved_state (MetaWindow *window)
{
  return NULL;
}

void
meta_window_release_saved_state (const MetaWindowSessionInfo *info)
{
  ;
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
#include "display-private.h"
#include "workspace.h"

static void ice_io_error_handler (IceConn connection);

static void new_ice_connection (IceConn connection, IcePointer client_data, 
				Bool opening, IcePointer *watch_data);

static void        save_state         (void);
static char*       load_state         (const char *previous_save_file);
static void        regenerate_save_file (void);
static const char* full_save_file       (void);
static void        warn_about_lame_clients_and_finish_interact (gboolean shutdown);

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

      return FALSE;
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
      
      fcntl (IceConnectionNumber (connection), F_SETFD,
             fcntl (IceConnectionNumber (connection), F_GETFD, 0) | FD_CLOEXEC);

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

static IceIOErrorHandler ice_installed_handler;

/* We call any handler installed before (or after) gnome_ice_init but 
   avoid calling the default libICE handler which does an exit() */
static void
ice_io_error_handler (IceConn connection)
{
    if (ice_installed_handler)
      (*ice_installed_handler) (connection);
}    

static void
ice_init (void)
{
  static gboolean ice_initted = FALSE;

  if (! ice_initted)
    {
      IceIOErrorHandler default_handler;

      ice_installed_handler = IceSetIOErrorHandler (NULL);
      default_handler = IceSetIOErrorHandler (ice_io_error_handler);

      if (ice_installed_handler == default_handler)
	ice_installed_handler = NULL;

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
  STATE_WAITING_FOR_INTERACT,
  STATE_DONE_WITH_INTERACT,
  STATE_SKIPPING_GLOBAL_SAVE,
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

static char *client_id = NULL;
static gpointer session_connection = NULL;
static ClientState current_state = STATE_DISCONNECTED;
static gboolean interaction_allowed = FALSE;

void
meta_session_init (const char *previous_client_id,
                   const char *previous_save_file)
{
  /* Some code here from twm */
  char buf[256];
  unsigned long mask;
  SmcCallbacks callbacks;
  char *saved_client_id;
  
  meta_topic (META_DEBUG_SM, "Initializing session with save file '%s'\n",
              previous_save_file ? previous_save_file : "(none)");

  if (previous_save_file)
    {
      saved_client_id = load_state (previous_save_file);
      previous_client_id = saved_client_id;
    }
  else if (previous_client_id)
    {
      char *save_file = g_strconcat (previous_client_id, ".ms", NULL);
      saved_client_id = load_state (save_file);
      g_free (save_file);
    }
  else
    {
      saved_client_id = NULL;
    }
  
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
                       (char*) previous_client_id,
                       &client_id,
                       255, buf);
  
  if (session_connection == NULL)
    {
      meta_topic (META_DEBUG_SM, 
                  "Failed to a open connection to a session manager, so window positions will not be saved: %s\n",
                  buf);

      goto out;
    }
  else
    {
      if (client_id == NULL)
        meta_bug ("Session manager gave us a NULL client ID?");
      meta_topic (META_DEBUG_SM, "Obtained session ID '%s'\n", client_id);
    }

  if (previous_client_id && strcmp (previous_client_id, client_id) == 0)
    current_state = STATE_IDLE;
  else
    current_state = STATE_REGISTERING;
  
  {
    SmProp prop1, prop2, prop3, prop4, prop5, prop6, *props[6];
    SmPropValue prop1val, prop2val, prop3val, prop4val, prop5val, prop6val;
    char pid[32];
    char hint = SmRestartImmediately;
    char priority = 20; /* low to run before other apps */
    
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
    prop2val.value = (char*) g_get_user_name ();
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
    prop5val.value = (char*) g_get_home_dir ();
    prop5val.length = strlen (prop5val.value);

    prop6.name = "_GSM_Priority";
    prop6.type = SmCARD8;
    prop6.num_vals = 1;
    prop6.vals = &prop6val;
    prop6val.value = &priority;
    prop6val.length = 1;
    
    props[0] = &prop1;
    props[1] = &prop2;
    props[2] = &prop3;
    props[3] = &prop4;
    props[4] = &prop5;
    props[5] = &prop6;
    
    SmcSetProperties (session_connection, 6, props);
  }

 out:
  g_free (saved_client_id);
}

void
meta_session_shutdown (void)
{
  /* Change our restart mode to IfRunning */
  
  SmProp prop1;
  SmPropValue prop1val;
  SmProp *props[1];
  char hint = SmRestartIfRunning;

  if (session_connection == NULL)
    return;
  
  prop1.name = SmRestartStyleHint;
  prop1.type = SmCARD8;
  prop1.num_vals = 1;
  prop1.vals = &prop1val;
  prop1val.value = &hint;
  prop1val.length = 1;
    
  props[0] = &prop1;
  
  SmcSetProperties (session_connection, 1, props);
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
  meta_topic (META_DEBUG_SM,
              "save possibly done shutdown = %d success = %d\n",
              shutdown, successful);
  
  if (current_state == STATE_SAVING_PHASE_1)
    {
      Status status;
      
      status = SmcRequestSaveYourselfPhase2 (session_connection,
                                             save_phase_2_callback,
                                             GINT_TO_POINTER (shutdown));

      if (status)
        current_state = STATE_WAITING_FOR_PHASE_2;

      meta_topic (META_DEBUG_SM,
                  "Requested phase 2, status = %d\n", status);
    }

  if (current_state == STATE_SAVING_PHASE_2 &&
      interaction_allowed)
    {
      Status status;

      status = SmcInteractRequest (session_connection,
                                   /* ignore this feature of the protocol by always
                                    * claiming normal
                                    */
                                   SmDialogNormal,
                                   interact_callback,
                                   GINT_TO_POINTER (shutdown));

      if (status)
        current_state = STATE_WAITING_FOR_INTERACT;

      meta_topic (META_DEBUG_SM,
                  "Requested interact, status = %d\n", status);
    }
  
  if (current_state == STATE_SAVING_PHASE_1 ||
      current_state == STATE_SAVING_PHASE_2 ||
      current_state == STATE_DONE_WITH_INTERACT ||
      current_state == STATE_SKIPPING_GLOBAL_SAVE)
    {
      meta_topic (META_DEBUG_SM, "Sending SaveYourselfDone\n");
      
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

  meta_topic (META_DEBUG_SM, "Phase 2 save");
  
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

  meta_topic (META_DEBUG_SM, "SaveYourself received");
  
  successful = TRUE;
  
  /* The first SaveYourself after registering for the first time
   * is a special case (SM specs 7.2).
   */

#if 0 /* I think the GnomeClient rationale for this doesn't apply */
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
#endif

  /* ignore Global style saves
   * 
   * This interpretaion of the Local/Global/Both styles
   * was discussed extensively on the xdg-list. See:
   *
   * https://listman.redhat.com/pipermail/xdg-list/2002-July/000615.html
   */
  if (save_style == SmSaveGlobal)
    {
      current_state = STATE_SKIPPING_GLOBAL_SAVE;
      save_yourself_possibly_done (shutdown, successful);
      return;
    }

  interaction_allowed = interact_style != SmInteractStyleNone;
  
  current_state = STATE_SAVING_PHASE_1;

  regenerate_save_file ();
  
  set_clone_restart_commands ();

  save_yourself_possibly_done (shutdown, successful);
}


static void
die_callback (SmcConn smc_conn, SmPointer client_data)
{
  meta_topic (META_DEBUG_SM, "Exiting at request of session manager\n");
  disconnect ();
  meta_quit (META_EXIT_SUCCESS);
}

static void
save_complete_callback (SmcConn smc_conn, SmPointer client_data)
{
  /* nothing */
  meta_topic (META_DEBUG_SM, "SaveComplete received\n");
}

static void
shutdown_cancelled_callback (SmcConn smc_conn, SmPointer client_data)
{
  meta_topic (META_DEBUG_SM, "Shutdown cancelled received\n");
  
  if (session_connection != NULL &&
      (current_state != STATE_IDLE && current_state != STATE_FROZEN))
    {
      SmcSaveYourselfDone (session_connection, True);
      current_state = STATE_IDLE;
    }
}

static void 
interact_callback (SmcConn smc_conn, SmPointer client_data)
{
  /* nothing */
  gboolean shutdown;

  meta_topic (META_DEBUG_SM, "Interaction permission received\n");
  
  shutdown = GPOINTER_TO_INT (client_data);

  current_state = STATE_DONE_WITH_INTERACT;

  warn_about_lame_clients_and_finish_interact (shutdown);
}

static void
set_clone_restart_commands (void)
{
  char *restartv[10];
  char *clonev[10];
  char *discardv[10];
  int i;
  SmProp prop1, prop2, prop3, *props[3];
  
  /* Restart (use same client ID) */
  
  prop1.name = SmRestartCommand;
  prop1.type = SmLISTofARRAY8;
  
  g_return_if_fail (client_id);
  
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
  discardv[i] = (char*) full_save_file ();
  ++i;
  discardv[i] = NULL;
  
  prop3.name = SmDiscardCommand;
  prop3.type = SmLISTofARRAY8;
  
  prop3.vals = g_new (SmPropValue, i);
  i = 0;
  while (discardv[i])
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

  g_free (prop1.vals);
  g_free (prop2.vals);
  g_free (prop3.vals);
}

/* The remaining code in this file actually loads/saves the session,
 * while the code above this comment handles chatting with the
 * session manager.
 */

static const char*
window_type_to_string (MetaWindowType type)
{
  switch (type)
    {
    case META_WINDOW_NORMAL:
      return "normal";
    case META_WINDOW_DESKTOP:
      return "desktop";
    case META_WINDOW_DOCK:
      return "dock";
    case META_WINDOW_DIALOG:
      return "dialog";
    case META_WINDOW_MODAL_DIALOG:
      return "modal_dialog";
    case META_WINDOW_TOOLBAR:
      return "toolbar";
    case META_WINDOW_MENU:
      return "menu";
    case META_WINDOW_SPLASHSCREEN:
      return "splashscreen";
    case META_WINDOW_UTILITY:
      return "utility";
    case META_WINDOW_DROPDOWN_MENU:
      return "dropdown_menu";
    case META_WINDOW_POPUP_MENU:
      return "popup_menu";
    case META_WINDOW_TOOLTIP:
      return "tooltip";
    case META_WINDOW_NOTIFICATION:
      return "notification";
    case META_WINDOW_COMBO:
      return "combo";
    case META_WINDOW_DND:
      return "dnd";
    case META_WINDOW_OVERRIDE_OTHER:
      return "override_redirect";
    }

  return "";
} 

static MetaWindowType
window_type_from_string (const char *str)
{
  if (strcmp (str, "normal") == 0)
    return META_WINDOW_NORMAL;
  else if (strcmp (str, "desktop") == 0)
    return META_WINDOW_DESKTOP;
  else if (strcmp (str, "dock") == 0)
    return META_WINDOW_DOCK;
  else if (strcmp (str, "dialog") == 0)
    return META_WINDOW_DIALOG;
  else if (strcmp (str, "modal_dialog") == 0)
    return META_WINDOW_MODAL_DIALOG;
  else if (strcmp (str, "toolbar") == 0)
    return META_WINDOW_TOOLBAR;
  else if (strcmp (str, "menu") == 0)
    return META_WINDOW_MENU;
  else if (strcmp (str, "utility") == 0)
    return META_WINDOW_UTILITY;
  else if (strcmp (str, "splashscreen") == 0)
    return META_WINDOW_SPLASHSCREEN;
  else
    return META_WINDOW_NORMAL;
}

static int
window_gravity_from_string (const char *str)
{
  if (strcmp (str, "NorthWestGravity") == 0)
    return NorthWestGravity;
  else if (strcmp (str, "NorthGravity") == 0)
    return NorthGravity;
  else if (strcmp (str, "NorthEastGravity") == 0)
    return NorthEastGravity;
  else if (strcmp (str, "WestGravity") == 0)
    return WestGravity;
  else if (strcmp (str, "CenterGravity") == 0)
    return CenterGravity;
  else if (strcmp (str, "EastGravity") == 0)
    return EastGravity;
  else if (strcmp (str, "SouthWestGravity") == 0)
    return SouthWestGravity;
  else if (strcmp (str, "SouthGravity") == 0)
    return SouthGravity;
  else if (strcmp (str, "SouthEastGravity") == 0)
    return SouthEastGravity;
  else if (strcmp (str, "StaticGravity") == 0)
    return StaticGravity;
  else
    return NorthWestGravity;
}

static char*
encode_text_as_utf8_markup (const char *text)
{
  /* text can be any encoding, and is nul-terminated.
   * we pretend it's Latin-1 and encode as UTF-8
   */
  GString *str;
  const char *p;
  char *escaped;
  
  str = g_string_new ("");

  p = text;
  while (*p)
    {
      g_string_append_unichar (str, *p);
      ++p;
    }

  escaped = g_markup_escape_text (str->str, str->len);
  g_string_free (str, TRUE);
  
  return escaped;
}

static char*
decode_text_from_utf8 (const char *text)
{
  /* Convert back from the encoded (but not escaped) UTF-8 */
  GString *str;
  const char *p;

  str = g_string_new ("");
  
  p = text;
  while (*p)
    {
      /* obviously this barfs if the UTF-8 contains chars > 255 */
      g_string_append_c (str, g_utf8_get_char (p));

      p = g_utf8_next_char (p);
    }

  return g_string_free (str, FALSE);
}

static void
save_state (void)
{
  char *metacity_dir;
  char *session_dir;
  FILE *outfile;
  GSList *windows;
  GSList *tmp;
  int stack_position;
  
  g_assert (client_id);

  outfile = NULL;
  
  /*
   * g_get_user_config_dir() is guaranteed to return an existing directory.
   * Eventually, if SM stays with the WM, I'd like to make this
   * something like <config>/window_placement in a standard format.
   * Future optimisers should note also that by the time we get here
   * we probably already have full_save_path figured out and therefore
   * can just use the directory name from that.
   */
  metacity_dir = g_strconcat (g_get_user_config_dir (),
                              G_DIR_SEPARATOR_S "metacity",
                              NULL);
  
  session_dir = g_strconcat (metacity_dir,
                             G_DIR_SEPARATOR_S "sessions",
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

  meta_topic (META_DEBUG_SM, "Saving session to '%s'\n", full_save_file ());
  
  outfile = fopen (full_save_file (), "w");

  if (outfile == NULL)
    {
      meta_warning (_("Could not open session file '%s' for writing: %s\n"),
                    full_save_file (), g_strerror (errno));
      goto out;
    }

  /* The file format is:
   * <metacity_session id="foo">
   *   <window id="bar" class="XTerm" name="xterm" title="/foo/bar" role="blah" type="normal" stacking="5">
   *     <workspace index="2"/>
   *     <workspace index="4"/>
   *     <sticky/> <minimized/> <maximized/>
   *     <geometry x="100" y="100" width="200" height="200" gravity="northwest"/>
   *   </window>
   * </metacity_session>
   *
   * Note that attributes on <window> are the match info we use to
   * see if the saved state applies to a restored window, and
   * child elements are the saved state to be applied.
   * 
   */
  
  fprintf (outfile, "<metacity_session id=\"%s\">\n",
           client_id);

  windows = meta_display_list_windows (meta_get_display ());
  stack_position = 0;

  windows = g_slist_sort (windows, meta_display_stack_cmp);
  tmp = windows;
  stack_position = 0;

  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      if (window->sm_client_id)
        {
          char *sm_client_id;
          char *res_class;
          char *res_name;
          char *role;
          char *title;

          /* client id, class, name, role are not expected to be
           * in UTF-8 (I think they are in XPCS which is Latin-1?
           * in practice they are always ascii though.)
           */
              
          sm_client_id = encode_text_as_utf8_markup (window->sm_client_id);
          res_class = window->res_class ?
            encode_text_as_utf8_markup (window->res_class) : NULL;
          res_name = window->res_name ?
            encode_text_as_utf8_markup (window->res_name) : NULL;
          role = window->role ?
            encode_text_as_utf8_markup (window->role) : NULL;
          if (window->title)
            title = g_markup_escape_text (window->title, -1);
          else
            title = NULL;
              
          meta_topic (META_DEBUG_SM, "Saving session managed window %s, client ID '%s'\n",
                      window->desc, window->sm_client_id);

          fprintf (outfile,
                   "  <window id=\"%s\" class=\"%s\" name=\"%s\" title=\"%s\" role=\"%s\" type=\"%s\" stacking=\"%d\">\n",
                   sm_client_id,
                   res_class ? res_class : "",
                   res_name ? res_name : "",
                   title ? title : "",
                   role ? role : "",
                   window_type_to_string (window->type),
                   stack_position);

          g_free (sm_client_id);
          g_free (res_class);
          g_free (res_name);
          g_free (role);
          g_free (title);
              
          /* Sticky */
          if (window->on_all_workspaces)
            fputs ("    <sticky/>\n", outfile);

          /* Minimized */
          if (window->minimized)
            fputs ("    <minimized/>\n", outfile);

          /* Maximized */
          if (META_WINDOW_MAXIMIZED (window))
            {
              fprintf (outfile,
                       "    <maximized saved_x=\"%d\" saved_y=\"%d\" saved_width=\"%d\" saved_height=\"%d\"/>\n", 
                       window->saved_rect.x,
                       window->saved_rect.y,
                       window->saved_rect.width,
                       window->saved_rect.height);
            }
              
          /* Workspaces we're on */
          {
            int n;
            n = meta_workspace_index (window->workspace);
            fprintf (outfile,
                     "    <workspace index=\"%d\"/>\n", n);
          }

          /* Gravity */
          {
            int x, y, w, h;
            meta_window_get_geometry (window, &x, &y, &w, &h);
            
            fprintf (outfile,
                     "    <geometry x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" gravity=\"%s\"/>\n",
                     x, y, w, h,
                     meta_gravity_to_string (window->size_hints.win_gravity));
          }
              
          fputs ("  </window>\n", outfile);
        }
      else
        {
          meta_topic (META_DEBUG_SM, "Not saving window '%s', not session managed\n",
                      window->desc);
        }
          
      tmp = tmp->next;
      ++stack_position;
    }
      
  g_slist_free (windows);

  fputs ("</metacity_session>\n", outfile);
  
 out:
  if (outfile)
    {
      /* FIXME need a dialog for this */
      if (ferror (outfile))
        {
          meta_warning (_("Error writing session file '%s': %s\n"),
                        full_save_file (), g_strerror (errno));
        }
      if (fclose (outfile))
        {
          meta_warning (_("Error closing session file '%s': %s\n"),
                        full_save_file (), g_strerror (errno));
        }
    }
  
  g_free (metacity_dir);
  g_free (session_dir);
}

typedef enum
{
  WINDOW_TAG_NONE,
  WINDOW_TAG_DESKTOP,
  WINDOW_TAG_STICKY,
  WINDOW_TAG_MINIMIZED,
  WINDOW_TAG_MAXIMIZED,
  WINDOW_TAG_GEOMETRY
} WindowTag;

typedef struct
{
  MetaWindowSessionInfo *info;
  char *previous_id;
} ParseData;

static void                   session_info_free (MetaWindowSessionInfo *info);
static MetaWindowSessionInfo* session_info_new  (void);

static void start_element_handler (GMarkupParseContext  *context,
                                   const gchar          *element_name,
                                   const gchar         **attribute_names,
                                   const gchar         **attribute_values,
                                   gpointer              user_data,
                                   GError              **error);
static void end_element_handler   (GMarkupParseContext  *context,
                                   const gchar          *element_name,
                                   gpointer              user_data,
                                   GError              **error);
static void text_handler          (GMarkupParseContext  *context,
                                   const gchar          *text,
                                   gsize                 text_len,
                                   gpointer              user_data,
                                   GError              **error);

static GMarkupParser metacity_session_parser = {
  start_element_handler,
  end_element_handler,
  text_handler,
  NULL,
  NULL
};

static GSList *window_info_list = NULL;

static char*
load_state (const char *previous_save_file)
{
  GMarkupParseContext *context;
  GError *error;
  ParseData parse_data;
  char *text;
  gsize length;
  char *session_file;

  session_file = g_strconcat (g_get_user_config_dir (),
                              G_DIR_SEPARATOR_S "metacity"
                              G_DIR_SEPARATOR_S "sessions" G_DIR_SEPARATOR_S,
                              previous_save_file,
                              NULL);

  error = NULL;
  if (!g_file_get_contents (session_file,
                            &text,
                            &length,
                            &error))
    {
      char *canonical_session_file = session_file;

      /* Maybe they were doing it the old way, with ~/.metacity */
      session_file = g_strconcat (g_get_home_dir (),
                                  G_DIR_SEPARATOR_S ".metacity"
                                  G_DIR_SEPARATOR_S "sessions"
                                  G_DIR_SEPARATOR_S,
                                  previous_save_file,
                                  NULL);
      
      if (!g_file_get_contents (session_file,
                                &text,
                                &length,
                                NULL))
        {
          /* oh, just give up */

          meta_warning (_("Failed to read saved session file %s: %s\n"),
                    canonical_session_file, error->message);
          g_error_free (error);
          g_free (session_file);
          g_free (canonical_session_file);
          return NULL;
        }

      g_free (canonical_session_file);
    }

  meta_topic (META_DEBUG_SM, "Parsing saved session file %s\n", session_file);
  g_free (session_file);
  session_file = NULL;
  
  parse_data.info = NULL;
  parse_data.previous_id = NULL;
  
  context = g_markup_parse_context_new (&metacity_session_parser,
                                        0, &parse_data, NULL);

  error = NULL;
  if (!g_markup_parse_context_parse (context,
                                     text,
                                     length,
                                     &error))
    goto error;
  
  
  error = NULL;
  if (!g_markup_parse_context_end_parse (context, &error))
    goto error;

  g_markup_parse_context_free (context);

  goto out;

 error:
  
  meta_warning (_("Failed to parse saved session file: %s\n"),
                error->message);
  g_error_free (error);

  if (parse_data.info)
    session_info_free (parse_data.info);

  g_free (parse_data.previous_id);
  parse_data.previous_id = NULL;
  
 out:
  
  g_free (text);

  return parse_data.previous_id;
}

/* FIXME this isn't very robust against bogus session files */
static void
start_element_handler  (GMarkupParseContext *context,
                        const gchar         *element_name,
                        const gchar        **attribute_names,
                        const gchar        **attribute_values,
                        gpointer             user_data,
                        GError             **error)
{
  ParseData *pd;

  pd = user_data;

  if (strcmp (element_name, "metacity_session") == 0)
    {
      /* Get previous ID */
      int i;      

      i = 0;
      while (attribute_names[i])
        {
          const char *name;
          const char *val;
          
          name = attribute_names[i];
          val = attribute_values[i];

          if (pd->previous_id)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                       G_MARKUP_ERROR_PARSE,
                           _("<metacity_session> attribute seen but we already have the session ID"));
              return;
            }
          
          if (strcmp (name, "id") == 0)
            {
              pd->previous_id = decode_text_from_utf8 (val);
            }
          else
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                           _("Unknown attribute %s on <metacity_session> element"),
                           name);
              return;
            }
          
          ++i;
        }
    }
  else if (strcmp (element_name, "window") == 0)
    {
      int i;
      
      if (pd->info)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_PARSE,
                       _("nested <window> tag"));
          return;
        }
      
      pd->info = session_info_new ();

      i = 0;
      while (attribute_names[i])
        {
          const char *name;
          const char *val;
          
          name = attribute_names[i];
          val = attribute_values[i];
          
          if (strcmp (name, "id") == 0)
            {
              if (*val)
                pd->info->id = decode_text_from_utf8 (val);
            }
          else if (strcmp (name, "class") == 0)
            {
              if (*val)
                pd->info->res_class = decode_text_from_utf8 (val);
            }
          else if (strcmp (name, "name") == 0)
            {
              if (*val)
                pd->info->res_name = decode_text_from_utf8 (val);
            }
          else if (strcmp (name, "title") == 0)
            {
              if (*val)
                pd->info->title = g_strdup (val);
            }
          else if (strcmp (name, "role") == 0)
            {
              if (*val)
                pd->info->role = decode_text_from_utf8 (val);
            }
          else if (strcmp (name, "type") == 0)
            {
              if (*val)
                pd->info->type = window_type_from_string (val);
            }
          else if (strcmp (name, "stacking") == 0)
            {
              if (*val)
                {
                  pd->info->stack_position = atoi (val);
                  pd->info->stack_position_set = TRUE;
                }
            }
          else
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                           _("Unknown attribute %s on <window> element"),
                           name);
              session_info_free (pd->info);
              pd->info = NULL;
              return;
            }
          
          ++i;
        }
    }
  else if (strcmp (element_name, "workspace") == 0)
    {
      int i;

      i = 0;
      while (attribute_names[i])
        {
          const char *name;

          name = attribute_names[i];
          
          if (strcmp (name, "index") == 0)
            {
              pd->info->workspace_indices =
                g_slist_prepend (pd->info->workspace_indices,
                                 GINT_TO_POINTER (atoi (attribute_values[i])));
            }
          else
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                           _("Unknown attribute %s on <window> element"),
                           name);
              session_info_free (pd->info);
              pd->info = NULL;
              return;
            }
          
          ++i;
        }
    }
  else if (strcmp (element_name, "sticky") == 0)
    {
      pd->info->on_all_workspaces = TRUE;
      pd->info->on_all_workspaces_set = TRUE;
    }
  else if (strcmp (element_name, "minimized") == 0)
    {
      pd->info->minimized = TRUE;
      pd->info->minimized_set = TRUE;
    }
  else if (strcmp (element_name, "maximized") == 0)
    {
      int i;

      i = 0;
      pd->info->maximized = TRUE;
      pd->info->maximized_set = TRUE;
      while (attribute_names[i])
        {
          const char *name;
          const char *val;

          name = attribute_names[i];
          val = attribute_values[i];

          if (strcmp (name, "saved_x") == 0)
            {
              if (*val)
                {
                  pd->info->saved_rect.x = atoi (val);
                  pd->info->saved_rect_set = TRUE;
                }
            }
          else if (strcmp (name, "saved_y") == 0)
            {
              if (*val)
                {
                  pd->info->saved_rect.y = atoi (val);
                  pd->info->saved_rect_set = TRUE;
                }
            }
          else if (strcmp (name, "saved_width") == 0)
            {
              if (*val)
                {
                  pd->info->saved_rect.width = atoi (val);
                  pd->info->saved_rect_set = TRUE;
                }
            }
          else if (strcmp (name, "saved_height") == 0)
            {
              if (*val)
                {
                  pd->info->saved_rect.height = atoi (val);
                  pd->info->saved_rect_set = TRUE;
                }
            }
          else
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                           _("Unknown attribute %s on <maximized> element"),
                           name);
              return;
            }

          ++i;
        }

      if (pd->info->saved_rect_set)
        meta_topic (META_DEBUG_SM, "Saved unmaximized size %d,%d %dx%d \n",
                    pd->info->saved_rect.x,
                    pd->info->saved_rect.y,
                    pd->info->saved_rect.width,
                    pd->info->saved_rect.height);
    }  
  else if (strcmp (element_name, "geometry") == 0)
    {
      int i;

      pd->info->geometry_set = TRUE;
      
      i = 0;
      while (attribute_names[i])
        {
          const char *name;
          const char *val;
          
          name = attribute_names[i];
          val = attribute_values[i];
          
          if (strcmp (name, "x") == 0)
            {
              if (*val)
                pd->info->rect.x = atoi (val);
            }
          else if (strcmp (name, "y") == 0)
            {
              if (*val)
                pd->info->rect.y = atoi (val);
            }
          else if (strcmp (name, "width") == 0)
            {
              if (*val)
                pd->info->rect.width = atoi (val);
            }
          else if (strcmp (name, "height") == 0)
            {
              if (*val)
                pd->info->rect.height = atoi (val);
            }
          else if (strcmp (name, "gravity") == 0)
            {
              if (*val)
                pd->info->gravity = window_gravity_from_string (val);
            }
          else
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                           _("Unknown attribute %s on <geometry> element"),
                           name);
              return;
            }
          
          ++i;
        }

      meta_topic (META_DEBUG_SM, "Loaded geometry %d,%d %dx%d gravity %s\n",
                  pd->info->rect.x,
                  pd->info->rect.y,
                  pd->info->rect.width,
                  pd->info->rect.height,
                  meta_gravity_to_string (pd->info->gravity));
    }
  else
    {
      g_set_error (error,
                   G_MARKUP_ERROR,
                   G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                   _("Unknown element %s"),
                   element_name);
      return;
    }
}

static void
end_element_handler    (GMarkupParseContext *context,
                        const gchar         *element_name,
                        gpointer             user_data,
                        GError             **error)
{
  ParseData *pd;

  pd = user_data;

  if (strcmp (element_name, "window") == 0)
    {
      g_assert (pd->info);

      window_info_list = g_slist_prepend (window_info_list,
                                          pd->info);
      
      meta_topic (META_DEBUG_SM, "Loaded window info from session with class: %s name: %s role: %s\n",
                  pd->info->res_class ? pd->info->res_class : "(none)",
                  pd->info->res_name ? pd->info->res_name : "(none)",
                  pd->info->role ? pd->info->role : "(none)");
      
      pd->info = NULL;
    }
}

static void
text_handler           (GMarkupParseContext *context,
                        const gchar         *text,
                        gsize                text_len,
                        gpointer             user_data,
                        GError             **error)
{
  /* Right now we don't have any elements where we care about their
   * content
   */
}

static gboolean
both_null_or_matching (const char *a,
                       const char *b)
{
  if (a == NULL && b == NULL)
    return TRUE;
  else if (a && b && strcmp (a, b) == 0)
    return TRUE;
  else
    return FALSE;
}

static GSList*
get_possible_matches (MetaWindow *window)
{
  /* Get all windows with this client ID */
  GSList *retval;
  GSList *tmp;
  gboolean ignore_client_id;
  
  retval = NULL;

  ignore_client_id = g_getenv ("METACITY_DEBUG_SM") != NULL;
  
  tmp = window_info_list;
  while (tmp != NULL)
    {
      MetaWindowSessionInfo *info;

      info = tmp->data;
      
      if ((ignore_client_id ||
           both_null_or_matching (info->id, window->sm_client_id)) && 
          both_null_or_matching (info->res_class, window->res_class) &&
          both_null_or_matching (info->res_name, window->res_name) &&
          both_null_or_matching (info->role, window->role))
        {
          meta_topic (META_DEBUG_SM, "Window %s may match saved window with class: %s name: %s role: %s\n",
                      window->desc,
                      info->res_class ? info->res_class : "(none)",
                      info->res_name ? info->res_name : "(none)",
                      info->role ? info->role : "(none)");

          retval = g_slist_prepend (retval, info);
        }
      else
        {
          if (meta_is_verbose ())
            {
              if (!both_null_or_matching (info->id, window->sm_client_id))
                meta_topic (META_DEBUG_SM, "Window %s has SM client ID %s, saved state has %s, no match\n",
                            window->desc,
                            window->sm_client_id ? window->sm_client_id : "(none)",
                            info->id ? info->id : "(none)");
              else if (!both_null_or_matching (info->res_class, window->res_class))
                meta_topic (META_DEBUG_SM, "Window %s has class %s doesn't match saved class %s, no match\n",
                            window->desc,
                            window->res_class ? window->res_class : "(none)",
                            info->res_class ? info->res_class : "(none)");
              
              else if (!both_null_or_matching (info->res_name, window->res_name))
                meta_topic (META_DEBUG_SM, "Window %s has name %s doesn't match saved name %s, no match\n",
                            window->desc,
                            window->res_name ? window->res_name : "(none)",
                            info->res_name ? info->res_name : "(none)");
              else if (!both_null_or_matching (info->role, window->role))
                meta_topic (META_DEBUG_SM, "Window %s has role %s doesn't match saved role %s, no match\n",
                            window->desc,
                            window->role ? window->role : "(none)",
                            info->role ? info->role : "(none)");
              else
                meta_topic (META_DEBUG_SM, "???? should not happen - window %s doesn't match saved state %s for no good reason\n",
                            window->desc, info->id);
            }
        }
      
      tmp = tmp->next;
    }

  return retval;
}

static const MetaWindowSessionInfo*
find_best_match (GSList     *infos,
                 MetaWindow *window)
{
  GSList *tmp;
  const MetaWindowSessionInfo *matching_title;
  const MetaWindowSessionInfo *matching_type;
  
  matching_title = NULL;
  matching_type = NULL;
  
  tmp = infos;
  while (tmp != NULL)
    {
      MetaWindowSessionInfo *info;

      info = tmp->data;

      if (matching_title == NULL &&
          both_null_or_matching (info->title, window->title))
        matching_title = info;

      if (matching_type == NULL &&
          info->type == window->type)
        matching_type = info;
      
      tmp = tmp->next;
    }

  /* Prefer same title, then same type of window, then
   * just pick something. Eventually we could enhance this
   * to e.g. break ties by geometry hint similarity,
   * or other window features.
   */
  
  if (matching_title)
    return matching_title;
  else if (matching_type)
    return matching_type;
  else
    return infos->data;
}

const MetaWindowSessionInfo*
meta_window_lookup_saved_state (MetaWindow *window)
{
  GSList *possibles;
  const MetaWindowSessionInfo *info;
  
  /* Window is not session managed.
   * I haven't yet figured out how to deal with these
   * in a way that doesn't cause broken side effects in
   * situations other than on session restore.
   */
  if (window->sm_client_id == NULL)
    {
      meta_topic (META_DEBUG_SM,
                  "Window %s is not session managed, not checking for saved state\n",
                  window->desc);
      return NULL;
    }

  possibles = get_possible_matches (window);

  if (possibles == NULL)
    {
      meta_topic (META_DEBUG_SM, "Window %s has no possible matches in the list of saved window states\n",
                  window->desc);
      return NULL;
    }

  info = find_best_match (possibles, window);
  
  g_slist_free (possibles);
  
  return info;
}

void
meta_window_release_saved_state (const MetaWindowSessionInfo *info)
{
  /* We don't want to use the same saved state again for another
   * window.
   */
  window_info_list = g_slist_remove (window_info_list, info);

  session_info_free ((MetaWindowSessionInfo*) info);
}

static void
session_info_free (MetaWindowSessionInfo *info)
{
  g_free (info->id);
  g_free (info->res_class);
  g_free (info->res_name);
  g_free (info->title);
  g_free (info->role);

  g_slist_free (info->workspace_indices);
  
  g_free (info);
}

static MetaWindowSessionInfo*
session_info_new (void)
{
  MetaWindowSessionInfo *info;

  info = g_new0 (MetaWindowSessionInfo, 1);

  info->type = META_WINDOW_NORMAL;
  info->gravity = NorthWestGravity;
  
  return info;
}

static char* full_save_path = NULL;

static void
regenerate_save_file (void)
{
  g_free (full_save_path);

  if (client_id)
    full_save_path = g_strconcat (g_get_user_config_dir (),
                                  G_DIR_SEPARATOR_S "metacity"
                                  G_DIR_SEPARATOR_S "sessions" G_DIR_SEPARATOR_S,
                                  client_id,
                                  ".ms",
                                  NULL);
  else
    full_save_path = NULL;
}

static const char*
full_save_file (void)
{
  return full_save_path;
}

static int
windows_cmp_by_title (MetaWindow *a,
                      MetaWindow *b)
{
  return g_utf8_collate (a->title, b->title);
}

typedef struct
{
  int child_pid;
  int child_pipe;
  gboolean shutdown;
} LameClientsDialogData;

static void
finish_interact (gboolean shutdown)
{
  if (current_state == STATE_DONE_WITH_INTERACT) /* paranoia */
    {
      SmcInteractDone (session_connection, False /* don't cancel logout */);
      
      save_yourself_possibly_done (shutdown, TRUE);
    }
}

static gboolean  
io_from_warning_dialog (GIOChannel   *channel,
                        GIOCondition  condition,
                        gpointer      data)
{
  LameClientsDialogData *d;

  d = data;
  
  meta_topic (META_DEBUG_PING,
              "IO handler from lame clients dialog, condition = %x\n",
              condition);
  
  if (condition & (G_IO_HUP | G_IO_NVAL | G_IO_ERR))
    {
      finish_interact (d->shutdown);

      /* Remove the callback, freeing data */
      return FALSE; 
    }
  else if (condition & G_IO_IN)
    {
      /* Check for EOF */
      
      char buf[16];
      int ret;
 
      ret = read (d->child_pipe, buf, sizeof (buf));
      if (ret == 0)
 	{
 	  finish_interact (d->shutdown);
 	  return FALSE;
 	}
    }

  /* Keep callback installed */
  return TRUE;
}

static void
warn_about_lame_clients_and_finish_interact (gboolean shutdown)
{
  GSList *lame;
  GSList *windows;
  char **argv;
  int i;
  GSList *tmp;
  int len;
  int child_pid;
  int child_pipe;
  GError *err;
  GIOChannel *channel;
  LameClientsDialogData *d;
  guint32 timestamp;
  char timestampbuf[32];
  
  lame = NULL;
  windows = meta_display_list_windows (meta_get_display ());
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *window;
          
      window = tmp->data;

      /* only complain about normal windows, the others
       * are kind of dumb to worry about
       */
      if (window->sm_client_id == NULL &&
          window->type == META_WINDOW_NORMAL)
        lame = g_slist_prepend (lame, window);
          
      tmp = tmp->next;
    }
      
  g_slist_free (windows);
  
  if (lame == NULL)
    {
      /* No lame apps. */
      finish_interact (shutdown);
      return;
    }
  
  lame = g_slist_sort (lame, (GCompareFunc) windows_cmp_by_title);

  timestamp = meta_display_get_current_time_roundtrip (meta_get_display ());
  sprintf (timestampbuf, "%u", timestamp);

  len = g_slist_length (lame);
  len *= 2; /* titles and also classes */
  len += 2; /* --timestamp flag and actual timestamp */
  len += 1; /* NULL term */
  len += 2; /* metacity-dialog command and option */
  
  argv = g_new0 (char*, len);
  
  i = 0;

  argv[i] = METACITY_LIBEXECDIR"/metacity-dialog";
  ++i;
  argv[i] = "--timestamp";
  ++i;
  argv[i] = timestampbuf;
  ++i;
  argv[i] = "--warn-about-no-sm-support";
  ++i;
  
  tmp = lame;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;

      argv[i] = w->title;
      ++i;
      argv[i] = w->res_class ? w->res_class : "";
      ++i;

      tmp = tmp->next;
    }

  child_pipe = -1;
  child_pid = -1;
  err = NULL;
  if (!g_spawn_async_with_pipes ("/",
                                 argv,
                                 NULL,
                                 0,
                                 NULL, NULL,
                                 &child_pid,
                                 NULL,
                                 &child_pipe,
                                 NULL,
                                 &err))
    {
      meta_warning (_("Error launching metacity-dialog to warn about apps that don't support session management: %s\n"),
                    err->message);
      g_error_free (err);
    }

  g_free (argv);
  g_slist_free (lame);

  d = g_new0 (LameClientsDialogData, 1);
  d->child_pipe = child_pipe;
  d->child_pid = child_pid;
  d->shutdown = shutdown;
  
  channel = g_io_channel_unix_new (d->child_pipe);
  g_io_add_watch_full (channel, G_PRIORITY_DEFAULT,
                       G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
                       io_from_warning_dialog,
                       d, g_free);
  g_io_channel_unref (channel);
}

#endif /* HAVE_SM */
