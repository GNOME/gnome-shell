/* Clutter -  An OpenGL based 'interactive canvas' library.
 * OSX backend
 *
 * Copyright (C) 2007  Tommi Komulainen <tommi.komulainen@iki.fi>
 * Copyright (C) 2007  OpenedHand Ltd.
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
#ifndef __CLUTTER_OSX_H__
#define __CLUTTER_OSX_H__

#include <clutter/clutter-stage.h>

@class NSEvent;

G_BEGIN_DECLS

#define CLUTTER_OSX_POOL_ALLOC()        NSAutoreleasePool *autorelease_pool = [[NSAutoreleasePool alloc] init]
#define CLUTTER_OSX_POOL_RELEASE()      [autorelease_pool release];

void _clutter_events_osx_init   (void);
void _clutter_events_osx_uninit (void);

void _clutter_event_osx_put     (NSEvent *nsevent, ClutterStage *wrapper);

G_END_DECLS

#endif /* __CLUTTER_OSX_H__ */
