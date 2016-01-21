/*
 * Copyright (C) 2013 Red Hat, Inc.
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

#ifndef META_WAYLAND_TYPES_H
#define META_WAYLAND_TYPES_H

typedef struct _MetaWaylandCompositor MetaWaylandCompositor;

typedef struct _MetaWaylandSeat MetaWaylandSeat;
typedef struct _MetaWaylandPointer MetaWaylandPointer;
typedef struct _MetaWaylandPointerGrab MetaWaylandPointerGrab;
typedef struct _MetaWaylandPointerGrabInterface MetaWaylandPointerGrabInterface;
typedef struct _MetaWaylandPopupGrab MetaWaylandPopupGrab;
typedef struct _MetaWaylandPopup MetaWaylandPopup;
typedef struct _MetaWaylandPopupSurface MetaWaylandPopupSurface;
typedef struct _MetaWaylandKeyboard MetaWaylandKeyboard;
typedef struct _MetaWaylandKeyboardGrab MetaWaylandKeyboardGrab;
typedef struct _MetaWaylandKeyboardGrabInterface MetaWaylandKeyboardGrabInterface;
typedef struct _MetaWaylandTouch MetaWaylandTouch;
typedef struct _MetaWaylandDragDestFuncs MetaWaylandDragDestFuncs;
typedef struct _MetaWaylandDataOffer MetaWaylandDataOffer;
typedef struct _MetaWaylandDataDevice MetaWaylandDataDevice;

typedef struct _MetaWaylandBuffer MetaWaylandBuffer;
typedef struct _MetaWaylandRegion MetaWaylandRegion;

typedef struct _MetaWaylandSurface MetaWaylandSurface;

typedef struct _MetaWaylandOutput MetaWaylandOutput;

typedef struct _MetaWaylandSerial MetaWaylandSerial;

typedef struct _MetaWaylandPointerClient MetaWaylandPointerClient;

#endif
