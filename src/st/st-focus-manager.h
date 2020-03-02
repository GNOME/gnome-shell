/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-focus-manager.h: Keyboard focus manager
 *
 * Copyright 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_FOCUS_MANAGER_H__
#define __ST_FOCUS_MANAGER_H__

#include <st/st-types.h>
#include <st/st-widget.h>

G_BEGIN_DECLS

#define ST_TYPE_FOCUS_MANAGER                   (st_focus_manager_get_type ())
G_DECLARE_FINAL_TYPE (StFocusManager, st_focus_manager, ST, FOCUS_MANAGER, GObject)

typedef struct _StFocusManager                 StFocusManager;
typedef struct _StFocusManagerPrivate          StFocusManagerPrivate;

/**
 * StFocusManager:
 *
 * The #StFocusManager struct contains only private data
 */
struct _StFocusManager
{
  /*< private >*/
  GObject parent_instance;

  StFocusManagerPrivate *priv;
};

StFocusManager *st_focus_manager_get_for_stage (ClutterStage *stage);

void            st_focus_manager_add_group     (StFocusManager *manager,
                                                StWidget       *root);
void            st_focus_manager_remove_group  (StFocusManager *manager,
                                                StWidget       *root);
StWidget       *st_focus_manager_get_group     (StFocusManager *manager,
                                                StWidget       *widget);
gboolean        st_focus_manager_navigate_from_event (StFocusManager *manager,
                                                      ClutterEvent   *event);

G_END_DECLS

#endif /* __ST_FOCUS_MANAGER_H__ */
