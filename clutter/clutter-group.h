/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_GROUP_H__
#define __CLUTTER_GROUP_H__

#include <glib-object.h>
#include <clutter/clutter-types.h>
#include <clutter/clutter-actor.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_GROUP              (clutter_group_get_type ())
#define CLUTTER_GROUP(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_GROUP, ClutterGroup))
#define CLUTTER_GROUP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_GROUP, ClutterGroupClass))
#define CLUTTER_IS_GROUP(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_GROUP))
#define CLUTTER_IS_GROUP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_GROUP))
#define CLUTTER_GROUP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_GROUP, ClutterGroupClass))

/* XXX - ClutterGroup is to be considered fully deprecated; the only
 * reason we keep this header is because ClutterStage inherits from
 * ClutterGroup, and thus we need to have a structure definition for
 * the Stage object to expand.
 */

typedef struct _ClutterGroup            ClutterGroup;
typedef struct _ClutterGroupClass       ClutterGroupClass;
typedef struct _ClutterGroupPrivate     ClutterGroupPrivate;

/**
 * ClutterGroup:
 *
 * The #ClutterGroup structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 0.1
 */
struct _ClutterGroup
{
  /*< private >*/
  ClutterActor parent_instance;

  ClutterGroupPrivate *priv;
};

/**
 * ClutterGroupClass:
 *
 * The #ClutterGroupClass structure contains only private data
 *
 * Since: 0.1
 */
struct _ClutterGroupClass
{
  /*< private >*/
  ClutterActorClass parent_class;

  /* padding for future expansion */
  void (*_clutter_reserved1) (void);
  void (*_clutter_reserved2) (void);
  void (*_clutter_reserved3) (void);
  void (*_clutter_reserved4) (void);
  void (*_clutter_reserved5) (void);
  void (*_clutter_reserved6) (void);
};

CLUTTER_AVAILABLE_IN_ALL
GType clutter_group_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_GROUP_H__ */
