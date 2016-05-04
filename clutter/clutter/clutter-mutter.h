/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2016 Red Hat Inc.
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
 */

#ifndef __CLUTTER_MUTTER_H__
#define __CLUTTER_MUTTER_H__

#define __CLUTTER_H_INSIDE__

#include "clutter-backend.h"
#include "clutter-macros.h"

CLUTTER_AVAILABLE_IN_MUTTER
void clutter_set_custom_backend_func (ClutterBackend *(* func) (void));

#undef __CLUTTER_H_INSIDE__

#endif /* __CLUTTER_MUTTER_H__ */
