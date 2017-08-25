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

#ifndef META_REMOTE_DESKTOP_SESSION_H
#define META_REMOTE_DESKTOP_SESSION_H

#include <glib-object.h>

#include "backends/meta-remote-desktop.h"
#include "backends/meta-screen-cast-session.h"

#define META_TYPE_REMOTE_DESKTOP_SESSION (meta_remote_desktop_session_get_type ())
G_DECLARE_FINAL_TYPE (MetaRemoteDesktopSession, meta_remote_desktop_session,
                      META, REMOTE_DESKTOP_SESSION,
                      MetaDBusRemoteDesktopSessionSkeleton)

char * meta_remote_desktop_session_get_object_path (MetaRemoteDesktopSession *session);

char * meta_remote_desktop_session_get_session_id (MetaRemoteDesktopSession *session);

gboolean meta_remote_desktop_session_register_screen_cast (MetaRemoteDesktopSession  *session,
                                                           MetaScreenCastSession     *screen_cast_session,
                                                           GError                   **error);

void meta_remote_desktop_session_close (MetaRemoteDesktopSession *session);

MetaRemoteDesktopSession * meta_remote_desktop_session_new (MetaRemoteDesktop  *remote_desktop,
                                                            const char         *peer_name,
                                                            GError            **error);

#endif /* META_REMOTE_DESKTOP_SESSION_H */
