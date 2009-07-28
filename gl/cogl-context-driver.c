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

void
_cogl_create_context_driver (CoglContext *_context)
{
  _context->drv.pf_glGenRenderbuffersEXT = NULL;
  _context->drv.pf_glBindRenderbufferEXT = NULL;
  _context->drv.pf_glRenderbufferStorageEXT = NULL;
  _context->drv.pf_glGenFramebuffersEXT = NULL;
  _context->drv.pf_glBindFramebufferEXT = NULL;
  _context->drv.pf_glFramebufferTexture2DEXT = NULL;
  _context->drv.pf_glFramebufferRenderbufferEXT = NULL;
  _context->drv.pf_glCheckFramebufferStatusEXT = NULL;
  _context->drv.pf_glDeleteFramebuffersEXT = NULL;
  _context->drv.pf_glBlitFramebufferEXT = NULL;
  _context->drv.pf_glRenderbufferStorageMultisampleEXT = NULL;

  _context->drv.pf_glCreateProgramObjectARB = NULL;
  _context->drv.pf_glCreateShaderObjectARB = NULL;
  _context->drv.pf_glShaderSourceARB = NULL;
  _context->drv.pf_glCompileShaderARB = NULL;
  _context->drv.pf_glAttachObjectARB = NULL;
  _context->drv.pf_glLinkProgramARB = NULL;
  _context->drv.pf_glUseProgramObjectARB = NULL;
  _context->drv.pf_glGetUniformLocationARB = NULL;
  _context->drv.pf_glDeleteObjectARB = NULL;
  _context->drv.pf_glGetInfoLogARB = NULL;
  _context->drv.pf_glGetObjectParameterivARB = NULL;
  _context->drv.pf_glUniform1fARB = NULL;
  _context->drv.pf_glUniform2fARB = NULL;
  _context->drv.pf_glUniform3fARB = NULL;
  _context->drv.pf_glUniform4fARB = NULL;
  _context->drv.pf_glUniform1fvARB = NULL;
  _context->drv.pf_glUniform2fvARB = NULL;
  _context->drv.pf_glUniform3fvARB = NULL;
  _context->drv.pf_glUniform4fvARB = NULL;
  _context->drv.pf_glUniform1iARB = NULL;
  _context->drv.pf_glUniform2iARB = NULL;
  _context->drv.pf_glUniform3iARB = NULL;
  _context->drv.pf_glUniform4iARB = NULL;
  _context->drv.pf_glUniform1ivARB = NULL;
  _context->drv.pf_glUniform2ivARB = NULL;
  _context->drv.pf_glUniform3ivARB = NULL;
  _context->drv.pf_glUniform4ivARB = NULL;
  _context->drv.pf_glUniformMatrix2fvARB = NULL;
  _context->drv.pf_glUniformMatrix3fvARB = NULL;
  _context->drv.pf_glUniformMatrix4fvARB = NULL;

  _context->drv.pf_glDrawRangeElements = NULL;
  _context->drv.pf_glActiveTexture = NULL;
  _context->drv.pf_glClientActiveTexture = NULL;

  _context->drv.pf_glBlendFuncSeparate = NULL;
  _context->drv.pf_glBlendEquationSeparate = NULL;


}
