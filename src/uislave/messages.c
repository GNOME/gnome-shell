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
#include <ctype.h>
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

  memset (&check, 0, META_MESSAGE_LENGTH (MetaMessageCheck));
  check.header.message_code = MetaMessageCheckCode;
  check.header.length = META_MESSAGE_LENGTH (MetaMessageCheck);

  strncpy (check.metacity_version, VERSION, META_MESSAGE_MAX_VERSION_LEN);
  check.metacity_version[META_MESSAGE_MAX_VERSION_LEN] = '\0';

  strncpy (check.host_alias, HOST_ALIAS, META_MESSAGE_MAX_HOST_ALIAS_LEN);
  check.host_alias[META_MESSAGE_MAX_HOST_ALIAS_LEN] = '\0';

  check.messages_version = META_MESSAGES_VERSION;

  send_message ((MetaMessage*)&check);
}

static int
write_bytes (void *buf, int bytes)
{
  const char *p;
  int left;

  left = bytes;
  p = (char*) buf;
  while (left > 0)
    {
      int written;

      written = write (1, p, left);

      if (written < 0)
        return -1;

      left -= written;
      p += written;
    }

  g_assert (p == ((char*)buf) + bytes); 
  
  return 0;
}

#if 0
static void
print_mem (int fd, void *mem, int len)
{
  char *p = (char*) mem;
  int i;
  char unknown = 'Z';
  char null = 'n';
  
  i = 0;
  while (i < len)
    {
      if (p[i] == '\0')
        write (fd, &null, 1);
      else if (isascii (p[i]))
        write (fd, p + i, 1);
      else
        write (fd, &unknown, 1);
      
      ++i;
    }
}
#endif

static void
send_message (MetaMessage *message)
{
  static int serial = 0;
  MetaMessageFooter *footer;
  
  message->header.serial = serial;
  footer = META_MESSAGE_FOOTER (message);
  
  footer->checksum = META_MESSAGE_CHECKSUM (message);
  ++serial;

#if 0
  meta_ui_warning ("'");
  print_mem (2, message, message->header.length);
  meta_ui_warning ("'\n");
#endif
  
  if (write_bytes (META_MESSAGE_ESCAPE, META_MESSAGE_ESCAPE_LEN) < 0)
    meta_ui_warning ("Failed to write escape sequence: %s\n",
                     g_strerror (errno));
  if (write_bytes (message, message->header.length) < 0)
    meta_ui_warning ("Failed to write message: %s\n",
                     g_strerror (errno));
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
