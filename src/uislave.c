/* Metacity UI Slave */

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

#include "uislave.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

typedef enum
{
  READ_FAILED = 0, /* FALSE */
  READ_OK,
  READ_EOF
} ReadResult;

static void       respawn_child   (MetaUISlave   *uislave);
static gboolean   error_callback  (GIOChannel    *source,
                                   GIOCondition   condition,
                                   gpointer       data);
static void       kill_child      (MetaUISlave   *uislave);
static void       reset_vals      (MetaUISlave   *uislave);
static ReadResult read_data       (GString       *str,
                                   gint           fd);

/* Message queue main loop source */
static gboolean mq_prepare  (GSource     *source,
                             gint        *timeout);
static gboolean mq_check    (GSource     *source);
static gboolean mq_dispatch (GSource     *source,
                             GSourceFunc  callback,
                             gpointer     user_data);
static void     mq_destroy  (GSource     *source);

static GSourceFuncs mq_funcs = {
  mq_prepare,
  mq_check,
  mq_dispatch,
  mq_destroy
};

MetaUISlave*
meta_ui_slave_new (const char *display_name,
                   MetaUISlaveFunc  func,
                   gpointer         data)
{
  MetaUISlave *uislave;
  GSource *source;

  source = g_source_new (&mq_funcs, sizeof (MetaUISlave));

  uislave = (MetaUISlave*) source;
  
  uislave->display_name = g_strdup (display_name);
  uislave->queue = g_queue_new ();
  uislave->buf = g_string_new ("");
  uislave->current_message = g_string_new ("");
  
  reset_vals (uislave);
  
  /* This may fail; all UISlave functions become no-ops
   * if uislave->child_pids == 0, and metacity just runs
   * with no UI features other than window borders.
   */
  respawn_child (uislave);

  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_can_recurse (source, TRUE);

  g_source_set_callback (source, (GSourceFunc) func, data, NULL);
  
  g_source_attach (source, NULL);
  
  return uislave;
}

void
meta_ui_slave_free (MetaUISlave *uislave)
{
  GSource *source;

  source = (GSource*) uislave;
  
  g_source_destroy (source);
}

void
meta_ui_slave_disable (MetaUISlave *uislave)
{
  /* Change UI slave into "black hole" mode,
   * we found out it's hosed for some reason.
   */
  kill_child (uislave);
  uislave->no_respawn = TRUE;
}

static void
respawn_child (MetaUISlave *uislave)
{
  GError *error;
  const char *uislavedir;
  char *argv[] = { "./metacity-uislave", NULL };
  char *envp[2] = { NULL, NULL };
  int child_pid, inpipe, outpipe, errpipe;

  if (uislave->no_respawn)
    return;
  
  uislavedir = g_getenv ("METACITY_UISLAVE_DIR");
  if (uislavedir == NULL)
    uislavedir = METACITY_LIBEXECDIR;

  envp[0] = g_strconcat ("DISPLAY=", uislave->display_name, NULL);
  
  error = NULL;
  if (g_spawn_async_with_pipes (uislavedir,
                                argv,
                                envp,
                                /* flags */
                                0,
                                /* setup func, data */
                                NULL, NULL,
                                &child_pid,
                                &inpipe, &outpipe, &errpipe,
                                &error))
    {
      uislave->child_pid = child_pid;
      uislave->in_pipe = inpipe;
      uislave->err_pipe = errpipe;

      uislave->err_channel = g_io_channel_unix_new (errpipe);
      
      uislave->errwatch = g_io_add_watch (uislave->err_channel,
                                          G_IO_IN,
                                          error_callback,
                                          uislave);
      
      uislave->out_poll.fd = outpipe;
      uislave->out_poll.events = G_IO_IN;

      g_source_add_poll ((GSource*)uislave, &uislave->out_poll);
      
      meta_verbose ("Spawned UI slave with PID %d\n", uislave->child_pid);
    }
  else
    {
      meta_warning ("Failed to create user interface process: %s\n",
                    error->message);
      g_error_free (error);
    }
  
  g_free (envp[0]);  
}

static void
append_pending (MetaUISlave *uislave)
{
  int needed;
  
  needed = uislave->current_required_len - uislave->current_message->len;
  g_assert (needed >= 0);
  
  needed = MIN (needed, uislave->buf->len);
  
  /* Move data from buf to current_message */
  if (needed > 0)
    {
      meta_verbose ("Moving %d bytes from buffer to current incomplete message\n",
                    needed);
      g_string_append_len (uislave->current_message,
                           uislave->buf->str,
                           needed);
      g_string_erase (uislave->buf,
                      0, needed);
    }
  
  g_assert (uislave->current_message->len <= uislave->current_required_len);

  if (uislave->current_required_len > 0 &&
      uislave->current_message->len == uislave->current_required_len)
    {
      MetaMessage *message;
      MetaMessageFooter *footer;
  
      message = g_new (MetaMessage, 1);
      
      memcpy (message,
              uislave->current_message->str, uislave->current_message->len);

      if (message->header.length != uislave->current_required_len)
        meta_bug ("Message length changed?\n");

      if (message->header.serial != uislave->last_serial)
        meta_bug ("Message serial changed?\n");
      
      footer = META_MESSAGE_FOOTER (message);
      
      if (footer->checksum == META_MESSAGE_CHECKSUM (message))
        {
          g_queue_push_tail (uislave->queue, message);

          meta_verbose ("Added %d-byte message serial %d to queue\n",
                        uislave->current_message->len, message->header.serial);
        }
      else
        {
          meta_bug ("Bad checksum %d on %d-byte message from UI slave\n",
                    footer->checksum, uislave->current_message->len);
        }
      
      uislave->current_required_len = 0;
      g_string_truncate (uislave->current_message, 0);
    }
  else if (uislave->current_required_len > 0)
    {
      meta_verbose ("Storing %d bytes of incomplete message\n",
                    uislave->current_message->len);
    }
}

static gboolean
error_callback  (GIOChannel   *source,
                 GIOCondition  condition,
                 gpointer      data)
{  
  /* Relay slave errors to WM stderr */
#define BUFSIZE 1024
  MetaUISlave *uislave;
  char buf[1024];
  int n;
  
  uislave = data;
  
  /* Classic loop from Stevens */
  n = read (uislave->err_pipe, buf, BUFSIZE);
  if (n > 0)
    {
      if (write (2, buf, n) != n)
        ; /* error, but printing a message to stderr will hardly help. */
    }
  else if (n < 0)
    meta_warning (_("Error reading errors from UI slave: %s\n"),
                  g_strerror (errno));
  
  return TRUE;
#undef BUFSIZE
}

static void
mq_queue_messages (MetaUISlave *uislave)
{  
  if (uislave->out_poll.revents & G_IO_IN)
    {
      ReadResult res;

      res = read_data (uislave->buf, uislave->out_poll.fd);
      
      switch (res)
        {
        case READ_OK:
          meta_verbose ("Read data from slave, %d bytes in buffer\n",
                        uislave->buf->len);
          break;
        case READ_EOF:
          meta_verbose ("EOF reading stdout from slave process\n");
          break;
          
        case READ_FAILED:
          /* read_data printed the error */
          break;
        }
    }
  
  while (uislave->buf->len > 0)
    {  
      if (uislave->current_required_len > 0)
        {
          /* We had a pending message. */
          append_pending (uislave);
        }
      else if (uislave->buf->len > META_MESSAGE_ESCAPE_LEN)
        {
          /* See if we can start a current message */
          const char *p;
          int esc_pos;
          const char *esc;
          MetaMessageHeader header;

          g_assert (uislave->current_required_len == 0);
          g_assert (uislave->current_message->len == 0);
          
          meta_verbose ("Scanning for escape sequence in %d bytes\n",
                        uislave->buf->len);
          
          /* note that the data from the UI slave includes the nul byte */
          esc = META_MESSAGE_ESCAPE;
      
          esc_pos = -1;
          p = uislave->buf->str;
          while (p != (uislave->buf->str + uislave->buf->len) &&
                 esc_pos < META_MESSAGE_ESCAPE_LEN)
            {
              ++esc_pos;
              if (*p != esc[esc_pos])
                esc_pos = -1;

              ++p;
            }

          if (esc_pos == META_MESSAGE_ESCAPE_LEN)
            {
              /* We found an entire escape sequence; can safely toss
               * out the entire buffer before it
               */
              int ignored;

              ignored = p - uislave->buf->str;
              ignored -= META_MESSAGE_ESCAPE_LEN;

              g_assert (ignored >= 0);
              
              if (ignored > 0)
                {
                  g_string_erase (uislave->buf, 0, ignored);
                  meta_verbose ("Ignoring %d bytes before escape, new buffer len %d\n",
                                ignored, uislave->buf->len);
                }
              else
                {
                  g_assert (p == (uislave->buf->str + META_MESSAGE_ESCAPE_LEN));
                }
            }
          else if (esc_pos < 0)
            {
              /* End of buffer doesn't begin an escape sequence;
               * toss out entire buffer.
               */
              meta_verbose ("Emptying %d-byte buffer not containing escape sequence\n",
                            uislave->buf->len);
              g_string_truncate (uislave->buf, 0);
              goto need_more_data;
            }
          else
            {
              meta_verbose ("Buffer ends with partial escape sequence: '%s'\n",
                            uislave->buf->str + (uislave->buf->len - esc_pos));
              goto need_more_data;
            }

          g_assert (strcmp (uislave->buf->str, META_MESSAGE_ESCAPE) == 0);
          
          if (uislave->buf->len < (META_MESSAGE_ESCAPE_LEN + sizeof (MetaMessageHeader)))
            {
              meta_verbose ("Buffer has full escape sequence but lacks header\n");
              goto need_more_data;
            }

          g_string_erase (uislave->buf, 0, META_MESSAGE_ESCAPE_LEN);
          meta_verbose ("Stripped escape off front of buffer, new buffer len %d\n",
                        uislave->buf->len);

          g_assert (uislave->buf->len >= sizeof (MetaMessageHeader));
          
          memcpy (&header, uislave->buf->str, sizeof (MetaMessageHeader));

          /* Length includes the header even though it's in the header. */
          meta_verbose ("Read header, code: %d length: %d serial: %d\n",
                        header.message_code, header.length, header.serial);

          if (header.serial != uislave->last_serial + 1)
            meta_bug ("Wrong message serial number %d from UI slave!\n", header.serial);

          uislave->last_serial = header.serial;
          uislave->current_required_len = header.length;
          
          append_pending (uislave);
        }
      else
        goto need_more_data;
    }

 need_more_data:
  return;
}

static gboolean
mq_messages_pending (MetaUISlave *uislave)
{
  return uislave->queue->length > 0 ||
    uislave->buf->len > 0 ||
    uislave->current_message->len > 0;
}

static gboolean
mq_prepare (GSource *source, gint *timeout)
{
  MetaUISlave *uislave;

  uislave = (MetaUISlave*) source;
  
  *timeout = -1;

  mq_queue_messages (uislave);
  
  return mq_messages_pending (uislave);
}

static gboolean  
mq_check (GSource  *source) 
{
  MetaUISlave *uislave;
  
  uislave = (MetaUISlave*) source;

  mq_queue_messages (uislave);
  uislave->out_poll.revents = 0;
  
  return mq_messages_pending (uislave);
}

static gboolean  
mq_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
  MetaUISlave *uislave;

  uislave = (MetaUISlave*) source;
  
  if (uislave->queue->length > 0)
    {
      MetaUISlaveFunc func;
      MetaMessage *msg;
      static int count = 0;

      ++count;
      
      msg = g_queue_pop_head (uislave->queue);
      func = (MetaUISlaveFunc) callback;
      
      (* func) (uislave, msg, user_data);

      meta_verbose ("%d messages dispatched\n", count);
      
      g_free (msg);
    }
  
  return TRUE;
}

static void
kill_child (MetaUISlave *uislave)
{
  if (uislave->errwatch != 0)
    g_source_remove (uislave->errwatch);  

  if (uislave->err_channel)
    g_io_channel_unref (uislave->err_channel);

  if (uislave->out_poll.fd >= 0)
    {
      g_source_remove_poll ((GSource*)uislave, &uislave->out_poll);
      close (uislave->out_poll.fd);
    }

  if (uislave->in_pipe >= 0)
    close (uislave->in_pipe);

  if (uislave->err_pipe >= 0)
    close (uislave->err_pipe);
  
  while (uislave->queue->length > 0)
    {
      MetaMessage *msg;
      
      msg = g_queue_pop_head (uislave->queue);

      g_free (msg);
    }

  if (uislave->buf->len > 0)
    g_string_truncate (uislave->buf, 0);

  if (uislave->current_message->len > 0)
    g_string_truncate (uislave->current_message, 0);
  
  if (uislave->child_pid > 0)
    {
      /* don't care if this fails except in verbose mode */
      if (kill (uislave->child_pid, SIGTERM) != 0)
        {
          meta_verbose ("Kill of UI slave process %d failed: %s\n",
                        uislave->child_pid, g_strerror (errno));
        }
      
      uislave->child_pid = 0;
    }
  
  reset_vals (uislave);
}

static void
reset_vals (MetaUISlave *uislave)
{
  uislave->child_pid = 0;
  uislave->in_pipe = -1;
  uislave->err_pipe = -1;
  uislave->out_poll.fd = -1;
  uislave->no_respawn = FALSE;
  uislave->err_channel = NULL;
  uislave->errwatch = 0;
  uislave->current_required_len = 0;
  uislave->last_serial = -1;
}

static void
mq_destroy (GSource *source)
{
  MetaUISlave *uislave;

  uislave = (MetaUISlave*) source;

  meta_verbose ("Deleting UI slave for display '%s'\n",
                uislave->display_name);
  
  kill_child (uislave);

  g_string_free (uislave->buf, TRUE);
  g_string_free (uislave->current_message, TRUE);
  
  g_queue_free (uislave->queue);

  g_free (uislave->display_name);
  
  /* source itself is freed by glib */
}

static ReadResult
read_data (GString *str,
           gint     fd)
{
#define BUFSIZE 4000
  gint bytes;
  gchar buf[BUFSIZE];

 again:
  
  bytes = read (fd, &buf, BUFSIZE);

  if (bytes == 0)
    return READ_EOF;
  else if (bytes > 0)
    {
      g_string_append_len (str, buf, bytes);
      return READ_OK;
    }
  else if (bytes < 0 && errno == EINTR)
    goto again;
  else if (bytes < 0)
    {
      meta_warning (_("Failed to read data from UI slave: %s\n"),
                    g_strerror (errno));
      
      return READ_FAILED;
    }
  else
    return READ_OK;
}
