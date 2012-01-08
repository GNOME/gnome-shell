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

#ifndef __COGL_PRIMITIVES_PRIVATE_H
#define __COGL_PRIMITIVES_PRIVATE_H

#include <glib.h>

G_BEGIN_DECLS

/* Draws a rectangle without going through the journal so that it will
   be flushed immediately. This should only be used in situations
   where the code may be called while the journal is already being
   flushed. In that case using the journal would go wrong */
void
_cogl_rectangle_immediate (CoglFramebuffer *framebuffer,
                           CoglPipeline *pipeline,
                           float x_1,
                           float y_1,
                           float x_2,
                           float y_2);

G_END_DECLS

#endif /* __COGL_PRIMITIVES_PRIVATE_H */
