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

static GList* find_property_link_by_name (MsmClient *client,
                                          const char *name);

static SmProp* find_property_by_name (MsmClient *client,
                                      const char *name);

static gboolean find_card8_property (MsmClient *client,
                                     const char *name,
                                     int *result)
     
static gboolean find_string_property (MsmClient *client,
                                      const char *name,
                                      char **result);

static gboolean find_vector_property (MsmClient *client,
                                      const char *name,
                                      int *argcp,
                                      char ***argvp);

static gboolean get_card8_value  (SmProp   *prop,
                                  int      *result);
static gboolean get_string_value (SmProp   *prop,
                                  char    **result);
static gboolean get_vector_value (SmProp   *prop,
                                  int      *argcp,
                                  char   ***argvp);

static SmProp* copy_property (SmProp *prop);

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

  p = SmsClientHostName (smsConn);
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
  g_return_if_fail (client->interact_requested);
  
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
  internal_save (client, SmSaveLocal, allow_interaction, shut_down);
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
  GList *list;

  if (prop->name == NULL)
    {
      SmFreeProperty (prop);
      return;
    }
  
  list = find_property_link_by_name (prop->name);
  if (list)
    {
      SmFreeProperty (list->data);
      list->data = prop;
    }
  else
    {
      client->properties = g_list_prepend (client->properties,
                                           prop);
    }

  /* update pieces of the client struct */
  if (strcmp (prop->name, "SmRestartStyleHint") == 0)
    {
      int hint;
      if (get_card8_value (prop, &hint))
        client->restart_style = hint;
      else
        client->restart_style = DEFAULT_RESTART_STYLE;
    }
}

void
msm_client_unset_property (MsmClient *client,
                           const char *name)
{
  GList *list;

  list = find_property_link_by_name (prop->name);
  if (list)
    {
      SmFreeProperty (list->data);
      client->properties = g_list_delete_link (client->properties,
                                               list);
    }

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
  props = g_new (SmProp, n_props);

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

/* Property functions stolen from gnome-session */

static GList*
find_property_link_by_name (MsmClient *client,
                            const char *name)
{
  GList *list;

  for (list = client->properties; list; list = list->next)
    {
      SmProp *prop = (SmProp *) list->data;
      if (strcmp (prop->name, name) == 0)
	return list;
    }

  return NULL;
}


SmProp*
find_property_by_name (MsmClient *client, const char *name)
{
  GList *list;

  list = find_property_link_by_name (client, name);

  return list ? list->data : NULL;
}

gboolean
find_card8_property (MsmClient *client, const char *name,
		     int *result)
{
  SmProp *prop;

  g_return_val_if_fail (result != NULL, FALSE);

  prop = find_property_by_name (client, name);
  if (prop == NULL)
    return FALSE;
  else
    return get_card8_value (prop, result);
}

gboolean
find_string_property (MsmClient *client, const char *name,
		      char **result)
{
  SmProp *prop;

  g_return_val_if_fail (result != NULL, FALSE);

  prop = find_property_by_name (client, name);
  if (prop == NULL)
    return FALSE;
  else
    return get_string_value (prop, result);
}

gboolean
find_vector_property (MsmClient *client, const char *name,
		      int *argcp, char ***argvp)
{
  SmProp *prop;

  g_return_val_if_fail (argcp != NULL, FALSE);
  g_return_val_if_fail (argvp != NULL, FALSE);

  prop = find_property_by_name (client, name);
  if (prop == NULL)
    return FALSE;
  else
    return get_vector_value (prop, argcp, argvp);
}

static gboolean
get_card8_value  (SmProp   *prop,
                  int      *result)
{
  g_return_val_if_fail (result != NULL, FALSE);

  if (strcmp (prop->type, SmCARD8) == 0)
    {
      char *p;
      p = prop->vals[0].value;
      *result = *p;
      return TRUE;
    }
  else
    return FALSE
}

static gboolean
get_string_value (SmProp   *prop,
                  char    **result)
{
  g_return_val_if_fail (result != NULL, FALSE);

  if (strcmp (prop->type, SmARRAY8) == 0)
    {
      *result = g_malloc (prop->vals[0].length + 1);
      memcpy (*result, prop->vals[0].value, prop->vals[0].length);
      (*result)[prop->vals[0].length] = '\0';
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
get_vector_value (SmProp   *prop,
                  int      *argcp,
                  char   ***argvp)
{
  g_return_val_if_fail (argcp != NULL, FALSE);
  g_return_val_if_fail (argvp != NULL, FALSE);

  if (strcmp (prop->type, SmLISTofARRAY8) == 0)
    {
      int i;
      
      *argcp = prop->num_vals;
      *argvp = g_new0 (char *, *argcp + 1);
      for (i = 0; i < *argcp; ++i)
        {
          (*argvp)[i] = g_malloc (prop->vals[i].length + 1);
          memcpy ((*argvp)[i], prop->vals[i].value, prop->vals[i].length);
          (*argvp)[i][prop->vals[i].length] = '\0';
        }

      return TRUE;
    }
  else
    return FALSE;
}

static SmProp*
copy_property (SmProp *prop)
{
  int i;
  SmProp *copy;

  /* This all uses malloc so we can use SmFreeProperty() */
  
  copy = msm_non_glib_malloc (sizeof (SmProp));

  if (prop->name)
    copy->name = msm_non_glib_strdup (prop->name);
  else
    copy->name = NULL;

  if (prop->type)
    copy->type = msm_non_glib_strdup (prop->type);
  else
    copy->type = NULL;

  copy->num_vals = prop->num_vals;
  copy->vals = NULL;

  if (copy->num_vals > 0 && prop->vals)
    {
      copy->vals = msm_non_glib_malloc (sizeof (SmPropValue) * copy->num_vals);
      
      for (i = 0; i < copy->num_vals; i++)
        {
          if (prop->vals[i].value)
            {
              copy->vals[i].length = prop->vals[i].length;
              copy->vals[i].value = msm_non_glib_malloc (copy->vals[i].length);
              memcpy (copy->vals[i].value, prop->vals[i].value,
                      copy->vals[i].length);
            }
          else
            {
              copy->vals[i].length = 0;
              copy->vals[i].value = NULL;
            }
        }
    }

  return copy;
}
