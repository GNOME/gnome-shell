/* Metacity UI Slave Messages */

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

#include "messages.h"
#include "main.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <config.h>

typedef enum
{
  READ_FAILED = 0, /* FALSE */
  READ_OK,
  READ_EOF
} ReadResult;

static ReadResult read_data       (GString       *str,
                                   gint           fd);

static void send_message (MetaMessage *message);

void
meta_message_send_check (void)
{
  MetaMessageCheck check;

  memset (&check, 0, sizeof (check));
  check.header.message_code = MetaMessageCheckCode;
  check.header.length = sizeof (check);
  strcpy (check.metacity_version, VERSION);
  strcpy (check.host_alias, HOST_ALIAS);
  check.metacity_version[META_MESSAGE_MAX_VERSION_LEN] = '\0';
  check.host_alias[META_MESSAGE_MAX_HOST_ALIAS_LEN] = '\0';
  check.messages_version = META_MESSAGES_VERSION;

  send_message ((MetaMessage*)&check);
}

static int
write_bytes (void *buf, int bytes)
{
  const char *p;

  p = (char*) buf;
  while (bytes > 0)
    {
      int written;

      written = write (1, p, bytes);

      if (written < 0)
        return -1;

      bytes -= written;
      p += written;
    }

  return 0;
}

static void
send_message (MetaMessage *message)
{
  /* Not much point checking for errors here. We can't
   * really report them anyway.
   */

  write_bytes (META_MESSAGE_ESCAPE, META_MESSAGE_ESCAPE_LEN);
  write_bytes (message, message->header.length);
}

static ReadResult
read_data (GString *str,
           gint     fd)
{
  gint bytes;
  gchar buf[4096];

 again:
  
  bytes = read (fd, &buf, 4096);

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
      meta_ui_warning (_("Failed to read data from window manager (%s)\n"),
                       g_strerror (errno));
      
      return READ_FAILED;
    }
  else
    return READ_OK;
}
