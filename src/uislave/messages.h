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

#ifndef META_UI_SLAVE_MESSAGES_H
#define META_UI_SLAVE_MESSAGES_H

#include <glib.h>

/* This thing badly violates the KISS principle. */

/* This header is shared between the WM and the UI slave */
/* Note that our IPC can be kind of lame; we trust both sides
 * of the connection, and assume that they were compiled at the
 * same time vs. the same libs on the same arch
 */
/* (and lo and behold, our IPC is kind of lame) */

/* We increment this when we change this header, so we can
 * check for mismatched UI slave and WM
 */
#define META_MESSAGES_VERSION 1

/* We have an escape sequence, just in case some part of GTK
 * decides to write to stdout, so that we have a good chance
 * of surviving that. GTK probably won't print this string.
 * This string has to stay the same always so we can ping
 * old UI slaves.
 */
#define META_MESSAGE_ESCAPE "|~-metacity-~|"
/* len includes nul byte which is a required part of the escape */
#define META_MESSAGE_ESCAPE_LEN 15

/* This is totally useless of course. Playing around. */
#define META_MESSAGE_CHECKSUM(msg) ((msg)->header.length | (msg)->header.serial << 16) 
#define META_MESSAGE_FOOTER(msg) ((MetaMessageFooter*) (((char*)(msg)) + ((msg)->header.length - sizeof (MetaMessageFooter))));
#define META_MESSAGE_LENGTH(real_type) \
   (G_STRUCT_OFFSET (real_type, footer) + sizeof (MetaMessageFooter))

#define META_MESSAGE_MAX_SIZE (sizeof(MetaMessage));

#define META_MESSAGE_MAX_VERSION_LEN 15
#define META_MESSAGE_MAX_HOST_ALIAS_LEN 50
#define META_MESSAGE_MAX_TIP_LEN 128

typedef union  _MetaMessage               MetaMessage;
typedef struct _MetaMessageHeader         MetaMessageHeader;
typedef struct _MetaMessageFooter         MetaMessageFooter;
typedef struct _MetaMessageCheck          MetaMessageCheck;
typedef struct _MetaMessageShowTip        MetaMessageShowTip;
typedef struct _MetaMessageHideTip        MetaMessageHideTip;
typedef struct _MetaMessageShowWindowMenu MetaMessageShowWindowMenu;
typedef struct _MetaMessageHideWindowMenu MetaMessageHideWindowMenu;

typedef enum
{
  /* Keep NullCode and CheckCode unchanged, as with the escape sequence,
   * so we can check old UI slaves.
   */
  MetaMessageNullCode,
  MetaMessageCheckCode,
  MetaMessageShowTipCode,
  MetaMessageHideTipCode,
  MetaMessageShowWindowMenuCode,
  MetaMessageHideWindowMenuCode
} MetaMessageCode;

struct _MetaMessageHeader
{
  MetaMessageCode message_code;
  int length;
  int serial;
};

/* The footer thing was pretty much just a debug hack and could die. */
struct _MetaMessageFooter
{
  int checksum;
};

/* just a ping to see if we have the right
 * version of UI slave.
 */
struct _MetaMessageCheck
{
  MetaMessageHeader header;

  /* it's OK if the max sizes aren't large enough in all cases, these
   * are just paranoia checks
   */
  char metacity_version[META_MESSAGE_MAX_VERSION_LEN + 1];
  char host_alias[META_MESSAGE_MAX_HOST_ALIAS_LEN + 1];
  int messages_version;

  MetaMessageFooter footer;
};

struct _MetaMessageShowTip
{
  MetaMessageHeader header;
  int root_x;
  int root_y;
  char markup[META_MESSAGE_MAX_TIP_LEN + 1];
  MetaMessageFooter footer;
};

struct _MetaMessageHideTip
{
  MetaMessageHeader header;
  /* just hides the current tip */
  MetaMessageFooter footer;
};

typedef enum
{
  META_MESSAGE_MENU_DELETE      = 1 << 0,
  META_MESSAGE_MENU_MINIMIZE    = 1 << 1,
  META_MESSAGE_MENU_MAXIMIZE    = 1 << 2,
  META_MESSAGE_MENU_SHADE       = 1 << 3,
  META_MESSAGE_MENU_WORKSPACES  = 1 << 4,
  META_MESSAGE_MENU_ALL         = META_MESSAGE_MENU_DELETE |
                                  META_MESSAGE_MENU_MINIMIZE |
                                  META_MESSAGE_MENU_MAXIMIZE |
                                  META_MESSAGE_MENU_SHADE |
                                  META_MESSAGE_MENU_WORKSPACES
} MetaMessageWindowMenuOps;

struct _MetaMessageShowWindowMenu
{
  MetaMessageHeader header;
  MetaMessageWindowMenuOps ops;
  MetaMessageWindowMenuOps insensitive;
  gulong window;
  int root_x;
  int root_y;
  guint32 timestamp;
  int button;
  MetaMessageFooter footer;
};

struct _MetaMessageHideWindowMenu
{
  MetaMessageHeader header;

  MetaMessageFooter footer;
};

union _MetaMessage
{
  MetaMessageHeader header;
  MetaMessageCheck check;
  MetaMessageShowTip show_tip;
  MetaMessageShowTip hide_tip;
  MetaMessageShowWindowMenu show_menu;
  MetaMessageHideWindowMenu hide_menu;
};

/* Slave-side message send/read code */

void meta_message_send_check (void);

#endif
