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

struct _MsmServer
{
  GList *clients;
  IceAuthDataEntry *auth_entries;
  int n_auth_entries;
};

static Status register_client_callback              (SmsConn         smsConn,
                                                     SmPointer       managerData,
                                                     char           *previousId);
static void   interact_request_callback             (SmsConn         smsConn,
                                                     SmPointer       managerData,
                                                     int             dialogType);
static void   interact_done_callback                (SmsConn         smsConn,
                                                     SmPointer       managerData,
                                                     Bool            cancelShutdown);
static void   save_yourself_request_callback        (SmsConn         smsConn,
                                                     SmPointer       managerData,
                                                     int             saveType,
                                                     Bool            shutdown,
                                                     int             interactStyle,
                                                     Bool            fast,
                                                     Bool            global);
static void   save_yourself_phase2_request_callback (SmsConn         smsConn,
                                                     SmPointer       managerData);
static void   save_yourself_done_callback           (SmsConn         smsConn,
                                                     SmPointer       managerData,
                                                     Bool            success);
static void   close_connection_callback             (SmsConn         smsConn,
                                                     SmPointer       managerData,
                                                     int             count,
                                                     char          **reasonMsgs);
static Status new_client_callback                   (SmsConn         smsConn,
                                                     SmPointer       managerData,
                                                     unsigned long  *maskRet,
                                                     SmsCallbacks   *callbacksRet,
                                                     char          **failureReasonRet);
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
  
  if (!SmsInitialize (PACKAGE, VERSION,
                      new_client_callback,
                      server,
                      host_auth_callback,
                      sizeof (errbuf), errbuf))
    msm_fatal (_("Could not initialize SMS: %s\n"), errbuf);
  
  ice_init ();

  
}

void
msm_server_free (MsmServer *server)
{

  free_auth_entries (server->auth_entries, server->n_auth_entries);

  g_free (server);
}

static Status
register_client_callback              (SmsConn         smsConn,
                                       SmPointer       managerData,
                                       char           *previousId)
{
}

static void
interact_request_callback             (SmsConn         smsConn,
                                       SmPointer       managerData,
                                       int             dialogType)
{
}

static void
interact_done_callback                (SmsConn         smsConn,
                                       SmPointer       managerData,
                                       Bool            cancelShutdown)
{
}

static void
save_yourself_request_callback        (SmsConn         smsConn,
                                       SmPointer       managerData,
                                       int             saveType,
                                       Bool            shutdown,
                                       int             interactStyle,
                                       Bool            fast,
                                       Bool            global)
{
}

static void
save_yourself_phase2_request_callback (SmsConn         smsConn,
                                       SmPointer       managerData)
{
}

static void
save_yourself_done_callback           (SmsConn         smsConn,
                                       SmPointer       managerData,
                                       Bool            success)
{
}

static void
close_connection_callback             (SmsConn         smsConn,
                                       SmPointer       managerData,
                                       int             count,
                                       char          **reasonMsgs)
{
}

static Status
new_client_callback                   (SmsConn         smsConn,
                                       SmPointer       managerData,
                                       unsigned long  *maskRet,
                                       SmsCallbacks   *callbacksRet,
                                       char          **failureReasonRet)
{
}

static Bool
host_auth_callback (char *hostname)
{

  /* not authorized */
  return False;
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

