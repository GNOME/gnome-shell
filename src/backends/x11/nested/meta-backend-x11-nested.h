/*
 * Copyright (C) 2017 Red Hat
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

#ifndef META_BACKEND_X11_NESTED_H
#define META_BACKEND_X11_NESTED_H

#include <glib-object.h>

#include "backends/x11/meta-backend-x11.h"

#define META_TYPE_BACKEND_X11_NESTED (meta_backend_x11_nested_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaBackendX11Nested, meta_backend_x11_nested,
                          META, BACKEND_X11_NESTED, MetaBackendX11)

struct _MetaBackendX11NestedClass
{
  MetaBackendX11Class parent_class;
};

#endif /* META_BACKEND_X11_NESTED_H */
