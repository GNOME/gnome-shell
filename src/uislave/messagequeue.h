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

#ifndef META_MESSAGE_QUEUE_H
#define META_MESSAGE_QUEUE_H

#include <glib.h>

#ifdef METACITY_COMPILE
#include "uislave/messages.h"
#else
#include "messages.h"
#endif

typedef struct _MetaMessageQueue MetaMessageQueue;

typedef void (* MetaMessageQueueFunc) (MetaMessageQueue *mq,
                                       MetaMessage      *message,
                                       gpointer          data);

MetaMessageQueue* meta_message_queue_new  (int                   fd,
                                           MetaMessageQueueFunc  func,
                                           gpointer              data);
void              meta_message_queue_free (MetaMessageQueue     *mq);

void              meta_message_queue_wait_for_reply (MetaMessageQueue *mq,
                                                     int               serial_of_request);

#endif
