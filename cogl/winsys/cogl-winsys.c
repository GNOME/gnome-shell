/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"

#include <gmodule.h>

uint32_t
_cogl_winsys_error_quark (void)
{
  return g_quark_from_static_string ("cogl-winsys-error-quark");
}

/* FIXME: we should distinguish renderer and context features */
CoglBool
_cogl_winsys_has_feature (CoglWinsysFeature feature)
{
  _COGL_GET_CONTEXT (ctx, FALSE);

  return COGL_FLAGS_GET (ctx->winsys_features, feature);
}
