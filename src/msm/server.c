/* msm server object */

/* 
 * Copyright (C) 2001 Havoc Pennington
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
 *
 * Some code in here from xsm:
 *
 * Copyright 1993, 1998  The Open Group
 * 
 * All Rights Reserved.
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the name of The Open Group
 * shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written
 * authorization from The Open Group.
 */

#include "server.h"

/* FIXME we need to time out anytime we're waiting for a client
 * response, such as InteractDone, SaveYourselfDone, ConnectionClosed
 * (after sending Die)
 */

struct _MsmServer
{
  GList *clients;
  IceAuthDataEntry *auth_entries;
  int n_auth_entries;
  MsmClient *currently_interacting;
  GList     *interact_pending;
  
  guint in_shutdown : 1;
  guint save_allows_interaction : 1;
};

static Status register_client_callback              (SmsConn         cnxn,
                                                     SmPointer       manager_data,
                                                     char           *previous_id);
static void   interact_request_callback             (SmsConn         cnxn,
                                                     SmPointer       manager_data,
                                                     int             dialog_type);
static void   interact_done_callback                (SmsConn         cnxn,
                                                     SmPointer       manager_data,
                                                     Bool            cancel_shutdown);
static void   save_yourself_request_callback        (SmsConn         cnxn,
                                                     SmPointer       manager_data,
                                                     int             save_type,
                                                     Bool            shutdown,
                                                     int             interact_style,
                                                     Bool            fast,
                                                     Bool            global);
static void   save_yourself_phase2_request_callback (SmsConn         cnxn,
                                                     SmPointer       manager_data);
static void   save_yourself_done_callback           (SmsConn         cnxn,
                                                     SmPointer       manager_data,
                                                     Bool            success);
static void   close_connection_callback             (SmsConn         cnxn,
                                                     SmPointer       manager_data,
                                                     int             count,
                                                     char          **reasonMsgs);
static void set_properties_callback    (SmsConn     cnxn,
                                        SmPointer   manager_data,
                                        int         numProps,
                                        SmProp    **props);
static void delete_properties_callback (SmsConn     cnxn,
                                        SmPointer   manager_data,
                                        int         numProps,
                                        char      **propNames);
static void get_properties_callback    (SmsConn     cnxn,
                                        SmPointer   manager_data);


static Status new_client_callback                   (SmsConn         cnxn,
                                                     SmPointer       manager_data,
                                                     unsigned long  *maskRet,
                                                     SmsCallbacks   *callbacksRet,
                                                     char          **failure_reason_ret);
static Bool   host_auth_callback                    (char           *hostname);


static void ice_init (MsmServer *server);

static gboolean create_auth_entries (MsmServer       *server,
                                     IceListenObject *listen_objs,
                                     int              n_listen_objs);
static void free_auth_entries       (IceAuthDataEntry *entries);

MsmServer*
msm_server_new (void)
{
  char errbuf[256];
  MsmServer *server;

  server = g_new (MsmServer, 1);

  server->clients = NULL;
  server->auth_entries = NULL;
  server->n_auth_entries = 0;
  server->currently_interacting = NULL;
  server->interact_pending = NULL;
  server->in_shutdown = FALSE;
  server->save_allows_interaction = FALSE;
  
  if (!SmsInitialize (PACKAGE, VERSION,
                      new_client_callback,
                      server,
                      host_auth_callback,
                      sizeof (errbuf), errbuf))
    msm_fatal (_("Could not initialize SMS: %s\n"), errbuf);
  
  ice_init (server);

  return server;
}

void
msm_server_free (MsmServer *server)
{
  g_list_free (server->clients);
  g_list_free (server->interact_pending);
  
  free_auth_entries (server->auth_entries, server->n_auth_entries);

  g_free (server);
}


void
msm_server_drop_client (MsmServer *server,
                        MsmClient *client)
{
  server->clients = g_list_remove (server->clients, client);

  if (server->currently_interacting == client)
    msm_server_next_pending_interaction (server);

  msm_client_free (client);

  msm_server_consider_phase_change (server);

  /* We can quit after all clients have been dropped. */
  if (server->in_shutdown &&
      server->clients == NULL)
    msm_quit ();
}

void
msm_server_next_pending_interaction (MsmServer *server)
{
  server->currently_interacting = NULL;
  if (server->interact_pending)
    {
      /* Start up the next interaction */
      server->currently_interacting = server->interact_pending->data;
      server->interact_pending =
        g_list_remove (server->interact_pending,
                       server->currently_interacting);
      msm_client_begin_interact (server->currently_interacting);
    }
}

static Status
register_client_callback              (SmsConn         cnxn,
                                       SmPointer       manager_data,
                                       char           *previous_id)
{
  /* This callback should:
   *  a) if previous_id is NULL, this is a new client; register
   *     it and return TRUE
   *  b) if previous_id is non-NULL and is an ID we know about,
   *     register client and return TRUE
   *  c) if previous_id is non-NULL and we've never heard of it,
   *     return FALSE
   *
   *  Whenever previous_id is non-NULL we need to free() it.
   *  (What an incredibly broken interface...)
   */

  MsmClient *client;

  client = manager_data;

  if (previous_id == NULL)
    {
      char *id;

      id = SmsGenerateClientID (msm_client_get_connection (client));

      msm_client_register (client, id);

      free (id);

      /* FIXME ksm and gnome-session send a SaveYourself to the client
       * here. I don't understand why though.
       */
      
      return TRUE;
    }
  else
    {
      /* FIXME check for pending/known client IDs and register the client,
       * return TRUE if we know about this previous_id
       */

      free (previous_id);
      return FALSE;
    }
}

static void
interact_request_callback (SmsConn         cnxn,
                           SmPointer       manager_data,
                           int             dialog_type)
{
  MsmClient *client;
  MsmServer *server;
  
  client = manager_data;
  server = msm_client_get_server (client);
  
  if (!server->save_allows_interaction)
    {
      msm_warning (_("Client '%s' requested interaction, but interaction is not allowed right now.\n"),
                   msm_client_get_description (client));

      return;
    }
  
  msm_client_interact_request (client);
}

static void
interact_done_callback (SmsConn         cnxn,
                        SmPointer       manager_data,
                        Bool            cancel_shutdown)
{
  MsmClient *client;
  MsmServer *server;

  client = manager_data;
  server = msm_client_get_server (client);
  
  if (cancel_shutdown &&
      server->in_shutdown &&
      server->save_allows_interaction)
    {
      msm_server_cancel_shutdown (server);
    }
  else
    {
      if (server->currently_interacting == client)
        {
          msm_server_next_pending_interaction (server);
        }
      else
        {
          msm_warning (_("Received InteractDone from client '%s' which should not be interacting right now\n"),
                       msm_client_get_description (client));
        }
    }
}

static void
save_yourself_request_callback (SmsConn         cnxn,
                                SmPointer       manager_data,
                                int             save_type,
                                Bool            shutdown,
                                int             interact_style,
                                Bool            fast,
                                Bool            global)
{
  /* The spec says we "may" honor this exactly as requested;
   * we decide not to, because some of the fields are stupid
   * and/or useless
   */
  MsmClient *client;
  MsmServer *server;

  client = manager_data;
  server = msm_client_get_server (client);
  
  if (global)
    {
      msm_server_save_all (server,
                           interact_style != SmInteractStyleNone,
                           shutdown != FALSE);
    }
  else
    {
      if (msm_client_get_state (client) == MSM_CLIENT_STATE_IDLE)
        msm_client_save (client,
                         interact_style != SmInteractStyleNone,
                         shutdown != FALSE);
      else
        msm_warning (_("Client '%s' requested save, but is not currently in the idle state\n"),
                     msm_client_get_description (client));
    }
}

static void
save_yourself_phase2_request_callback (SmsConn         cnxn,
                                       SmPointer       manager_data)
{
  MsmClient *client;
  MsmServer *server;

  client = manager_data;
  server = msm_client_get_server (client);

  msm_client_phase2_request (client);
}

static void
save_yourself_done_callback           (SmsConn         cnxn,
                                       SmPointer       manager_data,
                                       Bool            success)
{
  MsmClient *client;
  MsmServer *server;

  client = manager_data;
  server = msm_client_get_server (client);

  msm_client_save_confirmed (client, success != FALSE);
  
  msm_server_consider_phase_change (server);
}

static void
close_connection_callback             (SmsConn         cnxn,
                                       SmPointer       manager_data,
                                       int             count,
                                       char          **reasonMsgs)
{
  MsmClient *client;
  MsmServer *server;

  client = manager_data;
  server = msm_client_get_server (client);

  msm_server_drop_client (server, client);

  /* I'm assuming these messages would be on crack, and therefore not
   * displaying them.
   */
  SmFreeReasons (count, reasonMsgs);
}

static void
set_properties_callback (SmsConn     cnxn,
                         SmPointer   manager_data,
                         int         numProps,
                         SmProp    **props)
{
  int i;
  MsmClient *client;
  MsmServer *server;

  client = manager_data;
  server = msm_client_get_server (client);
  
  i = 0;
  while (i < numProps)
    {
      msm_client_set_property (client, props[i]);

      SmFreeProperty (props[i]);

      ++i;
    }

  free (props);
}

static void
delete_properties_callback (SmsConn     cnxn,
                            SmPointer   manager_data,
                            int         numProps,
                            char      **propNames)
{
  int i;
  MsmClient *client;
  MsmServer *server;

  client = manager_data;
  server = msm_client_get_server (client);
  
  i = 0;
  while (i < numProps)
    {
      msm_client_unset_property (propNames[i]);

      ++i;
    }
}

static void
get_properties_callback (SmsConn     cnxn,
                         SmPointer   manager_data)
{
  MsmClient *client;
  MsmServer *server;

  client = manager_data;
  server = msm_client_get_server (client);

  msm_client_send_properties (client);
}

static Status
new_client_callback                   (SmsConn         cnxn,
                                       SmPointer       manager_data,
                                       unsigned long  *maskRet,
                                       SmsCallbacks   *callbacksRet,
                                       char          **failure_reason_ret)
{
  MsmClient *client;
  MsmServer *server;

  server = manager_data;

  
  /* If we want to disallow the new client, here we fill
   * failure_reason_ret with a malloc'd string and return FALSE
   */
  if (server->in_shutdown)
    {
      /* have to use malloc() */
      *failure_reason_ret = malloc (256);
      g_strncpy (*failure_reason_ret, _("Refusing new client connection because the session is currently being shut down\n"), 255);
      (*failure_reason_ret)[255] = '\0'; /* paranoia */
      return FALSE;
    }
  
  client = msm_client_new (server, cnxn);
  server->clients = g_list_prepend (server->clients, client);
  
  *maskRet = 0;
  
  *maskRet |= SmsRegisterClientProcMask;
  callbacksRet->register_client.callback = register_client_callback;
  callbacksRet->register_client.manager_data  = client;

  *maskRet |= SmsInteractRequestProcMask;
  callbacksRet->interact_request.callback = interact_request_callback;
  callbacksRet->interact_request.manager_data = client;

  *maskRet |= SmsInteractDoneProcMask;
  callbacksRet->interact_done.callback = interact_done_callback;
  callbacksRet->interact_done.manager_data = client;

  *maskRet |= SmsSaveYourselfRequestProcMask;
  callbacksRet->save_yourself_request.callback = save_yourself_request_callback;
  callbacksRet->save_yourself_request.manager_data = client;

  *maskRet |= SmsSaveYourselfP2RequestProcMask;
  callbacksRet->save_yourself_phase2_request.callback = save_yourself_phase2_request_callback;
  callbacksRet->save_yourself_phase2_request.manager_data = client;

  *maskRet |= SmsSaveYourselfDoneProcMask;
  callbacksRet->save_yourself_done.callback = save_yourself_done_callback;
  callbacksRet->save_yourself_done.manager_data = client;

  *maskRet |= SmsCloseConnectionProcMask;
  callbacksRet->close_connection.callback = close_connection_callback;
  callbacksRet->close_connection.manager_data  = client;

  *maskRet |= SmsSetPropertiesProcMask;
  callbacksRet->set_properties.callback = set_properties_callback;
  callbacksRet->set_properties.manager_data = client;

  *maskRet |= SmsDeletePropertiesProcMask;
  callbacksRet->delete_properties.callback = delete_properties_callback;
  callbacksRet->delete_properties.manager_data   = client;

  *maskRet |= SmsGetPropertiesProcMask;
  callbacksRet->get_properties.callback	= get_properties_callback;
  callbacksRet->get_properties.manager_data   = client;

  return TRUE;
}

static Bool
host_auth_callback (char *hostname)
{

  /* not authorized */
  return False;
}

void
msm_server_queue_interaction (MsmServer *server,
                              MsmClient *client)
{
  if (server->currently_interacting == client ||
      g_list_find (server->interact_pending, client) != NULL)
    return;   /* Already queued */  

  server->interact_pending = g_list_prepend (server->interact_pending,
                                             client);

  msm_server_next_pending_interaction (server);
}

void
msm_server_save_all (MsmServer *server,
                     gboolean   allow_interaction,
                     gboolean   shut_down)
{
  GList *tmp;

  if (shut_down) /* never cancel a shutdown here */
    server->in_shutdown = TRUE;

  /* We just assume the most recent request for interaction or no is
   * correct
   */
  server->save_allows_interaction = allow_interaction;
  
  tmp = server->clients;
  while (tmp != NULL)
    {
      MsmClient *client;

      client = tmp->data;
      
      if (msm_client_get_state (client) == MSM_CLIENT_STATE_IDLE)
        msm_client_save (client,
                         server->save_allows_interaction,
                         server->in_shutdown);
      
      tmp = tmp->next;
    }
}

void
msm_server_cancel_shutdown (MsmServer *server)
{
  GList *tmp;
  
  if (!server->in_shutdown)
    return;
  
  server->in_shutdown = FALSE;

  /* Cancel any interactions in progress */
  g_list_free (server->interact_pending);
  server->interact_pending = NULL;
  server->currently_interacting = NULL;
  
  tmp = server->clients;
  while (tmp != NULL)
    {
      MsmClient *client;

      client = tmp->data;
      
      if (msm_client_get_state (client) == MSM_CLIENT_STATE_SAVING)
        msm_client_shutdown_cancelled (client);
      
      tmp = tmp->next;
    }
}

/* Think about whether to move to phase 2, return to idle state,
 * or shut down
 */
void
msm_server_consider_phase_change (MsmServer *server)
{
  GList *tmp;
  gboolean some_phase1;
  gboolean some_phase2;
  gboolean some_phase2_requested;
  gboolean some_alive;
  
  some_phase1 = FALSE;
  some_phase2 = FALSE;
  some_phase2_requested = FALSE;
  some_alive = FALSE;
  
  tmp = server->clients;
  while (tmp != NULL)
    {
      MsmClient *client;

      client = tmp->data;
      
      switch (msm_client_get_state (client))
        {
        case MSM_CLIENT_STATE_SAVING:
          some_phase1 = TRUE;
          break;
        case MSM_CLIENT_STATE_SAVING_PHASE2:
          some_phase2 = TRUE;
          break;
        case MSM_CLIENT_STATE_PHASE2_REQUESTED:
          some_phase2_requested = TRUE;
          break;
        default:
          break;
        }

      if (msm_client_get_state (client) != MSM_CLIENT_STATE_DEAD)
        some_alive = TRUE;
      
      tmp = tmp->next;
    }
  
  if (some_phase1)
    return; /* still saving phase 1 */

  if (some_phase2)
    return; /* we are in phase 2 */

  if (some_phase2_requested)
    {
      tmp = server->clients;
      while (tmp != NULL)
        {
          MsmClient *client;
          
          client = tmp->data;
          
          if (msm_client_get_state (client) == MSM_CLIENT_STATE_PHASE2_REQUESTED)
            msm_client_save_phase2 (client);
          
          tmp = tmp->next;
        }
      
      return;
    }

  if (server->in_shutdown)
    {
      /* We are shutting down, and all clients are in the idle state.
       * Tell all clients to die. When they all close their connections,
       * we can exit.
       */

      if (some_alive)
        {
          tmp = server->clients;
          while (tmp != NULL)
            {
              MsmClient *client = tmp->data;
              
              if (msm_client_get_state (client) != MSM_CLIENT_STATE_DEAD)
                msm_client_die (client);
              
              tmp = tmp->next;
            }
        }
    }
  else
    {
      /* Send SaveComplete to all clients that are finished saving */
      GList *tmp;
      
      tmp = server->clients;
      while (tmp != NULL)
        {
          MsmClient *client = tmp->data;

          switch (msm_client_get_state (client))
            {
            case MSM_CLIENT_STATE_SAVE_DONE:
            case MSM_CLIENT_STATE_SAVE_FAILED:
              msm_client_save_complete (client);
              break;
            default:
              break;
            }
          
          tmp = tmp->next;
        }
    }
}

void
msm_server_foreach_client (MsmServer *server,
                           MsmClientFunc func)
{
  GList *tmp;
  
  tmp = server->clients;
  while (tmp != NULL)
    {
      MsmClient *client = tmp->data;

      (* func) (client);

      tmp = tmp->next;
    }
}


/*
 * ICE utility code, cut-and-pasted from Metacity, and in turn
 * from libgnomeui, and also some merged in from gsm, and xsm,
 * and even ksm
 */

static void ice_io_error_handler (IceConn connection);

static void new_ice_connection (IceConn connection, IcePointer client_data, 
				Bool opening, IcePointer *watch_data);

static void setup_authentication (MsmServer *server,
                                  IceListenObject *listen_objs,
                                  int              n_listen_objs);

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

static gboolean
accept_connection (GIOChannel *channel,
                   GIOCondition condition,
                   gpointer client_data)
{
  IceListenObject *listen_obj;
  IceAcceptStatus status;
  IceConnectStatus cstatus;
  IceConn cnxn;
  
  listen_obj = client_data;

  cnxn = IceAcceptConnection (listen_obj,
                              &status);

  if (cnxn == NULL || status != IceAcceptSuccess)
    {
      msm_warning (_("Failed to accept new ICE connection\n"));
      return TRUE;
    }

  /* I believe this means we refuse to argue with clients over
   * whether we are going to shut their ass down. But I could
   * be wrong.
   */
  IceSetShutdownNegotiation (cnxn, False);

  /* FIXME This is a busy wait, I believe. The libSM docs say we need
   * to select on all the ICE file descriptors. We could do that by
   * reentering the main loop as gnome-session does; but that would
   * complicate everything.  So for now, copying ksm and doing it this
   * way.
   *
   * If this causes problems, we can try adding a g_main_iteration()
   * in here.
   *
   * FIXME time this out eventually
   */
  cstatus = IceConnectionStatus (cnxn);
  while (cstatus == IceConnectPending)
    {
      IceProcessMessages (cnxn, NULL, NULL);
      cstatus = IceConnectionStatus (cnxn);
    }

  if (cstatus != IceConnectAccepted)
    {
      if (cstatus == IceConnectIOError)
        msm_warning (_("IO error trying to accept new connection (client may have crashed trying to connect to the session manager, or client may be broken, or someone yanked the ethernet cable)"));
      else
        msm_warning (_("Rejecting new connection (some client was not allowed to connect to the session manager)"));

      IceCloseConnection (cnxn);
    }

  return TRUE;
}

/* We call any handler installed before (or after) ice_init but 
 * avoid calling the default libICE handler which does an exit()
 */
static void
ice_io_error_handler (IceConn connection)
{
  IceCloseConnection (connection);
}    

static void
ice_init (MsmServer *server)
{
  static gboolean ice_initted = FALSE;

  if (! ice_initted)
    {
      int saved_umask;
      int n_listen_objs;
      IceListenObj *listen_objs;
      char errbuf[256];
      char *ids;
      char *p;
      int i;

      IceSetIOErrorHandler (ice_io_error_handler);

      IceAddConnectionWatch (new_ice_connection, NULL);

      /* Some versions of IceListenForConnections have a bug which causes
       * the umask to be set to 0 on certain types of failures.  So we
       * work around this by saving and restoring the umask.
       */
      saved_umask = umask (0);
      umask (saved_umask);

      if (!IceListenForConnections (&n_listen_objs,
                                    &listen_objs,
                                    sizeof (errbuf),
                                    errbuf))
        msm_fatal (_("Could not initialize ICE: %s\n"), errbuf);

      /* See above.  */
      umask (saved_umask);

      i = 0;
      while (i < n_listen_objs)
        {
          GIOChannel *channel;
          
          channel = g_io_channel_unix_new (IceGetListenConnectionNumber (listen_objs[i]));
          
          g_io_add_watch (channel, G_IO_IN,
                          accept_connection,
                          &listen_objs[i]);

          g_io_channel_unref (channel);
          
          ++i;
        }

      if (!create_auth_entries (server, listen_objs, n_listen_objs))
        {
          meta_fatal (_("Could not set up authentication"));
          return;
        }
      
      ids = IceComposeNetworkIdList (n_listen_objs, listen_objs);
      
      p = g_strconcat ("SESSION_MANAGER=", ids, NULL);
      putenv (p);

      /* example code I can find doesn't free "ids", and we don't free "p"
       * since putenv is lame
       */
      
      ice_initted = TRUE;
    }
}

/* The idea here is to create a temporary file with an iceauth script
 * in it, and run that through iceauth. At the same time,
 * we generate a temporary file to use for removing the auth entries, and
 * call that on exit. The code here is from xsm and every other session
 * manager has cut-and-pasted it.
 */
static char *add_file = NULL;
static char *remove_file = NULL;

static gboolean
run_iceauth_script (const char *filename)
{
  char *argv[4];
  GError *err;
  int status;

  argv[0] = "iceauth";
  argv[1] = "source";
  argv[2] = filename;
  argv[3] = NULL;
  
  err = NULL;
  status = 1;
  if (!g_spawn_sync (NULL, argv, NULL,
                     G_SPAWN_SEARCH_PATH,
                     NULL, NULL,
                     NULL, NULL,
                     &status,
                     &err) ||
      status != 0)
    {
      msm_warning (_("Failed to run iceauth script %s: %s\n"),
                   filename,
                   err ? err->message : _("iceauth returned nonzero status"));
      if (err)
        g_error_free (err);

      return FALSE;
    }

  return TRUE;
}

static gboolean
create_auth_entries (MsmServer       *server,
                     IceListenObject *listen_objs,
                     int              n_listen_objs)
{
  FILE *addfp = NULL;
  FILE *removefp = NULL;
  const char *path;
  int original_umask;
  int i;
  int fd = -1;
  char *tmpl = NULL;
  GError *err;
  IceAuthDataEntry *entries;
  
  original_umask = umask (0077); /* disallow non-owner access */

  path = g_getenv ("SM_SAVE_DIR");
  if (!path)
    {
      path = g_get_home_dir ();
      if (!path)
        path = ".";
    }

  err = NULL;
  tmpl = g_strconcat (path, "/msm-add-commands-XXXXXX", NULL);
  fd = g_file_open_tmp (tmpl, &add_file, &err);
  if (err)
    {
      msm_fatal (_("Could not create ICE authentication script: %s\n"),
                 err->message);
      g_assert_not_reached ();
      return;
    }
  
  addfp = fdopen (fd, "w");
  if (addfp == NULL)
    goto bad;

  g_free (tmpl);
  tmpl = NULL;
  

  err = NULL;
  tmpl = g_strconcat (path, "/msm-remove-commands-XXXXXX", NULL);
  fd = g_file_open_tmp (tmpl, &remove_file, &err);
  if (err)
    {
      msm_fatal (_("Could not create ICE authentication script: %s\n"),
                 err->message);
      g_assert_not_reached ();
      return;
    }
  
  removefp = fdopen (fd, "w");
  if (removefp == NULL)
    goto bad;

  g_free (tmpl);
  tmpl = NULL;

  server->n_auth_entries = n_listen_objs * 2;
  server->auth_entries = g_new (IceAuthDataEntry, server->n_auth_entries);
  entries = server->auth_entries;
  
  for (i = 0; i <  server->n_auth_entries; i += 2)
    {
      entries[i].network_id =
        IceGetListenConnectionString (listen_objs[i/2]);
      entries[i].protocol_name = "ICE";
      entries[i].auth_name = "MIT-MAGIC-COOKIE-1";

      entries[i].auth_data =
        IceGenerateMagicCookie (MAGIC_COOKIE_LEN);
      entries[i].auth_data_length = MAGIC_COOKIE_LEN;

      entries[i+1].network_id =
        IceGetListenConnectionString (listen_objs[i/2]);
      entries[i+1].protocol_name = "XSMP";
      entries[i+1].auth_name = "MIT-MAGIC-COOKIE-1";

      entries[i+1].auth_data = 
        IceGenerateMagicCookie (MAGIC_COOKIE_LEN);
      entries[i+1].auth_data_length = MAGIC_COOKIE_LEN;

      write_iceauth (addfp, removefp, &entries[i]);
      write_iceauth (addfp, removefp, &entries[i+1]);

      IceSetPaAuthData (2, &entries[i]);

      IceSetHostBasedAuthProc (listen_objs[i/2], host_auth_callback);
    }

  fclose (addfp);
  fclose (removefp);

  umask (original_umask);

  if (!run_iceauth_script (add_file))
    return FALSE;
  
  unlink (add_file);

  return TRUE;

 bad:

  if (tmpl)
  g_free (tmpl);
  
  if (addfp)
    fclose (addfp);

  if (removefp)
    fclose (removefp);

  if (addAuthFile)
    {
      unlink (addAuthFile);
      free (addAuthFile);
    }
  if (remAuthFile)
    {
      unlink (remAuthFile);
      free (remAuthFile);
    }

  return FALSE;  
}

static void
free_auth_entries (IceAuthDataEntry *entries,
                   int               n_entries)
{
  int i;

  for (i = 0; i < n_entries; i++)
    {
      g_free (entries[i].network_id);
      g_free (entries[i].auth_data);
    }

  g_free (entries);

  run_iceauth_script (remove_file);
  
  unlink (remove_file);

  g_free (add_file);
  g_free (remove_file);

  add_file = NULL;
  remove_file = NULL;
}

