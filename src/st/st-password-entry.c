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
#include "st-settings.h"

#define BLACK_CIRCLE 9679

#define ST_PASSWORD_ENTRY_PRIV(x) st_password_entry_get_instance_private ((StPasswordEntry *) x)

typedef struct _StPasswordEntryPrivate   StPasswordEntryPrivate;

struct _StPasswordEntry
{
  /*< private >*/
  StEntry parent_instance;
};

struct _StPasswordEntryPrivate
{
  ClutterActor *peek_password_icon;

  gboolean      password_visible;
  gboolean      show_peek_icon;
};

enum
{
  PROP_0,

  PROP_PASSWORD_VISIBLE,
  PROP_SHOW_PEEK_ICON,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (StPasswordEntry, st_password_entry, ST_TYPE_ENTRY);

static gboolean
show_password_locked_down (StPasswordEntry *entry)
{
  gboolean disable_show_password = FALSE;

  g_object_get (st_settings_get (), "disable-show-password", &disable_show_password, NULL);

  return disable_show_password;
}

static void
st_password_entry_secondary_icon_clicked (StEntry *entry)
{
  StPasswordEntry *password_entry = ST_PASSWORD_ENTRY (entry);
  StPasswordEntryPrivate *priv = ST_PASSWORD_ENTRY_PRIV (password_entry);

  st_password_entry_set_password_visible (password_entry, !priv->password_visible);
}

static void
st_password_entry_get_property (GObject    *gobject,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  StPasswordEntry *entry = ST_PASSWORD_ENTRY (gobject);
  StPasswordEntryPrivate *priv = ST_PASSWORD_ENTRY_PRIV (gobject);

  switch (prop_id)
    {
    case PROP_PASSWORD_VISIBLE:
      g_value_set_boolean (value, priv->password_visible);
      break;

    case PROP_SHOW_PEEK_ICON:
      g_value_set_boolean (value, st_password_entry_get_show_peek_icon (entry));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_password_entry_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  StPasswordEntry *entry = ST_PASSWORD_ENTRY (gobject);

  switch (prop_id)
    {
    case PROP_PASSWORD_VISIBLE:
      st_password_entry_set_password_visible (entry, g_value_get_boolean (value));
      break;

    case PROP_SHOW_PEEK_ICON:
      st_password_entry_set_show_peek_icon (entry, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_password_entry_dispose (GObject *gobject)
{
  StPasswordEntryPrivate *priv = ST_PASSWORD_ENTRY_PRIV (gobject);

  g_clear_object (&priv->peek_password_icon);

  G_OBJECT_CLASS(st_password_entry_parent_class)->dispose (gobject);
}

static void
st_password_entry_class_init (StPasswordEntryClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  StEntryClass *st_entry_class = ST_ENTRY_CLASS (klass);

  gobject_class->get_property = st_password_entry_get_property;
  gobject_class->set_property = st_password_entry_set_property;
  gobject_class->dispose = st_password_entry_dispose;

  st_entry_class->secondary_icon_clicked = st_password_entry_secondary_icon_clicked;

  /**
   * StPasswordEntry:password-visible:
   *
   * Whether the text in the entry is masked for privacy.
   */
  props[PROP_PASSWORD_VISIBLE] = g_param_spec_boolean ("password-visible",
                                                       "Password visible",
                                                       "Whether the text in the entry is masked or not",
                                                       FALSE,
                                                       ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StPasswordEntry:show-peek-icon:
   *
   * Whether to display an icon button to toggle the masking enabled by the
   * #StPasswordEntry:password-visible property.
   */
  props[PROP_SHOW_PEEK_ICON] = g_param_spec_boolean ("show-peek-icon",
                                                     "Show peek icon",
                                                     "Whether to show the password peek icon",
                                                     TRUE,
                                                     ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, props);
}

static void
update_peek_icon (StPasswordEntry *entry)
{
  StPasswordEntryPrivate *priv = ST_PASSWORD_ENTRY_PRIV (entry);
  gboolean show_peek_icon;

  show_peek_icon = st_password_entry_get_show_peek_icon (entry);

  if (show_peek_icon)
    st_entry_set_secondary_icon (ST_ENTRY (entry), priv->peek_password_icon);
  else
    st_entry_set_secondary_icon (ST_ENTRY (entry), NULL);
}

static void
on_disable_show_password_changed (GObject    *object,
                                  GParamSpec *pspec,
                                  gpointer    user_data)
{
  StPasswordEntry *entry = ST_PASSWORD_ENTRY (user_data);

  if (show_password_locked_down (entry))
    st_password_entry_set_password_visible (entry, FALSE);

  update_peek_icon (entry);

  g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_SHOW_PEEK_ICON]);
}

static void
clutter_text_password_char_cb (GObject    *object,
                               GParamSpec *pspec,
                               gpointer    user_data)
{
  StPasswordEntry *entry = ST_PASSWORD_ENTRY (user_data);
  ClutterActor *clutter_text;

  clutter_text = st_entry_get_clutter_text (ST_ENTRY (entry));
  if (clutter_text_get_password_char (CLUTTER_TEXT (clutter_text)) == 0)
    st_password_entry_set_password_visible (entry, TRUE);
  else
    st_password_entry_set_password_visible (entry, FALSE);
}

static void
st_password_entry_init (StPasswordEntry *entry)
{
  StPasswordEntryPrivate *priv = ST_PASSWORD_ENTRY_PRIV (entry);
  ClutterActor *clutter_text;

  priv->peek_password_icon = g_object_new (ST_TYPE_ICON,
                                           "style-class", "peek-password",
                                           "icon-name", "view-reveal-symbolic",
                                           NULL);
  st_entry_set_secondary_icon (ST_ENTRY (entry), priv->peek_password_icon);

  st_password_entry_set_show_peek_icon (entry, TRUE);

  g_signal_connect_object (st_settings_get (),
                           "notify::disable-show-password",
                           G_CALLBACK (on_disable_show_password_changed),
                           entry,
                           0);

  clutter_text = st_entry_get_clutter_text (ST_ENTRY (entry));
  clutter_text_set_password_char (CLUTTER_TEXT (clutter_text), BLACK_CIRCLE);

  st_entry_set_input_purpose (ST_ENTRY (entry), CLUTTER_INPUT_CONTENT_PURPOSE_PASSWORD);

  g_signal_connect (clutter_text, "notify::password-char",
                    G_CALLBACK (clutter_text_password_char_cb), entry);
}

/**
 * st_password_entry_new:
 *
 * Create a new #StPasswordEntry.
 *
 * Returns: a new #StEntry
 */
StEntry*
st_password_entry_new (void)
{
  return ST_ENTRY (g_object_new (ST_TYPE_PASSWORD_ENTRY, NULL));
}

/**
 * st_password_entry_set_show_peek_icon:
 * @entry: a #StPasswordEntry
 * @value: %TRUE to show the peek-icon in the entry
 *
 * Sets whether to show or hide the peek-icon in the password entry. If %TRUE,
 * a icon button for temporarily unmasking the password will be shown at the
 * end of the entry.
 */
void
st_password_entry_set_show_peek_icon (StPasswordEntry *entry,
                                      gboolean         value)
{
  StPasswordEntryPrivate *priv;

  g_return_if_fail (ST_IS_PASSWORD_ENTRY (entry));

  priv = ST_PASSWORD_ENTRY_PRIV (entry);
  if (priv->show_peek_icon == value)
    return;

  priv->show_peek_icon = value;

  update_peek_icon (entry);

  if (st_password_entry_get_show_peek_icon (entry) != value)
    g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_SHOW_PEEK_ICON]);
}

/**
 * st_password_entry_get_show_peek_icon:
 * @entry: a #StPasswordEntry
 *
 * Gets whether peek-icon is shown or hidden in the password entry.
 *
 * Returns: %TRUE if visible
 */
gboolean
st_password_entry_get_show_peek_icon (StPasswordEntry *entry)
{
  StPasswordEntryPrivate *priv;

  g_return_val_if_fail (ST_IS_PASSWORD_ENTRY (entry), TRUE);

  priv = ST_PASSWORD_ENTRY_PRIV (entry);
  return priv->show_peek_icon && !show_password_locked_down (entry);
}

/**
 * st_password_entry_set_password_visible:
 * @entry: a #StPasswordEntry
 * @value: %TRUE to show the password in the entry, #FALSE otherwise
 *
 * Sets whether to show or hide text in the password entry.
 */
void
st_password_entry_set_password_visible (StPasswordEntry *entry,
                                        gboolean         value)
{
  StPasswordEntryPrivate *priv;
  ClutterActor *clutter_text;

  g_return_if_fail (ST_IS_PASSWORD_ENTRY (entry));

  priv = ST_PASSWORD_ENTRY_PRIV (entry);
  if (priv->password_visible == value)
    return;

  priv->password_visible = value;

  clutter_text = st_entry_get_clutter_text (ST_ENTRY (entry));
  if (priv->password_visible)
    {
      clutter_text_set_password_char (CLUTTER_TEXT (clutter_text), 0);
      st_icon_set_icon_name (ST_ICON (priv->peek_password_icon), "view-conceal-symbolic");
    }
  else
    {
      clutter_text_set_password_char (CLUTTER_TEXT (clutter_text), BLACK_CIRCLE);
      st_icon_set_icon_name (ST_ICON (priv->peek_password_icon), "view-reveal-symbolic");
    }

  g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_PASSWORD_VISIBLE]);
}

/**
 * st_password_entry_get_password_visible:
 * @entry: a #StPasswordEntry
 *
 * Gets whether the text is masked in the password entry.
 *
 * Returns: %TRUE if visible
 */
gboolean
st_password_entry_get_password_visible (StPasswordEntry *entry)
{
  StPasswordEntryPrivate *priv;

  g_return_val_if_fail (ST_IS_PASSWORD_ENTRY (entry), FALSE);

  priv = ST_PASSWORD_ENTRY_PRIV (entry);
  return priv->password_visible;
}
