/*
 * Copyright (C) 2018 Red Hat
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include <canberra.h>

#include "shell-sound-player.h"

#define EVENT_SOUNDS_KEY "event-sounds"
#define THEME_NAME_KEY   "theme-name"

typedef struct _ShellPlayRequest ShellPlayRequest;

struct _ShellSoundPlayer
{
  GObject parent;
  GThreadPool *queue;
  GSettings *settings;
  ca_context *context;
  uint32_t id_pool;
};

struct _ShellPlayRequest
{
  ca_proplist *props;
  uint32_t id;
  gulong cancel_id;
  GCancellable *cancellable;
  ShellSoundPlayer *player;
};

const char * const cache_allow_list[] = {
  "bell-window-system",
  "desktop-switch-left",
  "desktop-switch-right",
  "desktop-switch-up",
  "desktop-switch-down",
  NULL
};

G_DEFINE_TYPE (ShellSoundPlayer, shell_sound_player, G_TYPE_OBJECT)

static ShellPlayRequest *
shell_play_request_new (ShellSoundPlayer *player,
                        ca_proplist      *props,
                        GCancellable     *cancellable)
{
  ShellPlayRequest *req;

  req = g_new0 (ShellPlayRequest, 1);
  req->props = props;
  req->player = player;
  g_set_object (&req->cancellable, cancellable);

  return req;
}

static void
shell_play_request_free (ShellPlayRequest *req)
{
  g_clear_object (&req->cancellable);
  ca_proplist_destroy (req->props);
  g_free (req);
}

static void
shell_sound_player_finalize (GObject *object)
{
  ShellSoundPlayer *player = SHELL_SOUND_PLAYER (object);

  g_object_unref (player->settings);
  g_thread_pool_free (player->queue, FALSE, TRUE);
  ca_context_destroy (player->context);

  G_OBJECT_CLASS (shell_sound_player_parent_class)->finalize (object);
}

static void
shell_sound_player_class_init (ShellSoundPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = shell_sound_player_finalize;
}

static void
cancelled_cb (GCancellable    *cancellable,
              ShellPlayRequest *req)
{
  ca_context_cancel (req->player->context, req->id);
}

static void
finish_cb (ca_context *context,
           uint32_t    id,
           int         error_code,
           gpointer    user_data)
{
  ShellPlayRequest *req = user_data;

  if (error_code != CA_ERROR_CANCELED)
    g_cancellable_disconnect (req->cancellable, req->cancel_id);
  else if (req->cancellable != NULL)
    g_clear_signal_handler (&req->cancel_id, req->cancellable);

  shell_play_request_free (req);
}

static void
play_sound (ShellPlayRequest *req,
            ShellSoundPlayer *player)
{
  req->id = player->id_pool++;

  if (ca_context_play_full (player->context, req->id, req->props,
                            finish_cb, req) != CA_SUCCESS)
    {
      shell_play_request_free (req);
      return;
    }

  if (req->cancellable)
    {
      gulong cancel_id =
        g_cancellable_connect (req->cancellable,
                               G_CALLBACK (cancelled_cb), req, NULL);
      if (cancel_id)
        req->cancel_id = cancel_id;
    }
}

static void
settings_changed_cb (GSettings       *settings,
                     const char      *key,
                     ShellSoundPlayer *player)
{
  if (strcmp (key, EVENT_SOUNDS_KEY) == 0)
    {
      gboolean enabled;

      enabled = g_settings_get_boolean (settings, EVENT_SOUNDS_KEY);
      ca_context_change_props (player->context, CA_PROP_CANBERRA_ENABLE,
                               enabled ? "1" : "0", NULL);
    }
  else if (strcmp (key, THEME_NAME_KEY) == 0)
    {
      char *theme_name;

      theme_name = g_settings_get_string (settings, THEME_NAME_KEY);
      ca_context_change_props (player->context, CA_PROP_CANBERRA_XDG_THEME_NAME,
                               theme_name, NULL);
      g_free (theme_name);
    }
}

static ca_context *
create_context (GSettings *settings)
{
  ca_context *context;
  ca_proplist *props;
  gboolean enabled;
  char *theme_name;

  if (ca_context_create (&context) != CA_SUCCESS)
    return NULL;

  if (ca_proplist_create (&props) != CA_SUCCESS)
    {
      ca_context_destroy (context);
      return NULL;
    }

  ca_proplist_sets (props, CA_PROP_APPLICATION_NAME, "Mutter");

  enabled = g_settings_get_boolean (settings, EVENT_SOUNDS_KEY);
  ca_proplist_sets (props, CA_PROP_CANBERRA_ENABLE, enabled ? "1" : "0");

  theme_name = g_settings_get_string (settings, THEME_NAME_KEY);
  ca_proplist_sets (props, CA_PROP_CANBERRA_XDG_THEME_NAME, theme_name);
  g_free (theme_name);

  ca_context_change_props_full (context, props);
  ca_proplist_destroy (props);

  return context;
}

static void
shell_sound_player_init (ShellSoundPlayer *player)
{
  player->queue = g_thread_pool_new ((GFunc) play_sound,
				     player, 1, FALSE, NULL);
  player->settings = g_settings_new ("org.gnome.desktop.sound");
  player->context = create_context (player->settings);

  g_signal_connect (player->settings, "changed",
                    G_CALLBACK (settings_changed_cb), player);
}

static void
build_ca_proplist (ca_proplist  *props,
                   const char   *event_property,
                   const char   *event_id,
                   const char   *event_description)
{
  ca_proplist_sets (props, event_property, event_id);
  ca_proplist_sets (props, CA_PROP_EVENT_DESCRIPTION, event_description);
}

/**
 * shell_sound_player_play_from_theme:
 * @player: a #ShellSoundPlayer
 * @name: sound theme name of the event
 * @description: description of the event
 * @cancellable: cancellable for the request
 *
 * Plays a sound from the sound theme.
 **/
void
shell_sound_player_play_from_theme (ShellSoundPlayer *player,
                                   const char      *name,
                                   const char      *description,
                                   GCancellable    *cancellable)
{
  ShellPlayRequest *req;
  ca_proplist *props;

  g_return_if_fail (SHELL_IS_SOUND_PLAYER (player));
  g_return_if_fail (name != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  ca_proplist_create (&props);
  build_ca_proplist (props, CA_PROP_EVENT_ID, name, description);

  if (g_strv_contains (cache_allow_list, name))
    ca_proplist_sets (props, CA_PROP_CANBERRA_CACHE_CONTROL, "permanent");
  else
    ca_proplist_sets (props, CA_PROP_CANBERRA_CACHE_CONTROL, "volatile");

  req = shell_play_request_new (player, props, cancellable);
  g_thread_pool_push (player->queue, req, NULL);
}

