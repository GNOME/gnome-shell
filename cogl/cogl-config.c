/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-debug.h"
#include "cogl-config-private.h"

#include <glib.h>

char *_cogl_config_driver;
char *_cogl_config_renderer;

static void
_cogl_config_process (GKeyFile *key_file)
{
  char *value;

  value = g_key_file_get_string (key_file, "global", "COGL_DEBUG", NULL);
  if (value)
    {
      _cogl_parse_debug_string (value,
                                TRUE /* enable the flags */,
                                TRUE /* ignore help option */);
      g_free (value);
    }

  value = g_key_file_get_string (key_file, "global", "COGL_NO_DEBUG", NULL);
  if (value)
    {
      _cogl_parse_debug_string (value,
                                FALSE /* disable the flags */,
                                TRUE /* ignore help option */);
      g_free (value);
    }

  value = g_key_file_get_string (key_file, "global", "COGL_DRIVER", NULL);
  if (value)
    {
      if (_cogl_config_driver)
        g_free (_cogl_config_driver);

      _cogl_config_driver = value;
    }

  value = g_key_file_get_string (key_file, "global", "COGL_RENDERER", NULL);
  if (value)
    {
      if (_cogl_config_renderer)
        g_free (_cogl_config_renderer);

      _cogl_config_renderer = value;
    }
}

void
_cogl_config_read (void)
{
  GKeyFile *key_file = g_key_file_new ();
  const char * const *system_dirs = g_get_system_config_dirs ();
  char *filename;
  CoglBool status = FALSE;
  int i;

  for (i = 0; system_dirs[i]; i++)
    {
      filename = g_build_filename (system_dirs[i], "cogl", "cogl.conf", NULL);
      status = g_key_file_load_from_file (key_file,
                                          filename,
                                          0,
                                          NULL);
      g_free (filename);
      if (status)
        {
          _cogl_config_process (key_file);
          g_key_file_free (key_file);
          key_file = g_key_file_new ();
          break;
        }
    }

  filename = g_build_filename (g_get_user_config_dir (), "cogl", "cogl.conf", NULL);
  status = g_key_file_load_from_file (key_file,
                                      filename,
                                      0,
                                      NULL);
  g_free (filename);

  if (status)
    _cogl_config_process (key_file);

  g_key_file_free (key_file);
}
