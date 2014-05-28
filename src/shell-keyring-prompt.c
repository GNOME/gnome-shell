/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2012 Red Hat, Inc.
 *           2012 Stef Walter <stefw@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author: Stef Walter <stefw@gnome.org>
 */

#include "config.h"

#include "shell-keyring-prompt.h"
#include "shell-secure-text-buffer.h"

#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr-base.h>

#include <glib/gi18n.h>

#include <string.h>

typedef struct _ShellPasswordPromptClass    ShellPasswordPromptClass;
typedef struct _ShellPasswordPromptPrivate  ShellPasswordPromptPrivate;

typedef enum
{
  PROMPTING_NONE,
  PROMPTING_FOR_CONFIRM,
  PROMPTING_FOR_PASSWORD
} PromptingMode;

struct _ShellKeyringPrompt
{
  GObject parent;

  gchar *title;
  gchar *message;
  gchar *description;
  gchar *warning;
  gchar *choice_label;
  gboolean choice_chosen;
  gboolean password_new;
  guint password_strength;
  gchar *continue_label;
  gchar *cancel_label;

  GcrPromptReply last_reply;
  GSimpleAsyncResult *async_result;
  ClutterText *password_actor;
  ClutterText *confirm_actor;
  PromptingMode mode;
  gboolean shown;
};

typedef struct _ShellKeyringPromptClass
{
  GObjectClass parent_class;
} ShellKeyringPromptClass;

enum {
  PROP_0,
  PROP_TITLE,
  PROP_MESSAGE,
  PROP_DESCRIPTION,
  PROP_WARNING,
  PROP_CHOICE_LABEL,
  PROP_CHOICE_CHOSEN,
  PROP_PASSWORD_NEW,
  PROP_PASSWORD_STRENGTH,
  PROP_CALLER_WINDOW,
  PROP_CONTINUE_LABEL,
  PROP_CANCEL_LABEL,
  PROP_PASSWORD_VISIBLE,
  PROP_CONFIRM_VISIBLE,
  PROP_WARNING_VISIBLE,
  PROP_CHOICE_VISIBLE,
  PROP_PASSWORD_ACTOR,
  PROP_CONFIRM_ACTOR
};

static void    shell_keyring_prompt_iface     (GcrPromptIface *iface);

G_DEFINE_TYPE_WITH_CODE (ShellKeyringPrompt, shell_keyring_prompt, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_PROMPT, shell_keyring_prompt_iface);
);

enum {
  SIGNAL_SHOW_PASSWORD,
  SIGNAL_SHOW_CONFIRM,
  SIGNAL_LAST
};

static gint signals[SIGNAL_LAST];

static void
shell_keyring_prompt_init (ShellKeyringPrompt *self)
{

}

static void
shell_keyring_prompt_set_property (GObject      *obj,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ShellKeyringPrompt *self = SHELL_KEYRING_PROMPT (obj);

  switch (prop_id) {
  case PROP_TITLE:
    g_free (self->title);
    self->title = g_value_dup_string (value);
    g_object_notify (obj, "title");
    break;
  case PROP_MESSAGE:
    g_free (self->message);
    self->message = g_value_dup_string (value);
    g_object_notify (obj, "message");
    break;
  case PROP_DESCRIPTION:
    g_free (self->description);
    self->description = g_value_dup_string (value);
    g_object_notify (obj, "description");
    break;
  case PROP_WARNING:
    g_free (self->warning);
    self->warning = g_value_dup_string (value);
    if (!self->warning)
        self->warning = g_strdup ("");
    g_object_notify (obj, "warning");
    g_object_notify (obj, "warning-visible");
    break;
  case PROP_CHOICE_LABEL:
    g_free (self->choice_label);
    self->choice_label = g_value_dup_string (value);
    if (!self->choice_label)
        self->choice_label = g_strdup ("");
    g_object_notify (obj, "choice-label");
    g_object_notify (obj, "choice-visible");
    break;
  case PROP_CHOICE_CHOSEN:
    self->choice_chosen = g_value_get_boolean (value);
    g_object_notify (obj, "choice-chosen");
    break;
  case PROP_PASSWORD_NEW:
    self->password_new = g_value_get_boolean (value);
    g_object_notify (obj, "password-new");
    g_object_notify (obj, "confirm-visible");
    break;
  case PROP_CALLER_WINDOW:
    /* ignored */
    break;
  case PROP_CONTINUE_LABEL:
    g_free (self->continue_label);
    self->continue_label = g_value_dup_string (value);
    g_object_notify (obj, "continue-label");
    break;
  case PROP_CANCEL_LABEL:
    g_free (self->cancel_label);
    self->cancel_label = g_value_dup_string (value);
    g_object_notify (obj, "cancel-label");
    break;
  case PROP_PASSWORD_ACTOR:
    shell_keyring_prompt_set_password_actor (self, g_value_get_object (value));
    break;
  case PROP_CONFIRM_ACTOR:
    shell_keyring_prompt_set_confirm_actor (self, g_value_get_object (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
shell_keyring_prompt_get_property (GObject    *obj,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ShellKeyringPrompt *self = SHELL_KEYRING_PROMPT (obj);

  switch (prop_id) {
  case PROP_TITLE:
    g_value_set_string (value, self->title ? self->title : "");
    break;
  case PROP_MESSAGE:
    g_value_set_string (value, self->message ? self->message : "");
    break;
  case PROP_DESCRIPTION:
    g_value_set_string (value, self->description ? self->description : "");
    break;
  case PROP_WARNING:
    g_value_set_string (value, self->warning ? self->warning : "");
    break;
  case PROP_CHOICE_LABEL:
    g_value_set_string (value, self->choice_label ? self->choice_label : "");
    break;
  case PROP_CHOICE_CHOSEN:
    g_value_set_boolean (value, self->choice_chosen);
    break;
  case PROP_PASSWORD_NEW:
    g_value_set_boolean (value, self->password_new);
    break;
  case PROP_PASSWORD_STRENGTH:
    g_value_set_int (value, self->password_strength);
    break;
  case PROP_CALLER_WINDOW:
    g_value_set_string (value, "");
    break;
  case PROP_CONTINUE_LABEL:
    g_value_set_string (value, self->continue_label);
    break;
  case PROP_CANCEL_LABEL:
    g_value_set_string (value, self->cancel_label);
    break;
  case PROP_PASSWORD_VISIBLE:
    g_value_set_boolean (value, self->mode == PROMPTING_FOR_PASSWORD);
    break;
  case PROP_CONFIRM_VISIBLE:
    g_value_set_boolean (value, self->password_new &&
                                self->mode == PROMPTING_FOR_PASSWORD);
    break;
  case PROP_WARNING_VISIBLE:
    g_value_set_boolean (value, self->warning && self->warning[0]);
    break;
  case PROP_CHOICE_VISIBLE:
    g_value_set_boolean (value, self->choice_label && self->choice_label[0]);
    break;
  case PROP_PASSWORD_ACTOR:
    g_value_set_object (value, shell_keyring_prompt_get_password_actor (self));
    break;
  case PROP_CONFIRM_ACTOR:
    g_value_set_object (value, shell_keyring_prompt_get_confirm_actor (self));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
shell_keyring_prompt_dispose (GObject *obj)
{
  ShellKeyringPrompt *self = SHELL_KEYRING_PROMPT (obj);

  if (self->shown)
    gcr_prompt_close (GCR_PROMPT (self));

  if (self->async_result)
    shell_keyring_prompt_cancel (self);
  g_assert (self->async_result == NULL);

  shell_keyring_prompt_set_password_actor (self, NULL);
  shell_keyring_prompt_set_confirm_actor (self, NULL);

  G_OBJECT_CLASS (shell_keyring_prompt_parent_class)->dispose (obj);
}

static void
shell_keyring_prompt_finalize (GObject *obj)
{
  ShellKeyringPrompt *self = SHELL_KEYRING_PROMPT (obj);

  g_free (self->title);
  g_free (self->message);
  g_free (self->description);
  g_free (self->warning);
  g_free (self->choice_label);
  g_free (self->continue_label);
  g_free (self->cancel_label);

  G_OBJECT_CLASS (shell_keyring_prompt_parent_class)->finalize (obj);
}

static void
shell_keyring_prompt_class_init (ShellKeyringPromptClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shell_keyring_prompt_get_property;
  gobject_class->set_property = shell_keyring_prompt_set_property;
  gobject_class->dispose = shell_keyring_prompt_dispose;
  gobject_class->finalize = shell_keyring_prompt_finalize;

  g_object_class_override_property (gobject_class, PROP_TITLE, "title");

  g_object_class_override_property (gobject_class, PROP_MESSAGE, "message");

  g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");

  g_object_class_override_property (gobject_class, PROP_WARNING, "warning");

  g_object_class_override_property (gobject_class, PROP_PASSWORD_NEW, "password-new");

  g_object_class_override_property (gobject_class, PROP_PASSWORD_STRENGTH, "password-strength");

  g_object_class_override_property (gobject_class, PROP_CHOICE_LABEL, "choice-label");

  g_object_class_override_property (gobject_class, PROP_CHOICE_CHOSEN, "choice-chosen");

  g_object_class_override_property (gobject_class, PROP_CALLER_WINDOW, "caller-window");

  g_object_class_override_property (gobject_class, PROP_CONTINUE_LABEL, "continue-label");

  g_object_class_override_property (gobject_class, PROP_CANCEL_LABEL, "cancel-label");

  /**
   * ShellKeyringPrompt:password-visible:
   *
   * Whether the password entry is visible or not.
   */
  g_object_class_install_property (gobject_class, PROP_PASSWORD_VISIBLE,
             g_param_spec_boolean ("password-visible", "Password visible", "Password field is visible",
                                   FALSE, G_PARAM_READABLE));

  /**
    * ShellKeyringPrompt:confirm-visible:
    *
    * Whether the password confirm entry is visible or not.
    */
  g_object_class_install_property (gobject_class, PROP_CONFIRM_VISIBLE,
             g_param_spec_boolean ("confirm-visible", "Confirm visible", "Confirm field is visible",
                                   FALSE, G_PARAM_READABLE));

  /**
   * ShellKeyringPrompt:warning-visible:
   *
   * Whether the warning label is visible or not.
   */
  g_object_class_install_property (gobject_class, PROP_WARNING_VISIBLE,
             g_param_spec_boolean ("warning-visible", "Warning visible", "Warning is visible",
                                   FALSE, G_PARAM_READABLE));

  /**
   * ShellKeyringPrompt:choice-visible:
   *
   * Whether the choice check box is visible or not.
   */
  g_object_class_install_property (gobject_class, PROP_CHOICE_VISIBLE,
             g_param_spec_boolean ("choice-visible", "Choice visible", "Choice is visible",
                                   FALSE, G_PARAM_READABLE));

  /**
   * ShellKeyringPrompt:password-actor:
   *
   * Text field for password
   */
  g_object_class_install_property (gobject_class, PROP_PASSWORD_ACTOR,
              g_param_spec_object ("password-actor", "Password actor", "Text field for password",
                                   CLUTTER_TYPE_TEXT, G_PARAM_READWRITE));

  /**
   * ShellKeyringPrompt:confirm-actor:
   *
   * Text field for confirmation password
   */
  g_object_class_install_property (gobject_class, PROP_CONFIRM_ACTOR,
              g_param_spec_object ("confirm-actor", "Confirm actor", "Text field for confirming password",
                                   CLUTTER_TYPE_TEXT, G_PARAM_READWRITE));

  signals[SIGNAL_SHOW_PASSWORD] = g_signal_new ("show-password", G_TYPE_FROM_CLASS (klass),
                                                0, 0, NULL, NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE, 0);

  signals[SIGNAL_SHOW_CONFIRM] =  g_signal_new ("show-confirm", G_TYPE_FROM_CLASS (klass),
                                                0, 0, NULL, NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE, 0);
}

static void
shell_keyring_prompt_password_async (GcrPrompt          *prompt,
                                     GCancellable       *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer            user_data)
{
  ShellKeyringPrompt *self = SHELL_KEYRING_PROMPT (prompt);
  GObject *obj;

  if (self->async_result != NULL) {
      g_warning ("this prompt can only show one prompt at a time");
      return;
  }

  self->mode = PROMPTING_FOR_PASSWORD;
  self->async_result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
                                                  shell_keyring_prompt_password_async);

  obj = G_OBJECT (self);
  g_object_notify (obj, "password-visible");
  g_object_notify (obj, "confirm-visible");
  g_object_notify (obj, "warning-visible");
  g_object_notify (obj, "choice-visible");

  self->shown = TRUE;
  g_signal_emit (self, signals[SIGNAL_SHOW_PASSWORD], 0);
}

static const gchar *
shell_keyring_prompt_password_finish (GcrPrompt    *prompt,
                                      GAsyncResult *result,
                                      GError      **error)
{
  ShellKeyringPrompt *self = SHELL_KEYRING_PROMPT (prompt);

  g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (prompt),
                        shell_keyring_prompt_password_async), NULL);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
    return NULL;

  if (self->last_reply == GCR_PROMPT_REPLY_CONTINUE)
    return clutter_text_get_text (self->password_actor);

  return NULL;
}

static void
shell_keyring_prompt_confirm_async (GcrPrompt          *prompt,
                                    GCancellable       *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer            user_data)
{
  ShellKeyringPrompt *self = SHELL_KEYRING_PROMPT (prompt);
  GObject *obj;

  if (self->async_result != NULL) {
      g_warning ("this prompt is already prompting");
      return;
  }

  self->mode = PROMPTING_FOR_CONFIRM;
  self->async_result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
                                                  shell_keyring_prompt_confirm_async);

  obj = G_OBJECT (self);
  g_object_notify (obj, "password-visible");
  g_object_notify (obj, "confirm-visible");
  g_object_notify (obj, "warning-visible");
  g_object_notify (obj, "choice-visible");

  self->shown = TRUE;
  g_signal_emit (self, signals[SIGNAL_SHOW_CONFIRM], 0);
}

static GcrPromptReply
shell_keyring_prompt_confirm_finish (GcrPrompt    *prompt,
                                     GAsyncResult *result,
                                     GError      **error)
{
  ShellKeyringPrompt *self = SHELL_KEYRING_PROMPT (prompt);

  g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (prompt),
                        shell_keyring_prompt_confirm_async), GCR_PROMPT_REPLY_CANCEL);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
    return GCR_PROMPT_REPLY_CANCEL;

  return self->last_reply;
}

static void
shell_keyring_prompt_close (GcrPrompt *prompt)
{
  ShellKeyringPrompt *self = SHELL_KEYRING_PROMPT (prompt);

  /*
   * We expect keyring.js to connect to this signal and do the
   * actual work of closing the prompt.
   */

  self->shown = FALSE;
}

static void
shell_keyring_prompt_iface (GcrPromptIface *iface)
{
  iface->prompt_password_async = shell_keyring_prompt_password_async;
  iface->prompt_password_finish = shell_keyring_prompt_password_finish;
  iface->prompt_confirm_async = shell_keyring_prompt_confirm_async;
  iface->prompt_confirm_finish = shell_keyring_prompt_confirm_finish;
  iface->prompt_close = shell_keyring_prompt_close;
}

/**
 * shell_keyring_prompt_new:
 *
 * Create new internal prompt base
 *
 * Returns: (transfer full): new internal prompt
 */
ShellKeyringPrompt *
shell_keyring_prompt_new (void)
{
	return g_object_new (SHELL_TYPE_KEYRING_PROMPT, NULL);
}

/**
 * shell_keyring_prompt_get_password_actor:
 * @self: the internal prompt
 *
 * Get the prompt password text actor
 *
 * Returns: (transfer none) (nullable): the password actor
 */
ClutterText *
shell_keyring_prompt_get_password_actor (ShellKeyringPrompt *self)
{
  g_return_val_if_fail (SHELL_IS_KEYRING_PROMPT (self), NULL);
  return self->password_actor;
}

/**
 * shell_keyring_prompt_get_confirm_actor:
 * @self: the internal prompt
 *
 * Get the prompt password text actor
 *
 * Returns: (transfer none) (nullable): the password actor
 */
ClutterText *
shell_keyring_prompt_get_confirm_actor (ShellKeyringPrompt *self)
{
  g_return_val_if_fail (SHELL_IS_KEYRING_PROMPT (self), NULL);
  return self->confirm_actor;
}

static guint
calculate_password_strength (const gchar *password)
{
  int upper, lower, digit, misc;
  gdouble pwstrength;
  int length, i;

  /*
   * This code is based on the Master Password dialog in Firefox
   * (pref-masterpass.js)
   * Original code triple-licensed under the MPL, GPL, and LGPL
   * so is license-compatible with this file
   */

  length = strlen (password);

  /* Always return 0 for empty passwords */
  if (length == 0)
    return 0;

  upper = 0;
  lower = 0;
  digit = 0;
  misc = 0;

  for (i = 0; i < length ; i++)
    {
      if (g_ascii_isdigit (password[i]))
        digit++;
      else if (g_ascii_islower (password[i]))
        lower++;
      else if (g_ascii_isupper (password[i]))
        upper++;
      else
        misc++;
    }

  if (length > 5)
    length = 5;
  if (digit > 3)
    digit = 3;
  if (upper > 3)
    upper = 3;
  if (misc > 3)
    misc = 3;

  pwstrength = ((length * 1) - 2) +
      (digit * 1) +
      (misc * 1.5) +
      (upper * 1);

  /* Always return 1+ for non-empty passwords */
  if (pwstrength < 1.0)
    pwstrength = 1.0;
  if (pwstrength > 10.0)
    pwstrength = 10.0;

  return (guint)pwstrength;
}

static void
on_password_changed (ClutterText *text,
                     gpointer user_data)
{
  ShellKeyringPrompt *self = SHELL_KEYRING_PROMPT (user_data);
  const gchar *password;

  password = clutter_text_get_text (self->password_actor);

  self->password_strength = calculate_password_strength (password);
  g_object_notify (G_OBJECT (self), "password-strength");
}

/**
 * shell_keyring_prompt_set_password_actor:
 * @self: the internal prompt
 * @password_actor: (nullable): the password actor
 *
 * Set the prompt password text actor
 */
void
shell_keyring_prompt_set_password_actor (ShellKeyringPrompt *self,
                                         ClutterText *password_actor)
{
  ClutterTextBuffer *buffer;

  g_return_if_fail (SHELL_IS_KEYRING_PROMPT (self));
  g_return_if_fail (password_actor == NULL || CLUTTER_IS_TEXT (password_actor));

  if (password_actor)
    {
      buffer = shell_secure_text_buffer_new ();
      clutter_text_set_buffer (password_actor, buffer);
      g_object_unref (buffer);

      g_signal_connect (password_actor, "text-changed", G_CALLBACK (on_password_changed), self);
      g_object_ref (password_actor);
    }
  if (self->password_actor)
    {
      g_signal_handlers_disconnect_by_func (self->password_actor, on_password_changed, self);
      g_object_unref (self->password_actor);
    }

  self->password_actor = password_actor;
  g_object_notify (G_OBJECT (self), "password-actor");
}

/**
 * shell_keyring_prompt_set_confirm_actor:
 * @self: the internal prompt
 * @confirm_actor: (nullable): the confirm password actor
 *
 * Set the prompt password confirmation text actor
 */
void
shell_keyring_prompt_set_confirm_actor (ShellKeyringPrompt *self,
                                        ClutterText *confirm_actor)
{
  ClutterTextBuffer *buffer;

  g_return_if_fail (SHELL_IS_KEYRING_PROMPT (self));
  g_return_if_fail (confirm_actor == NULL || CLUTTER_IS_TEXT (confirm_actor));

  if (confirm_actor)
    {
      buffer = shell_secure_text_buffer_new ();
      clutter_text_set_buffer (confirm_actor, buffer);
      g_object_unref (buffer);

      g_object_ref (confirm_actor);
    }
  if (self->confirm_actor)
    g_object_unref (self->confirm_actor);
  self->confirm_actor = confirm_actor;
  g_object_notify (G_OBJECT (self), "confirm-actor");
}

/**
 * shell_keyring_prompt_complete:
 * @self: the internal prompt
 *
 * Called by the implementation when the prompt completes. There are various
 * checks done. %TRUE is returned if the prompt actually should complete.
 *
 * Returns: whether the prompt completed
 */
gboolean
shell_keyring_prompt_complete (ShellKeyringPrompt *self)
{
  GSimpleAsyncResult *res;
  const gchar *password;
  const gchar *confirm;
  const gchar *env;

  g_return_val_if_fail (SHELL_IS_KEYRING_PROMPT (self), FALSE);
  g_return_val_if_fail (self->mode != PROMPTING_NONE, FALSE);
  g_return_val_if_fail (self->async_result != NULL, FALSE);

  if (self->mode == PROMPTING_FOR_PASSWORD)
    {
      password = clutter_text_get_text (self->password_actor);

      /* Is it a new password? */
      if (self->password_new)
        {
          confirm = clutter_text_get_text (self->confirm_actor);

          /* Do the passwords match? */
          if (!g_str_equal (password, confirm))
            {
              gcr_prompt_set_warning (GCR_PROMPT (self), _("Passwords do not match."));
              return FALSE;
          }

          /* Don't allow blank passwords if in paranoid mode */
          env = g_getenv ("GNOME_KEYRING_PARANOID");
          if (env && *env)
            {
              gcr_prompt_set_warning (GCR_PROMPT (self), _("Password cannot be blank"));
              return FALSE;
            }
        }

      self->password_strength = calculate_password_strength (password);
      g_object_notify (G_OBJECT (self), "password-strength");
    }

  self->last_reply = GCR_PROMPT_REPLY_CONTINUE;

  res = self->async_result;
  self->async_result = NULL;
  self->mode = PROMPTING_NONE;

  g_simple_async_result_complete (res);
  g_object_unref (res);

  return TRUE;
}

/**
 * shell_keyring_prompt_cancel:
 * @self: the internal prompt
 *
 * Called by implementation when the prompt is cancelled.
 */
void
shell_keyring_prompt_cancel (ShellKeyringPrompt *self)
{
  GSimpleAsyncResult *res;

  g_return_if_fail (SHELL_IS_KEYRING_PROMPT (self));

  /*
   * If cancelled while not prompting, we should just close the prompt,
   * the user wants it to go away.
   */
  if (self->mode == PROMPTING_NONE)
    {
      if (self->shown)
        gcr_prompt_close (GCR_PROMPT (self));
      return;
    }

  g_return_if_fail (self->async_result != NULL);
  self->last_reply = GCR_PROMPT_REPLY_CANCEL;

  res = self->async_result;
  self->async_result = NULL;
  self->mode = PROMPTING_NONE;

  g_simple_async_result_complete_in_idle (res);
  g_object_unref (res);
}
