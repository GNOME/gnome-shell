/* msm utils */

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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>


void
msm_fatal (const char *format, ...)
{
  va_list args;
  gchar *str;
  
  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  fputs ("Session manager: ", stderr);
  fputs (str, stderr);

  fflush (stderr);
  
  g_free (str);

  exit (1);
}


void
msm_warning (const char *format, ...)
{
  va_list args;
  gchar *str;
  
  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  fputs ("Session manager: ", stderr);
  fputs (str, stderr);

  fflush (stderr);
  
  g_free (str);

  exit (1);
}
     
gboolean
msm_create_dir_and_parents (const char *dir,
                            int         mode,
                            GError    **error)
{
  char *parent;
  GSList *parents;
  GSList *tmp;

  /* This function is crap; GNU fileutils has some really
   * robust code that does this.
   */
  
  parents = NULL;
  parent = g_path_get_dirname (dir);
  while (parent && parent[0] &&
         strcmp (parent, ".") != 0 &&
         strcmp (parent, "..") != 0 &&
         strcmp (parent, "/") != 0 &&
         /* an optimization since we will normally be using a homedir */
         strcmp (parent, g_get_home_dir ()) != 0)
    {
      parents = g_slist_prepend (parents, parent);
      parent = g_path_get_dirname (parent);
    }

  /* Errors are a bit tricky; if we can't create /foo because
   * we lack write perms, and can't create /foo/bar because it exists,
   * but can create /foo/bar/baz, then it's not really an error.
   *
   * We more or less punt, and just display an error for the last mkdir.
   */
  tmp = parents;
  while (tmp != NULL)
    {
      mkdir (tmp->data, mode);

      g_free (tmp->data);
      
      tmp = tmp->next;
    }

  g_slist_free (parents);

  if (mkdir (dir, mode) < 0)
    {
      if (errno != EEXIST)
        {
          g_set_error (error, 
                       G_FILE_ERROR,
                       g_file_error_from_errno (errno),
                       _("Failed to create directory '%s': %s\n"),
                       dir, g_strerror (errno));
          return FALSE;
        }
    }

  return TRUE;
}

const char*
msm_get_work_directory (void)
{
  static const char *dir = NULL;

  if (dir == NULL)
    {
      dir = g_getenv ("SM_SAVE_DIR");
      if (dir == NULL)
        dir = g_strconcat (g_get_home_dir (), "/.msm", NULL);
    }

  /* ignore errors here, we'll catch them later when we
   * try to use the dir
   */
  msm_create_dir_and_parents (dir, 0700, NULL);
  
  return dir;
}

char*
msm_non_glib_strdup (const char *str)
{
  char *new_str;

  if (str)
    {
      new_str = msm_non_glib_malloc (strlen (str) + 1);
      strcpy (new_str, str);
    }
  else
    new_str = NULL;

  return new_str;
}

void*
msm_non_glib_malloc (int bytes)
{
  void *ptr;

  ptr = malloc (bytes);
  if (ptr == NULL)
    g_error ("Failed to allocate %d bytes\n", bytes);

  return ptr;
}
