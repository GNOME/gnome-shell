/* msm SmProp utils */

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

#include "props.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

/* Property functions stolen from gnome-session */

GList*
proplist_find_link_by_name (GList      *list,
                            const char *name)
{
  for (; list; list = list->next)
    {
      SmProp *prop = (SmProp *) list->data;
      if (strcmp (prop->name, name) == 0)
	return list;
    }

  return NULL;
}


SmProp*
proplist_find_by_name (GList *list, const char *name)
{
  GList *ret;

  ret = proplist_find_link_by_name (list, name);

  return ret ? ret->data : NULL;
}

gboolean
proplist_find_card8 (GList *list, const char *name,
		     int *result)
{
  SmProp *prop;

  g_return_val_if_fail (result != NULL, FALSE);

  prop = proplist_find_by_name (list, name);
  if (prop == NULL)
    return FALSE;
  else
    return smprop_get_card8 (prop, result);
}

gboolean
proplist_find_string (GList *list, const char *name,
		      char **result)
{
  SmProp *prop;

  g_return_val_if_fail (result != NULL, FALSE);

  prop = proplist_find_by_name (list, name);
  if (prop == NULL)
    return FALSE;
  else
    return smprop_get_string (prop, result);
}

GList*
proplist_replace (GList        *list,
                  SmProp       *new_prop)
{
  GList *link;
  
  link = proplist_find_link_by_name (list, new_prop->name);
  if (link)
    {
      SmFreeProperty (link->data);
      link->data = new_prop;
    }
  else
    {
      list = g_list_prepend (list, new_prop);
    }

  return list;
}

GList*
proplist_delete (GList        *list,
                 const char   *name)
{
  GList *link;
  
  link = proplist_find_link_by_name (list, name);
  if (link)
    {
      SmFreeProperty (link->data);
      list = g_list_delete_link (list, link);
    }

  return list;
}

GList*
proplist_replace_card8 (GList        *list,
                        const char   *name,
                        int           value)
{
  SmProp *prop;

  prop = smprop_new_card8 (name, value);

  return proplist_replace (list, prop);  
}

GList*
proplist_replace_string (GList        *list,
                         const char   *name,
                         const char   *str,
                         int           len)
{
  SmProp *prop;

  prop = smprop_new_string (name, str, len);

  return proplist_replace (list, prop);
}

GList*
proplist_replace_vector (GList        *list,
                         const char   *name,
                         int           argc,
                         char        **argv)
{
  SmProp *prop;

  prop = smprop_new_vector (name, argc, argv);

  return proplist_replace (list, prop);
}

gboolean
proplist_find_vector (GList *list, const char *name,
		      int *argcp, char ***argvp)
{
  SmProp *prop;

  g_return_val_if_fail (argcp != NULL, FALSE);
  g_return_val_if_fail (argvp != NULL, FALSE);

  prop = proplist_find_by_name (list, name);
  if (prop == NULL)
    return FALSE;
  else
    return smprop_get_vector (prop, argcp, argvp);
}

gboolean
smprop_get_card8  (SmProp   *prop,
                   int      *result)
{
  g_return_val_if_fail (result != NULL, FALSE);

  if (strcmp (prop->type, SmCARD8) == 0)
    {
      char *p;
      p = prop->vals[0].value;
      *result = *p;
      return TRUE;
    }
  else
    return FALSE;
}

gboolean
smprop_get_string (SmProp   *prop,
                   char    **result)
{
  g_return_val_if_fail (result != NULL, FALSE);

  if (strcmp (prop->type, SmARRAY8) == 0)
    {
      *result = g_malloc (prop->vals[0].length + 1);
      memcpy (*result, prop->vals[0].value, prop->vals[0].length);
      (*result)[prop->vals[0].length] = '\0';
      return TRUE;
    }
  else
    return FALSE;
}

gboolean
smprop_get_vector (SmProp   *prop,
                   int      *argcp,
                   char   ***argvp)
{
  g_return_val_if_fail (argcp != NULL, FALSE);
  g_return_val_if_fail (argvp != NULL, FALSE);

  if (strcmp (prop->type, SmLISTofARRAY8) == 0)
    {
      int i;
      
      *argcp = prop->num_vals;
      *argvp = g_new0 (char *, *argcp + 1);
      for (i = 0; i < *argcp; ++i)
        {
          (*argvp)[i] = g_malloc (prop->vals[i].length + 1);
          memcpy ((*argvp)[i], prop->vals[i].value, prop->vals[i].length);
          (*argvp)[i][prop->vals[i].length] = '\0';
        }

      return TRUE;
    }
  else
    return FALSE;
}

SmProp*
smprop_copy (SmProp *prop)
{
  int i;
  SmProp *copy;

  /* This all uses malloc so we can use SmFreeProperty() */
  
  copy = msm_non_glib_malloc (sizeof (SmProp));

  if (prop->name)
    copy->name = msm_non_glib_strdup (prop->name);
  else
    copy->name = NULL;

  if (prop->type)
    copy->type = msm_non_glib_strdup (prop->type);
  else
    copy->type = NULL;

  copy->num_vals = prop->num_vals;
  copy->vals = NULL;

  if (copy->num_vals > 0 && prop->vals)
    {
      copy->vals = msm_non_glib_malloc (sizeof (SmPropValue) * copy->num_vals);
      
      for (i = 0; i < copy->num_vals; i++)
        {
          if (prop->vals[i].value)
            {
              copy->vals[i].length = prop->vals[i].length;
              copy->vals[i].value = msm_non_glib_malloc (copy->vals[i].length);
              memcpy (copy->vals[i].value, prop->vals[i].value,
                      copy->vals[i].length);
            }
          else
            {
              copy->vals[i].length = 0;
              copy->vals[i].value = NULL;
            }
        }
    }

  return copy;
}

SmProp*
smprop_new_vector (const char  *name,
                   int          argc,
                   char       **argv)
{
  SmProp *prop;
  int i;
  
  prop = msm_non_glib_malloc (sizeof (SmProp));
  prop->name = msm_non_glib_strdup (name);
  prop->type = msm_non_glib_strdup (SmLISTofARRAY8);

  prop->num_vals = argc;
  prop->vals = msm_non_glib_malloc (sizeof (SmPropValue) * prop->num_vals);
  i = 0;
  while (i < argc)
    {
      prop->vals[i].length = strlen (argv[i]);
      prop->vals[i].value = msm_non_glib_strdup (argv[i]);
      
      ++i;
    }

  return prop;
}

SmProp*
smprop_new_string (const char  *name,
                   const char  *str,
                   int          len)
{
  SmProp *prop;

  if (len < 0)
    len = strlen (str);
  
  prop = msm_non_glib_malloc (sizeof (SmProp));
  prop->name = msm_non_glib_strdup (name);
  prop->type = msm_non_glib_strdup (SmARRAY8);
  
  prop->num_vals = 1;
  prop->vals = msm_non_glib_malloc (sizeof (SmPropValue) * prop->num_vals);

  prop->vals[0].length = len;
  prop->vals[0].value = msm_non_glib_malloc (len);
  memcpy (prop->vals[0].value, str, len);

  return prop;
}

SmProp*
smprop_new_card8  (const char  *name,
                   int          value)
{
  SmProp *prop;
  
  prop = msm_non_glib_malloc (sizeof (SmProp));
  prop->name = msm_non_glib_strdup (name);
  prop->type = msm_non_glib_strdup (SmARRAY8);

  prop->num_vals = 1;
  prop->vals = msm_non_glib_malloc (sizeof (SmPropValue) * prop->num_vals);

  prop->vals[0].length = 1;
  prop->vals[0].value = msm_non_glib_malloc (1);
  (* (char*)  prop->vals[0].value) = (char) value;

  return prop;
}
