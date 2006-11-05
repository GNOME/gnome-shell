/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity preferences */

/* 
 * Copyright (C) 2001 Havoc Pennington, Copyright (C) 2002 Red Hat Inc.
 * Copyright (C) 2006 Elijah Newren
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

#define MAX_REASONABLE_WORKSPACES 36

#define MAX_COMMANDS (32 + NUM_EXTRA_COMMANDS)
#define NUM_EXTRA_COMMANDS 2
#define SCREENSHOT_COMMAND_IDX (MAX_COMMANDS - 2)
#define WIN_SCREENSHOT_COMMAND_IDX (MAX_COMMANDS - 1)

/* If you add a key, it needs updating in init() and in the gconf
 * notify listener and of course in the .schemas file
 */
#define KEY_MOUSE_BUTTON_MODS "/apps/metacity/general/mouse_button_modifier"
#define KEY_FOCUS_MODE "/apps/metacity/general/focus_mode"
#define KEY_FOCUS_NEW_WINDOWS "/apps/metacity/general/focus_new_windows"
#define KEY_RAISE_ON_CLICK "/apps/metacity/general/raise_on_click"
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
#define KEY_REDUCED_RESOURCES "/apps/metacity/general/reduced_resources"
#define KEY_GNOME_ACCESSIBILITY "/desktop/gnome/interface/accessibility"

#define KEY_COMMAND_PREFIX "/apps/metacity/keybinding_commands/command_"

#define KEY_TERMINAL_COMMAND "/desktop/gnome/applications/terminal/exec"

#define KEY_SCREEN_BINDINGS_PREFIX "/apps/metacity/global_keybindings"
#define KEY_WINDOW_BINDINGS_PREFIX "/apps/metacity/window_keybindings"
#define KEY_LIST_BINDINGS_SUFFIX "_list"

#define KEY_WORKSPACE_NAME_PREFIX "/apps/metacity/workspace_names/name_"

#define KEY_VISUAL_BELL "/apps/metacity/general/visual_bell"
#define KEY_AUDIBLE_BELL "/apps/metacity/general/audible_bell"
#define KEY_VISUAL_BELL_TYPE "/apps/metacity/general/visual_bell_type"
#define KEY_CURSOR_THEME "/desktop/gnome/peripherals/mouse/cursor_theme"
#define KEY_CURSOR_SIZE "/desktop/gnome/peripherals/mouse/cursor_size"
#define KEY_COMPOSITING_MANAGER "/apps/metacity/general/compositing_manager"

#ifdef HAVE_GCONF
static GConfClient *default_client = NULL;
static GList *changes = NULL;
static guint changed_idle;
#endif
static GList *listeners = NULL;

static gboolean use_system_font = FALSE;
static PangoFontDescription *titlebar_font = NULL;
static MetaVirtualModifier mouse_button_mods = Mod1Mask;
static MetaFocusMode focus_mode = META_FOCUS_MODE_CLICK;
static MetaFocusNewWindows focus_new_windows = META_FOCUS_NEW_WINDOWS_SMART;
static gboolean raise_on_click = TRUE;
static char* current_theme = NULL;
static int num_workspaces = 4;
static MetaActionDoubleClickTitlebar action_double_click_titlebar =
    META_ACTION_DOUBLE_CLICK_TITLEBAR_TOGGLE_MAXIMIZE;
static gboolean application_based = FALSE;
static gboolean disable_workarounds = FALSE;
static gboolean auto_raise = FALSE;
static gboolean auto_raise_delay = 500;
static gboolean provide_visual_bell = FALSE;
static gboolean bell_is_audible = TRUE;
static gboolean reduced_resources = FALSE;
static gboolean gnome_accessibility = FALSE;
static char *cursor_theme = NULL;
static int   cursor_size = 24;
static gboolean compositing_manager = FALSE;

static MetaVisualBellType visual_bell_type = META_VISUAL_BELL_FULLSCREEN_FLASH;
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

/* The screenshot commands are at the end */
static char *commands[MAX_COMMANDS] = { NULL, };

static char *terminal_command = NULL;

static char *workspace_names[MAX_REASONABLE_WORKSPACES] = { NULL, };

#ifdef HAVE_GCONF
static gboolean update_use_system_font    (gboolean    value);
static gboolean update_titlebar_font      (const char *value);
static gboolean update_mouse_button_mods  (const char *value);
static gboolean update_focus_mode         (const char *value);
static gboolean update_focus_new_windows  (const char *value);
static gboolean update_raise_on_click     (gboolean    value);
static gboolean update_theme              (const char *value);
static gboolean update_visual_bell        (gboolean v1, gboolean v2);
static gboolean update_visual_bell_type   (const char *value);
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
static gboolean find_and_update_list_binding (MetaKeyPref *bindings,
                                              const char  *name,
                                              GSList      *value);
static gboolean update_window_list_binding (const char *name,
                                            GSList      *value);
static gboolean update_screen_list_binding (const char *name,
                                            GSList      *value);
static gboolean update_command            (const char  *name,
                                           const char  *value);
static gboolean update_terminal_command   (const char *value);
static gboolean update_workspace_name     (const char  *name,
                                           const char  *value);
static gboolean update_reduced_resources  (gboolean     value);
static gboolean update_gnome_accessibility  (gboolean     value);
static gboolean update_cursor_theme       (const char *value);
static gboolean update_cursor_size        (int size);
static gboolean update_compositing_manager (gboolean	value);

static void change_notify (GConfClient    *client,
                           guint           cnxn_id,
                           GConfEntry     *entry,
                           gpointer        user_data);

static char* gconf_key_for_workspace_name (int i);

static void queue_changed (MetaPreference  pref);
#endif /* HAVE_GCONF */
static gboolean update_binding            (MetaKeyPref *binding,
                                           const char  *value);

typedef enum
  {
    META_LIST_OF_STRINGS,
    META_LIST_OF_GCONFVALUE_STRINGS
  } MetaStringListType;

static gboolean update_list_binding       (MetaKeyPref *binding,
                                           GSList      *value,
                                           MetaStringListType type_of_value);

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

/* get_bool returns TRUE if *val is filled in, FALSE otherwise */
static gboolean
get_bool (const char *key, gboolean *val)
{
  GError     *err = NULL;
  GConfValue *value;
  gboolean    filled_in = FALSE;

  value = gconf_client_get (default_client, key, &err);
  cleanup_error (&err);
  if (value)
    {
      if (value->type == GCONF_VALUE_BOOL)
        {
          *val = gconf_value_get_bool (value);
          filled_in = TRUE;
        }
      gconf_value_free (value);
    }

  return filled_in;
}
#endif /* HAVE_GCONF */

void
meta_prefs_init (void)
{
#ifdef HAVE_GCONF
  GError *err = NULL;
  char *str_val;
  int int_val;
  gboolean bool_val, bool_val_2;
  gboolean update_visual;
  gboolean update_audible;
  
  if (default_client != NULL)
    return;
  
  /* returns a reference which we hold forever */
  default_client = gconf_client_get_default ();

  gconf_client_add_dir (default_client, "/apps/metacity",
                        GCONF_CLIENT_PRELOAD_RECURSIVE,
                        &err);
  cleanup_error (&err);

  gconf_client_add_dir (default_client, "/desktop/gnome/applications/terminal",
                        GCONF_CLIENT_PRELOAD_RECURSIVE,
                        &err);
  cleanup_error (&err);

  gconf_client_add_dir (default_client, KEY_GNOME_ACCESSIBILITY,
                        GCONF_CLIENT_PRELOAD_RECURSIVE,
                        &err);
  cleanup_error (&err);

  gconf_client_add_dir (default_client, "/desktop/gnome/peripherals/mouse",
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

  str_val = gconf_client_get_string (default_client, KEY_FOCUS_NEW_WINDOWS,
                                     &err);
  cleanup_error (&err);
  update_focus_new_windows (str_val);
  g_free (str_val);  

  if (get_bool (KEY_RAISE_ON_CLICK, &bool_val))
    update_raise_on_click (bool_val);
  
  str_val = gconf_client_get_string (default_client,
                                     KEY_ACTION_DOUBLE_CLICK_TITLEBAR,
				     &err);
  cleanup_error (&err);
  update_action_double_click_titlebar (str_val);
  g_free (str_val);

  if (get_bool (KEY_AUTO_RAISE, &bool_val))
    update_auto_raise (bool_val);

  /* FIXME: Since auto_raise_delay of 0 is valid and gconf_client_get_int
   * silently returns that value when KEY_AUTO_RAISE_DELAY doesn't exist,
   * we should be using gconf_client_get() instead if we cared about
   * careful error checking of the key-doesn't-exist case.  Since this
   * setting is crap, though, I'm adding a FIXME instead of fixing it.  ;-)
   */
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
  
  if (get_bool (KEY_USE_SYSTEM_FONT, &bool_val))
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

  if (get_bool (KEY_APPLICATION_BASED, &bool_val))
    update_application_based (bool_val);

  if (get_bool (KEY_DISABLE_WORKAROUNDS, &bool_val))
    update_disable_workarounds (bool_val);

  str_val = gconf_client_get_string (default_client, KEY_BUTTON_LAYOUT,
                                     &err);
  cleanup_error (&err);
  update_button_layout (str_val);
  g_free (str_val);

  bool_val = provide_visual_bell;
  bool_val_2 = bell_is_audible;
  update_visual = get_bool (KEY_VISUAL_BELL,  &bool_val);
  update_audible = get_bool (KEY_AUDIBLE_BELL, &bool_val_2);
  if (update_visual || update_audible)
    update_visual_bell (bool_val, bool_val_2);

  bool_val = compositing_manager;
  if (get_bool (KEY_COMPOSITING_MANAGER, &bool_val))
    update_compositing_manager (bool_val);
  
  str_val = gconf_client_get_string (default_client, KEY_VISUAL_BELL_TYPE,
                                     &err);
  cleanup_error (&err);
  update_visual_bell_type (str_val);
  g_free (str_val);

  if (get_bool (KEY_REDUCED_RESOURCES, &bool_val))
    update_reduced_resources (bool_val);

  str_val = gconf_client_get_string (default_client, KEY_TERMINAL_COMMAND,
                                     &err);
  cleanup_error (&err);
  update_terminal_command (str_val);
  g_free (str_val);

  if (get_bool (KEY_GNOME_ACCESSIBILITY, &bool_val))
    update_gnome_accessibility (bool_val);

  str_val = gconf_client_get_string (default_client, KEY_CURSOR_THEME,
                                     &err);
  cleanup_error (&err);
  update_cursor_theme (str_val);
  g_free (str_val);

  int_val = gconf_client_get_int (default_client, KEY_CURSOR_SIZE,
                                  &err);
  cleanup_error (&err);
  update_cursor_size (int_val);
#else  /* HAVE_GCONF */
  /* Set defaults for some values that can't be set at initialization time of
   * the static globals.  In the case of the theme, note that there is code
   * elsewhere that will do everything possible to fallback to an existing theme
   * if the one here does not exist.
   */
  titlebar_font = pango_font_description_from_string ("Sans Bold 10");
  current_theme = g_strdup ("Atlanta");
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
  gconf_client_notify_add (default_client, KEY_TERMINAL_COMMAND,
                           change_notify,
                           NULL,
                           NULL,
                           &err);
  gconf_client_notify_add (default_client, KEY_GNOME_ACCESSIBILITY,
                           change_notify,
                           NULL,
                           NULL,
                           &err);
  gconf_client_notify_add (default_client, "/desktop/gnome/peripherals/mouse",
                           change_notify,
                           NULL,
                           NULL,
                           &err);

  cleanup_error (&err);  
#endif /* HAVE_GCONF */
}

#ifdef HAVE_GCONF

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
  else if (strcmp (key, KEY_FOCUS_NEW_WINDOWS) == 0)
    {
      const char *str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_FOCUS_NEW_WINDOWS);
          goto out;
        }
      
      str = value ? gconf_value_get_string (value) : NULL;

      if (update_focus_new_windows (str))
        queue_changed (META_PREF_FOCUS_NEW_WINDOWS);
    }
  else if (strcmp (key, KEY_RAISE_ON_CLICK) == 0)
    {
      gboolean b;

      if (value && value->type != GCONF_VALUE_BOOL)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_RAISE_ON_CLICK);
          goto out;
        }

      b = value ? gconf_value_get_bool (value) : TRUE;
      
      if (update_raise_on_click (b))
        queue_changed (META_PREF_RAISE_ON_CLICK);
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
  else if (g_str_has_prefix (key, KEY_WINDOW_BINDINGS_PREFIX))
    {
      if (g_str_has_suffix (key, KEY_LIST_BINDINGS_SUFFIX))
        {
          GSList *list;

          if (value && value->type != GCONF_VALUE_LIST)
            {
              meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                            key);
              goto out;
            }

          list = value ? gconf_value_get_list (value) : NULL;

          if (update_window_list_binding (key, list))
             queue_changed (META_PREF_WINDOW_KEYBINDINGS);
        }
      else
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
    }
  else if (g_str_has_prefix (key, KEY_SCREEN_BINDINGS_PREFIX))
    {
      if (g_str_has_suffix (key, KEY_LIST_BINDINGS_SUFFIX))
        {
          GSList *list;

          if (value && value->type != GCONF_VALUE_LIST)
            {
              meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                            key);
              goto out;
            }

          list = value ? gconf_value_get_list (value) : NULL;

          if (update_screen_list_binding (key, list))
             queue_changed (META_PREF_SCREEN_KEYBINDINGS);
         }
      else
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
  else if (g_str_has_prefix (key, KEY_COMMAND_PREFIX))
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
  else if (strcmp (key, KEY_TERMINAL_COMMAND) == 0)
    {
      const char *str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_TERMINAL_COMMAND);
          goto out;
        }
      
      str = value ? gconf_value_get_string (value) : NULL;

      if (update_terminal_command (str))
        queue_changed (META_PREF_TERMINAL_COMMAND);
    }
  else if (g_str_has_prefix (key, KEY_WORKSPACE_NAME_PREFIX))
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
  else if (strcmp (key, KEY_VISUAL_BELL) == 0)
    {
      gboolean b;

      if (value && value->type != GCONF_VALUE_BOOL)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        key);
          goto out;
        }

      b = value ? gconf_value_get_bool (value) : provide_visual_bell;      
      if (update_visual_bell (b, bell_is_audible))
	queue_changed (META_PREF_VISUAL_BELL);	    
    }
  else if (strcmp (key, KEY_AUDIBLE_BELL) == 0)
    {
      gboolean b;

      if (value && value->type != GCONF_VALUE_BOOL)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        key);
          goto out;
        }

      b = value ? gconf_value_get_bool (value) : bell_is_audible;      
      if (update_visual_bell (provide_visual_bell, b))
	queue_changed (META_PREF_AUDIBLE_BELL);
    }
  else if (strcmp (key, KEY_VISUAL_BELL_TYPE) == 0)
    {
      const char * str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_VISUAL_BELL_TYPE);
          goto out;
        }
      
      str = value ? gconf_value_get_string (value) : NULL;
      if (update_visual_bell_type (str))
	queue_changed (META_PREF_VISUAL_BELL_TYPE);
    }
  else if (strcmp (key, KEY_REDUCED_RESOURCES) == 0)
    {
      gboolean b;

      if (value && value->type != GCONF_VALUE_BOOL)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                        KEY_REDUCED_RESOURCES);
          goto out;
        }

      b = value ? gconf_value_get_bool (value) : reduced_resources;

      if (update_reduced_resources (b))
        queue_changed (META_PREF_REDUCED_RESOURCES);
    }
  else if (strcmp (key, KEY_GNOME_ACCESSIBILITY) == 0)
    {
      gboolean b;

      if (value && value->type != GCONF_VALUE_BOOL)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                       KEY_GNOME_ACCESSIBILITY);
          goto out;
        }

      b = value ? gconf_value_get_bool (value) : gnome_accessibility;

      if (update_gnome_accessibility (b))
        queue_changed (META_PREF_GNOME_ACCESSIBILITY);
    }
  else if (strcmp (key, KEY_CURSOR_THEME) == 0)
    {
      const char *str;

      if (value && value->type != GCONF_VALUE_STRING)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                       KEY_CURSOR_THEME);
          goto out;
        }

      str = value ? gconf_value_get_string (value) : NULL;

      if (update_cursor_theme (str))
	queue_changed (META_PREF_CURSOR_THEME);
    }
  else if (strcmp (key, KEY_CURSOR_SIZE) == 0)
    {
      int d;

      if (value && value->type != GCONF_VALUE_INT)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                       KEY_CURSOR_SIZE);
          goto out;
        }

      d = value ? gconf_value_get_int (value) : 24;

      if (update_cursor_size (d))
	queue_changed (META_PREF_CURSOR_SIZE);
    }
  else if (strcmp (key, KEY_COMPOSITING_MANAGER) == 0)
    {
      gboolean b;

      if (value && value->type != GCONF_VALUE_BOOL)
        {
          meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                       KEY_COMPOSITING_MANAGER);
          goto out;
        }

      b = value ? gconf_value_get_bool (value) : compositing_manager;

      if (update_compositing_manager (b))
        queue_changed (META_PREF_COMPOSITING_MANAGER);
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
update_focus_new_windows (const char *value)
{
  MetaFocusNewWindows old_mode = focus_new_windows;
  
  if (value != NULL)
    {
      if (g_ascii_strcasecmp (value, "smart") == 0)
        focus_new_windows = META_FOCUS_NEW_WINDOWS_SMART;
      else if (g_ascii_strcasecmp (value, "strict") == 0)
        focus_new_windows = META_FOCUS_NEW_WINDOWS_STRICT;
      else
        meta_warning (_("GConf key '%s' is set to an invalid value\n"),
                      KEY_FOCUS_NEW_WINDOWS);
    }

  return (old_mode != focus_new_windows);
}
#endif /* HAVE_GCONF */

#ifdef HAVE_GCONF
static gboolean
update_raise_on_click (gboolean value)
{
  gboolean old = raise_on_click;

  raise_on_click = value;

  return old != value;
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

MetaFocusNewWindows
meta_prefs_get_focus_new_windows (void)
{
  return focus_new_windows;
}

gboolean
meta_prefs_get_raise_on_click (void)
{
  /* Force raise_on_click on for click-to-focus, as requested by Havoc
   * in #326156.
   */
  return raise_on_click || focus_mode == META_FOCUS_MODE_CLICK;
}

const char*
meta_prefs_get_theme (void)
{
  return current_theme;
}

#ifdef HAVE_GCONF
static gboolean
update_cursor_theme (const char *value)
{
  char *old_theme;
  gboolean changed;
  
  old_theme = cursor_theme;
  
  if (value != NULL && *value)
    {
      cursor_theme = g_strdup (value);
    }

  changed = TRUE;
  if ((old_theme && cursor_theme &&
       strcmp (old_theme, cursor_theme) == 0) ||
      (old_theme == NULL && cursor_theme == NULL))
    changed = FALSE;

  if (old_theme != cursor_theme)
    g_free (old_theme);

  return changed;
}
#endif /* HAVE_GCONF */

const char*
meta_prefs_get_cursor_theme (void)
{
  return cursor_theme;
}

#ifdef HAVE_GCONF
static gboolean
update_cursor_size (int value)
{
  int old = cursor_size;

  if (value >= 1 && value <= 128)
    cursor_size = value;
  else
    meta_warning (_("%d stored in GConf key %s is not a reasonable cursor_size; must be in the range 1..128\n"),
                    value, KEY_CURSOR_SIZE);

  return old != value;
}
#endif

int
meta_prefs_get_cursor_size (void)
{
  return cursor_size;
}

#ifdef HAVE_GCONF
static gboolean
update_use_system_font (gboolean value)
{
  gboolean old = use_system_font;

  use_system_font = value;

  return old != value;
}

static MetaVisualBellType
visual_bell_type_from_string (const char *value)
{
  if (value) 
    {
      if (!strcmp (value, "fullscreen")) 
	{
	  return META_VISUAL_BELL_FULLSCREEN_FLASH;
	}
      else if (!strcmp (value, "frame_flash"))
	{
	  return META_VISUAL_BELL_FRAME_FLASH;
	}
    }
  return META_VISUAL_BELL_FULLSCREEN_FLASH;
}

static gboolean
update_visual_bell_type (const char *value)
{
  MetaVisualBellType old_bell_type;

  old_bell_type = visual_bell_type;
  visual_bell_type = visual_bell_type_from_string (value);

  return (visual_bell_type != old_bell_type);
}

static gboolean
update_visual_bell (gboolean visual_bell, gboolean audible_bell)
{
  gboolean old_visual = provide_visual_bell;
  gboolean old_audible = bell_is_audible;
  gboolean has_changed;

  provide_visual_bell = visual_bell;
  bell_is_audible = audible_bell;
  has_changed = (old_visual != provide_visual_bell) || 
    (old_audible != bell_is_audible);

  return has_changed;
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
  else if (strcmp (str, "shade") == 0)
    return META_BUTTON_FUNCTION_SHADE;
  else if (strcmp (str, "above") == 0)
    return META_BUTTON_FUNCTION_ABOVE;
  else if (strcmp (str, "stick") == 0)
    return META_BUTTON_FUNCTION_STICK;
  else 
    /* don't know; give up */
    return META_BUTTON_FUNCTION_LAST;
}

static MetaButtonFunction
button_opposite_function (MetaButtonFunction ofwhat)
{
  switch (ofwhat)
    {
    case META_BUTTON_FUNCTION_SHADE:
      return META_BUTTON_FUNCTION_UNSHADE;
    case META_BUTTON_FUNCTION_UNSHADE:
      return META_BUTTON_FUNCTION_SHADE;

    case META_BUTTON_FUNCTION_ABOVE:
      return META_BUTTON_FUNCTION_UNABOVE;
    case META_BUTTON_FUNCTION_UNABOVE:
      return META_BUTTON_FUNCTION_ABOVE;

    case META_BUTTON_FUNCTION_STICK:
      return META_BUTTON_FUNCTION_UNSTICK;
    case META_BUTTON_FUNCTION_UNSTICK:
      return META_BUTTON_FUNCTION_STICK;

    default:
      return META_BUTTON_FUNCTION_LAST;
    }
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

              f = button_opposite_function (f);

              if (f != META_BUTTON_FUNCTION_LAST)
                new_layout.left_buttons[i++] = f;
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

              f = button_opposite_function (f);

              if (f != META_BUTTON_FUNCTION_LAST)
                new_layout.right_buttons[i++] = f;
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
  else if (strcmp (str, "minimize") == 0)
    return META_ACTION_DOUBLE_CLICK_TITLEBAR_MINIMIZE;
  else if (strcmp (str, "none") == 0)
    return META_ACTION_DOUBLE_CLICK_TITLEBAR_NONE;
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

static gboolean
update_reduced_resources (gboolean value)
{
  gboolean old = reduced_resources;

  reduced_resources = value;

  return old != reduced_resources;
}

static gboolean
update_gnome_accessibility (gboolean value)
{
  gboolean old = gnome_accessibility;

  gnome_accessibility = value;
  
  return old != gnome_accessibility;
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

    case META_PREF_FOCUS_NEW_WINDOWS:
      return "FOCUS_NEW_WINDOWS";

    case META_PREF_RAISE_ON_CLICK:
      return "RAISE_ON_CLICK";
      
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

    case META_PREF_TERMINAL_COMMAND:
      return "TERMINAL_COMMAND";

    case META_PREF_BUTTON_LAYOUT:
      return "BUTTON_LAYOUT";

    case META_PREF_WORKSPACE_NAMES:
      return "WORKSPACE_NAMES";

    case META_PREF_VISUAL_BELL:
      return "VISUAL_BELL";

    case META_PREF_AUDIBLE_BELL:
      return "AUDIBLE_BELL";

    case META_PREF_VISUAL_BELL_TYPE:
      return "VISUAL_BELL_TYPE";

    case META_PREF_REDUCED_RESOURCES:
      return "REDUCED_RESOURCES";

    case META_PREF_GNOME_ACCESSIBILITY:
      return "GNOME_ACCESSIBILTY";

    case META_PREF_CURSOR_THEME:
      return "CURSOR_THEME";

    case META_PREF_CURSOR_SIZE:
      return "CURSOR_SIZE";

    case META_PREF_COMPOSITING_MANAGER:
      return "COMPOSITING_MANAGER";
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
  { META_KEYBINDING_WORKSPACE_1, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_2, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_3, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_4, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_5, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_6, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_7, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_8, NULL, FALSE }, 
  { META_KEYBINDING_WORKSPACE_9, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_10, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_11, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_12, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_LEFT, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_RIGHT, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_UP, NULL, FALSE },
  { META_KEYBINDING_WORKSPACE_DOWN, NULL, FALSE },
  { META_KEYBINDING_SWITCH_GROUP, NULL, TRUE },
  { META_KEYBINDING_SWITCH_GROUP_BACKWARD, NULL, TRUE },
  { META_KEYBINDING_SWITCH_WINDOWS, NULL, TRUE },
  { META_KEYBINDING_SWITCH_WINDOWS_BACKWARD, NULL, TRUE },
  { META_KEYBINDING_SWITCH_PANELS, NULL, TRUE },
  { META_KEYBINDING_SWITCH_PANELS_BACKWARD, NULL, TRUE },
  { META_KEYBINDING_CYCLE_GROUP, NULL, TRUE },
  { META_KEYBINDING_CYCLE_GROUP_BACKWARD, NULL, TRUE },
  { META_KEYBINDING_CYCLE_WINDOWS, NULL, TRUE },
  { META_KEYBINDING_CYCLE_WINDOWS_BACKWARD, NULL, TRUE },
  { META_KEYBINDING_CYCLE_PANELS, NULL, TRUE },
  { META_KEYBINDING_CYCLE_PANELS_BACKWARD, NULL, TRUE },
  { META_KEYBINDING_SHOW_DESKTOP, NULL, FALSE },
  { META_KEYBINDING_PANEL_MAIN_MENU, NULL, FALSE },
  { META_KEYBINDING_PANEL_RUN_DIALOG, NULL, FALSE },
  { META_KEYBINDING_COMMAND_1, NULL, FALSE },
  { META_KEYBINDING_COMMAND_2, NULL, FALSE },
  { META_KEYBINDING_COMMAND_3, NULL, FALSE },
  { META_KEYBINDING_COMMAND_4, NULL, FALSE },
  { META_KEYBINDING_COMMAND_5, NULL, FALSE },
  { META_KEYBINDING_COMMAND_6, NULL, FALSE },
  { META_KEYBINDING_COMMAND_7, NULL, FALSE },
  { META_KEYBINDING_COMMAND_8, NULL, FALSE }, 
  { META_KEYBINDING_COMMAND_9, NULL, FALSE },
  { META_KEYBINDING_COMMAND_10, NULL, FALSE },
  { META_KEYBINDING_COMMAND_11, NULL, FALSE },
  { META_KEYBINDING_COMMAND_12, NULL, FALSE },
  { META_KEYBINDING_COMMAND_13, NULL, FALSE },
  { META_KEYBINDING_COMMAND_14, NULL, FALSE },
  { META_KEYBINDING_COMMAND_15, NULL, FALSE },
  { META_KEYBINDING_COMMAND_16, NULL, FALSE },
  { META_KEYBINDING_COMMAND_17, NULL, FALSE },
  { META_KEYBINDING_COMMAND_18, NULL, FALSE },
  { META_KEYBINDING_COMMAND_19, NULL, FALSE },
  { META_KEYBINDING_COMMAND_20, NULL, FALSE },
  { META_KEYBINDING_COMMAND_21, NULL, FALSE },
  { META_KEYBINDING_COMMAND_22, NULL, FALSE },
  { META_KEYBINDING_COMMAND_23, NULL, FALSE },
  { META_KEYBINDING_COMMAND_24, NULL, FALSE },
  { META_KEYBINDING_COMMAND_25, NULL, FALSE },
  { META_KEYBINDING_COMMAND_26, NULL, FALSE },
  { META_KEYBINDING_COMMAND_27, NULL, FALSE },
  { META_KEYBINDING_COMMAND_28, NULL, FALSE },
  { META_KEYBINDING_COMMAND_29, NULL, FALSE },
  { META_KEYBINDING_COMMAND_30, NULL, FALSE },
  { META_KEYBINDING_COMMAND_31, NULL, FALSE },
  { META_KEYBINDING_COMMAND_32, NULL, FALSE },
  { META_KEYBINDING_COMMAND_SCREENSHOT, NULL, FALSE },
  { META_KEYBINDING_COMMAND_WIN_SCREENSHOT, NULL, FALSE },
  { META_KEYBINDING_RUN_COMMAND_TERMINAL, NULL, FALSE },
  { NULL, NULL, FALSE}
};

static MetaKeyPref window_bindings[] = {
  { META_KEYBINDING_WINDOW_MENU, NULL, FALSE },
  { META_KEYBINDING_TOGGLE_FULLSCREEN, NULL, FALSE },
  { META_KEYBINDING_TOGGLE_MAXIMIZE, NULL, FALSE },
  { META_KEYBINDING_TOGGLE_ABOVE, NULL, FALSE },
  { META_KEYBINDING_MAXIMIZE, NULL, FALSE },
  { META_KEYBINDING_UNMAXIMIZE, NULL, FALSE },
  { META_KEYBINDING_TOGGLE_SHADE, NULL, FALSE },
  { META_KEYBINDING_MINIMIZE, NULL, FALSE },
  { META_KEYBINDING_CLOSE, NULL, FALSE },
  { META_KEYBINDING_BEGIN_MOVE, NULL, FALSE },
  { META_KEYBINDING_BEGIN_RESIZE, NULL, FALSE },
  { META_KEYBINDING_TOGGLE_STICKY, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_1, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_2, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_3, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_4, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_5, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_6, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_7, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_8, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_9, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_10, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_11, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_12, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_LEFT, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_RIGHT, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_UP, NULL, FALSE },
  { META_KEYBINDING_MOVE_WORKSPACE_DOWN, NULL, FALSE },
  { META_KEYBINDING_RAISE_OR_LOWER, NULL, FALSE },
  { META_KEYBINDING_RAISE, NULL, FALSE },
  { META_KEYBINDING_LOWER, NULL, FALSE },
  { META_KEYBINDING_MAXIMIZE_VERTICALLY, NULL, FALSE },
  { META_KEYBINDING_MAXIMIZE_HORIZONTALLY, NULL, FALSE },
  { META_KEYBINDING_MOVE_TO_CORNER_NW, NULL, FALSE },
  { META_KEYBINDING_MOVE_TO_CORNER_NE, NULL, FALSE },
  { META_KEYBINDING_MOVE_TO_CORNER_SW, NULL, FALSE },
  { META_KEYBINDING_MOVE_TO_CORNER_SE, NULL, FALSE },
  { META_KEYBINDING_MOVE_TO_SIDE_N, NULL, FALSE },
  { META_KEYBINDING_MOVE_TO_SIDE_S, NULL, FALSE },
  { META_KEYBINDING_MOVE_TO_SIDE_E, NULL, FALSE },
  { META_KEYBINDING_MOVE_TO_SIDE_W, NULL, FALSE },
  { NULL, NULL, FALSE }
};

#ifndef HAVE_GCONF
typedef struct
{
  const char *name;
  const char *keybinding;
} MetaSimpleKeyMapping;

/* Name field must occur in the same order as screen_bindings, though entries
 * can be skipped
 */
static MetaSimpleKeyMapping screen_string_bindings[] = {
  { META_KEYBINDING_WORKSPACE_LEFT,         "<Control><Alt>Left"         },
  { META_KEYBINDING_WORKSPACE_RIGHT,        "<Control><Alt>Right"        },
  { META_KEYBINDING_WORKSPACE_UP,           "<Control><Alt>Up"           },
  { META_KEYBINDING_WORKSPACE_DOWN,         "<Control><Alt>Down"         },
  { META_KEYBINDING_SWITCH_WINDOWS,         "<Alt>Tab"                   },
  { META_KEYBINDING_SWITCH_PANELS,          "<Control><Alt>Tab"          },
  { META_KEYBINDING_CYCLE_GROUP,            "<Alt>F6"                    },
  { META_KEYBINDING_CYCLE_WINDOWS,          "<Alt>Escape"                },
  { META_KEYBINDING_CYCLE_PANELS,           "<Control><Alt>Escape"       },
  { META_KEYBINDING_SHOW_DESKTOP,           "<Control><Alt>d"            },
  { META_KEYBINDING_PANEL_MAIN_MENU,        "<Alt>F1"                    },
  { META_KEYBINDING_PANEL_RUN_DIALOG,       "<Alt>F2"                    },
  { META_KEYBINDING_COMMAND_SCREENSHOT,     "Print"                      },
  { META_KEYBINDING_COMMAND_WIN_SCREENSHOT, "<Alt>Print"                 },
  { NULL,                                   NULL                         }
};

/* Name field must occur in the same order as window_bindings, though entries
 * can be skipped
 */
static MetaSimpleKeyMapping window_string_bindings[] = {
  { META_KEYBINDING_WINDOW_MENU,            "<Alt>Print"                 },
  { META_KEYBINDING_MAXIMIZE,               "<Alt>F10"                   },
  { META_KEYBINDING_UNMAXIMIZE,             "<Alt>F5"                    },
  { META_KEYBINDING_MINIMIZE,               "<Alt>F9"                    },
  { META_KEYBINDING_CLOSE,                  "<Alt>F4"                    },
  { META_KEYBINDING_BEGIN_MOVE,             "<Alt>F7"                    },
  { META_KEYBINDING_BEGIN_RESIZE,           "<Alt>F8"                    },
  { META_KEYBINDING_MOVE_WORKSPACE_LEFT,    "<Control><Shift><Alt>Left"  },
  { META_KEYBINDING_MOVE_WORKSPACE_RIGHT,   "<Control><Shift><Alt>Right" },
  { META_KEYBINDING_MOVE_WORKSPACE_UP,      "<Control><Shift><Alt>Up"    },
  { META_KEYBINDING_MOVE_WORKSPACE_DOWN,    "<Control><Shift><Alt>Down"  },
  { NULL,                                   NULL                         }
};
#endif /* NOT HAVE_GCONF */

static void
init_bindings (void)
{
#ifdef HAVE_GCONF
  int i;
  GError *err;
  
  i = 0;
  while (window_bindings[i].name)
    {
      GSList *list_val, *tmp;
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

      key = g_strconcat (KEY_WINDOW_BINDINGS_PREFIX, "/",
                         window_bindings[i].name,
                         KEY_LIST_BINDINGS_SUFFIX, NULL);

      err = NULL;

      list_val = gconf_client_get_list (default_client, key, GCONF_VALUE_STRING, &err);
      cleanup_error (&err);

      update_list_binding (&window_bindings[i], list_val, META_LIST_OF_STRINGS);

      tmp = list_val;
      while (tmp)
        {
          g_free (tmp->data);
          tmp = tmp->next;
        }
      g_slist_free (list_val);
      g_free (key);

      ++i;
    }

  i = 0;
  while (screen_bindings[i].name)
    {
      GSList *list_val, *tmp;
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

      key = g_strconcat (KEY_SCREEN_BINDINGS_PREFIX, "/",
                         screen_bindings[i].name,
                         KEY_LIST_BINDINGS_SUFFIX, NULL);

      err = NULL;

      list_val = gconf_client_get_list (default_client, key, GCONF_VALUE_STRING, &err);
      cleanup_error (&err);

      update_list_binding (&screen_bindings[i], list_val, META_LIST_OF_STRINGS);

      tmp = list_val;
      while (tmp)
        {
          g_free (tmp->data);
          tmp = tmp->next;
        }
      g_slist_free (list_val);
      g_free (key);

      ++i;
    }
#else /* HAVE_GCONF */
  int i = 0;
  int which = 0;
  while (window_string_bindings[i].name)
    {
      /* Find which window_bindings entry this window_string_bindings entry
       * corresponds to.
       */
      while (strcmp(window_bindings[which].name, 
                    window_string_bindings[i].name) != 0)
        which++;

      /* Set the binding */
      update_binding (&window_bindings[which],
                      window_string_bindings[i].keybinding);

      ++i;
    }

  i = 0;
  which = 0;
  while (screen_string_bindings[i].name)
    {
      /* Find which window_bindings entry this window_string_bindings entry
       * corresponds to.
       */
      while (strcmp(screen_bindings[which].name, 
                    screen_string_bindings[i].name) != 0)
        which++;

      /* Set the binding */
      update_binding (&screen_bindings[which], 
                      screen_string_bindings[i].keybinding);

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
#else
  int i;
  for (i = 0; i < MAX_COMMANDS; i++)
    commands[i] = NULL;
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
#else
  int i;
  for (i = 0; i < MAX_REASONABLE_WORKSPACES; i++)
    workspace_names[i] = g_strdup_printf (_("Workspace %d"), i + 1);

  meta_topic (META_DEBUG_PREFS,
              "Initialized workspace names\n");
#endif /* HAVE_GCONF */
}

static gboolean
update_binding (MetaKeyPref *binding,
                const char  *value)
{
  unsigned int keysym;
  unsigned int keycode;
  MetaVirtualModifier mods;
  MetaKeyCombo *combo;
  gboolean changed;
  
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Binding \"%s\" has new gconf value \"%s\"\n",
              binding->name, value ? value : "none");
  
  keysym = 0;
  keycode = 0;
  mods = 0;
  if (value)
    {
      if (!meta_ui_parse_accelerator (value, &keysym, &keycode, &mods))
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Failed to parse new gconf value\n");
          meta_warning (_("\"%s\" found in configuration database is not a valid value for keybinding \"%s\"\n"),
                        value, binding->name);

          return FALSE;
        }
    }

  /* If there isn't already a first element, make one. */
  if (!binding->bindings)
    {
      MetaKeyCombo *blank = g_malloc0 (sizeof (MetaKeyCombo));
      binding->bindings = g_slist_alloc();
      binding->bindings->data = blank;
    }
  
   combo = binding->bindings->data;

   /* Bug 329676: Bindings which can be shifted must not have no modifiers,
   * nor only SHIFT as a modifier.
   */

  if (binding->add_shift &&
      0 != keysym &&
      (META_VIRTUAL_SHIFT_MASK == mods || 0 == mods))
    {
      gchar *old_setting;
      gchar *key;
      GError *err = NULL;
      
      meta_warning ("Cannot bind \"%s\" to %s: it needs a modifier "
                    "such as Ctrl or Alt.\n",
                    binding->name,
                    value);

      old_setting = meta_ui_accelerator_name (combo->keysym,
                                              combo->modifiers);

      if (!strcmp(old_setting, value))
        {
          /* We were about to set it to the same value
           * that it had originally! This must be caused
           * by getting an invalid string back from
           * meta_ui_accelerator_name. Bail out now
           * so we don't get into an infinite loop.
           */
           g_free (old_setting);
           return TRUE;
        }

      meta_warning ("Reverting \"%s\" to %s.\n",
                    binding->name,
                    old_setting);

      key = g_strconcat (KEY_SCREEN_BINDINGS_PREFIX, "/",
                         binding->name, NULL);
      
      gconf_client_set_string (gconf_client_get_default (),
                               key, old_setting, &err);

      if (err)
        {
          meta_warning ("Error while reverting keybinding: %s\n",
                        err->message);
          g_error_free (err);
          err = NULL;
        }
      
      g_free (old_setting);
      g_free (key);

      /* The call to gconf_client_set_string() will cause this function
       * to be called again with the new value, so there's no need to
       * carry on.
       */
      return TRUE;
    }
  
  changed = FALSE;
  if (keysym != combo->keysym ||
      keycode != combo->keycode ||
      mods != combo->modifiers)
    {
      changed = TRUE;
      
      combo->keysym = keysym;
      combo->keycode = keycode;
      combo->modifiers = mods;
      
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "New keybinding for \"%s\" is keysym = 0x%x keycode = 0x%x mods = 0x%x\n",
                  binding->name, combo->keysym, combo->keycode,
                  combo->modifiers);
    }
  else
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Keybinding for \"%s\" is unchanged\n", binding->name);
    }
  
  return changed;
}

static gboolean
update_list_binding (MetaKeyPref *binding,
                     GSList      *value,
                     MetaStringListType type_of_value)
{
  unsigned int keysym;
  unsigned int keycode;
  MetaVirtualModifier mods;
  gboolean changed = FALSE;
  const gchar *pref_string;
  GSList *pref_iterator = value, *tmp;
  MetaKeyCombo *combo;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Binding \"%s\" has new gconf value\n",
              binding->name);
  
  if (binding->bindings == NULL)
    {
      /* We need to insert a dummy element into the list, because the first
       * element is the one governed by update_binding. We only handle the
       * subsequent elements.
       */
      MetaKeyCombo *blank = g_malloc0 (sizeof (MetaKeyCombo));
      binding->bindings = g_slist_alloc();
      binding->bindings->data = blank;
    }
       
  /* Okay, so, we're about to provide a new list of key combos for this
   * action. Delete any pre-existing list.
   */
  tmp = binding->bindings->next;
  while (tmp)
    {
      g_free (tmp->data);
      tmp = tmp->next;
    }
  g_slist_free (binding->bindings->next);
  binding->bindings->next = NULL;
  
  while (pref_iterator)
    {
      keysym = 0;
      keycode = 0;
      mods = 0;

      if (!pref_iterator->data)
        {
          pref_iterator = pref_iterator->next;
          continue;
        }

      switch (type_of_value)
        {
        case META_LIST_OF_STRINGS:
          pref_string = pref_iterator->data;
          break;
        case META_LIST_OF_GCONFVALUE_STRINGS:
          pref_string = gconf_value_get_string (pref_iterator->data);
          break;
        default:
          g_assert_not_reached ();
        }
      
      if (!meta_ui_parse_accelerator (pref_string, &keysym, &keycode, &mods))
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Failed to parse new gconf value\n");
          meta_warning (_("\"%s\" found in configuration database is not a valid value for keybinding \"%s\"\n"),
                        pref_string, binding->name);

          /* Should we remove this value from the list in gconf? */
          pref_iterator = pref_iterator->next;
          continue;
        }

      /* Bug 329676: Bindings which can be shifted must not have no modifiers,
       * nor only SHIFT as a modifier.
       */

      if (binding->add_shift &&
          0 != keysym &&
          (META_VIRTUAL_SHIFT_MASK == mods || 0 == mods))
        {
          meta_warning ("Cannot bind \"%s\" to %s: it needs a modifier "
                        "such as Ctrl or Alt.\n",
                        binding->name,
                        pref_string);

          /* Should we remove this value from the list in gconf? */

          pref_iterator = pref_iterator->next;
          continue;
        }
  
      changed = TRUE;

      combo = g_malloc0 (sizeof (MetaKeyCombo));
      combo->keysym = keysym;
      combo->keycode = keycode;
      combo->modifiers = mods;
      binding->bindings->next = g_slist_prepend (binding->bindings->next, combo);

      meta_topic (META_DEBUG_KEYBINDINGS,
                      "New keybinding for \"%s\" is keysym = 0x%x keycode = 0x%x mods = 0x%x\n",
                      binding->name, keysym, keycode, mods);

      pref_iterator = pref_iterator->next;
    }  
  return changed;
}

#ifdef HAVE_GCONF
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
find_and_update_list_binding (MetaKeyPref *bindings,
                              const char  *name,
                              GSList      *value)
{
  const char *key;
  int i;
  gchar *name_without_suffix = g_strdup(name);

  name_without_suffix[strlen(name_without_suffix) - strlen(KEY_LIST_BINDINGS_SUFFIX)] = 0;

  /* FIXME factor out dupld code */
  if (*name_without_suffix == '/')
    key = relative_key (name_without_suffix);
  else
    key = name_without_suffix;

  i = 0;
  while (bindings[i].name &&
         strcmp (key, bindings[i].name) != 0)
    ++i;

  g_free (name_without_suffix);

  if (bindings[i].name)
    return update_list_binding (&bindings[i], value, META_LIST_OF_GCONFVALUE_STRINGS);
  else
    return FALSE;
}

static gboolean
update_window_list_binding (const char *name,
                            GSList *value)
{
  return find_and_update_list_binding (window_bindings, name, value);
}

static gboolean
update_screen_list_binding (const char *name,
                            GSList *value)
{
  return find_and_update_list_binding (screen_bindings, name, value);
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

  if (g_ascii_isdigit (*p))
    {
      i = atoi (p);
      i -= 1; /* count from 0 not 1 */
    }
  else
    {
      p = strrchr (name, '/');
      ++p;

      if (strcmp (p, "command_screenshot") == 0)
        {
          i = SCREENSHOT_COMMAND_IDX;
        }
      else if (strcmp (p, "command_window_screenshot") == 0)
        {
          i = WIN_SCREENSHOT_COMMAND_IDX;
        }
      else
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Command %s doesn't end in number?\n", name);
          return FALSE;
        }
    }
  
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

static gboolean
update_terminal_command (const char *value)
{
  char *old_terminal_command;
  gboolean changed;
  
  old_terminal_command = terminal_command;
  
  if (value != NULL && *value)
    {
      terminal_command = g_strdup (value);
    }

  changed = TRUE;
  if ((old_terminal_command && terminal_command &&
       strcmp (old_terminal_command, terminal_command) == 0) ||
      (old_terminal_command == NULL && terminal_command == NULL))
    changed = FALSE;

  if (old_terminal_command != terminal_command)
    g_free (old_terminal_command);

  return changed;
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

  switch (i)
    {
    case SCREENSHOT_COMMAND_IDX:
      key = g_strdup (KEY_COMMAND_PREFIX "screenshot");
      break;
    case WIN_SCREENSHOT_COMMAND_IDX:
      key = g_strdup (KEY_COMMAND_PREFIX "window_screenshot");
      break;
    default:
      key = g_strdup_printf (KEY_COMMAND_PREFIX"%d", i + 1);
      break;
    }
  
  return key;
}

const char*
meta_prefs_get_terminal_command (void)
{
  return terminal_command;
}

const char*
meta_prefs_get_gconf_key_for_terminal_command (void)
{
  return KEY_TERMINAL_COMMAND;
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
#else
  g_free (workspace_names[i]);
  workspace_names[i] = g_strdup (name);
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

gboolean
meta_prefs_get_visual_bell (void)
{
  return provide_visual_bell;
}

gboolean
meta_prefs_bell_is_audible (void)
{
  return bell_is_audible;
}

MetaVisualBellType
meta_prefs_get_visual_bell_type (void)
{
  return visual_bell_type;
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
meta_prefs_get_action_double_click_titlebar (void)
{
  return action_double_click_titlebar;
}

gboolean
meta_prefs_get_auto_raise (void)
{
  return auto_raise;
}

int
meta_prefs_get_auto_raise_delay (void)
{
  return auto_raise_delay;
}

gboolean
meta_prefs_get_reduced_resources (void)
{
  return reduced_resources;
}

gboolean
meta_prefs_get_gnome_accessibility ()
{
  return gnome_accessibility;
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

/* This is used by the menu system to decide what key binding
 * to display next to an option. We return the first non-disabled
 * binding, if any.
 */
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
          GSList *s = window_bindings[i].bindings;

          while (s)
            {
              MetaKeyCombo *c = s->data;

              if (c->keysym!=0 || c->modifiers!=0)
                {
                  *keysym = c->keysym;
                  *modifiers = c->modifiers;
                  return;
                }

              s = s->next;
            }

          /* Not found; return the disabled value */
          *keysym = *modifiers = 0;
          return;
        }
      
      --i;
    }

  g_assert_not_reached ();
}

#ifdef HAVE_GCONF
static gboolean
update_compositing_manager (gboolean value)
{
  gboolean old = compositing_manager;

  compositing_manager = value;
  
  return old != compositing_manager;
}
#endif

gboolean
meta_prefs_get_compositing_manager (void)
{
  return compositing_manager;
}
