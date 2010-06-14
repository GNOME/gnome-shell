/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2008 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 *
 * Based on GailContainer from GAIL
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __CALLY_GROUP_H__
#define __CALLY_GROUP_H__

#include "cally-actor.h"

G_BEGIN_DECLS

#define CALLY_TYPE_GROUP                         (cally_group_get_type ())
#define CALLY_GROUP(obj)                         (G_TYPE_CHECK_INSTANCE_CAST ((obj), CALLY_TYPE_GROUP, CallyGroup))
#define CALLY_GROUP_CLASS(klass)                 (G_TYPE_CHECK_CLASS_CAST ((klass), CALLY_TYPE_GROUP, CallyGroupClass))
#define CALLY_IS_GROUP(obj)                      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CALLY_TYPE_GROUP))
#define CALLY_IS_GROUP_CLASS(klass)              (G_TYPE_CHECK_CLASS_TYPE ((klass), CALLY_TYPE_GROUP))
#define CALLY_GROUP_GET_CLASS(obj)               (G_TYPE_INSTANCE_GET_CLASS ((obj), CALLY_TYPE_GROUP, CallyGroupClass))

typedef struct _CallyGroup        CallyGroup;
typedef struct _CallyGroupClass   CallyGroupClass;
typedef struct _CallyGroupPrivate CallyGroupPrivate;

struct _CallyGroup
{
  CallyActor parent;

  /* < private > */
  CallyGroupPrivate *priv;
};

struct _CallyGroupClass
{
  CallyActorClass parent_class;

  /* padding for future expansion */
  gpointer _padding_dummy[30];
};

GType      cally_group_get_type (void);
AtkObject* cally_group_new      (ClutterActor *actor);

G_END_DECLS

#endif /* __CALLY_GROUP_H__ */
