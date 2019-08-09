/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-password-entry.c: Password entry actor based on st-entry
 *
 * Copyright 2019 Endless Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "st-private.h"
#include "st-password-entry.h"
#include "st-icon.h"

#define BLACK_CIRCLE 9679

#define ST_PASSWORD_ENTRY_PRIV(x) st_password_entry_get_instance_private ((StPasswordEntry *) x)

/* properties */
enum
{
  PROP_0,

  PROP_CAPS_LOCK_WARNING,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

struct _StPasswordEntryPrivate
{
  ClutterActor *peek_password_icon;
  gboolean      capslock_warning_shown;
  gboolean      password_shown;
};

G_DEFINE_TYPE_WITH_PRIVATE (StPasswordEntry, st_password_entry, ST_TYPE_ENTRY);

static void
st_password_entry_get_property (GObject    *gobject,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  StPasswordEntryPrivate *priv = ST_PASSWORD_ENTRY_PRIV (gobject);

  switch (prop_id)
    {
    case PROP_CAPS_LOCK_WARNING:
      g_value_set_boolean (value, priv->capslock_warning_shown);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
update_caps_lock_warning_visibility (ClutterKeymap *keymap,
                                     gpointer       user_data)
{
  StPasswordEntry *entry = ST_PASSWORD_ENTRY (user_data);
  StPasswordEntryPrivate *priv = ST_PASSWORD_ENTRY_PRIV (entry);

  if (!priv->password_shown && clutter_keymap_get_caps_lock_state (keymap))
    priv->capslock_warning_shown = TRUE;
  else
    priv->capslock_warning_shown = FALSE;

  g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_CAPS_LOCK_WARNING]);
}

static void
st_password_entry_dispose (GObject *object)
{
  StPasswordEntry *entry = ST_PASSWORD_ENTRY (object);
  ClutterKeymap *keymap;

  keymap = clutter_backend_get_keymap (clutter_get_default_backend ());
  g_signal_handlers_disconnect_by_func (keymap, update_caps_lock_warning_visibility, entry);

  G_OBJECT_CLASS (st_password_entry_parent_class)->dispose (object);
}

static void
st_password_entry_secondary_icon_clicked (StEntry *entry)
{
  st_password_entry_toggle_peek_password (ST_PASSWORD_ENTRY (entry));
}

static void
st_password_entry_class_init (StPasswordEntryClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  StEntryClass *st_entry_class = ST_ENTRY_CLASS (klass);

  gobject_class->get_property = st_password_entry_get_property;
  gobject_class->dispose = st_password_entry_dispose;

  st_entry_class->secondary_icon_clicked = st_password_entry_secondary_icon_clicked;

  props[PROP_CAPS_LOCK_WARNING] = g_param_spec_boolean ("caps-lock-warning",
                                                        "Caps lock warning",
                                                        "Whether to show the capslock-warning",
                                                        FALSE,
                                                        ST_PARAM_READABLE);

  g_object_class_install_properties (gobject_class, N_PROPS, props);
}

static void
clutter_text_focus_in_cb (ClutterText     *text,
                          StPasswordEntry *entry)
{
  ClutterKeymap *keymap;

  keymap = clutter_backend_get_keymap (clutter_get_default_backend ());
  update_caps_lock_warning_visibility (keymap, entry);
  g_signal_connect (keymap, "state-changed",
                    G_CALLBACK (update_caps_lock_warning_visibility), entry);
}

static void
clutter_text_focus_out_cb (ClutterText     *text,
                           StPasswordEntry *entry)
{
  ClutterKeymap *keymap;

  keymap = clutter_backend_get_keymap (clutter_get_default_backend ());
  g_signal_handlers_disconnect_by_func (keymap, update_caps_lock_warning_visibility, entry);
}

static void
clutter_text_password_char_cb (GObject    *object,
                               GParamSpec *pspec,
                               gpointer    user_data)
{
  StPasswordEntry *entry = ST_PASSWORD_ENTRY (user_data);
  ClutterKeymap *keymap;

  keymap = clutter_backend_get_keymap (clutter_get_default_backend ());
  update_caps_lock_warning_visibility (keymap, entry);
}

static void
st_password_entry_init (StPasswordEntry *entry)
{
  StPasswordEntryPrivate *priv = ST_PASSWORD_ENTRY_PRIV (entry);
  ClutterActor *clutter_text;

  st_entry_set_text (ST_ENTRY (entry), "");

  priv->peek_password_icon = g_object_new (ST_TYPE_ICON,
                                           "style-class", "peek-password",
                                           "icon-name", "eye-not-looking-symbolic",
                                           NULL);
  st_entry_set_secondary_icon (ST_ENTRY(entry), priv->peek_password_icon);
  clutter_text = st_entry_get_clutter_text (ST_ENTRY (entry));
  clutter_text_set_password_char (CLUTTER_TEXT (clutter_text), BLACK_CIRCLE);
  priv->password_shown = FALSE;

  st_entry_set_input_purpose (ST_ENTRY (entry), CLUTTER_INPUT_CONTENT_PURPOSE_PASSWORD);

  g_signal_connect (clutter_text, "key-focus-in",
                    G_CALLBACK (clutter_text_focus_in_cb), entry);

  g_signal_connect (clutter_text, "key-focus-out",
                    G_CALLBACK (clutter_text_focus_out_cb), entry);

  g_signal_connect (clutter_text, "notify::password-char",
                    G_CALLBACK (clutter_text_password_char_cb), entry);

}

/**
 * st_password_entry_new:
 *
 * Create a new #StPasswordEntry.
 *
 * Returns: a new #StPasswordEntry
 */
StPasswordEntry*
st_password_entry_new (void)
{
  StPasswordEntry *entry;

  entry = g_object_new (ST_PASSWORD_TYPE_ENTRY, NULL);

  return entry;
}

/**
 * st_password_entry_show_password:
 * @entry: a #StPasswordEntry
 *
 * Reveals the current text in the password entry.
 */
void
st_password_entry_show_password (StPasswordEntry *entry)
{
  StPasswordEntryPrivate *priv;
  ClutterActor *clutter_text;

  g_return_if_fail (ST_IS_PASSWORD_ENTRY (entry));

  priv = ST_PASSWORD_ENTRY_PRIV (entry);
  if (priv->password_shown)
    return;

  priv->password_shown = TRUE;

  clutter_text = st_entry_get_clutter_text (ST_ENTRY (entry));
  clutter_text_set_password_char (CLUTTER_TEXT (clutter_text), 0);

  st_icon_set_icon_name (ST_ICON (priv->peek_password_icon), "eye-open-negative-filled-symbolic");
}

/**
 * st_password_entry_hide_password:
 * @entry: a #StPasswordEntry
 *
 * Hides the current text as series of • in the password entry.
 */
void
st_password_entry_hide_password (StPasswordEntry *entry)
{
  StPasswordEntryPrivate *priv;
  ClutterActor *clutter_text;

  g_return_if_fail (ST_IS_PASSWORD_ENTRY (entry));

  priv = ST_PASSWORD_ENTRY_PRIV (entry);
  if (!priv->password_shown)
    return;

  priv->password_shown = FALSE;

  clutter_text = st_entry_get_clutter_text (ST_ENTRY (entry));
  clutter_text_set_password_char (CLUTTER_TEXT (clutter_text), BLACK_CIRCLE);

  st_icon_set_icon_name (ST_ICON (priv->peek_password_icon), "eye-not-looking-symbolic");
}

/**
 * st_password_entry_toggle_peek_password:
 * @entry: a #StPasswordEntry
 *
 * Toggles the revelability of the current text. If the text
 * is hidden, calling this function reveals it and vice-versa.
 */
void
st_password_entry_toggle_peek_password (StPasswordEntry *entry)
{
  StPasswordEntryPrivate *priv = ST_PASSWORD_ENTRY_PRIV (entry);

  g_return_if_fail (ST_IS_PASSWORD_ENTRY (entry));

  if (priv->password_shown)
    st_password_entry_hide_password (entry);
  else
    st_password_entry_show_password (entry);
}

/**
 * st_password_entry_disable_password_peek_icon:
 * @entry: a #StPasswordEntry
 *
 * Disables visibility of peek-password icons from the password entry.
 * This is useful in cases where the peeking the password functionality
 * needs to be avoided.
 */
void
st_password_entry_disable_password_peek_icon (StPasswordEntry *entry)
{
  g_return_if_fail (ST_IS_PASSWORD_ENTRY (entry));

  //TODO: How to disable the secondary_icon_clicked signal handler here?
  st_entry_set_secondary_icon (ST_ENTRY (entry), NULL);
}

/**
 * st_password_entry_get_caps_lock_feedback:
 * @entry: a #StPasswordEntry
 *
 * Returns whether to show the caps lock warning based on the caps
 * lock key status.
 */
gboolean
st_password_entry_get_caps_lock_feedback (StPasswordEntry *entry)
{
  StPasswordEntryPrivate *priv = ST_PASSWORD_ENTRY_PRIV (entry);

  g_return_val_if_fail (ST_IS_PASSWORD_ENTRY (entry), FALSE);

  return priv->capslock_warning_shown;
}
