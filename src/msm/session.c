/* msm session */

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
 */

#include "session.h"

typedef struct _MsmSavedClient MsmSavedClient;

struct _MsmSavedClient
{
  char **restart_command;

};

struct _MsmSession
{
  char *name;
  GList *clients;
  char *filename;
  char *full_filename;
  int lock_fd;
};

typedef enum
{
  MSM_SESSION_FAILURE_OPENING_FILE,
  MSM_SESSION_FAILURE_LOCKING,
  MSM_SESSION_FAILURE_BAD_FILE,
  MSM_SESSION_FAILURE_EMPTY
} MsmSessionFailureReason;

static GHashTable *sessions = NULL;

static MsmSession* recover_failed_session (MsmSession              *session,
                                           MsmSessionFailureReason  reason,
                                           const char              *details);

static gboolean    parse_session_file     (MsmSession *session,
                                           GError    **error);

void
msm_session_update_client (MsmSession  *session,
                           MsmClient   *client)
{
  

}

void
msm_session_remove_client (MsmSession  *session,
                           MsmClient   *client)
{


}

gboolean
msm_session_client_id_known (MsmSession *session,
                             const char *previous_id)
{
  

}

void
msm_session_launch (MsmSession  *session)
{


}

static const char*
session_dir (void)
{
  static char *dir;

  if (dir == NULL)
    {
      dir = g_strconcat (msm_get_work_directory (),
                         "/sessions",
                         NULL);
    }
  
  return dir;
}

static void
set_close_on_exec (int fd)
{
  int val;

  val = fcntl (fd, F_GETFD, 0);
  if (val < 0)
    {
      gconf_log (GCL_DEBUG, "couldn't F_GETFD: %s\n", g_strerror (errno));
      return;
    }

  val |= FD_CLOEXEC;

  if (fcntl (fd, F_SETFD, val) < 0)
    gconf_log (GCL_DEBUG, "couldn't F_SETFD: %s\n", g_strerror (errno));
}

/* Your basic Stevens cut-and-paste */
static int
lock_reg (int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
  struct flock lock;

  lock.l_type = type; /* F_RDLCK, F_WRLCK, F_UNLCK */
  lock.l_start = offset; /* byte offset relative to whence */
  lock.l_whence = whence; /* SEEK_SET, SEEK_CUR, SEEK_END */
  lock.l_len = len; /* #bytes, 0 for eof */

  return fcntl (fd, cmd, &lock);
}

#define lock_entire_file(fd) \
  lock_reg ((fd), F_SETLK, F_WRLCK, 0, SEEK_SET, 0)
#define unlock_entire_file(fd) \
  lock_reg ((fd), F_SETLK, F_UNLCK, 0, SEEK_SET, 0)

static MsmSession*
msm_session_get_for_filename (const char *name,
                              const char *filename)
{
  MsmSession *session;
  int fd = -1;
  GError *dir_error = NULL;
  GError *err;
  gboolean use_global_file;
  
  session = g_hash_table_lookup (sessions, filename);
  if (session)
    return session;

  session = g_new0 (MsmSession, 1);
  session->name = g_strdup (name);
  session->clients = NULL;
  session->filename = g_strdup (filename);
  session->full_filename = g_strconcat (session_dir (), "/", filename, NULL);
  session->lock_fd = -1;

  dir_error = NULL;
  msm_create_dir_and_parents (session_dir (), &dir_error);
  /* We save dir_error for later; if creating the file fails,
   * we give dir_error in the reason.
   */
  
  /* To use a session, we need to lock the file in the user's
   * save dir (by default in .msm/sessions/).
   * 
   * If the file didn't previously exist, then we
   * init the session from the global session of the same name,
   * if any.
   *
   * This locking stuff has several races in it, and probably doesn't
   * work over NFS, and all that jazz, but avoiding the races
   * introduces stale lock issues, which are in practice more serious
   * for users than the usual issues one worries about when locking.
   */
  
  fd = open (session->full_filename, O_RDWR | O_CREAT | O_EXCL, 0700);
  
  if (fd < 0)
    {
      char *message;
      
      message = g_strdup_printf (_("Failed to open the session file '%s': %s (%s)"),
                                 session->full_filename,
                                 g_strerror (errno),
                                 dir_error ?
                                 dir_error->message :
                                 _("file's parent directory created successfully"));
                                 
      if (dir_error)
        g_error_free (dir_error);
      
      session = recover_failed_session (session,
                                        MSM_SESSION_FAILURE_OPENING_FILE,
                                        message);

      g_free (message);

      return session;
    }
  
  if (dir_error)
    {
      g_error_free (dir_error);
      dir_error = NULL;
    }

  if (lock_entire_file (fd) < 0)
    {
      char *message;

      close (fd);
      
      message = g_strdup_printf (_("Failed to lock the session file '%s': %s"),
                                 session->full_filename,
                                 g_strerror (errno));
      
      session = recover_failed_session (session,
                                        MSM_SESSION_FAILURE_LOCKING,
                                        message);

      g_free (message);
      
      return session;
    }

  session->lock_fd = fd;
  set_close_on_exec (fd);
  
  err = NULL;
  if (!parse_session_file (session, &err))
    {
      char *message;

      message = g_strdup_printf (_("Failed to parse the session file '%s': %s\n"),
                                 session->full_filename,
                                 err->message);

      g_error_free (err);
      
      session = recover_failed_session (session,
                                        MSM_SESSION_FAILURE_BAD_FILE,
                                        message);

      g_free (message);
      
      return session;
    }

  if (session->clients == NULL)
    {
      session = recover_failed_session (session,
                                        MSM_SESSION_FAILURE_EMPTY,
                                        _("Session doesn't contain any applications"));

      return session;
    }

  return session;
}

MsmSession*
msm_session_get (const char *name)
{  
  if (name == NULL)
    {
      return msm_session_get_for_filename (_("Default"), "Default.session");
    }
  else
    {
      char *filename;
      char *p;
      MsmSession *session;
      
      filename = g_strconcat (name, ".session", NULL);

      /* Remove path separators from the filename */
      p = filename;
      while (*p)
        {
          if (*p == '/')
            *p = '_';
          ++p;
        }

      session = msm_session_get_for_filename (name, filename);

      g_free (filename);

      return session;
    }
}

MsmSession*
msm_session_get_failsafe  (void)
{  
  return msm_session_get_for_filename (_("Failsafe"), "Failsafe.session");
}

void
msm_session_save (MsmSession  *session,
                  MsmServer   *server)
{
  

}

static void
recover_failed_session (MsmSession             *session,
                        MsmSessionFailureReason reason,
                        const char             *details)
{


}

static gboolean
parse_session_file (MsmSession *session,
                    GError    **error)
{
  char *parse_file;
  struct stat sb;
  gboolean file_empty;
  
  parse_file = NULL;
  file_empty = FALSE;
  
  /* If the file is empty, probably because we just created it or have
   * never saved our session, then parse the global session file
   * instead of the user session file for our initial state.
   */
  if (fstat (session->lock_fd, &sb) < 0)
    {
      /* Can't imagine this actually happening */
      msm_warning (_("Failed to stat new session file descriptor (%s)\n"),
                   g_strerror (errno));
    }
  else
    {
      if (sb.st_size == 0)
        file_empty = TRUE;
    }

  if (file_empty)
    parse_file = g_strconcat (MSM_PKGDATADIR, "/", session->filename, NULL);
  else
    parse_file = g_strdup (session->full_filename);

  /* FIXME do the parsing */
  
  g_free (parse_file);
  
  return TRUE;
}
