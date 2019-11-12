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

#define BLACK_CIRCLE 9679

#define ST_PASSWORD_ENTRY_PRIV(x) st_password_entry_get_instance_private ((StPasswordEntry *) x)

struct _StPasswordEntryPrivate
{
  gboolean      password_visible;
};

enum
{
  PROP_0,

  PROP_PASSWORD_VISIBLE,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };


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
    case PROP_PASSWORD_VISIBLE:
      g_value_set_boolean (value, priv->password_visible);
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}
static void
st_password_entry_class_init (StPasswordEntryClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = st_password_entry_get_property;
  gobject_class->set_property = st_password_entry_set_property;

  props[PROP_PASSWORD_VISIBLE] = g_param_spec_boolean ("password-visible",
                                                       "Password visible",
                                                       "Whether to text in the entry is masked or not",
                                                       FALSE,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (gobject_class, N_PROPS, props);

}

static void
st_password_entry_init (StPasswordEntry *entry)
{
  ClutterActor *clutter_text;

  st_entry_set_text (ST_ENTRY (entry), "");

  clutter_text = st_entry_get_clutter_text (ST_ENTRY (entry));
  clutter_text_set_password_char (CLUTTER_TEXT (clutter_text), BLACK_CIRCLE);

  st_entry_set_input_purpose (ST_ENTRY (entry), CLUTTER_INPUT_CONTENT_PURPOSE_PASSWORD);
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
 * st_password_entry_set_password_visible:
 * @entry: a #StPasswordEntry
 * @value: #TRUE to show the password in the entry, #FALSE otherwise
 *
 * Sets whether to show or hide text in the password entry.
 */
void
st_password_entry_set_password_visible (StPasswordEntry *entry, gboolean value)
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
    clutter_text_set_password_char (CLUTTER_TEXT (clutter_text), 0);
  else
    clutter_text_set_password_char (CLUTTER_TEXT (clutter_text), BLACK_CIRCLE);

  g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_PASSWORD_VISIBLE]);
}

/**
 * st_password_entry_get_password_visible:
 * @entry: a #StPasswordEntry
 *
 * Gets whether the text is masked in the password entry.
 */
gboolean
st_password_entry_get_password_visible (StPasswordEntry *entry)
{
  StPasswordEntryPrivate *priv;

  g_return_val_if_fail (ST_IS_PASSWORD_ENTRY (entry), FALSE);

  priv = ST_PASSWORD_ENTRY_PRIV (entry);
  return priv->password_visible;
}
