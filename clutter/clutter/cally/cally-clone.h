/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2010 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
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

#ifndef __CALLY_CLONE_H__
#define __CALLY_CLONE_H__

#if !defined(__CALLY_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cally/cally.h> can be included directly."
#endif

#include <clutter/clutter.h>
#include <cally/cally-actor.h>

G_BEGIN_DECLS

#define CALLY_TYPE_CLONE            (cally_clone_get_type ())
#define CALLY_CLONE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CALLY_TYPE_CLONE, CallyClone))
#define CALLY_CLONE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CALLY_TYPE_CLONE, CallyCloneClass))
#define CALLY_IS_CLONE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CALLY_TYPE_CLONE))
#define CALLY_IS_CLONE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CALLY_TYPE_CLONE))
#define CALLY_CLONE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CALLY_TYPE_CLONE, CallyCloneClass))

typedef struct _CallyClone        CallyClone;
typedef struct _CallyCloneClass   CallyCloneClass;
typedef struct _CallyClonePrivate CallyClonePrivate;

/**
 * CallyClone:
 *
 * The <structname>CallyClone</structname> structure contains only private
 * data and should be accessed using the provided API
 *
 * Since: 1.4
 */
struct _CallyClone
{
  /*< private >*/
  CallyActor parent;

  CallyClonePrivate *priv;
};

/**
 * CallyCloneClass:
 *
 * The <structname>CallyCloneClass</structname> structure contains only
 * private data
 *
 * Since: 1.4
 */
struct _CallyCloneClass
{
  /*< private >*/
  CallyActorClass parent_class;

  /* padding for future expansion */
  gpointer _padding_dummy[8];
};

CLUTTER_AVAILABLE_IN_1_4
GType      cally_clone_get_type (void) G_GNUC_CONST;
CLUTTER_AVAILABLE_IN_1_4
AtkObject *cally_clone_new      (ClutterActor *actor);

G_END_DECLS

#endif /* __CALLY_CLONE_H__ */
