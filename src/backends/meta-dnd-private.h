/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Hyungwon Hwang
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_DND_PRIVATE__H
#define META_DND_PRIVATE__H

#include <X11/Xlib.h>

gboolean meta_dnd_handle_xdnd_event (MetaBackend    *backend,
                                     MetaCompositor *compositor,
                                     MetaDisplay    *display,
                                     XEvent         *xev);

#ifdef HAVE_WAYLAND
void meta_dnd_wayland_handle_begin_modal (MetaCompositor *compositor);
#endif

#endif /* META_DND_PRIVATE_H */
