/* msm client object */

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

#include "client.h"
#include "props.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

struct _MsmClient
{
  MsmServer *server;
  SmsConn cnxn;
  MsmClientState state;
  char *id;
  char *hostname;
  char *desc;
  int restart_style;
  GList *properties;
};

#define DEFAULT_RESTART_STYLE SmRestartIfRunning

MsmClient*
msm_client_new (MsmServer *server,
                SmsConn    cnxn)
{
  MsmClient *client;

  client = g_new (MsmClient, 1);

  client->server = server;
  client->cnxn = cnxn;
  client->state = MSM_CLIENT_STATE_NEW;
  client->id = NULL;
  client->hostname = NULL;
  client->desc = g_strdup ("unknown");
  client->restart_style = DEFAULT_RESTART_STYLE;
  client->properties = NULL;
  
  return client;
}

void
msm_client_free (MsmClient *client)
{
  IceConn ice_cnxn;
  GList *tmp;
  
  ice_cnxn = SmsGetIceConnection (client->cnxn);
  SmsCleanUp (client->cnxn);
  IceSetShutdownNegotiation (ice_cnxn, False);
  IceCloseConnection (ice_cnxn);

  tmp = client->properties;
  while (tmp != NULL)
    {
      SmProp *prop = tmp->data;

      SmFreeProperty (prop);
      
      tmp = tmp->next;
    }

  g_list_free (client->properties);
  
  g_free (client->id);
  g_free (client->hostname);
  g_free (client->desc);
    
  g_free (client);
}

SmsConn
msm_client_get_connection (MsmClient *client)
{
  return client->cnxn;
}

const char*
msm_client_get_description (MsmClient *client)
{
  return client->desc;
}

MsmClientState
msm_client_get_state (MsmClient *client)
{
  return client->state;
}

MsmServer*
msm_client_get_server (MsmClient *client)
{
  return client->server;
}

const char*
msm_client_get_id (MsmClient *client)
{
  return client->id;
}

int
msm_client_get_restart_style (MsmClient *client)
{
  return client->restart_style;
}

void
msm_client_register (MsmClient  *client,
                     const char *id)
{
  char *p;

  if (client->state != MSM_CLIENT_STATE_NEW)
    {
      msm_warning (_("Client '%s' attempted to register when it was already registered\n"), client->desc);

      return;
    }
  
  client->state = MSM_CLIENT_STATE_IDLE;
  client->id = g_strdup (id);

  SmsRegisterClientReply (client->cnxn, client->id);

  p = SmsClientHostName (client->cnxn);
  client->hostname = g_strdup (p);
  free (p);
}

void
msm_client_interact_request (MsmClient *client)
{
  if (client->state != MSM_CLIENT_STATE_SAVING &&
      client->state != MSM_CLIENT_STATE_SAVING_PHASE2)
    {
      msm_warning (_("Client '%s' requested interaction when it was not being saved\n"),
                   client->desc);

      return;
    }
  
  msm_server_queue_interaction (client->server, client);
}

void
msm_client_begin_interact (MsmClient *client)
{  
  SmsInteract (client->cnxn);
}

static void
internal_save (MsmClient *client,
               int        save_style,
               gboolean   allow_interaction,
               gboolean   shut_down)
{
  if (client->state != MSM_CLIENT_STATE_IDLE)
    {
      msm_warning (_("Tried to save client '%s' but it was not in the idle state\n"),
                   client->desc);

      return;
    }

  client->state = MSM_CLIENT_STATE_SAVING;
  
  SmsSaveYourself (client->cnxn,
                   save_style,
                   shut_down,
                   allow_interaction ? SmInteractStyleAny : SmInteractStyleNone,
                   FALSE /* not "fast" */);
}

void
msm_client_save (MsmClient *client,
                 gboolean   allow_interaction,
                 gboolean   shut_down)
{  
  internal_save (client, SmSaveBoth, /* ? don't know what to do here */
                 allow_interaction, shut_down);
}

void
msm_client_initial_save (MsmClient  *client)
{
  /* This is the save on client registration in the spec under
   * RegisterClientReply
   */
  internal_save (client, SmSaveLocal, FALSE, FALSE);
}

void
msm_client_shutdown_cancelled (MsmClient *client)
{
  if (client->state != MSM_CLIENT_STATE_SAVING &&
      client->state != MSM_CLIENT_STATE_SAVING_PHASE2)
    {
      msm_warning (_("Tried to send cancel shutdown to client '%s' which was not saving\n"),
                   client->desc);
      return;
    }

  client->state = MSM_CLIENT_STATE_IDLE;
  SmsShutdownCancelled (client->cnxn);
}

void
msm_client_phase2_request (MsmClient *client)
{
  if (client->state != MSM_CLIENT_STATE_SAVING)
    {
      msm_warning (_("Client '%s' requested phase 2 save but was not in a phase 1 save\n"),
                   client->desc);
      return;
    }
  
  client->state = MSM_CLIENT_STATE_PHASE2_REQUESTED;
}

void
msm_client_save_phase2 (MsmClient *client)
{
  if (client->state != MSM_CLIENT_STATE_PHASE2_REQUESTED)
    {
      msm_warning (_("We tried to save client '%s' in phase 2, but it hadn't requested it.\n"), client->desc);
      return;
    }

  SmsSaveYourselfPhase2 (client->cnxn);
}

void
msm_client_die (MsmClient  *client)
{
  client->state = MSM_CLIENT_STATE_DEAD;
  SmsDie (client->cnxn);
}

void
msm_client_save_complete (MsmClient *client)
{
  client->state = MSM_CLIENT_STATE_IDLE;
  SmsSaveComplete (client->cnxn);
}

void
msm_client_save_confirmed (MsmClient  *client,
                           gboolean    successful)
{
  if (client->state != MSM_CLIENT_STATE_SAVING &&
      client->state != MSM_CLIENT_STATE_SAVING_PHASE2)
    {
      msm_warning (_("Client '%s' said it was done saving, but it hadn't been told to save\n"),
                   client->desc);
      return;
    }

  if (successful)
    client->state = MSM_CLIENT_STATE_SAVE_DONE;
  else
    client->state = MSM_CLIENT_STATE_SAVE_FAILED;
}

void
msm_client_set_property_taking_ownership (MsmClient *client,
                                          SmProp    *prop)
{
  /* we own prop which should be freed with SmFreeProperty() */

  /* pass our ownership into the proplist */
  client->properties = proplist_replace (client->properties, prop);

  /* update pieces of the client struct */
  if (strcmp (prop->name, "SmRestartStyleHint") == 0)
    {
      int hint;
      if (smprop_get_card8 (prop, &hint))
        client->restart_style = hint;
      else
        client->restart_style = DEFAULT_RESTART_STYLE;
    }
}

void
msm_client_unset_property (MsmClient *client,
                           const char *name)
{
  client->properties = proplist_delete (client->properties, name);

  /* Return to default values */
  if (strcmp (name, "SmRestartStyleHint") == 0)
    {
      client->restart_style = DEFAULT_RESTART_STYLE;
    }
}

void
msm_client_send_properties (MsmClient *client)
{
  int n_props;
  SmProp **props;
  GList *tmp;
  int i;
  
  n_props = g_list_length (client->properties);
  props = g_new (SmProp*, n_props);

  i = 0;
  tmp = client->properties;
  while (tmp != NULL)
    {
      props[i] = tmp->data;

      tmp = tmp->next;
      ++i;
    }
  
  SmsReturnProperties (client->cnxn, n_props, props);

  g_free (props);
}
