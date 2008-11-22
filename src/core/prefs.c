/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity preferences */

/* 
 * Copyright (C) 2001 Havoc Pennington, Copyright (C) 2002 Red Hat Inc.
 * Copyright (C) 2006 Elijah Newren
 * Copyright (C) 2008 Thomas Thurman
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
 * notify listener and of course in the .schemas file.
 *
 * Keys which are handled by one of the unified handlers below are
 * not given a name here, because the purpose of the unified handlers
 * is that keys should be referred to exactly once.
 */
#define KEY_TITLEBAR_FONT "/apps/metacity/general/titlebar_font"
#define KEY_NUM_WORKSPACES "/apps/metacity/general/num_workspaces"
#define KEY_COMPOSITOR "/apps/metacity/general/compositing_manager"
#define KEY_GNOME_ACCESSIBILITY "/desktop/gnome/interface/accessibility"

#define KEY_COMMAND_PREFIX "/apps/metacity/keybinding_commands/command_"

#define KEY_TERMINAL_DIR "/desktop/gnome/applications/terminal"
#define KEY_TERMINAL_COMMAND KEY_TERMINAL_DIR "/exec"

#define KEY_SCREEN_BINDINGS_PREFIX "/apps/metacity/global_keybindings"
#define KEY_WINDOW_BINDINGS_PREFIX "/apps/metacity/window_keybindings"
#define KEY_LIST_BINDINGS_SUFFIX "_list"

#define KEY_WORKSPACE_NAME_PREFIX "/apps/metacity/workspace_names/name_"


#ifdef HAVE_GCONF
static GConfClient *default_client = NULL;
static GList *changes = NULL;
static guint changed_idle;
static GList *listeners = NULL;
#endif

static gboolean use_system_font = FALSE;
static PangoFontDescription *titlebar_font = NULL;
static MetaVirtualModifier mouse_button_mods = Mod1Mask;
static MetaFocusMode focus_mode = META_FOCUS_MODE_CLICK;
static MetaFocusNewWindows focus_new_windows = META_FOCUS_NEW_WINDOWS_SMART;
static gboolean raise_on_click = TRUE;
static char* current_theme = NULL;
static int num_workspaces = 4;
static MetaActionTitlebar action_double_click_titlebar = META_ACTION_TITLEBAR_TOGGLE_MAXIMIZE;
static MetaActionTitlebar action_middle_click_titlebar = META_ACTION_TITLEBAR_LOWER;
static MetaActionTitlebar action_right_click_titlebar = META_ACTION_TITLEBAR_MENU;
static gboolean application_based = FALSE;
static gboolean disable_workarounds = FALSE;
static gboolean auto_raise = FALSE;
static gboolean auto_raise_delay = 500;
static gboolean provide_visual_bell = FALSE;
static gboolean bell_is_audible = TRUE;
static gboolean reduced_resources = FALSE;
static gboolean gnome_accessibility = FALSE;
static gboolean gnome_animations = TRUE;
static char *cursor_theme = NULL;
static int   cursor_size = 24;
static gboolean compositing_manager = FALSE;

static MetaVisualBellType visual_bell_type = META_VISUAL_BELL_FULLSCREEN_FLASH;
static MetaButtonLayout button_layout;

/* The screenshot commands are at the end */
static char *commands[MAX_COMMANDS] = { NULL, };

static char *terminal_command = NULL;

static char *workspace_names[MAX_REASONABLE_WORKSPACES] = { NULL, };

#ifdef HAVE_GCONF
static gboolean handle_preference_update_enum (const gchar *key, GConfValue *value);

static gboolean update_key_binding     (const char *name,
                                        const char *value);
static gboolean find_and_update_list_binding (MetaKeyPref *bindings,
                                              const char  *name,
                                              GSList      *value);
static gboolean update_key_list_binding (const char *name,
                                         GSList      *value);
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

typedef enum
  {
    META_LIST_OF_STRINGS,
    META_LIST_OF_GCONFVALUE_STRINGS
  } MetaStringListType;

static gboolean update_list_binding       (MetaKeyPref *binding,
                                           GSList      *value,
                                           MetaStringListType type_of_value);

static void     cleanup_error             (GError **error);
static gboolean get_bool                  (const char *key, gboolean *val);
static void maybe_give_disable_workarounds_warning (void);

static void titlebar_handler (MetaPreference, const gchar*, gboolean*);
static void theme_name_handler (MetaPreference, const gchar*, gboolean*);
static void mouse_button_mods_handler (MetaPreference, const gchar*, gboolean*);
static void button_layout_handler (MetaPreference, const gchar*, gboolean*);

#endif /* HAVE_GCONF */

static gboolean update_binding            (MetaKeyPref *binding,
                                           const char  *value);

static void     init_bindings             (void);
static void     init_commands             (void);
static void     init_workspace_names      (void);

#ifndef HAVE_GCONF
static void     init_button_layout        (void);
#endif /* !HAVE_GCONF */

#ifdef HAVE_GCONF

typedef struct
{
  MetaPrefsChangedFunc func;
  gpointer data;
} MetaPrefsListener;

static GConfEnumStringPair symtab_focus_mode[] =
  {
    { META_FOCUS_MODE_CLICK,  "click" },
    { META_FOCUS_MODE_SLOPPY, "sloppy" },
    { META_FOCUS_MODE_MOUSE,  "mouse" },
    { 0, NULL },
  };

static GConfEnumStringPair symtab_focus_new_windows[] =
  {
    { META_FOCUS_NEW_WINDOWS_SMART,  "smart" },
    { META_FOCUS_NEW_WINDOWS_STRICT, "strict" },
    { 0, NULL },
  };

static GConfEnumStringPair symtab_visual_bell_type[] =
  {
    /* Note to the reader: 0 is an invalid value; these start at 1. */
    { META_VISUAL_BELL_FULLSCREEN_FLASH, "fullscreen" },
    { META_VISUAL_BELL_FRAME_FLASH,      "frame_flash" },
    { 0, NULL },
  };

static GConfEnumStringPair symtab_titlebar_action[] =
  {
    { META_ACTION_TITLEBAR_TOGGLE_SHADE,    "toggle_shade" },
    { META_ACTION_TITLEBAR_TOGGLE_MAXIMIZE, "toggle_maximize" },
    { META_ACTION_TITLEBAR_TOGGLE_MAXIMIZE_HORIZONTALLY,
                                "toggle_maximize_horizontally" },
    { META_ACTION_TITLEBAR_TOGGLE_MAXIMIZE_VERTICALLY,
                                "toggle_maximize_vertically" },
    { META_ACTION_TITLEBAR_MINIMIZE,        "minimize" },
    { META_ACTION_TITLEBAR_NONE,            "none" },
    { META_ACTION_TITLEBAR_LOWER,           "lower" },
    { META_ACTION_TITLEBAR_MENU,            "menu" },
    { META_ACTION_TITLEBAR_TOGGLE_SHADE,    "toggle_shade" },
    { 0, NULL },
  };

/**
 * The details of one preference which is constrained to be
 * one of a small number of string values-- in other words,
 * an enumeration.
 *
 * We could have done this other ways.  One particularly attractive
 * possibility would have been to represent the entire symbol table
 * as a space-separated string literal in the list of symtabs, so
 * the focus mode enums could have been represented simply by
 * "click sloppy mouse".  However, the simplicity gained would have
 * been outweighed by the bugs caused when the ordering of the enum
 * strings got out of sync with the actual enum statement.  Also,
 * there is existing library code to use this kind of symbol tables.
 *
 * Other things we might consider doing to clean this up in the
 * future include:
 *
 *   - most of the keys begin with the same prefix, and perhaps we
 *     could assume it if they don't start with a slash
 *
 *   - there are several cases where a single identifier could be used
 *     to generate an entire entry, and perhaps this could be done
 *     with a macro.  (This would reduce clarity, however, and is
 *     probably a bad thing.)
 *
 *   - these types all begin with a gchar* (and contain a MetaPreference)
 *     and we can factor out the repeated code in the handlers by taking
 *     advantage of this using some kind of union arrangement.
 */
typedef struct
{
  gchar *key;
  MetaPreference pref;
  GConfEnumStringPair *symtab;
  gpointer target;
} MetaEnumPreference;

typedef struct
{
  gchar *key;
  MetaPreference pref;
  gboolean *target;
  gboolean becomes_true_on_destruction;
} MetaBoolPreference;

typedef struct
{
  gchar *key;
  MetaPreference pref;

  /**
   * A handler.  Many of the string preferences aren't stored as
   * strings and need parsing; others of them have default values
   * which can't be solved in the general case.  If you include a
   * function pointer here, it will be called before the string
   * value is written out to the target variable.
   *
   * The function is passed two arguments: the preference, and
   * the new string as a gchar*.  It returns a gboolean;
   * only if this is true, the listeners will be informed that
   * the preference has changed.
   *
   * This may be NULL.  If it is, see "target", below.
   */
  void (*handler) (MetaPreference pref,
                     const gchar *string_value,
                     gboolean *inform_listeners);

  /**
   * Where to write the incoming string.
   *
   * This must be NULL if the handler is non-NULL.
   * If the incoming string is NULL, no change will be made.
   */
  gchar **target;

} MetaStringPreference;

#define METAINTPREFERENCE_NO_CHANGE_ON_DESTROY G_MININT

typedef struct
{
  gchar *key;
  MetaPreference pref;
  gint *target;
  /**
   * Minimum and maximum values of the integer.
   * If the new value is out of bounds, it will be discarded with a warning.
   */
  gint minimum, maximum;
  /**
   * Value to use if the key is destroyed.
   * If this is METAINTPREFERENCE_NO_CHANGE_ON_DESTROY, it will
   * not be changed when the key is destroyed.
   */
  gint value_if_destroyed;
} MetaIntPreference;

/* FIXMEs: */
/* @@@ Don't use NULL lines at the end; glib can tell you how big it is */
/* @@@ /apps/metacity/general should be assumed if first char is not / */
/* @@@ Will it ever be possible to merge init and update? If not, why not? */

static MetaEnumPreference preferences_enum[] =
  {
    { "/apps/metacity/general/focus_new_windows",
      META_PREF_FOCUS_NEW_WINDOWS,
      symtab_focus_new_windows,
      &focus_new_windows,
    },
    { "/apps/metacity/general/focus_mode",
      META_PREF_FOCUS_MODE,
      symtab_focus_mode,
      &focus_mode,
    },
    { "/apps/metacity/general/visual_bell_type",
      META_PREF_VISUAL_BELL_TYPE,
      symtab_visual_bell_type,
      &visual_bell_type,
    },
    { "/apps/metacity/general/action_double_click_titlebar",
      META_PREF_ACTION_DOUBLE_CLICK_TITLEBAR,
      symtab_titlebar_action,
      &action_double_click_titlebar,
    },
    { "/apps/metacity/general/action_middle_click_titlebar",
      META_PREF_ACTION_MIDDLE_CLICK_TITLEBAR,
      symtab_titlebar_action,
      &action_middle_click_titlebar,
    },
    { "/apps/metacity/general/action_right_click_titlebar",
      META_PREF_ACTION_RIGHT_CLICK_TITLEBAR,
      symtab_titlebar_action,
      &action_right_click_titlebar,
    },
    { NULL, 0, NULL, NULL },
  };

static MetaBoolPreference preferences_bool[] =
  {
    { "/apps/metacity/general/raise_on_click",
      META_PREF_RAISE_ON_CLICK,
      &raise_on_click,
      TRUE,
    },
    { "/apps/metacity/general/titlebar_uses_system_font",
      META_PREF_TITLEBAR_FONT, /* note! shares a pref */
      &use_system_font,
      TRUE,
    },
    { "/apps/metacity/general/application_based",
      META_PREF_APPLICATION_BASED,
      NULL, /* feature is known but disabled */
      FALSE,
    },
    { "/apps/metacity/general/disable_workarounds",
      META_PREF_DISABLE_WORKAROUNDS,
      &disable_workarounds,
      FALSE,
    },
    { "/apps/metacity/general/auto_raise",
      META_PREF_AUTO_RAISE,
      &auto_raise,
      FALSE,
    },
    { "/apps/metacity/general/visual_bell",
      META_PREF_VISUAL_BELL,
      &provide_visual_bell, /* FIXME: change the name: it's confusing */
      FALSE,
    },
    { "/apps/metacity/general/audible_bell",
      META_PREF_AUDIBLE_BELL,
      &bell_is_audible, /* FIXME: change the name: it's confusing */
      FALSE,
    },
    { "/apps/metacity/general/reduced_resources",
      META_PREF_REDUCED_RESOURCES,
      &reduced_resources,
      FALSE,
    },
    { "/desktop/gnome/interface/accessibility",
      META_PREF_GNOME_ACCESSIBILITY,
      &gnome_accessibility,
      FALSE,
    },
    { "/desktop/gnome/interface/enable_animations",
      META_PREF_GNOME_ANIMATIONS,
      &gnome_animations,
      TRUE,
    },
    { "/apps/metacity/general/compositing_manager",
      META_PREF_COMPOSITING_MANAGER,
      &compositing_manager,
      FALSE,
    },
    { NULL, 0, NULL, FALSE },
  };

static MetaStringPreference preferences_string[] =
  {
    { "/apps/metacity/general/mouse_button_modifier",
      META_PREF_MOUSE_BUTTON_MODS,
      mouse_button_mods_handler,
      NULL,
    },
    { "/apps/metacity/general/theme",
      META_PREF_THEME,
      theme_name_handler,
      NULL,
    },
    { KEY_TITLEBAR_FONT,
      META_PREF_TITLEBAR_FONT,
      titlebar_handler,
      NULL,
    },
    { KEY_TERMINAL_COMMAND,
      META_PREF_TERMINAL_COMMAND,
      NULL,
      &terminal_command,
    },
    { "/apps/metacity/general/button_layout",
      META_PREF_BUTTON_LAYOUT,
      button_layout_handler,
      NULL,
    },
    { "/desktop/gnome/peripherals/mouse/cursor_theme",
      META_PREF_CURSOR_THEME,
      NULL,
      &cursor_theme,
    },
    { NULL, 0, NULL, NULL },
  };

static MetaIntPreference preferences_int[] =
  {
    { "/apps/metacity/general/num_workspaces",
      META_PREF_NUM_WORKSPACES,
      &num_workspaces,
      /* I would actually recommend we change the destroy value to 4
       * and get rid of METAINTPREFERENCE_NO_CHANGE_ON_DESTROY entirely.
       *  -- tthurman
       */
      1, MAX_REASONABLE_WORKSPACES, METAINTPREFERENCE_NO_CHANGE_ON_DESTROY,
    },
    { "/apps/metacity/general/auto_raise_delay",
      META_PREF_AUTO_RAISE_DELAY,
      &auto_raise_delay,
      0, 10000, 0,
      /* @@@ Get rid of MAX_REASONABLE_AUTO_RAISE_DELAY */
    },
    { "/desktop/gnome/peripherals/mouse/cursor_size",
      META_PREF_CURSOR_SIZE,
      &cursor_size,
      1, 128, 24,
    },
    { NULL, 0, NULL, 0, 0, 0, },
  };

static void
handle_preference_init_enum (void)
{
  MetaEnumPreference *cursor = preferences_enum;

  while (cursor->key!=NULL)
    {
      char *value;
      GError *error = NULL;

      if (cursor->target==NULL)
        {
          ++cursor;
          continue;
        }

      value = gconf_client_get_string (default_client,
                                       cursor->key,
                                       &error);
      cleanup_error (&error);

      if (value==NULL)
        {
          ++cursor;
          continue;
        }

      if (!gconf_string_to_enum (cursor->symtab,
                                 value,
                                 (gint *) cursor->target))
        meta_warning (_("GConf key '%s' is set to an invalid value\n"),
                      cursor->key);

      g_free (value);

      ++cursor;
    }
}

static void
handle_preference_init_bool (void)
{
  MetaBoolPreference *cursor = preferences_bool;

  while (cursor->key!=NULL)
    {
      if (cursor->target!=NULL)
        get_bool (cursor->key, cursor->target);

      ++cursor;
    }

  maybe_give_disable_workarounds_warning ();
}

static void
handle_preference_init_string (void)
{
  MetaStringPreference *cursor = preferences_string;

  while (cursor->key!=NULL)
    {
      char *value;
      GError *error = NULL;
      gboolean dummy = TRUE;

      /* the string "value" will be newly allocated */
      value = gconf_client_get_string (default_client,
                                       cursor->key,
                                       &error);
      cleanup_error (&error);

      if (cursor->handler)
        {
          if (cursor->target)
            meta_bug ("%s has both a target and a handler\n", cursor->key);

          cursor->handler (cursor->pref, value, &dummy);

          g_free (value);
        }
      else if (cursor->target)
        {
          if (*(cursor->target))
            g_free (*(cursor->target));

          *(cursor->target) = value;
        }

      ++cursor;
    }
}

static void
handle_preference_init_int (void)
{
  MetaIntPreference *cursor = preferences_int;

  
  while (cursor->key!=NULL)
    {
      gint value;
      GError *error = NULL;

      value = gconf_client_get_int (default_client,
                                    cursor->key,
                                    &error);
      cleanup_error (&error);

      if (value < cursor->minimum || value > cursor->maximum)
        {
          meta_warning (_("%d stored in GConf key %s is out of range %d to %d\n"),
                        value, cursor->key,  cursor->minimum, cursor->maximum);
          /* Former behaviour for out-of-range values was:
           *   - number of workspaces was clamped;
           *   - auto raise delay was always reset to zero even if too high!;
           *   - cursor size was ignored.
           *
           * These seem to be meaningless variations.  If they did
           * have meaning we could have put them into MetaIntPreference.
           * The last of these is the closest to how we behave for
           * other types, so I think we should standardise on that.
           */
        }
      else if (cursor->target)
        *cursor->target = value;

      ++cursor;
    }
}

static gboolean
handle_preference_update_enum (const gchar *key, GConfValue *value)
{
  MetaEnumPreference *cursor = preferences_enum;
  gint old_value;

  while (cursor->key!=NULL && strcmp (key, cursor->key)!=0)
    ++cursor;

  if (cursor->key==NULL)
    /* Didn't recognise that key. */
    return FALSE;
      
  /* Setting it to null (that is, removing it) always means
   * "don't change".
   */

  if (value==NULL)
    return TRUE;

  /* Check the type.  Enums are always strings. */

  if (value->type != GCONF_VALUE_STRING)
    {
      meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                    key);
      /* But we did recognise it. */
      return TRUE;
    }

  /* We need to know whether the value changes, so
   * store the current value away.
   */

  old_value = * ((gint *) cursor->target);
  
  /* Now look it up... */

  if (!gconf_string_to_enum (cursor->symtab,
                             gconf_value_get_string (value),
                             (gint *) cursor->target))
    {
      /*
       * We found it, but it was invalid.  Complain.
       *
       * FIXME: This replicates the original behaviour, but in the future
       * we might consider reverting invalid keys to their original values.
       * (We know the old value, so we can look up a suitable string in
       * the symtab.)
       *
       * (Empty comment follows so the translators don't see this.)
       */

      /*  */      
      meta_warning (_("GConf key '%s' is set to an invalid value\n"),
                    key);
      return TRUE;
    }

  /* Did it change?  If so, tell the listeners about it. */

  if (old_value != *((gint *) cursor->target))
    queue_changed (cursor->pref);

  return TRUE;
}

static gboolean
handle_preference_update_bool (const gchar *key, GConfValue *value)
{
  MetaBoolPreference *cursor = preferences_bool;
  gboolean old_value;

  while (cursor->key!=NULL && strcmp (key, cursor->key)!=0)
    ++cursor;

  if (cursor->key==NULL)
    /* Didn't recognise that key. */
    return FALSE;

  if (cursor->target==NULL)
    /* No work for us to do. */
    return TRUE;
      
  if (value==NULL)
    {
      /* Value was destroyed; let's get out of here. */

      if (cursor->becomes_true_on_destruction)
        /* This preserves the behaviour of the old system, but
         * for all I know that might have been an oversight.
         */
        *((gboolean *)cursor->target) = TRUE;

      return TRUE;
    }

  /* Check the type. */

  if (value->type != GCONF_VALUE_BOOL)
    {
      meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                    key);
      /* But we did recognise it. */
      return TRUE;
    }

  /* We need to know whether the value changes, so
   * store the current value away.
   */

  old_value = * ((gboolean *) cursor->target);
  
  /* Now look it up... */

  *((gboolean *) cursor->target) = gconf_value_get_bool (value);

  /* Did it change?  If so, tell the listeners about it. */

  if (old_value != *((gboolean *) cursor->target))
    queue_changed (cursor->pref);

  if (cursor->pref==META_PREF_DISABLE_WORKAROUNDS)
    maybe_give_disable_workarounds_warning ();

  return TRUE;
}

static gboolean
handle_preference_update_string (const gchar *key, GConfValue *value)
{
  MetaStringPreference *cursor = preferences_string;
  const gchar *value_as_string;
  gboolean inform_listeners = TRUE;

  while (cursor->key!=NULL && strcmp (key, cursor->key)!=0)
    ++cursor;

  if (cursor->key==NULL)
    /* Didn't recognise that key. */
    return FALSE;

  if (value==NULL)
    return TRUE;

  /* Check the type. */

  if (value->type != GCONF_VALUE_STRING)
    {
      meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                    key);
      /* But we did recognise it. */
      return TRUE;
    }

  /* Docs: "The returned string is not a copy, don't try to free it." */
  value_as_string = gconf_value_get_string (value);

  if (cursor->handler)
    cursor->handler (cursor->pref, value_as_string, &inform_listeners);
  else if (cursor->target)
    {
      if (*(cursor->target))
        g_free(*(cursor->target));

      if (value_as_string!=NULL)
        *(cursor->target) = g_strdup (value_as_string);
      else
        *(cursor->target) = NULL;

      inform_listeners =
        (value_as_string==NULL && *(cursor->target)==NULL) ||
        (value_as_string!=NULL && *(cursor->target)!=NULL &&
         strcmp (value_as_string, *(cursor->target))==0);
    }

  if (inform_listeners)
    queue_changed (cursor->pref);

  return TRUE;
}

static gboolean
handle_preference_update_int (const gchar *key, GConfValue *value)
{
  MetaIntPreference *cursor = preferences_int;
  gint new_value;

  while (cursor->key!=NULL && strcmp (key, cursor->key)!=0)
    ++cursor;

  if (cursor->key==NULL)
    /* Didn't recognise that key. */
    return FALSE;

  if (cursor->target==NULL)
    /* No work for us to do. */
    return TRUE;
      
  if (value==NULL)
    {
      /* Value was destroyed. */

      if (cursor->value_if_destroyed != METAINTPREFERENCE_NO_CHANGE_ON_DESTROY)
        *((gint *)cursor->target) = cursor->value_if_destroyed;

      return TRUE;
    }

  /* Check the type. */

  if (value->type != GCONF_VALUE_INT)
    {
      meta_warning (_("GConf key \"%s\" is set to an invalid type\n"),
                    key);
      /* But we did recognise it. */
      return TRUE;
    }

  new_value = gconf_value_get_int (value);

  if (new_value < cursor->minimum || new_value > cursor->maximum)
    {
      meta_warning (_("%d stored in GConf key %s is out of range %d to %d\n"),
                    new_value, cursor->key,
                    cursor->minimum, cursor->maximum);
      return TRUE;
    }

  /* Did it change?  If so, tell the listeners about it. */

  if (*cursor->target != new_value)
    {
      *cursor->target = new_value;
      queue_changed (cursor->pref);
    }

  return TRUE;
  
}


/****************************************************************************/
/* Listeners.                                                               */
/****************************************************************************/

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

#else /* HAVE_GCONF */

void
meta_prefs_add_listener (MetaPrefsChangedFunc func,
                         gpointer             data)
{
  /* Nothing, because they have gconf turned off */
}

void
meta_prefs_remove_listener (MetaPrefsChangedFunc func,
                            gpointer             data)
{
  /* Nothing, because they have gconf turned off */
}

#endif /* HAVE_GCONF */


/****************************************************************************/
/* Initialisation.                                                          */
/****************************************************************************/

#ifdef HAVE_GCONF
/* @@@ again, use glib's ability to tell you the size of the array */
static gchar *gconf_dirs_we_are_interested_in[] = {
  "/apps/metacity",
  KEY_TERMINAL_DIR,
  KEY_GNOME_ACCESSIBILITY,
  "/desktop/gnome/peripherals/mouse",
  "/desktop/gnome/interface",
  NULL,
};
#endif

void
meta_prefs_init (void)
{
#ifdef HAVE_GCONF
  GError *err = NULL;
  gchar **gconf_dir_cursor;
  
  if (default_client != NULL)
    return;
  
  /* returns a reference which we hold forever */
  default_client = gconf_client_get_default ();

  for (gconf_dir_cursor=gconf_dirs_we_are_interested_in;
       *gconf_dir_cursor!=NULL;
       gconf_dir_cursor++)
    {
      gconf_client_add_dir (default_client,
                            *gconf_dir_cursor,
                            GCONF_CLIENT_PRELOAD_RECURSIVE,
                            &err);
      cleanup_error (&err);
    }

  /* Pick up initial values. */

  handle_preference_init_enum ();
  handle_preference_init_bool ();
  handle_preference_init_string ();
  handle_preference_init_int ();

  /* @@@ Is there any reason we don't do the add_dir here? */
  for (gconf_dir_cursor=gconf_dirs_we_are_interested_in;
       *gconf_dir_cursor!=NULL;
       gconf_dir_cursor++)
    {
      gconf_client_notify_add (default_client,
                               *gconf_dir_cursor,
                               change_notify,
                               NULL,
                               NULL,
                               &err);
      cleanup_error (&err);
    }

#else  /* HAVE_GCONF */

  /* Set defaults for some values that can't be set at initialization time of
   * the static globals.  In the case of the theme, note that there is code
   * elsewhere that will do everything possible to fallback to an existing theme
   * if the one here does not exist.
   */
  titlebar_font = pango_font_description_from_string ("Sans Bold 10");
  current_theme = g_strdup ("Atlanta");
  
  init_button_layout();
#endif /* HAVE_GCONF */
  
  init_bindings ();
  init_commands ();
  init_workspace_names ();
}


/****************************************************************************/
/* Updates.                                                                 */
/****************************************************************************/

#ifdef HAVE_GCONF

gboolean (*preference_update_handler[]) (const gchar*, GConfValue*) = {
  handle_preference_update_enum,
  handle_preference_update_bool,
  handle_preference_update_string,
  handle_preference_update_int,
  NULL
};

static void
change_notify (GConfClient    *client,
               guint           cnxn_id,
               GConfEntry     *entry,
               gpointer        user_data)
{
  const char *key;
  GConfValue *value;
  gint i=0;
  
  key = gconf_entry_get_key (entry);
  value = gconf_entry_get_value (entry);

  /* First, search for a handler that might know what to do. */

  /* FIXME: When this is all working, since the first item in every
   * array is the gchar* of the key, there's no reason we can't
   * find the correct record for that key here and save code duplication.
   */

  while (preference_update_handler[i]!=NULL)
    {
      if (preference_update_handler[i] (key, value))
        goto out; /* Get rid of this eventually */

      i++;
    }
  
  if (g_str_has_prefix (key, KEY_WINDOW_BINDINGS_PREFIX) ||
      g_str_has_prefix (key, KEY_SCREEN_BINDINGS_PREFIX))
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

          if (update_key_list_binding (key, list))
            queue_changed (META_PREF_KEYBINDINGS);
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

          if (update_key_binding (key, str))
            queue_changed (META_PREF_KEYBINDINGS);
        }
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
  else
    {
      meta_topic (META_DEBUG_PREFS, "Key %s doesn't mean anything to Metacity\n",
                  key);
    }
  
 out:
  /* nothing */
  return; /* AIX compiler wants something after a label like out: */
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

/* get_bool returns TRUE if *val is filled in, FALSE otherwise */
/* @@@ probably worth moving this inline; only used once */
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

/**
 * Special case: give a warning the first time disable_workarounds
 * is turned on.
 */
static void
maybe_give_disable_workarounds_warning (void)
{
  static gboolean first_disable = TRUE;
    
  if (first_disable && disable_workarounds)
    {
      first_disable = FALSE;

      meta_warning (_("Workarounds for broken applications disabled. "
                      "Some applications may not behave properly.\n"));
    }
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

const char*
meta_prefs_get_cursor_theme (void)
{
  return cursor_theme;
}

int
meta_prefs_get_cursor_size (void)
{
  return cursor_size;
}


/****************************************************************************/
/* Handlers for string preferences.                                         */
/****************************************************************************/

#ifdef HAVE_GCONF

static void
titlebar_handler (MetaPreference pref,
                  const gchar    *string_value,
                  gboolean       *inform_listeners)
{
  PangoFontDescription *new_desc = NULL;

  if (string_value)
    new_desc = pango_font_description_from_string (string_value);

  if (new_desc == NULL)
    {
      meta_warning (_("Could not parse font description "
                      "\"%s\" from GConf key %s\n"),
                    string_value ? string_value : "(null)",
                    KEY_TITLEBAR_FONT);

      *inform_listeners = FALSE;

      return;
    }

  /* Is the new description the same as the old? */

  if (titlebar_font &&
      pango_font_description_equal (new_desc, titlebar_font))
    {
      pango_font_description_free (new_desc);
      *inform_listeners = FALSE;
      return;
    }

  /* No, so free the old one and put ours in instead. */

  if (titlebar_font)
    pango_font_description_free (titlebar_font);

  titlebar_font = new_desc;

}

static void
theme_name_handler (MetaPreference pref,
                    const gchar *string_value,
                    gboolean *inform_listeners)
{
  /* Fallback crackrock */
  if (string_value == NULL)
    current_theme = g_strdup ("Atlanta");
  else
    current_theme = g_strdup (string_value);
}

static void
mouse_button_mods_handler (MetaPreference pref,
                           const gchar *string_value,
                           gboolean *inform_listeners)
{
  MetaVirtualModifier mods;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Mouse button modifier has new gconf value \"%s\"\n",
              string_value);
  if (string_value && meta_ui_parse_modifier (string_value, &mods))
    {
      mouse_button_mods = mods;
    }
  else
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Failed to parse new gconf value\n");
          
      meta_warning (_("\"%s\" found in configuration database is "
                      "not a valid value for mouse button modifier\n"),
                    string_value);

      *inform_listeners = FALSE;
    }
}

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
      if (a->left_buttons_has_spacer[i] != b->left_buttons_has_spacer[i])
        return FALSE;
      if (a->right_buttons_has_spacer[i] != b->right_buttons_has_spacer[i])
        return FALSE;
      ++i;
    }

  return TRUE;
}

static MetaButtonFunction
button_function_from_string (const char *str)
{
  /* FIXME: gconf_string_to_enum is the obvious way to do this */

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

static void
button_layout_handler (MetaPreference pref,
                         const gchar *string_value,
                         gboolean *inform_listeners)
{
  MetaButtonLayout new_layout;
  char **sides = NULL;
  int i;
  
  /* We need to ignore unknown button functions, for
   * compat with future versions
   */
  
  if (string_value)
    sides = g_strsplit (string_value, ":", 2);

  if (sides != NULL && sides[0] != NULL)
    {
      char **buttons;
      int b;
      gboolean used[META_BUTTON_FUNCTION_LAST];

      i = 0;
      while (i < META_BUTTON_FUNCTION_LAST)
        {
          used[i] = FALSE;
          new_layout.left_buttons_has_spacer[i] = FALSE;
          ++i;
        }
      
      buttons = g_strsplit (sides[0], ",", -1);
      i = 0;
      b = 0;
      while (buttons[b] != NULL)
        {
          MetaButtonFunction f = button_function_from_string (buttons[b]);
          if (i > 0 && strcmp("spacer", buttons[b]) == 0)
            {
              new_layout.left_buttons_has_spacer[i-1] = TRUE;
              f = button_opposite_function (f);

              if (f != META_BUTTON_FUNCTION_LAST)
                {
                  new_layout.left_buttons_has_spacer[i-2] = TRUE;
                }
            }
          else
            {
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
            }
          
          ++b;
        }

      new_layout.left_buttons[i] = META_BUTTON_FUNCTION_LAST;
      new_layout.left_buttons_has_spacer[i] = FALSE;
      
      g_strfreev (buttons);
    }

  if (sides != NULL && sides[0] != NULL && sides[1] != NULL)
    {
      char **buttons;
      int b;
      gboolean used[META_BUTTON_FUNCTION_LAST];

      i = 0;
      while (i < META_BUTTON_FUNCTION_LAST)
        {
          used[i] = FALSE;
          new_layout.right_buttons_has_spacer[i] = FALSE;
          ++i;
        }
      
      buttons = g_strsplit (sides[1], ",", -1);
      i = 0;
      b = 0;
      while (buttons[b] != NULL)
        {
          MetaButtonFunction f = button_function_from_string (buttons[b]);
          if (i > 0 && strcmp("spacer", buttons[b]) == 0)
            {
              new_layout.right_buttons_has_spacer[i-1] = TRUE;
              f = button_opposite_function (f);
              if (f != META_BUTTON_FUNCTION_LAST)
                {
                  new_layout.right_buttons_has_spacer[i-2] = TRUE;
                }
            }
          else
            {
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
            }
          
          ++b;
        }

      new_layout.right_buttons[i] = META_BUTTON_FUNCTION_LAST;
      new_layout.right_buttons_has_spacer[i] = FALSE;
      
      g_strfreev (buttons);
    }

  g_strfreev (sides);
  
  /* Invert the button layout for RTL languages */
  if (meta_ui_get_direction() == META_UI_DIRECTION_RTL)
  {
    MetaButtonLayout rtl_layout;
    int j;
    
    for (i = 0; new_layout.left_buttons[i] != META_BUTTON_FUNCTION_LAST; i++);
    for (j = 0; j < i; j++)
      {
        rtl_layout.right_buttons[j] = new_layout.left_buttons[i - j - 1];
        if (j == 0)
          rtl_layout.right_buttons_has_spacer[i - 1] = new_layout.left_buttons_has_spacer[i - j - 1];
        else
          rtl_layout.right_buttons_has_spacer[j - 1] = new_layout.left_buttons_has_spacer[i - j - 1];
      }
    rtl_layout.right_buttons[j] = META_BUTTON_FUNCTION_LAST;
    rtl_layout.right_buttons_has_spacer[j] = FALSE;
      
    for (i = 0; new_layout.right_buttons[i] != META_BUTTON_FUNCTION_LAST; i++);
    for (j = 0; j < i; j++)
      {
        rtl_layout.left_buttons[j] = new_layout.right_buttons[i - j - 1];
        if (j == 0)
          rtl_layout.left_buttons_has_spacer[i - 1] = new_layout.right_buttons_has_spacer[i - j - 1];
        else
          rtl_layout.left_buttons_has_spacer[j - 1] = new_layout.right_buttons_has_spacer[i - j - 1];
      }
    rtl_layout.left_buttons[j] = META_BUTTON_FUNCTION_LAST;
    rtl_layout.left_buttons_has_spacer[j] = FALSE;

    new_layout = rtl_layout;
  }
  
  if (button_layout_equal (&button_layout, &new_layout))
    {
      /* Same as before, so duck out */
      *inform_listeners = FALSE;
    }
  else
    {
      button_layout = new_layout;
    }
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

int
meta_prefs_get_num_workspaces (void)
{
  return num_workspaces;
}

gboolean
meta_prefs_get_application_based (void)
{
  return FALSE; /* For now, we never want this to do anything */
  
  return application_based;
}

gboolean
meta_prefs_get_disable_workarounds (void)
{
  return disable_workarounds;
}

#ifdef HAVE_GCONF
#define MAX_REASONABLE_AUTO_RAISE_DELAY 10000
  
#endif /* HAVE_GCONF */

#ifdef WITH_VERBOSE_MODE
const char*
meta_preference_to_string (MetaPreference pref)
{
  /* FIXME: another case for gconf_string_to_enum */
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

    case META_PREF_KEYBINDINGS:
      return "KEYBINDINGS";

    case META_PREF_DISABLE_WORKAROUNDS:
      return "DISABLE_WORKAROUNDS";

    case META_PREF_ACTION_DOUBLE_CLICK_TITLEBAR:
      return "ACTION_DOUBLE_CLICK_TITLEBAR";

    case META_PREF_ACTION_MIDDLE_CLICK_TITLEBAR:
      return "ACTION_MIDDLE_CLICK_TITLEBAR";

    case META_PREF_ACTION_RIGHT_CLICK_TITLEBAR:
      return "ACTION_RIGHT_CLICK_TITLEBAR";

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

    case META_PREF_GNOME_ANIMATIONS:
      return "GNOME_ANIMATIONS";

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

#define keybind(name, handler, param, flags, stroke, description) \
  { #name, NULL, !!(flags & BINDING_REVERSES), !!(flags & BINDING_PER_WINDOW) },
static MetaKeyPref key_bindings[] = {
#include "all-keybindings.h"
  { NULL, NULL, FALSE }
};
#undef keybind

#ifndef HAVE_GCONF

/**
 * A type to map names of keybindings (such as "switch_windows")
 * to the binding strings themselves (such as "<Alt>Tab").
 * It exists only when GConf is turned off in ./configure and
 * functions as a sort of ersatz GConf.
 */
typedef struct
{
  const char *name;
  const char *keybinding;
} MetaSimpleKeyMapping;

/* FIXME: This would be neater if the array only contained entries whose
 * default keystroke was non-null.  You COULD do this by defining
 * ONLY_BOUND_BY_DEFAULT around various blocks at the cost of making
 * the bindings file way more complicated.  However, we could stop this being
 * data and move it into code.  Then the compiler would optimise away
 * the problem lines.
 */

#define keybind(name, handler, param, flags, stroke, description) \
  { #name, stroke },

static MetaSimpleKeyMapping key_string_bindings[] = {
#include "all-keybindings.h"
  { NULL, NULL }
};
#undef keybind

#endif /* NOT HAVE_GCONF */

static void
init_bindings (void)
{
#ifdef HAVE_GCONF  
  int i = 0;
  GError *err;

  while (key_bindings[i].name)
    {
      GSList *list_val, *tmp;
      char *str_val;
      char *key;
 
      key = g_strconcat (key_bindings[i].per_window?
                         KEY_WINDOW_BINDINGS_PREFIX:
                         KEY_SCREEN_BINDINGS_PREFIX,
                         "/",
                         key_bindings[i].name, NULL);
 
      err = NULL;
      str_val = gconf_client_get_string (default_client, key, &err);
      cleanup_error (&err);

      update_binding (&key_bindings[i], str_val);

      g_free (str_val);      
      g_free (key);

      key = g_strconcat (key_bindings[i].per_window?
                         KEY_WINDOW_BINDINGS_PREFIX:
                         KEY_SCREEN_BINDINGS_PREFIX,
                         "/",
                         key_bindings[i].name,
                         KEY_LIST_BINDINGS_SUFFIX, NULL);

      err = NULL;

      list_val = gconf_client_get_list (default_client, key, GCONF_VALUE_STRING, &err);
      cleanup_error (&err);
 
      update_list_binding (&key_bindings[i], list_val, META_LIST_OF_STRINGS);

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
  while (key_string_bindings[i].name)
    {
      if (key_string_bindings[i].keybinding == NULL) {
        ++i;
        continue;
      }
    
      while (strcmp(key_bindings[which].name, 
                    key_string_bindings[i].name) != 0)
        which++;

      /* Set the binding */
      update_binding (&key_bindings[which], 
                      key_string_bindings[i].keybinding);

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

#ifdef HAVE_GCONF
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

      /* FIXME: add_shift is currently screen_bindings only, but
       * there's no really good reason it should always be.
       * So we shouldn't blindly add KEY_SCREEN_BINDINGS_PREFIX
       * onto here.
       */
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
#endif
  
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

#ifdef HAVE_GCONF
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
update_key_binding (const char *name,
                       const char *value)
{
  return find_and_update_binding (key_bindings, name, value);
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
update_key_list_binding (const char *name,
                            GSList *value)
{
  return find_and_update_list_binding (key_bindings, name, value);
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
meta_prefs_get_key_bindings (const MetaKeyPref **bindings,
                                int                *n_bindings)
{
  
  *bindings = key_bindings;
  *n_bindings = (int) G_N_ELEMENTS (key_bindings) - 1;
}

MetaActionTitlebar
meta_prefs_get_action_double_click_titlebar (void)
{
  return action_double_click_titlebar;
}

MetaActionTitlebar
meta_prefs_get_action_middle_click_titlebar (void)
{
  return action_middle_click_titlebar;
}

MetaActionTitlebar
meta_prefs_get_action_right_click_titlebar (void)
{
  return action_right_click_titlebar;
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

gboolean
meta_prefs_get_gnome_animations ()
{
  return gnome_animations;
}

MetaKeyBindingAction
meta_prefs_get_keybinding_action (const char *name)
{
  int i;

  i = G_N_ELEMENTS (key_bindings) - 2; /* -2 for dummy entry at end */
  while (i >= 0)
    {
      if (strcmp (key_bindings[i].name, name) == 0)
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

  i = G_N_ELEMENTS (key_bindings) - 2; /* -2 for dummy entry at end */
  while (i >= 0)
    {
      if (key_bindings[i].per_window &&
          strcmp (key_bindings[i].name, name) == 0)
        {
          GSList *s = key_bindings[i].bindings;

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

gboolean
meta_prefs_get_compositing_manager (void)
{
  return compositing_manager;
}

void
meta_prefs_set_compositing_manager (gboolean whether)
{
#ifdef HAVE_GCONF
  GError *err = NULL;

  gconf_client_set_bool (default_client,
                         KEY_COMPOSITOR,
                         whether,
                         &err);

  if (err)
    {
      meta_warning (_("Error setting compositor status: %s\n"),
                    err->message);
      g_error_free (err);
    }
#else
  compositing_manager = whether;
#endif
}

#ifndef HAVE_GCONF
static void
init_button_layout(void)
{
  MetaButtonLayout button_layout_ltr = {
    {    
      /* buttons in the group on the left side */
      META_BUTTON_FUNCTION_MENU,
      META_BUTTON_FUNCTION_LAST
    },
    {
      /* buttons in the group on the right side */
      META_BUTTON_FUNCTION_MINIMIZE,
      META_BUTTON_FUNCTION_MAXIMIZE,
      META_BUTTON_FUNCTION_CLOSE,
      META_BUTTON_FUNCTION_LAST
    }
  };
  MetaButtonLayout button_layout_rtl = {
    {    
      /* buttons in the group on the left side */
      META_BUTTON_FUNCTION_CLOSE,
      META_BUTTON_FUNCTION_MAXIMIZE,
      META_BUTTON_FUNCTION_MINIMIZE,
      META_BUTTON_FUNCTION_LAST
    },
    {
      /* buttons in the group on the right side */
      META_BUTTON_FUNCTION_MENU,
      META_BUTTON_FUNCTION_LAST
    }
  };

  button_layout = meta_ui_get_direction() == META_UI_DIRECTION_LTR ?
    button_layout_ltr : button_layout_rtl;
};

#endif
