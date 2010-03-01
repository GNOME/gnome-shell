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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"

#ifdef HAVE_CLUTTER_GLX
#include <dlfcn.h>
#include <GL/glx.h>

typedef CoglFuncPtr (*GLXGetProcAddressProc) (const GLubyte *procName);
#endif


CoglFuncPtr
_cogl_winsys_get_proc_address (const char *name)
{
  static GLXGetProcAddressProc get_proc_func = NULL;
  static void                 *dlhand = NULL;

  if (get_proc_func == NULL && dlhand == NULL)
    {
      dlhand = dlopen (NULL, RTLD_LAZY);

      if (!dlhand)
        {
          g_warning ("Failed to dlopen (NULL, RTDL_LAZY): %s", dlerror ());
          return NULL;
        }

      dlerror ();

      get_proc_func =
        (GLXGetProcAddressProc) dlsym (dlhand, "glXGetProcAddress");

      if (dlerror () != NULL)
        {
          get_proc_func =
            (GLXGetProcAddressProc) dlsym (dlhand, "glXGetProcAddressARB");
        }

      if (dlerror () != NULL)
        {
          get_proc_func = NULL;
          g_warning ("failed to bind GLXGetProcAddress "
                     "or GLXGetProcAddressARB");
        }
    }

  if (get_proc_func)
    return get_proc_func ((GLubyte *) name);

  return NULL;
}

