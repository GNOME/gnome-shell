/* Metacity preferences */

/* 
 * Copyright (C) 2001 Havoc Pennington, Copyright (C) 2002 Red Hat Inc.
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
#include "ui.h"
#include "util.h"
#ifdef HAVE_GCONF
#include <gconf/gconf-client.h>
#endif
#include <string.h>
#include <stdlib.h>

#define MAX_REASONABLE_WORKSPACES 32
#define MAX_COMMANDS 32

/* If you add a key, it needs updating in init() and in the gconf
 * notify listener and of course in the .schemas file
 */
#define KEY_MOUSE_BUTTON_MODS "/apps/metacity/general/mouse_button_modifier"
#define KEY_FOCUS_MODE "/apps/metacity/general/focus_mode"
#define KEY_ACTION_DOUBLE_CLICK_TITLEBAR "/apps/metacity/general/action_double_click_titlebar"
#define KEY_AUTO_RAISE "/apps/metacity/general/auto_raise"
#define KEY_AUTO_RAISE_DELAY "/apps/metacity/general/auto_raise_delay"
#define KEY_THEME "/apps/metacity/general/theme"
#define KEY_USE_SYSTEM_FONT  "/apps/metacity/general/titlebar_uses_system_font"
#define KEY_TITLEBAR_FONT "/apps/metacity/general/titlebar_font"
#define KEY_NUM_WORKSPACES "/apps/metacity/general/num_workspaces"
#define KEY_APPLICATION_BASED "/apps/metacity/general/application_based"
#define KEY_DISABLE_WORKAROUNDS "/apps/metacity/general/disable_workarounds"
#define KEY_BUTTON_LAYOUT "/apps/metacity/general/button_layout"

#define KEY_COMMAND_PREFIX "/apps/metacity/keybinding_commands/command_"
#define KEY_SCREEN_BINDINGS_PREFIX "/apps/metacity/global_keybindings"
#define KEY_WINDOW_BINDINGS_PREFIX "/apps/metacity/window_keybindings"

#define KEY_WORKSPACE_NAME_PREFIX "/apps/metacity/workspace_names/name_"

#ifdef HAVE_GCONF
static GConfClient *default_client = NULL;
static GList *changes = NULL;
static guint changed_idle;
#endif
static GList *listeners = NULL;

static gboolean use_system_font = TRUE;
static PangoFontDescription *titlebar_font = NULL;
static MetaVirtualModifier mouse_button_mods = Mod1Mask;
static MetaFocusMode focus_mode = META_FOCUS_MODE_CLICK;
static char* current_theme = NULL;
static int num_workspaces = 4;
static MetaActionDoubleClickTitlebar action_double_click_titlebar =
    META_ACTION_DOUBLE_CLICK_TITLEBAR_TOGGLE_SHADE;
static gboolean application_based = FALSE;
static gboolean disable_workarounds = FALSE;
static gboolean auto_raise = FALSE;
static gboolean auto_raise_delay = 500;
static MetaButtonLayout button_layout = {
  {
    META_BUTTON_FUNCTION_MENU,
    META_BUTTON_FUNCTION_LAST,
    META_BUTTON_FUNCTION_LAST,
    META_BUTTON_FUNCTION_LAST
  },
  {
    META_BUTTON_FUNCTION_MINIMIZE,
    META_BUTTON_FUNCTION_MAXIMIZE,
    META_BUTTON_FUNCTION_CLOSE,
    META_BUTTON_FUNCTION_LAST
  }
};

static char *commands[MAX_COMMANDS] = { NULL, };

static char *workspace_names[MAX_REASONABLE_WORKSPACES] = { NULL, };

#ifdef HAVE_GCONF
static gboolean update_use_system_font   (gboolean    value);
static gboolean update_titlebar_font      (const char *value);
static gboolean update_mouse_button_mods  (const char *value);
static gboolean update_focus_mode         (const char *value);
static gboolean update_theme              (const char *value);
static gboolean update_num_workspaces     (int         value);
static gboolean update_application_based  (gboolean    value);
static gboolean update_disable_workarounds (gboolean   value);
static gboolean update_action_double_click_titlebar (const char *value);
static gboolean update_auto_raise          (gboolean   value);
static gboolean update_auto_raise_delay    (int        value);
static gboolean update_button_layout      (const char *value);
static gboolean update_window_binding     (const char *name,
                                           const char *value);
static gboolean update_screen_binding     (const char *name,
                                           const char *value);
static gboolean update_binding            (MetaKeyPref *binding,
                                           const char  *value);
static gboolean update_command            (const char  *name,
                                           const char  *value);
static gboolean update_workspace_name     (const char  *name,
                                           const char  *value);

static void change_notify (GConfClient    *client,
                           guint           cnxn_id,
                           GConfEntry     *entry,
                           gpointer        user_data);

static char* gconf_key_for_workspace_name (int i);

static void queue_changed (MetaPreference  pref);
#endif /* HAVE_GCONF */

static void     init_bindings             (void);
static void     init_commands             (void);
static void     init_workspace_names      (void);

       
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

#ifdef HAVE_GCONF
static void
emit_changed (MetaPreference pref)
{
  GList *tmp;
  GList *copy;

  meta_topic (META_DEBUG_PREFS, "Notifying listeners that pref %s changed\n",
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
#endif /* HAVE_GCONF */

#ifdef HAVE_GCONF
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
#endif /* HAVE_GCONF */

#ifdef HAVE_GCONF
static void
queue_changed (MetaPreference pref)
{
  meta_topic (META_DEBUG_PREFS, "Queueing change of pref %s\n",
              meta_preference_to_string (pref));  
  
  if (g_list_find (changes, GINT_TO_POINTER (pref)) == NULL)
    changes = g_list_prepend (changes, GINT_TO_POINTER (pref));
  else
    meta_topic (META_DEBUG_PREFS, "Change of pref %s was already pending\n",
                meta_preference_to_string (pref));

  /* add idle at priority below the gconf notify idle */
  if (changed_idle == 0)
    changed_idle = g_idle_add_full (META_PRIORITY_PREFS_NOTIFY,
                                    changed_idle_handler, NULL, NULL);
}
#endif /* HAVE_GCONF */

#ifdef HAVE_GCONF
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
#endif /* HAVE_GCONF */

void
meta_prefs_init (void)
{
#ifdef HAVE_GCONF
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

  str_val = gconf_client_get_string (default_client, KEY_MOUSE_BUTTON_MODS,
                                     &err);
  cleanup_error (&err);
  update_mouse_button_mods (str_val);
  g_free (str_val);
  
  str_val = gconf_client_get_string (default_client, KEY_FOCUS_MODE,
                                     &err);
  cleanup_error (&err);
  update_focus_mode (str_val);
  g_free (str_val);  

  str_val = gconf_client_get_string (default_client,
                                     KEY_ACTION_DOUBLE_CLICK_TITLEBAR,
				     &err);
  cleanup_error (&err);
  update_action_double_click_titlebar (str_val);
  g_free (str_val);

  bool_val = gconf_client_get_bool (default_client, KEY_AUTO_RAISE,
				    &err);
  cleanup_error (&err);
  update_auto_raise (bool_val);

  int_val = gconf_client_get_int (default_client, KEY_AUTO_RAISE_DELAY,
				  &err);
  cleanup_error (&err);
  update_auto_raise_delay (int_val);
  

  str_val = gconf_client_get_string (default_client, KEY_THEME,
                                     &err);
  cleanup_error (&err);
  update_theme (str_val);
  g_free (str_val);
  
  /* If the keys aren't set in the database, we use essentially
   * bogus values instead of any kind of default. This is
   * just lazy. But they keys ought to be set, anyhow.
   */
  
  bool_val = gconf_client_get_bool (default_client, KEY_USE_SYSTEM_FONT,
                                    &err);
  cleanup_error (&err);
  update_use_system_font (bool_val);  
  
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

  bool_val = gconf_client_get_bool (default_client, KEY_DISABLE_WORKAROUNDS,
                                    &err);
  cleanup_error (&err);
  update_disable_workarounds (bool_val);

  str_val = gconf_client_get_string (default_client, KEY_BUTTON_LAYOUT,
                                     &err);
  cleanup_error (&err);
  update_button_layout (str_val);
  g_free (str_val);
#endif /* HAVE_GCONF */
  
  /* Load keybindings prefs */
  init_bindings ();

  /* commands */
  init_commands ();

  /* workspace names */
  init_workspace_names ();

#ifdef HAVE_GCONF
  gconf_client_notify_add (default_client, "/apps/metacity",
                           change_notify,
                           NULL,
                           NULL,
                           &err);
  cleanup_error (&err);
#endif /* HAVE_GCONF */
}

#ifdef HAVE_GCONF
/* from eel */
static gboolean
str_has_prefix (const char *haystack, const char *needle)
{
  const char *h, *n;
  
  /* Eat one character at a time. */
  h = haystack == NULL ? "" : haystack;
  n = needle == NULL ? "" : needle;
  do
    {
      if (*n == '\0') 
        return TRUE;
      if (*h == '\0')
        return FALSE;
    }
  while (*h++ == *n++);

  return FALSE;
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

  if (strcmp (key, KEY_MOUSE_BUTTON_MODS) == 0)
    {
      const char *str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_MOUSE_BUTTON_MODS);
          goto out;
        }
      
      str = value ? gconf_value_get_string (value) : NULL;

      if (update_mouse_button_mods (str))
        queue_changed (META_PREF_MOUSE_BUTTON_MODS);
    }
  else if (strcmp (key, KEY_FOCUS_MODE) == 0)
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
  else if (strcmp (key, KEY_THEME) == 0)
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
  else if (strcmp (key, KEY_USE_SYSTEM_FONT) == 0)
    {
      gboolean b;

      if (value && value->type != GCONF_VALUE_BOOL)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_USE_SYSTEM_FONT);
          goto out;
        }

      b = value ? gconf_value_get_bool (value) : TRUE;
      
      /* There's no external pref for this, it just affects whether
       * get_titlebar_font returns NULL, so that's what we queue
       * the change on
       */
      if (update_use_system_font (b))
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
  else if (strcmp (key, KEY_DISABLE_WORKAROUNDS) == 0)
    {
      gboolean b;

      if (value && value->type != GCONF_VALUE_BOOL)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_APPLICATION_BASED);
          goto out;
        }

      b = value ? gconf_value_get_bool (value) : disable_workarounds;

      if (update_disable_workarounds (b))
        queue_changed (META_PREF_DISABLE_WORKAROUNDS);
    }
  else if (str_has_prefix (key, KEY_WINDOW_BINDINGS_PREFIX))
    {
      const char *str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        key);
          goto out;
        }

      str = value ? gconf_value_get_string (value) : NULL;

      if (update_window_binding (key, str))
        queue_changed (META_PREF_WINDOW_KEYBINDINGS);
    }
  else if (str_has_prefix (key, KEY_SCREEN_BINDINGS_PREFIX))
    {
      const char *str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        key);
          goto out;
        }

      str = value ? gconf_value_get_string (value) : NULL;

      if (update_screen_binding (key, str))
        queue_changed (META_PREF_SCREEN_KEYBINDINGS);
    }
  else if (strcmp (key, KEY_ACTION_DOUBLE_CLICK_TITLEBAR) == 0)
    {
      const char *str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        key);
          goto out;
        }

      str = value ? gconf_value_get_string (value) : NULL;

      if (update_action_double_click_titlebar (str))
        queue_changed (META_PREF_ACTION_DOUBLE_CLICK_TITLEBAR);
    }
  else if (strcmp (key, KEY_AUTO_RAISE) == 0)
    {
      gboolean b;

      if (value && value->type != GCONF_VALUE_BOOL)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_AUTO_RAISE);
          goto out;
        }

      b = value ? gconf_value_get_bool (value) : auto_raise;

      if (update_auto_raise (b))
        queue_changed (META_PREF_AUTO_RAISE);
    }
  else if (strcmp (key, KEY_AUTO_RAISE_DELAY) == 0)
    {
      int d;

      if (value && value->type != GCONF_VALUE_INT)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_AUTO_RAISE_DELAY);
          goto out;
        }        
      
      d = value ? gconf_value_get_int (value) : 0;

      if (update_auto_raise_delay (d))
        queue_changed (META_PREF_AUTO_RAISE_DELAY);
    
    }
  else if (str_has_prefix (key, KEY_COMMAND_PREFIX))
    {
      const char *str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        key);
          goto out;
        }

      str = value ? gconf_value_get_string (value) : NULL;

      if (update_command (key, str))
        queue_changed (META_PREF_COMMANDS);
    }
  else if (str_has_prefix (key, KEY_WORKSPACE_NAME_PREFIX))
    {
      const char *str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        key);
          goto out;
        }

      str = value ? gconf_value_get_string (value) : NULL;

      if (update_workspace_name (key, str))
        queue_changed (META_PREF_WORKSPACE_NAMES);
    }
  else if (strcmp (key, KEY_BUTTON_LAYOUT) == 0)
    {
      const char *str;
      
      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_BUTTON_LAYOUT);
          goto out;
        }
      
      str = value ? gconf_value_get_string (value) : NULL;

      if (update_button_layout (str))
        queue_changed (META_PREF_BUTTON_LAYOUT);
    }
  else
    {
      meta_topic (META_DEBUG_PREFS, "Key %s doesn't mean anything to Metacity\n",
                  key);
    }
  
 out:
  /* nothing */
  return; /* AIX compiler wants something after a label like out: */
}
#endif /* HAVE_GCONF */

#ifdef HAVE_GCONF
static gboolean
update_mouse_button_mods (const char *value)
{
  MetaVirtualModifier old_mods = mouse_button_mods;
  
  if (value != NULL)
    {
      MetaVirtualModifier mods;
  
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Mouse button modifier has new gconf value \"%s\"\n",
                  value ? value : "none");
  
      if (meta_ui_parse_modifier (value, &mods))
        {
          mouse_button_mods = mods;
        }
      else
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Failed to parse new gconf value\n");
          
          meta_warning (_("\"%s\" found in configuration database is not a valid value for mouse button modifier\n"),
                        value);
        }
    }

  return old_mods != mouse_button_mods;
}
#endif /* HAVE_GCONF */

#ifdef HAVE_GCONF
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
        meta_warning (_("GConf key '%s' is set to an invalid value\n"),
                      KEY_FOCUS_MODE);
    }

  return (old_mode != focus_mode);
}
#endif /* HAVE_GCONF */

#ifdef HAVE_GCONF
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
#endif /* HAVE_GCONF */

MetaVirtualModifier
meta_prefs_get_mouse_button_mods  (void)
{
  return mouse_button_mods;
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

#ifdef HAVE_GCONF
static gboolean
update_use_system_font (gboolean value)
{
  gboolean old = use_system_font;

  use_system_font = value;

  return old != value;
}
#endif /* HAVE_GCONF */

#ifdef HAVE_GCONF
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
#endif /* HAVE_GCONF */

#ifdef HAVE_GCONF
static gboolean
button_layout_equal (const MetaButtonLayout *a,
                     const MetaButtonLayout *b)
{  
  int i;

  i = 0;
  while (i < MAX_BUTTONS_PER_CORNER)
    {
      if (a->left_buttons[i] != b->left_buttons[i])
        return FALSE;
      if (a->right_buttons[i] != b->right_buttons[i])
        return FALSE;
      ++i;
    }

  return TRUE;
}

static MetaButtonFunction
button_function_from_string (const char *str)
{
  if (strcmp (str, "menu") == 0)
    return META_BUTTON_FUNCTION_MENU;
  else if (strcmp (str, "minimize") == 0)
    return META_BUTTON_FUNCTION_MINIMIZE;
  else if (strcmp (str, "maximize") == 0)
    return META_BUTTON_FUNCTION_MAXIMIZE;
  else if (strcmp (str, "close") == 0)
    return META_BUTTON_FUNCTION_CLOSE;
  else
    return META_BUTTON_FUNCTION_LAST;
}

static gboolean
update_button_layout (const char *value)
{
  MetaButtonLayout new_layout;
  char **sides;
  int i;
  gboolean changed;
  
  if (value == NULL)
    return FALSE;
  
  i = 0;
  while (i < MAX_BUTTONS_PER_CORNER)
    {
      new_layout.left_buttons[i] = META_BUTTON_FUNCTION_LAST;
      new_layout.right_buttons[i] = META_BUTTON_FUNCTION_LAST;
      ++i;
    }

  /* We need to ignore unknown button functions, for
   * compat with future versions
   */
  
  sides = g_strsplit (value, ":", 2);

  if (sides[0] != NULL)
    {
      char **buttons;
      int b;
      gboolean used[META_BUTTON_FUNCTION_LAST];

      i = 0;
      while (i < META_BUTTON_FUNCTION_LAST)
        {
          used[i] = FALSE;
          ++i;
        }
      
      buttons = g_strsplit (sides[0], ",", -1);
      i = 0;
      b = 0;
      while (buttons[b] != NULL)
        {
          MetaButtonFunction f = button_function_from_string (buttons[b]);

          if (f != META_BUTTON_FUNCTION_LAST && !used[f])
            {
              new_layout.left_buttons[i] = f;
              used[f] = TRUE;
              ++i;
            }
          else
            {
              meta_topic (META_DEBUG_PREFS, "Ignoring unknown or already-used button name \"%s\"\n",
                          buttons[b]);
            }
          
          ++b;
        }

      g_strfreev (buttons);
    }

  if (sides[0] != NULL && sides[1] != NULL)
    {
      char **buttons;
      int b;
      gboolean used[META_BUTTON_FUNCTION_LAST];

      i = 0;
      while (i < META_BUTTON_FUNCTION_LAST)
        {
          used[i] = FALSE;
          ++i;
        }
      
      buttons = g_strsplit (sides[1], ",", -1);
      i = 0;
      b = 0;
      while (buttons[b] != NULL)
        {
          MetaButtonFunction f = button_function_from_string (buttons[b]);

          if (f != META_BUTTON_FUNCTION_LAST && !used[f])
            {
              new_layout.right_buttons[i] = f;
              used[f] = TRUE;
              ++i;
            }
          else
            {
              meta_topic (META_DEBUG_PREFS, "Ignoring unknown or already-used button name \"%s\"\n",
                          buttons[b]);
            }
          
          ++b;
        }

      g_strfreev (buttons);
    }

  g_strfreev (sides);
  
  changed = !button_layout_equal (&button_layout, &new_layout);

  button_layout = new_layout;

  return changed;
}
#endif /* HAVE_GCONF */

const PangoFontDescription*
meta_prefs_get_titlebar_font (void)
{
  if (use_system_font)
    return NULL;
  else
    return titlebar_font;
}

#ifdef HAVE_GCONF
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
#endif /* HAVE_GCONF */

int
meta_prefs_get_num_workspaces (void)
{
  return num_workspaces;
}

#ifdef HAVE_GCONF
static gboolean
update_application_based (gboolean value)
{
  gboolean old = application_based;

  /* DISABLE application_based feature for now */
#if 0
  application_based = value;
#else
  application_based = FALSE;
#endif

  return old != application_based;
}
#endif /* HAVE_GCONF */

gboolean
meta_prefs_get_application_based (void)
{
  return FALSE; /* For now, we never want this to do anything */
  
  return application_based;
}

#ifdef HAVE_GCONF
static gboolean
update_disable_workarounds (gboolean value)
{
  gboolean old = disable_workarounds;

  disable_workarounds = value;

  {
    static gboolean first_disable = TRUE;
    
    if (disable_workarounds && first_disable)
      {
        first_disable = FALSE;

        meta_warning (_("Workarounds for broken applications disabled. Some applications may not behave properly.\n"));
      }
  }
  
  return old != disable_workarounds;
}
#endif /* HAVE_GCONF */

gboolean
meta_prefs_get_disable_workarounds (void)
{
  return disable_workarounds;
}

#ifdef HAVE_GCONF
static MetaActionDoubleClickTitlebar
action_double_click_titlebar_from_string (const char *str)
{
  if (strcmp (str, "toggle_shade") == 0)
    return META_ACTION_DOUBLE_CLICK_TITLEBAR_TOGGLE_SHADE;
  else if (strcmp (str, "toggle_maximize") == 0)
    return META_ACTION_DOUBLE_CLICK_TITLEBAR_TOGGLE_MAXIMIZE;
  else
    return META_ACTION_DOUBLE_CLICK_TITLEBAR_LAST;
}

static gboolean
update_action_double_click_titlebar (const char *value)
{
  MetaActionDoubleClickTitlebar old_action = action_double_click_titlebar;
  
  if (value != NULL)
    {
      action_double_click_titlebar = action_double_click_titlebar_from_string (value);

      if (action_double_click_titlebar == META_ACTION_DOUBLE_CLICK_TITLEBAR_LAST)
        {
          action_double_click_titlebar = old_action;
          meta_warning (_("GConf key '%s' is set to an invalid value\n"),
                        KEY_ACTION_DOUBLE_CLICK_TITLEBAR);
        }
    }

  return (old_action != action_double_click_titlebar);
}

static gboolean
update_auto_raise (gboolean value)
{
  gboolean old = auto_raise;

  auto_raise = value;

  return old != auto_raise;
}

#define MAX_REASONABLE_AUTO_RAISE_DELAY 10000
  
static gboolean
update_auto_raise_delay (int value)
{
  int old = auto_raise_delay;

  if (value < 0 || value > MAX_REASONABLE_AUTO_RAISE_DELAY)
    {
      meta_warning (_("%d stored in GConf key %s is out of range 0 to %d\n"),
                    value, KEY_AUTO_RAISE_DELAY, 
		    MAX_REASONABLE_AUTO_RAISE_DELAY);
      value = 0;
    }
  
  auto_raise_delay = value;

  return old != auto_raise_delay;
}
#endif /* HAVE_GCONF */

#ifdef WITH_VERBOSE_MODE
const char*
meta_preference_to_string (MetaPreference pref)
{
  switch (pref)
    {
    case META_PREF_MOUSE_BUTTON_MODS:
      return "MOUSE_BUTTON_MODS";

    case META_PREF_FOCUS_MODE:
      return "FOCUS_MODE";

    case META_PREF_THEME:
      return "THEME";

    case META_PREF_TITLEBAR_FONT:
      return "TITLEBAR_FONT";

    case META_PREF_NUM_WORKSPACES:
      return "NUM_WORKSPACES";

    case META_PREF_APPLICATION_BASED:
      return "APPLICATION_BASED";

    case META_PREF_SCREEN_KEYBINDINGS:
      return "SCREEN_KEYBINDINGS";

    case META_PREF_WINDOW_KEYBINDINGS:
      return "WINDOW_KEYBINDINGS";

    case META_PREF_DISABLE_WORKAROUNDS:
      return "DISABLE_WORKAROUNDS";

    case META_PREF_ACTION_DOUBLE_CLICK_TITLEBAR:
      return "ACTION_DOUBLE_CLICK_TITLEBAR";

    case META_PREF_AUTO_RAISE:
      return "AUTO_RAISE";
      
    case META_PREF_AUTO_RAISE_DELAY:
      return "AUTO_RAISE_DELAY";

    case META_PREF_COMMANDS:
      return "COMMANDS";

    case META_PREF_BUTTON_LAYOUT:
      return "BUTTON_LAYOUT";
      break;

    case META_PREF_WORKSPACE_NAMES:
      return "WORKSPACE_NAMES";
      break;
    }

  return "(unknown)";
}
#endif /* WITH_VERBOSE_MODE */

void
meta_prefs_set_num_workspaces (int n_workspaces)
{
#ifdef HAVE_GCONF
  GError *err;
  
  if (default_client == NULL)
    return;

  if (n_workspaces < 1)
    n_workspaces = 1;
  if (n_workspaces > MAX_REASONABLE_WORKSPACES)
    n_workspaces = MAX_REASONABLE_WORKSPACES;
  
  err = NULL;
  gconf_client_set_int (default_client,
                        KEY_NUM_WORKSPACES,
                        n_workspaces,
                        &err);

  if (err)
    {
      meta_warning (_("Error setting number of workspaces to %d: %s\n"),
                    num_workspaces,
                    err->message);
      g_error_free (err);
    }
#endif /* HAVE_GCONF */
}

/* Indexes must correspond to MetaKeybindingAction */
static MetaKeyPref screen_bindings[] = {
  { META_KEYBINDING_WORKSPACE_1, 0, 0 },
  { META_KEYBINDING_WORKSPACE_2, 0, 0 },
  { META_KEYBINDING_WORKSPACE_3, 0, 0 },
  { META_KEYBINDING_WORKSPACE_4, 0, 0 },
  { META_KEYBINDING_WORKSPACE_5, 0, 0 },
  { META_KEYBINDING_WORKSPACE_6, 0, 0 },
  { META_KEYBINDING_WORKSPACE_7, 0, 0 },
  { META_KEYBINDING_WORKSPACE_8, 0, 0 }, 
  { META_KEYBINDING_WORKSPACE_9, 0, 0 },
  { META_KEYBINDING_WORKSPACE_10, 0, 0 },
  { META_KEYBINDING_WORKSPACE_11, 0, 0 },
  { META_KEYBINDING_WORKSPACE_12, 0, 0 },
  { META_KEYBINDING_WORKSPACE_LEFT, 0, 0 },
  { META_KEYBINDING_WORKSPACE_RIGHT, 0, 0 },
  { META_KEYBINDING_WORKSPACE_UP, 0, 0 },
  { META_KEYBINDING_WORKSPACE_DOWN, 0, 0 },
  { META_KEYBINDING_SWITCH_WINDOWS, 0, 0 },
  { META_KEYBINDING_SWITCH_WINDOWS_BACKWARD, 0, 0 },
  { META_KEYBINDING_SWITCH_PANELS, 0, 0 },
  { META_KEYBINDING_SWITCH_PANELS_BACKWARD, 0, 0 },
  { META_KEYBINDING_CYCLE_WINDOWS, 0, 0 },
  { META_KEYBINDING_CYCLE_WINDOWS_BACKWARD, 0, 0 },
  { META_KEYBINDING_CYCLE_PANELS, 0, 0 },
  { META_KEYBINDING_CYCLE_PANELS_BACKWARD, 0, 0 },
  { META_KEYBINDING_SHOW_DESKTOP, 0, 0 },
  { META_KEYBINDING_COMMAND_1, 0, 0 },
  { META_KEYBINDING_COMMAND_2, 0, 0 },
  { META_KEYBINDING_COMMAND_3, 0, 0 },
  { META_KEYBINDING_COMMAND_4, 0, 0 },
  { META_KEYBINDING_COMMAND_5, 0, 0 },
  { META_KEYBINDING_COMMAND_6, 0, 0 },
  { META_KEYBINDING_COMMAND_7, 0, 0 },
  { META_KEYBINDING_COMMAND_8, 0, 0 }, 
  { META_KEYBINDING_COMMAND_9, 0, 0 },
  { META_KEYBINDING_COMMAND_10, 0, 0 },
  { META_KEYBINDING_COMMAND_11, 0, 0 },
  { META_KEYBINDING_COMMAND_12, 0, 0 },
  { META_KEYBINDING_COMMAND_13, 0, 0 },
  { META_KEYBINDING_COMMAND_14, 0, 0 },
  { META_KEYBINDING_COMMAND_15, 0, 0 },
  { META_KEYBINDING_COMMAND_16, 0, 0 },
  { META_KEYBINDING_COMMAND_17, 0, 0 },
  { META_KEYBINDING_COMMAND_18, 0, 0 },
  { META_KEYBINDING_COMMAND_19, 0, 0 },
  { META_KEYBINDING_COMMAND_20, 0, 0 },
  { META_KEYBINDING_COMMAND_21, 0, 0 },
  { META_KEYBINDING_COMMAND_22, 0, 0 },
  { META_KEYBINDING_COMMAND_23, 0, 0 },
  { META_KEYBINDING_COMMAND_24, 0, 0 },
  { META_KEYBINDING_COMMAND_25, 0, 0 },
  { META_KEYBINDING_COMMAND_26, 0, 0 },
  { META_KEYBINDING_COMMAND_27, 0, 0 },
  { META_KEYBINDING_COMMAND_28, 0, 0 },
  { META_KEYBINDING_COMMAND_29, 0, 0 },
  { META_KEYBINDING_COMMAND_30, 0, 0 },
  { META_KEYBINDING_COMMAND_31, 0, 0 },
  { META_KEYBINDING_COMMAND_32, 0, 0 },
  { NULL, 0, 0 }
};

static MetaKeyPref window_bindings[] = {
  { META_KEYBINDING_WINDOW_MENU, 0, 0 },
  { META_KEYBINDING_TOGGLE_FULLSCREEN, 0, 0 },
  { META_KEYBINDING_TOGGLE_MAXIMIZE, 0, 0 },
  { META_KEYBINDING_MAXIMIZE, 0, 0 },
  { META_KEYBINDING_UNMAXIMIZE, 0, 0 },
  { META_KEYBINDING_TOGGLE_SHADE, 0, 0 },
  { META_KEYBINDING_MINIMIZE, 0, 0 },
  { META_KEYBINDING_CLOSE, 0, 0 },
  { META_KEYBINDING_BEGIN_MOVE, 0, 0 },
  { META_KEYBINDING_BEGIN_RESIZE, 0, 0 },
  { META_KEYBINDING_TOGGLE_STICKY, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_1, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_2, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_3, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_4, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_5, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_6, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_7, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_8, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_9, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_10, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_11, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_12, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_LEFT, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_RIGHT, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_UP, 0, 0 },
  { META_KEYBINDING_MOVE_WORKSPACE_DOWN, 0, 0 },
  { META_KEYBINDING_RAISE_OR_LOWER, 0, 0 },
  { META_KEYBINDING_RAISE, 0, 0 },
  { META_KEYBINDING_LOWER, 0, 0 },
  { META_KEYBINDING_MAXIMIZE_VERTICALLY, 0, 0 },
  { META_KEYBINDING_MAXIMIZE_HORIZONTALLY, 0, 0 },
  { NULL, 0, 0 }
};

static void
init_bindings (void)
{
#ifdef HAVE_GCONF
  int i;
  GError *err;
  
  i = 0;
  while (window_bindings[i].name)
    {
      char *str_val;
      char *key;

      key = g_strconcat (KEY_WINDOW_BINDINGS_PREFIX, "/",
                         window_bindings[i].name, NULL);

      err = NULL;
      str_val = gconf_client_get_string (default_client, key, &err);
      cleanup_error (&err);

      update_binding (&window_bindings[i], str_val);

      g_free (str_val);      
      g_free (key);

      ++i;
    }

  i = 0;
  while (screen_bindings[i].name)
    {
      char *str_val;
      char *key;

      key = g_strconcat (KEY_SCREEN_BINDINGS_PREFIX, "/",
                         screen_bindings[i].name, NULL);

      err = NULL;
      str_val = gconf_client_get_string (default_client, key, &err);
      cleanup_error (&err);

      update_binding (&screen_bindings[i], str_val);

      g_free (str_val);      
      g_free (key);

      ++i;
    }
#endif /* HAVE_GCONF */
}

static void
init_commands (void)
{
#ifdef HAVE_GCONF
  int i;
  GError *err;
  
  i = 0;
  while (i < MAX_COMMANDS)
    {
      char *str_val;
      char *key;

      key = meta_prefs_get_gconf_key_for_command (i);

      err = NULL;
      str_val = gconf_client_get_string (default_client, key, &err);
      cleanup_error (&err);

      update_command (key, str_val);

      g_free (str_val);    
      g_free (key);

      ++i;
    }
#endif /* HAVE_GCONF */
}

static void
init_workspace_names (void)
{
#ifdef HAVE_GCONF
  int i;
  GError *err;
  
  i = 0;
  while (i < MAX_REASONABLE_WORKSPACES)
    {
      char *str_val;
      char *key;

      key = gconf_key_for_workspace_name (i);

      err = NULL;
      str_val = gconf_client_get_string (default_client, key, &err);
      cleanup_error (&err);

      update_workspace_name (key, str_val);

      g_assert (workspace_names[i] != NULL);
      
      g_free (str_val);    
      g_free (key);

      ++i;
    }
#endif /* HAVE_GCONF */
}

#ifdef HAVE_GCONF
static gboolean
update_binding (MetaKeyPref *binding,
                const char  *value)
{
  unsigned int keysym;
  MetaVirtualModifier mods;
  gboolean changed;
  
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Binding \"%s\" has new gconf value \"%s\"\n",
              binding->name, value ? value : "none");
  
  keysym = 0;
  mods = 0;
  if (value)
    {
      if (!meta_ui_parse_accelerator (value, &keysym, &mods))
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Failed to parse new gconf value\n");
          meta_warning (_("\"%s\" found in configuration database is not a valid value for keybinding \"%s\"\n"),
                        value, binding->name);
        }
    }
  
  changed = FALSE;
  if (keysym != binding->keysym ||
      mods != binding->modifiers)
    {
      changed = TRUE;
      
      binding->keysym = keysym;
      binding->modifiers = mods;
      
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "New keybinding for \"%s\" is keysym = 0x%x mods = 0x%x\n",
                  binding->name, binding->keysym, binding->modifiers);
    }
  else
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Keybinding for \"%s\" is unchanged\n", binding->name);
    }
  
  return changed;
}

static const gchar*
relative_key (const gchar* key)
{
  const gchar* end;
  
  end = strrchr (key, '/');

  ++end;

  return end;
}

/* Return value is TRUE if a preference changed and we need to
 * notify
 */
static gboolean
find_and_update_binding (MetaKeyPref *bindings, 
                         const char  *name,
                         const char  *value)
{
  const char *key;
  int i;
  
  if (*name == '/')
    key = relative_key (name);
  else
    key = name;

  i = 0;
  while (bindings[i].name &&
         strcmp (key, bindings[i].name) != 0)
    ++i;

  if (bindings[i].name)
    return update_binding (&bindings[i], value);
  else
    return FALSE;
}

static gboolean
update_window_binding (const char *name,
                       const char *value)
{
  return find_and_update_binding (window_bindings, name, value);
}

static gboolean
update_screen_binding (const char *name,
                       const char *value)
{
  return find_and_update_binding (screen_bindings, name, value);
}

static gboolean
update_command (const char  *name,
                const char  *value)
{
  char *p;
  int i;
  
  p = strrchr (name, '_');
  if (p == NULL)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Command %s has no underscore?\n", name);
      return FALSE;
    }
  
  ++p;

  if (!g_ascii_isdigit (*p))
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Command %s doesn't end in number?\n", name);
      return FALSE;
    }
  
  i = atoi (p);
  i -= 1; /* count from 0 not 1 */
  
  if (i >= MAX_COMMANDS)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Command %d is too highly numbered, ignoring\n", i);
      return FALSE;
    }

  if ((commands[i] == NULL && value == NULL) ||
      (commands[i] && value && strcmp (commands[i], value) == 0))
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Command %d is unchanged\n", i);
      return FALSE;
    }
  
  g_free (commands[i]);
  commands[i] = g_strdup (value);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Updated command %d to \"%s\"\n",
              i, commands[i] ? commands[i] : "none");
  
  return TRUE;
}
#endif /* HAVE_GCONF */

const char*
meta_prefs_get_command (int i)
{
  g_return_val_if_fail (i >= 0 && i < MAX_COMMANDS, NULL);
  
  return commands[i];
}

char*
meta_prefs_get_gconf_key_for_command (int i)
{
  char *key;
  
  key = g_strdup_printf (KEY_COMMAND_PREFIX"%d", i + 1);
  
  return key;
}

#ifdef HAVE_GCONF
static gboolean
update_workspace_name (const char  *name,
                       const char  *value)
{
  char *p;
  int i;
  
  p = strrchr (name, '_');
  if (p == NULL)
    {
      meta_topic (META_DEBUG_PREFS,
                  "Workspace name %s has no underscore?\n", name);
      return FALSE;
    }
  
  ++p;

  if (!g_ascii_isdigit (*p))
    {
      meta_topic (META_DEBUG_PREFS,
                  "Workspace name %s doesn't end in number?\n", name);
      return FALSE;
    }
  
  i = atoi (p);
  i -= 1; /* count from 0 not 1 */
  
  if (i >= MAX_REASONABLE_WORKSPACES)
    {
      meta_topic (META_DEBUG_PREFS,
                  "Workspace name %d is too highly numbered, ignoring\n", i);
      return FALSE;
    }

  if (workspace_names[i] && value && strcmp (workspace_names[i], value) == 0)
    {
      meta_topic (META_DEBUG_PREFS,
                  "Workspace name %d is unchanged\n", i);
      return FALSE;
    }  

  /* This is a bad hack. We have to treat empty string as
   * "unset" because the root window property can't contain
   * null. So it gets empty string instead and we don't want
   * that to result in setting the empty string as a value that
   * overrides "unset".
   */
  if (value != NULL && *value != '\0')
    {
      g_free (workspace_names[i]);
      workspace_names[i] = g_strdup (value);
    }
  else
    {
      /* use a default name */
      char *d;

      d = g_strdup_printf (_("Workspace %d"), i + 1);
      if (workspace_names[i] && strcmp (workspace_names[i], d) == 0)
        {
          g_free (d);
          return FALSE;
        }
      else
        {
          g_free (workspace_names[i]);
          workspace_names[i] = d;
        }
    }
  
  meta_topic (META_DEBUG_PREFS,
              "Updated workspace name %d to \"%s\"\n",
              i, workspace_names[i] ? workspace_names[i] : "none");
  
  return TRUE;
}
#endif /* HAVE_GCONF */

const char*
meta_prefs_get_workspace_name (int i)
{
  g_return_val_if_fail (i >= 0 && i < MAX_REASONABLE_WORKSPACES, NULL);

  g_assert (workspace_names[i] != NULL);

  meta_topic (META_DEBUG_PREFS,
              "Getting workspace name for %d: \"%s\"\n",
              i, workspace_names[i]);
  
  return workspace_names[i];
}

void
meta_prefs_change_workspace_name (int         i,
                                  const char *name)
{
#ifdef HAVE_GCONF
  char *key;
  GError *err;
  
  g_return_if_fail (i >= 0 && i < MAX_REASONABLE_WORKSPACES);

  meta_topic (META_DEBUG_PREFS,
              "Changing name of workspace %d to %s\n",
              i, name ? name : "none");

  /* This is a bad hack. We have to treat empty string as
   * "unset" because the root window property can't contain
   * null. So it gets empty string instead and we don't want
   * that to result in setting the empty string as a value that
   * overrides "unset".
   */
  if (name && *name == '\0')
    name = NULL;
  
  if ((name == NULL && workspace_names[i] == NULL) ||
      (name && workspace_names[i] && strcmp (name, workspace_names[i]) == 0))
    {
      meta_topic (META_DEBUG_PREFS,
                  "Workspace %d already has name %s\n",
                  i, name ? name : "none");
      return;
    }
  
  key = gconf_key_for_workspace_name (i);

  err = NULL;
  if (name != NULL)
    gconf_client_set_string (default_client,
                             key, name,
                             &err);
  else
    gconf_client_unset (default_client,
                        key, &err);

  
  if (err)
    {
      meta_warning (_("Error setting name for workspace %d to \"%s\": %s\n"),
                    i, name ? name : "none",
                    err->message);
      g_error_free (err);
    }
  
  g_free (key);
#endif /* HAVE_GCONF */
}

#ifdef HAVE_GCONF
static char*
gconf_key_for_workspace_name (int i)
{
  char *key;
  
  key = g_strdup_printf (KEY_WORKSPACE_NAME_PREFIX"%d", i + 1);
  
  return key;
}
#endif /* HAVE_GCONF */

void
meta_prefs_get_button_layout (MetaButtonLayout *button_layout_p)
{
  *button_layout_p = button_layout;
}

void
meta_prefs_get_screen_bindings (const MetaKeyPref **bindings,
                                int                *n_bindings)
{
  
  *bindings = screen_bindings;
  *n_bindings = (int) G_N_ELEMENTS (screen_bindings) - 1;
}

void
meta_prefs_get_window_bindings (const MetaKeyPref **bindings,
                                int                *n_bindings)
{
  *bindings = window_bindings;
  *n_bindings = (int) G_N_ELEMENTS (window_bindings) - 1;
}

MetaActionDoubleClickTitlebar
meta_prefs_get_action_double_click_titlebar ()
{
  return action_double_click_titlebar;
}

gboolean
meta_prefs_get_auto_raise ()
{
  return auto_raise;
}

int
meta_prefs_get_auto_raise_delay ()
{
  return auto_raise_delay;
}

MetaKeyBindingAction
meta_prefs_get_keybinding_action (const char *name)
{
  int i;

  i = G_N_ELEMENTS (screen_bindings) - 2; /* -2 for dummy entry at end */
  while (i >= 0)
    {
      if (strcmp (screen_bindings[i].name, name) == 0)
        return (MetaKeyBindingAction) i;
      
      --i;
    }

  return META_KEYBINDING_ACTION_NONE;
}

void
meta_prefs_get_window_binding (const char          *name,
                               unsigned int        *keysym,
                               MetaVirtualModifier *modifiers)
{
  int i;

  i = G_N_ELEMENTS (window_bindings) - 2; /* -2 for dummy entry at end */
  while (i >= 0)
    {
      if (strcmp (window_bindings[i].name, name) == 0)
        {
          *keysym = window_bindings[i].keysym;
          *modifiers = window_bindings[i].modifiers;
          return;
        }
      
      --i;
    }

  g_assert_not_reached ();
}
