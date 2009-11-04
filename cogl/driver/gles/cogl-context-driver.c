/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context.h"
#include "cogl-gles2-wrapper.h"

void
_cogl_create_context_driver (CoglContext *context)
{
  context->drv.pf_glGenRenderbuffers = NULL;
  context->drv.pf_glBindRenderbuffer = NULL;
  context->drv.pf_glRenderbufferStorage = NULL;
  context->drv.pf_glGenFramebuffers = NULL;
  context->drv.pf_glBindFramebuffer = NULL;
  context->drv.pf_glFramebufferTexture2D = NULL;
  context->drv.pf_glFramebufferRenderbuffer = NULL;
  context->drv.pf_glCheckFramebufferStatus = NULL;
  context->drv.pf_glDeleteFramebuffers = NULL;

  /* Init the GLES2 wrapper */
#ifdef HAVE_COGL_GLES2
  cogl_gles2_wrapper_init (&context->drv.gles2);
#endif
}

