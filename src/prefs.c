/* Metacity preferences */

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

#include <config.h>
#include "prefs.h"
#include "util.h"
#include <gconf/gconf-client.h>
#include <string.h>

/* If you add a key, it needs updating in init() and in the gconf
 * notify listener and of course in the .schemas file
 */
#define KEY_FOCUS_MODE "/apps/metacity/general/focus_mode"
#define KEY_THEME "/apps/metacity/general/theme"
#define KEY_USE_DESKTOP_FONT  "/apps/metacity/general/titlebar_uses_desktop_font"
#define KEY_TITLEBAR_FONT "/apps/metacity/general/titlebar_font"
#define KEY_TITLEBAR_FONT_SIZE "/apps/metacity/general/titlebar_font_size"
#define KEY_NUM_WORKSPACES "/apps/metacity/general/num_workspaces"
#define KEY_APPLICATION_BASED "/apps/metacity/general/application_based"

static GConfClient *default_client = NULL;
static GList *listeners = NULL;
static GList *changes = NULL;
static guint changed_idle;
static gboolean use_desktop_font = TRUE;
static PangoFontDescription *titlebar_font = NULL;
static int titlebar_font_size = 0;
static MetaFocusMode focus_mode = META_FOCUS_MODE_CLICK;
static char* current_theme = NULL;
static int num_workspaces = 4;
static gboolean application_based = FALSE;

static gboolean update_use_desktop_font   (gboolean    value);
static gboolean update_titlebar_font      (const char *value);
static gboolean update_titlebar_font_size (int         value);
static gboolean update_focus_mode         (const char *value);
static gboolean update_theme              (const char *value);
static gboolean update_num_workspaces     (int         value);
static gboolean update_application_based  (gboolean    value);

static void queue_changed (MetaPreference  pref);
static void change_notify (GConfClient    *client,
                           guint           cnxn_id,
                           GConfEntry     *entry,
                           gpointer        user_data);




typedef struct
{
  MetaPrefsChangedFunc func;
  gpointer data;
} MetaPrefsListener;

void
meta_prefs_add_listener (MetaPrefsChangedFunc func,
                         gpointer             data)
{
  MetaPrefsListener *l;

  l = g_new (MetaPrefsListener, 1);
  l->func = func;
  l->data = data;

  listeners = g_list_prepend (listeners, l);
}

void
meta_prefs_remove_listener (MetaPrefsChangedFunc func,
                            gpointer             data)
{
  GList *tmp;

  tmp = listeners;
  while (tmp != NULL)
    {
      MetaPrefsListener *l = tmp->data;

      if (l->func == func &&
          l->data == data)
        {
          g_free (l);
          listeners = g_list_delete_link (listeners, tmp);

          return;
        }
      
      tmp = tmp->next;
    }

  meta_bug ("Did not find listener to remove\n");
}

static void
emit_changed (MetaPreference pref)
{
  GList *tmp;
  GList *copy;

  meta_verbose ("Notifying listeners that pref %s changed\n",
                meta_preference_to_string (pref));
  
  copy = g_list_copy (listeners);
  
  tmp = copy;
  while (tmp != NULL)
    {
      MetaPrefsListener *l = tmp->data;

      (* l->func) (pref, l->data);
      
      tmp = tmp->next;
    }

  g_list_free (copy);
}

static gboolean
changed_idle_handler (gpointer data)
{
  GList *tmp;
  GList *copy;

  changed_idle = 0;
  
  copy = g_list_copy (changes); /* reentrancy paranoia */

  g_list_free (changes);
  changes = NULL;
  
  tmp = copy;
  while (tmp != NULL)
    {
      MetaPreference pref = GPOINTER_TO_INT (tmp->data);

      emit_changed (pref);
      
      tmp = tmp->next;
    }

  g_list_free (copy);
  
  return FALSE;
}

static void
queue_changed (MetaPreference pref)
{
  meta_verbose ("Queueing change of pref %s\n",
                meta_preference_to_string (pref));  
  
  if (g_list_find (changes, GINT_TO_POINTER (pref)) == NULL)
    changes = g_list_prepend (changes, GINT_TO_POINTER (pref));
  else
    meta_verbose ("Change of pref %s was already pending\n",
                  meta_preference_to_string (pref));
  
  if (changed_idle == 0)
    changed_idle = g_idle_add (changed_idle_handler, NULL);
}

static void
cleanup_error (GError **error)
{
  if (*error)
    {
      meta_warning ("%s\n", (*error)->message);
      
      g_error_free (*error);
      *error = NULL;
    }
}

void
meta_prefs_init (void)
{
  GError *err = NULL;
  char *str_val;
  int int_val;
  gboolean bool_val;
  
  if (default_client != NULL)
    return;
  
  /* returns a reference which we hold forever */
  default_client = gconf_client_get_default ();

  gconf_client_add_dir (default_client, "/apps/metacity",
                        GCONF_CLIENT_PRELOAD_RECURSIVE,
                        &err);
  cleanup_error (&err);

  str_val = gconf_client_get_string (default_client, KEY_FOCUS_MODE,
                                     &err);
  cleanup_error (&err);
  update_focus_mode (str_val);
  g_free (str_val);  

  str_val = gconf_client_get_string (default_client, KEY_THEME,
                                     &err);
  cleanup_error (&err);
  update_theme (str_val);
  g_free (str_val);
  
  /* If the keys aren't set in the database, we use essentially
   * bogus values instead of any kind of default. This is
   * just lazy. But they keys ought to be set, anyhow.
   */
  
  bool_val = gconf_client_get_bool (default_client, KEY_USE_DESKTOP_FONT,
                                    &err);
  cleanup_error (&err);
  update_use_desktop_font (bool_val);
  
  int_val = gconf_client_get_int (default_client, KEY_TITLEBAR_FONT_SIZE,
                                  &err);
  cleanup_error (&err);
  update_titlebar_font_size (int_val);
  
  str_val = gconf_client_get_string (default_client, KEY_TITLEBAR_FONT,
                                     &err);
  cleanup_error (&err);
  update_titlebar_font (str_val);
  g_free (str_val);

  int_val = gconf_client_get_int (default_client, KEY_NUM_WORKSPACES,
                                  &err);
  cleanup_error (&err);
  update_num_workspaces (int_val);

  bool_val = gconf_client_get_bool (default_client, KEY_APPLICATION_BASED,
                                    &err);
  cleanup_error (&err);
  update_application_based (bool_val);
  
  gconf_client_notify_add (default_client, "/apps/metacity",
                           change_notify,
                           NULL,
                           NULL,
                           &err);
  cleanup_error (&err);  
}

static void
change_notify (GConfClient    *client,
               guint           cnxn_id,
               GConfEntry     *entry,
               gpointer        user_data)
{
  const char *key;
  GConfValue *value;
  
  key = gconf_entry_get_key (entry);
  value = gconf_entry_get_value (entry);
  
  if (strcmp (key, KEY_FOCUS_MODE) == 0)
    {
      const char *str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_FOCUS_MODE);
          goto out;
        }
      
      str = value ? gconf_value_get_string (value) : NULL;

      if (update_focus_mode (str))
        queue_changed (META_PREF_FOCUS_MODE);
    }
  if (strcmp (key, KEY_THEME) == 0)
    {
      const char *str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_THEME);
          goto out;
        }
      
      str = value ? gconf_value_get_string (value) : NULL;

      if (update_theme (str))
        queue_changed (META_PREF_THEME);
    }
  else if (strcmp (key, KEY_TITLEBAR_FONT) == 0)
    {
      const char *str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_TITLEBAR_FONT);
          goto out;
        }
      
      str = value ? gconf_value_get_string (value) : NULL;

      if (update_titlebar_font (str))
        queue_changed (META_PREF_TITLEBAR_FONT);
    }
  else if (strcmp (key, KEY_TITLEBAR_FONT_SIZE) == 0)
    {
      int d;

      if (value && value->type != GCONF_VALUE_INT)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_TITLEBAR_FONT_SIZE);
          goto out;
        }        
      
      d = value ? gconf_value_get_int (value) : 0;

      if (update_titlebar_font_size (d))
        queue_changed (META_PREF_TITLEBAR_FONT_SIZE);
    }
  else if (strcmp (key, KEY_USE_DESKTOP_FONT) == 0)
    {
      gboolean b;

      if (value && value->type != GCONF_VALUE_BOOL)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_USE_DESKTOP_FONT);
          goto out;
        }

      b = value ? gconf_value_get_bool (value) : TRUE;
      
      /* There's no external pref for this, it just affects whether
       * get_titlebar_font returns NULL, so that's what we queue
       * the change on
       */
      if (update_use_desktop_font (b))
        queue_changed (META_PREF_TITLEBAR_FONT);
    }
  else if (strcmp (key, KEY_NUM_WORKSPACES) == 0)
    {
      int d;

      if (value && value->type != GCONF_VALUE_INT)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_NUM_WORKSPACES);
          goto out;
        }

      d = value ? gconf_value_get_int (value) : num_workspaces;

      if (update_num_workspaces (d))
        queue_changed (META_PREF_NUM_WORKSPACES);
    }
  else if (strcmp (key, KEY_APPLICATION_BASED) == 0)
    {
      gboolean b;

      if (value && value->type != GCONF_VALUE_BOOL)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_APPLICATION_BASED);
          goto out;
        }

      b = value ? gconf_value_get_bool (value) : application_based;

      if (update_application_based (b))
        queue_changed (META_PREF_APPLICATION_BASED);
    }
  else
    {
      meta_verbose ("Key %s doesn't mean anything to Metacity\n",
                    key);
    }
  
 out:
  /* nothing */
}

static gboolean
update_focus_mode (const char *value)
{
  MetaFocusMode old_mode = focus_mode;
  
  if (value != NULL)
    {
      if (g_ascii_strcasecmp (value, "click") == 0)
        focus_mode = META_FOCUS_MODE_CLICK;
      else if (g_ascii_strcasecmp (value, "sloppy") == 0)
        focus_mode = META_FOCUS_MODE_SLOPPY;
      else if (g_ascii_strcasecmp (value, "mouse") == 0)
        focus_mode = META_FOCUS_MODE_MOUSE;
      else
        meta_warning (_("GConf key '%s' is set to an invalid value"),
                      KEY_FOCUS_MODE);
    }

  return (old_mode != focus_mode);
}

static gboolean
update_theme (const char *value)
{
  char *old_theme;
  gboolean changed;
  
  old_theme = current_theme;
  
  if (value != NULL && *value)
    {
      current_theme = g_strdup (value);
    }

  changed = TRUE;
  if ((old_theme && current_theme &&
       strcmp (old_theme, current_theme) == 0) ||
      (old_theme == NULL && current_theme == NULL))
    changed = FALSE;

  if (old_theme != current_theme)
    g_free (old_theme);

  if (current_theme == NULL)
    {
      /* Fallback crackrock */
      current_theme = g_strdup ("Atlanta");
      changed = TRUE;
    }
  
  return changed;
}

MetaFocusMode
meta_prefs_get_focus_mode (void)
{
  return focus_mode;
}

const char*
meta_prefs_get_theme (void)
{
  return current_theme;
}

static gboolean
update_use_desktop_font (gboolean value)
{
  gboolean old = use_desktop_font;

  use_desktop_font = value;

  return old != value;
}

static gboolean
update_titlebar_font (const char *value)
{
  PangoFontDescription *new_desc;

  new_desc = NULL;

  if (value)
    {
      new_desc = pango_font_description_from_string (value);
      if (new_desc == NULL)
        meta_warning (_("Could not parse font description \"%s\" from GConf key %s\n"),
                      value, KEY_TITLEBAR_FONT);
    }

  if (new_desc && titlebar_font &&
      pango_font_description_equal (new_desc, titlebar_font))
    {
      pango_font_description_free (new_desc);
      return FALSE;
    }
  else
    {
      if (titlebar_font)
        pango_font_description_free (titlebar_font);

      titlebar_font = new_desc;

      return TRUE;
    }
}

const PangoFontDescription*
meta_prefs_get_titlebar_font (void)
{
  if (use_desktop_font)
    return NULL;
  else
    return titlebar_font;
}

static gboolean
update_titlebar_font_size (int value)
{
  int old = titlebar_font_size;

  if (value < 0)
    {
      meta_warning (_("%d stored in GConf key %s is not a valid font size\n"),
                    value, KEY_TITLEBAR_FONT_SIZE);
      value = 0;
    }
  
  titlebar_font_size = value;

  return old != titlebar_font_size;
}

int
meta_prefs_get_titlebar_font_size (void)
{
  return titlebar_font_size;
}


#define MAX_REASONABLE_WORKSPACES 32

static gboolean
update_num_workspaces (int value)
{
  int old = num_workspaces;

  if (value < 1 || value > MAX_REASONABLE_WORKSPACES)
    {
      meta_warning (_("%d stored in GConf key %s is not a reasonable number of workspaces, current maximum is %d\n"),
                    value, KEY_NUM_WORKSPACES, MAX_REASONABLE_WORKSPACES);
      if (value < 1)
        value = 1;
      else if (value > MAX_REASONABLE_WORKSPACES)
        value = MAX_REASONABLE_WORKSPACES;
    }
  
  num_workspaces = value;

  return old != num_workspaces;
}

int
meta_prefs_get_num_workspaces (void)
{
  return num_workspaces;
}

static gboolean
update_application_based (gboolean value)
{
  gboolean old = application_based;

  application_based = value;

  return old != application_based;
}

gboolean
meta_prefs_get_application_based (void)
{
  return application_based;
}

const char*
meta_preference_to_string (MetaPreference pref)
{
  switch (pref)
    {
    case META_PREF_FOCUS_MODE:
      return "FOCUS_MODE";
      break;

    case META_PREF_THEME:
      return "THEME";
      break;

    case META_PREF_TITLEBAR_FONT:
      return "TITLEBAR_FONT";
      break;

    case META_PREF_TITLEBAR_FONT_SIZE:
      return "TITLEBAR_FONT_SIZE";
      break;

    case META_PREF_NUM_WORKSPACES:
      return "NUM_WORKSPACES";
      break;

    case META_PREF_APPLICATION_BASED:
      return "APPLICATION_BASED";
      break;
    }

  return "(unknown)";
}
