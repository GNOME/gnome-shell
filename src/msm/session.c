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
#include "util.h"
#include "props.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>

#include <gtk/gtk.h>

typedef struct _MsmSavedClient MsmSavedClient;

struct _MsmSavedClient
{
  char *id;
  GList *properties;
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

MsmSavedClient *saved_new  (void);
void            saved_free (MsmSavedClient *saved);


static MsmSession* recover_failed_session (MsmSession              *session,
                                           MsmSessionFailureReason  reason,
                                           const char              *details);

static gboolean    parse_session_file     (MsmSession *session,
                                           GError    **error);

static char* decode_text_from_utf8 (const char *text);
static char* encode_text_as_utf8   (const char *text);

void
msm_session_clear (MsmSession  *session)
{


}

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
  

  return FALSE;
}

void
msm_session_launch (MsmSession  *session)
{
  system ("xclock &");
}

MsmSavedClient*
saved_new (void)
{
  MsmSavedClient *saved;

  saved = g_new (MsmSavedClient, 1);

  saved->id = NULL;
  saved->properties = NULL;
  
  return saved;
}

void
saved_free (MsmSavedClient *saved)
{
  g_free (saved->id);
  
  g_free (saved);
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
      msm_warning ("couldn't F_GETFD: %s\n", g_strerror (errno));
      return;
    }

  val |= FD_CLOEXEC;

  if (fcntl (fd, F_SETFD, val) < 0)
    msm_warning ("couldn't F_SETFD: %s\n", g_strerror (errno));
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

  if (sessions)
    {
      session = g_hash_table_lookup (sessions, filename);
      if (session)
        return session;
    }

  session = g_new0 (MsmSession, 1);
  session->name = g_strdup (name);
  session->clients = NULL;
  session->filename = g_strdup (filename);
  session->full_filename = g_strconcat (session_dir (), "/", filename, NULL);
  session->lock_fd = -1;

  dir_error = NULL;
  msm_create_dir_and_parents (session_dir (), 0700, &dir_error);
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
  
  fd = open (session->full_filename, O_RDWR | O_CREAT, 0700);
  
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

  /* FIXME FALSE */
  if (FALSE && session->clients == NULL)
    {
      session = recover_failed_session (session,
                                        MSM_SESSION_FAILURE_EMPTY,
                                        NULL);

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

static void
write_proplist (FILE *fp,
                GList *properties)
{
  GList *tmp;

  tmp = properties;
  while (tmp != NULL)
    {
      SmProp *prop = tmp->data;
      char *name_encoded;
      char *type_encoded;
      
      name_encoded = encode_text_as_utf8 (prop->name);
      type_encoded = encode_text_as_utf8 (prop->type);

      fprintf (fp, "    <prop name=\"%s\" type=\"%s\">\n",
               name_encoded, type_encoded);

      g_free (name_encoded);
      g_free (type_encoded);

      if (strcmp (prop->type, SmCARD8) == 0)
        {
          int val = 0;
          smprop_get_card8 (prop, &val);
          fprintf (fp, "      <value>%d</value>\n", val);
        }
      else if (strcmp (prop->type, SmARRAY8) == 0)
        {
          char *str = NULL;
          char *encoded = NULL;
          smprop_get_string (prop, &str);
          if (str)
            encoded = encode_text_as_utf8 (str);
          if (encoded)
            fprintf (fp, "      <value>%s</value>\n", encoded);

          g_free (encoded);
          g_free (str);
        }
      else if (strcmp (prop->type, SmLISTofARRAY8) == 0)
        {
          char **vec;
          int vec_len;
          int i;
          
          vec = NULL;
          vec_len = 0;
          
          smprop_get_vector (prop, &vec_len, &vec);

          i = 0;
          while (i < vec_len)
            {
              char *encoded;

              encoded = encode_text_as_utf8 (vec[i]);
              
              fprintf (fp, "      <value>%s</value>\n", encoded);

              g_free (encoded);
              
              ++i;
            }

          g_strfreev (vec);
        }
      else
        {
          msm_warning (_("Not saving unknown property type '%s'\n"),
                       prop->type);
        }
      
      fputs ("    </prop>\n", fp);

      tmp = tmp->next;
    }
}

void
msm_session_save (MsmSession  *session,
                  MsmServer   *server)
{
  /* We save to a secondary file then copy over, to handle
   * out-of-disk-space robustly
   */
  int new_fd;
  char *new_filename;
  char *error;
  FILE *fp;
  
  error = NULL;
  new_fd = -1;
  
  new_filename = g_strconcat (session->full_filename, ".new", NULL);
  new_fd = open (session->full_filename, O_RDWR | O_CREAT | O_EXCL, 0700);
  if (new_fd < 0)
    {
      error = g_strdup_printf (_("Failed to open '%s': %s\n"),
                               new_filename, g_strerror (errno));
      goto out;
    }

  if (lock_entire_file (new_fd) < 0)
    {
      error = g_strdup_printf (_("Failed to lock file '%s': %s"),
                               new_filename,
                               g_strerror (errno));
      goto out;
    }
  
  fp = fdopen (new_fd, "w");
  if (fp == NULL)
    {
      error = g_strdup_printf (_("Failed to write to new session file '%s': %s"),
                               new_filename, g_strerror (errno));
      goto out;
    }

  fputs ("<msm_session>\n", fp);

  {
    GList *tmp;
    tmp = session->clients;
    while (tmp != NULL)
      {
        MsmSavedClient *saved = tmp->data;
        char *encoded;

        encoded = encode_text_as_utf8 (saved->id);
        
        fprintf (fp, "  <client id=\"%s\">\n",
                 encoded);

        g_free (encoded);

        write_proplist (fp, saved->properties);
        
        fputs ("  </client>\n", fp);

        tmp = tmp->next;
      }
  }

  fputs ("</msm_session>\n", fp);

  if (ferror (fp))
    {
      error = g_strdup_printf (_("Error writing new session file '%s': %s"),
                               new_filename, g_strerror (errno));
      fclose (fp);
      goto out;
    }
  
  if (fclose (fp) < 0)
    {
      error = g_strdup_printf (_("Failed to close to new session file '%s': %s"),
                               new_filename, g_strerror (errno));
      goto out;
    }
  
  if (rename (new_filename, session->full_filename) < 0)
    {
      error = g_strdup_printf (_("Failed to replace the old session file '%s' with the new session contents in the temporary file '%s': %s"),
                               session->full_filename,
                               new_filename, g_strerror (errno));
      goto out;
    }


  
 out:
  g_free (new_filename);
  
  if (error)
    {
      if (new_fd >= 0)
        close (new_fd);
    }
  else
    {
      if (session->lock_fd >= 0)
        close (session->lock_fd);
      session->lock_fd = new_fd;
      set_close_on_exec (new_fd);
    }
}

static void
add_details_to_dialog (GtkDialog  *dialog,
                       const char *details)
{
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *label;
  GtkRequisition req;
  
  hbox = gtk_hbox_new (FALSE, 0);

  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
  
  gtk_box_pack_start (GTK_BOX (dialog->vbox),
                      hbox,
                      FALSE, FALSE, 0);

  button = gtk_button_new_with_mnemonic (_("_Details"));
  
  gtk_box_pack_end (GTK_BOX (hbox), button,
                    FALSE, FALSE, 0);
  
  label = gtk_label_new (details);

  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  
  gtk_box_pack_start (GTK_BOX (hbox), label,
                      TRUE, TRUE, 0);

  /* show the label on click */
  g_signal_connect_swapped (G_OBJECT (button),
                            "clicked",
                            G_CALLBACK (gtk_widget_show),
                            label);
  
  /* second callback destroys the button (note disconnects first callback) */
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);

  /* Set default dialog size to size with the label,
   * and without the button, but then rehide the label
   */
  gtk_widget_show_all (hbox);

  gtk_widget_size_request (GTK_WIDGET (dialog), &req);
#if 0
  /* Omitted for now because it triggers a GTK 1.3.7 bug */
  gtk_window_set_default_size (GTK_WINDOW (dialog), req.width, req.height);
#endif
  
  gtk_widget_hide (label);
}

static MsmSession*
recover_failed_session (MsmSession             *session,
                        MsmSessionFailureReason reason,
                        const char             *details)
{
  /* FIXME, actually give option to recover, don't just complain */
  GtkWidget *dialog;
  char *message;

  message = NULL;
  
  switch (reason)
    {
    case MSM_SESSION_FAILURE_OPENING_FILE:
      message = g_strdup_printf (_("Could not open the session \"%s.\""),
                                 session->name);
      /* FIXME recovery options:
       *  - give up and exit; something pathological is going on
       *  - choose another session?
       *  - use default session in read-only mode?
       *  - open xterm to repair the problem, then try again (experts only)
       */
      break;
      
    case MSM_SESSION_FAILURE_LOCKING:
      message = g_strdup_printf (_("You are already logged in elsewhere, using the session \"%s.\" You can only use a session from one location at a time."),
                                 session->name);
      /* FIXME recovery options:
       *  - log in anyhow, with possible weirdness
       *  - try again (after logging out the other session)
       *  - choose another session
       *  - open xterm to repair the problem, then try again (experts only)
       */
      break;
      
    case MSM_SESSION_FAILURE_BAD_FILE:
      message = g_strdup_printf (_("The session file for session \"%s\" appears to be invalid or corrupted."),
                                 session->name);
      /* FIXME recovery options:
       *  - revert session to defaults
       *  - choose another session
       *  - open xterm to repair the problem, then try again (experts only)
       */
      break;
      
    case MSM_SESSION_FAILURE_EMPTY:
      message = g_strdup_printf (_("The session \"%s\" contains no applications."),
                                 session->name);
      /* FIXME recovery options:
       *  - put default applications in the session
       *  - choose another session
       *  - open xterm to repair the problem, then try again (experts only)
       */
      break;
    }
  
  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   message);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  if (details)
    add_details_to_dialog (GTK_DIALOG (dialog), details);
  
  g_free (message);
  
  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  exit (1);

  /* FIXME instead of exiting, always recover by coming up with some sort
   * of session. Also, offer nice recovery options specific to each of the above
   * failure modes.
   */
  return NULL;
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


static char*
encode_text_as_utf8 (const char *text)
{
  /* text can be any encoding, and is nul-terminated.
   * we pretend it's Latin-1 and encode as UTF-8
   */
  GString *str;
  const char *p;
  
  str = g_string_new ("");

  p = text;
  while (*p)
    {
      g_string_append_unichar (str, *p);
      ++p;
    }

  return g_string_free (str, FALSE);
}

static char*
decode_text_from_utf8 (const char *text)
{
  /* Convert back from the encoded UTF-8 */
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
