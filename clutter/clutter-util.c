/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

/**
 * SECTION:clutter-util
 * @short_description: Misc utility functions.
 *
 * Various misc utilility functions.
 */


#include "clutter-util.h"
#include "clutter-main.h"

static int TrappedErrorCode = 0;
static int (*old_error_handler) (Display *, XErrorEvent *);

static int
error_handler(Display     *xdpy,
	      XErrorEvent *error)
{
  TrappedErrorCode = error->error_code;
  return 0;
}

void
clutter_util_trap_x_errors(void)
{
  TrappedErrorCode  = 0;
  old_error_handler = XSetErrorHandler(error_handler);
}

int
clutter_util_untrap_x_errors(void)
{
  XSetErrorHandler(old_error_handler);
  return TrappedErrorCode;
}

int 
clutter_util_next_p2 (int a)
{
  int rval=1;

  while(rval < a) 
    rval <<= 1;

  return rval;
}

#if 0
gboolean
clutter_util_can_create_texture (int width, int height)
{
  GLint new_width;

  glTexImage2D (GL_PROXY_VIDEO_TEXTURE_2D, 0, GL_RGBA,
                width, height, 0 /* border */,
                GL_RGBA, PIXEL_TYPE, NULL);

  glGetTexLevelParameteriv (GL_PROXY_VIDEO_TEXTURE_2D, 0,
                            GL_VIDEO_TEXTURE_WIDTH, &new_width);

  return new_width != 0;
}
#endif
