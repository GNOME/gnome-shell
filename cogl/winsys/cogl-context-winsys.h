/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_CONTEXT_WINSYS_H
#define __COGL_CONTEXT_WINSYS_H

typedef struct
{
  /* These are specific to winsys backends supporting Xlib. This
     should probably eventually be moved into a separate file specific
     to Xlib when Cogl gains a more complete winsys abstraction */
#ifdef COGL_HAS_XLIB_SUPPORT
  GSList *event_filters;
#endif

  int stub;
} CoglContextWinsys;

#endif /* __COGL_CONTEXT_WINSYS_H */
