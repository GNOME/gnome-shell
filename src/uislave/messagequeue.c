/* Metacity IPC message source for main loop */

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

#include "messagequeue.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#ifdef METACITY_COMPILE
#include "util.h"
#else
#include "main.h"
void
meta_debug_spew (const char *format, ...)
{
}

void
meta_verbose (const char *format, ...)
{
}

void
meta_bug (const char *format, ...)
{
  /* stop us in a debugger */
  abort ();
}

void
meta_warning (const char *format, ...)
{
}

void
meta_fatal (const char *format, ...)
{
  exit (1);
}
#endif /* !METACITY_COMPILE */

typedef enum
{
  READ_FAILED = 0, /* FALSE */
  READ_OK,
  READ_EOF
} ReadResult;

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

struct _MetaMessageQueue
{
  GSource source;

  GPollFD out_poll;
  GQueue *queue;
  GString *buf;
  GString *current_message;
  int current_required_len;
  int last_serial;
};

MetaMessageQueue*
meta_message_queue_new  (int                  fd,
                         MetaMessageQueueFunc func,
                         gpointer             data)
{
  MetaMessageQueue *mq;
  GSource *source;

  source = g_source_new (&mq_funcs, sizeof (MetaMessageQueue));

  mq = (MetaMessageQueue*) source;
  
  mq->queue = g_queue_new ();
  mq->buf = g_string_new ("");
  mq->current_message = g_string_new ("");
  mq->current_required_len = 0;
  mq->last_serial = 0;
  mq->out_poll.fd = fd;
  mq->out_poll.events = G_IO_IN;

  g_source_add_poll (source, &mq->out_poll);
  
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_can_recurse (source, TRUE);

  g_source_set_callback (source, (GSourceFunc) func, data, NULL);
  
  g_source_attach (source, NULL);
  
  return mq;
}

void
meta_message_queue_free (MetaMessageQueue *mq)
{
  GSource *source;

  source = (GSource*) mq;
  
  g_source_destroy (source);
}

static void
append_pending (MetaMessageQueue *mq)
{
  int needed;
  
  needed = mq->current_required_len - mq->current_message->len;
  g_assert (needed >= 0);
  
  needed = MIN (needed, mq->buf->len);
  
  /* Move data from buf to current_message */
  if (needed > 0)
    {
      meta_verbose ("Moving %d bytes from buffer to current incomplete message\n",
                    needed);
      g_string_append_len (mq->current_message,
                           mq->buf->str,
                           needed);
      g_string_erase (mq->buf,
                      0, needed);
    }
  
  g_assert (mq->current_message->len <= mq->current_required_len);

  if (mq->current_required_len > 0 &&
      mq->current_message->len == mq->current_required_len)
    {
      MetaMessage *message;
      MetaMessageFooter *footer;
  
      message = g_new (MetaMessage, 1);
      
      memcpy (message,
              mq->current_message->str, mq->current_message->len);

      if (message->header.length != mq->current_required_len)
        meta_bug ("Message length changed?\n");

      if (message->header.serial != mq->last_serial)
        meta_bug ("Message serial changed?\n");
      
      footer = META_MESSAGE_FOOTER (message);
      
      if (footer->checksum == META_MESSAGE_CHECKSUM (message))
        {
          g_queue_push_tail (mq->queue, message);

          meta_verbose ("Added %d-byte message serial %d to queue\n",
                        mq->current_message->len, message->header.serial);
        }
      else
        {
          meta_bug ("Bad checksum %d on %d-byte message from UI slave\n",
                    footer->checksum, mq->current_message->len);
        }
      
      mq->current_required_len = 0;
      g_string_truncate (mq->current_message, 0);
    }
  else if (mq->current_required_len > 0)
    {
      meta_verbose ("Storing %d bytes of incomplete message\n",
                    mq->current_message->len);
    }
}

static void
mq_queue_messages (MetaMessageQueue *mq)
{  
  while (mq->buf->len > 0)
    {  
      if (mq->current_required_len > 0)
        {
          /* We had a pending message. */
          append_pending (mq);
        }
      else if (mq->buf->len > META_MESSAGE_ESCAPE_LEN)
        {
          /* See if we can start a current message */
          const char *p;
          int esc_pos;
          const char *esc;
          MetaMessageHeader header;

          g_assert (mq->current_required_len == 0);
          g_assert (mq->current_message->len == 0);
          
          meta_verbose ("Scanning for escape sequence in %d bytes\n",
                        mq->buf->len);
          
          /* note that the data from the UI slave includes the nul byte */
          esc = META_MESSAGE_ESCAPE;
      
          esc_pos = -1;
          p = mq->buf->str;
          while (p != (mq->buf->str + mq->buf->len))
            {
              if (*p == *esc)
                {
                  esc_pos = 0;
                  while (*p == esc[esc_pos])
                    {
                      ++esc_pos;
                      ++p;

                      if (esc_pos == META_MESSAGE_ESCAPE_LEN ||
                          p == (mq->buf->str + mq->buf->len))
                        goto out;
                    }

                  esc_pos = -1;
                }
              else
                {
                  ++p;
                }
            }

        out:
          if (esc_pos == META_MESSAGE_ESCAPE_LEN)
            {
              /* We found an entire escape sequence; can safely toss
               * out the entire buffer before it
               */
              int ignored;

              g_assert (esc[META_MESSAGE_ESCAPE_LEN-1] == *(p-1));              

              ignored = p - mq->buf->str;
              ignored -= META_MESSAGE_ESCAPE_LEN;

              g_assert (ignored >= 0);
              g_assert (mq->buf->str[ignored] == esc[0]);
              
              if (ignored > 0)
                {
                  g_string_erase (mq->buf, 0, ignored);
                  meta_verbose ("Ignoring %d bytes before escape, new buffer len %d\n",
                                ignored, mq->buf->len);
                }
              else
                {
                  g_assert (p == (mq->buf->str + META_MESSAGE_ESCAPE_LEN));
                }
            }
          else if (esc_pos < 0)
            {
              /* End of buffer doesn't begin an escape sequence;
               * toss out entire buffer.
               */
              meta_verbose ("Emptying %d-byte buffer not containing escape sequence\n",
                            mq->buf->len);
              g_string_truncate (mq->buf, 0);
              goto need_more_data;
            }
          else
            {
              meta_verbose ("Buffer ends with partial escape sequence\n");
              goto need_more_data;
            }

          g_assert (strcmp (mq->buf->str, META_MESSAGE_ESCAPE) == 0);
          
          if (mq->buf->len < (META_MESSAGE_ESCAPE_LEN + sizeof (MetaMessageHeader)))
            {
              meta_verbose ("Buffer has full escape sequence but lacks header\n");
              goto need_more_data;
            }

          g_string_erase (mq->buf, 0, META_MESSAGE_ESCAPE_LEN);
          meta_verbose ("Stripped escape off front of buffer, new buffer len %d\n",
                        mq->buf->len);

          g_assert (mq->buf->len >= sizeof (MetaMessageHeader));
          
          memcpy (&header, mq->buf->str, sizeof (MetaMessageHeader));

          /* Length includes the header even though it's in the header. */
          meta_verbose ("Read header, code: %d length: %d serial: %d\n",
                        header.message_code, header.length, header.serial);

          if (header.serial != mq->last_serial + 1)
            meta_bug ("Wrong message serial number %d from UI slave!\n", header.serial);

          mq->last_serial = header.serial;
          mq->current_required_len = header.length;
          
          append_pending (mq);
        }
      else
        goto need_more_data;
    }

 need_more_data:
  return;
}

static gboolean
mq_messages_pending (MetaMessageQueue *mq)
{
  return mq->queue->length > 0;
  /* these are useless until we wake up on poll again */
#if 0
    mq->buf->len > 0 ||
    mq->current_message->len > 0;
#endif
}

static gboolean
mq_prepare (GSource *source, gint *timeout)
{
  MetaMessageQueue *mq;

  mq = (MetaMessageQueue*) source;
  
  *timeout = -1;

  mq_queue_messages (mq);
  
  return mq_messages_pending (mq);
}

static gboolean  
mq_check (GSource  *source) 
{
  MetaMessageQueue *mq;
  
  mq = (MetaMessageQueue*) source;

  mq_queue_messages (mq);

  if (mq->out_poll.revents & G_IO_IN)
    {
      ReadResult res;

      res = read_data (mq->buf, mq->out_poll.fd);
      
      switch (res)
        {
        case READ_OK:
          meta_verbose ("Read data from slave, %d bytes in buffer\n",
                        mq->buf->len);
          break;
        case READ_EOF:
#ifdef METACITY_COMPILE
          meta_verbose ("EOF reading stdout from slave process\n");
#else
          meta_ui_warning ("Metacity parent process disappeared\n");
          exit (1);
#endif
          break;
          
        case READ_FAILED:
          /* read_data printed the error */
          break;
        }
    }


  if (mq->out_poll.revents & G_IO_HUP)
    {
#ifdef METACITY_COMPILE
      meta_verbose ("UI slave hung up\n");
#else
      meta_ui_warning ("Metacity parent process hung up\n");
      exit (1);
#endif
    }
  
  mq->out_poll.revents = 0;
  
  return mq_messages_pending (mq);
}

static gboolean  
mq_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
  MetaMessageQueue *mq;

  mq = (MetaMessageQueue*) source;
  
  if (mq->queue->length > 0)
    {
      MetaMessageQueueFunc func;
      MetaMessage *msg;
      static int count = 0;

      ++count;
      
      msg = g_queue_pop_head (mq->queue);
      func = (MetaMessageQueueFunc) callback;
      
      (* func) (mq, msg, user_data);

      meta_verbose ("%d messages dispatched\n", count);
      
      g_free (msg);
    }
  
  return TRUE;
}
    
static void
mq_destroy (GSource *source)
{
  MetaMessageQueue *mq;

  mq = (MetaMessageQueue*) source;
  
  while (mq->queue->length > 0)
    {
      MetaMessage *msg;
      
      msg = g_queue_pop_head (mq->queue);

      g_free (msg);
    }

  g_string_free (mq->buf, TRUE);
  g_string_free (mq->current_message, TRUE);
  
  g_queue_free (mq->queue);
  
  /* source itself is freed by glib */
}

static ReadResult
read_data (GString *str,
           gint     fd)
{
#define BUFSIZE 1024
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

/* Wait forever and build infinite queue until we get
 * the desired serial_of_request or one higher than it
 */
void
meta_message_queue_wait_for_reply (MetaMessageQueue *mq,
                                   int               serial_of_request)
{
  ReadResult res;
  int prev_len;

  prev_len = 0;
  while (TRUE)
    {
      if (prev_len < mq->queue->length)
        {
          GList *tmp;

          tmp = g_list_nth (mq->queue->head, prev_len);
          while (tmp != NULL)
            {
              MetaMessage *msg = tmp->data;

              if (msg->header.request_serial == serial_of_request)
                return;

              if (msg->header.request_serial > serial_of_request)
                {
                  meta_warning ("Serial request %d is greater than the awaited request %d\n",
                                msg->header.request_serial, serial_of_request);
                  return;
                }
              
              tmp = tmp->next;
            }

          prev_len = mq->queue->length;
        }
      
      res = read_data (mq->buf, mq->out_poll.fd);
      
      switch (res)
        {
        case READ_OK:
          meta_verbose ("Read data from slave, %d bytes in buffer\n",
                        mq->buf->len);
          break;
          
        case READ_EOF:
#ifdef METACITY_COMPILE
          meta_verbose ("EOF reading stdout from slave process\n");
#else
          meta_ui_warning ("Metacity parent process disappeared\n");
          exit (1);
#endif
          return;
          break;
          
        case READ_FAILED:
          /* read_data printed the error */
          return;
          break;
        }

      mq_queue_messages (mq);
    }
}
