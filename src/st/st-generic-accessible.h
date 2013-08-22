/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-generic-accessible.h: generic accessible
 *
 * Copyright 2013 Igalia, S.L.
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

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_GENERIC_ACCESSIBLE_H__
#define __ST_GENERIC_ACCESSIBLE_H__

#include <clutter/clutter.h>
#include <st/st-widget-accessible.h>

G_BEGIN_DECLS

#define ST_TYPE_GENERIC_ACCESSIBLE                 (st_generic_accessible_get_type ())
#define ST_GENERIC_ACCESSIBLE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), ST_TYPE_GENERIC_ACCESSIBLE, StGenericAccessible))
#define ST_GENERIC_ACCESSIBLE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), ST_TYPE_GENERIC_ACCESSIBLE, StGenericAccessibleClass))
#define ST_IS_GENERIC_ACCESSIBLE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ST_TYPE_GENERIC_ACCESSIBLE))
#define ST_IS_GENERIC_ACCESSIBLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), ST_TYPE_GENERIC_ACCESSIBLE))
#define ST_GENERIC_ACCESSIBLE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), ST_TYPE_GENERIC_ACCESSIBLE, StGenericAccessibleClass))

typedef struct _StGenericAccessible        StGenericAccessible;
typedef struct _StGenericAccessibleClass   StGenericAccessibleClass;

typedef struct _StGenericAccessiblePrivate StGenericAccessiblePrivate;

struct _StGenericAccessible
{
    StWidgetAccessible parent;

    StGenericAccessiblePrivate *priv;
};

struct _StGenericAccessibleClass
{
    StWidgetAccessibleClass parent_class;
};

GType       st_generic_accessible_get_type         (void) G_GNUC_CONST;

AtkObject*  st_generic_accessible_new_for_actor (ClutterActor *actor);

G_END_DECLS

#endif /* __ST_GENERIC_ACCESSIBLE_H__ */
