/* msm server object */

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

#ifndef MSM_SERVER_H
#define MSM_SERVER_H

#include <glib.h>
#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>

typedef struct _MsmClient MsmClient;
typedef struct _MsmServer MsmServer;

typedef void (* MsmClientFunc) (MsmClient* client);

MsmServer* msm_server_new          (const char *session_name);
MsmServer* msm_server_new_failsafe (void);
void       msm_server_free         (MsmServer  *server);

void       msm_server_queue_interaction (MsmServer *server,
                                         MsmClient *client);

void       msm_server_save_all          (MsmServer *server,
                                         gboolean   allow_interaction,
                                         gboolean   shut_down);
void       msm_server_cancel_shutdown   (MsmServer *server);

void       msm_server_consider_phase_change (MsmServer *server);

void       msm_server_foreach_client    (MsmServer *server,
                                         MsmClientFunc func);

void       msm_server_drop_client       (MsmServer *server,
                                         MsmClient *client);

void       msm_server_next_pending_interaction (MsmServer *server);

gboolean   msm_server_client_id_in_use         (MsmServer  *server,
                                                const char *id);

void       msm_server_launch_session (MsmServer *server);

gboolean   msm_server_in_shutdown    (MsmServer *server);

#endif
