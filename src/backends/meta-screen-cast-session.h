/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015-2017 Red Hat Inc.
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
 */

#ifndef META_SCREEN_CAST_SESSION_H
#define META_SCREEN_CAST_SESSION_H

#include "backends/meta-screen-cast.h"

typedef enum _MetaScreenCastSessionType
{
  META_SCREEN_CAST_SESSION_TYPE_NORMAL,
  META_SCREEN_CAST_SESSION_TYPE_REMOTE_DESKTOP,
} MetaScreenCastSessionType;

#define META_TYPE_SCREEN_CAST_SESSION (meta_screen_cast_session_get_type ())
G_DECLARE_FINAL_TYPE (MetaScreenCastSession, meta_screen_cast_session,
                      META, SCREEN_CAST_SESSION,
                      MetaDBusScreenCastSessionSkeleton)

char * meta_screen_cast_session_get_object_path (MetaScreenCastSession *session);

MetaScreenCastSession * meta_screen_cast_session_new (MetaScreenCast             *screen_cast,
                                                      MetaScreenCastSessionType   session_type,
                                                      GError                    **error);

gboolean meta_screen_cast_session_start (MetaScreenCastSession  *session,
                                         GError                **error);

void meta_screen_cast_session_close (MetaScreenCastSession *session);

#endif /* META_SCREEN_CAST_SESSION_H */
