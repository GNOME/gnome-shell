/* Metacity utilities */

/* 
 * Copyright (C) 2001 Havoc Pennington
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

#include "util.h"
#include "main.h"
#include "display.h"

#include <stdio.h>
#include <stdlib.h>

static gboolean is_verbose = FALSE;
static gboolean is_debugging = FALSE;
static gboolean is_syncing = FALSE;

gboolean
meta_is_verbose (void)
{
  return is_verbose;
}

void
meta_set_verbose (gboolean setting)
{
  is_verbose = setting;
}

gboolean
meta_is_debugging (void)
{
  return is_debugging;
}

void
meta_set_debugging (gboolean setting)
{
  is_debugging = setting;
}


gboolean
meta_is_syncing (void)
{
  return is_syncing;
}

void
meta_set_syncing (gboolean setting)
{
  if (setting != is_syncing)
    {
      GSList *tmp;
      
      is_syncing = setting;

      tmp = meta_displays_list ();
      while (tmp != NULL)
        {
          MetaDisplay *display = tmp->data;
          XSynchronize (display->xdisplay, is_syncing);
          tmp = tmp->next;
        }
    }
}

void
meta_debug_spew (const char *format, ...)
{
  va_list args;
  gchar *str;

  g_return_if_fail (format != NULL);

  if (!is_debugging)
    return;
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  fputs ("Window manager: ", stderr);
  fputs (str, stderr);
  
  g_free (str);
}

void
meta_verbose (const char *format, ...)
{
  va_list args;
  gchar *str;

  g_return_if_fail (format != NULL);

  if (!is_verbose)
    return;
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  fputs ("Window manager: ", stderr);
  fputs (str, stderr);
  
  g_free (str);
}

void
meta_bug (const char *format, ...)
{
  va_list args;
  gchar *str;

  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  fputs ("Bug in window manager: ", stderr);
  fputs (str, stderr);
  
  g_free (str);

  /* stop us in a debugger */
  abort ();
}

void
meta_warning (const char *format, ...)
{
  va_list args;
  gchar *str;

  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  fputs ("Window manager: ", stderr);
  fputs (str, stderr);
  
  g_free (str);
}

void
meta_fatal (const char *format, ...)
{
  va_list args;
  gchar *str;

  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  fputs ("Window manager: ", stderr);
  fputs (str, stderr);
  
  g_free (str);

  meta_exit (META_EXIT_ERROR);
}
