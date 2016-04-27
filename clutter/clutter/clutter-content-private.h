/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011  Intel Corporation.
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
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

#ifndef __CLUTTER_CONTENT_PRIVATE_H__
#define __CLUTTER_CONTENT_PRIVATE_H__

#include <clutter/clutter-content.h>

G_BEGIN_DECLS

void            _clutter_content_attached               (ClutterContent   *content,
                                                         ClutterActor     *actor);
void            _clutter_content_detached               (ClutterContent   *content,
                                                         ClutterActor     *actor);

void            _clutter_content_paint_content          (ClutterContent   *content,
                                                         ClutterActor     *actor,
                                                         ClutterPaintNode *node);

G_END_DECLS

#endif /* __CLUTTER_CONTENT_PRIVATE_H__ */
