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

#ifndef MSM_CLIENT_H
#define MSM_CLIENT_H

#include <glib.h>

#include "server.h"

/* See xsmp docs for a state description. This enum doesn't
 * correspond exactly, but close enough.
 */
typedef enum
{
  /* Client has just newly connected, not yet registered */
  MSM_CLIENT_STATE_NEW,
  /* Client has registered with us successfully, isn't doing
   * anything special
   */
  MSM_CLIENT_STATE_IDLE,
  /* Client is saving self in phase 1 */
  MSM_CLIENT_STATE_SAVING,
  /* Client has requested phase 2 save, but we aren't in phase 2 yet */
  MSM_CLIENT_STATE_PHASE2_REQUESTED,
  /* Client is in phase 2 save; all the same things are
   * allowed as with STATE_SAVING, except you can't request
   * a phase 2 save
   */
  MSM_CLIENT_STATE_SAVING_PHASE2,

  /* Client sent SaveYourselfDone with success = TRUE */
  MSM_CLIENT_STATE_SAVE_DONE,

  /* Client sent SaveYourselfDone with success = FALSE */
  MSM_CLIENT_STATE_SAVE_FAILED,

  /* Client was asked to die */
  MSM_CLIENT_STATE_DEAD

} MsmClientState;

MsmClient* msm_client_new  (MsmServer *server,
                            SmsConn    cnxn);
void       msm_client_free (MsmClient *client);

SmsConn        msm_client_get_connection  (MsmClient *client);
const char*    msm_client_get_description (MsmClient *client);
MsmClientState msm_client_get_state       (MsmClient *client);
MsmServer*     msm_client_get_server      (MsmClient *client);
/* can return NULL */
const char*    msm_client_get_id          (MsmClient *client);
int            msm_client_get_restart_style (MsmClient *client);

void    msm_client_set_property_taking_ownership (MsmClient   *client,
                                                  SmProp      *prop);
void    msm_client_unset_property (MsmClient   *client,
                                   const char  *name);
void    msm_client_send_properties (MsmClient  *client);

void msm_client_register           (MsmClient  *client,
                                    const char *id);
void msm_client_interact_request   (MsmClient  *client);
void msm_client_begin_interact     (MsmClient  *client);
void msm_client_save               (MsmClient  *client,
                                    gboolean    allow_interaction,
                                    gboolean    shut_down);
void msm_client_initial_save       (MsmClient  *client);
void msm_client_shutdown_cancelled (MsmClient  *client);
void msm_client_phase2_request     (MsmClient  *client);
void msm_client_save_phase2        (MsmClient  *client);
void msm_client_save_confirmed     (MsmClient  *client,
                                    gboolean    successful);

void msm_client_die                (MsmClient  *client);
void msm_client_save_complete      (MsmClient  *client);

#endif

