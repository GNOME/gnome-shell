/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2008 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 *
 * Some parts are based on GailWidget from GAIL
 * GAIL - The GNOME Accessibility Implementation Library
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

#ifndef __CALLY_ACTOR_H__
#define __CALLY_ACTOR_H__

#include <atk/atk.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define CALLY_TYPE_ACTOR            (cally_actor_get_type ())
#define CALLY_ACTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CALLY_TYPE_ACTOR, CallyActor))
#define CALLY_ACTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CALLY_TYPE_ACTOR, CallyActorClass))
#define CALLY_IS_ACTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CALLY_TYPE_ACTOR))
#define CALLY_IS_ACTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CALLY_TYPE_ACTOR))
#define CALLY_ACTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CALLY_TYPE_ACTOR, CallyActorClass))

typedef struct _CallyActor           CallyActor;
typedef struct _CallyActorClass      CallyActorClass;
typedef struct _CallyActorPrivate    CallyActorPrivate;

/**
 * CallyActionFunc:
 * @cally_actor: a #CallyActor
 *
 * Action func, to be used on AtkAction implementation as a individual
 * action
 */
typedef void (*CallyActionFunc) (CallyActor *cally_actor);

struct _CallyActor
{
  AtkGObjectAccessible parent;

  /* < private > */
  CallyActorPrivate *priv;
};

struct _CallyActorClass
{
  AtkGObjectAccessibleClass parent_class;

  /* Signal handler for notify signal on Clutter Actor */
  void     (*notify_clutter) (GObject    *object,
                              GParamSpec *pspec);

  /*
   * Signal handler for key_focus_in and key_focus_out on Clutter Actor
   */
  gboolean (*focus_clutter)  (ClutterActor *actor,
                              gpointer      data);

  gint     (*add_actor)      (ClutterActor *container,
                              ClutterActor *actor,
                              gpointer      data);

  gint     (*remove_actor)   (ClutterActor *container,
                              ClutterActor *actor,
                              gpointer      data);

  /* padding for future expansion */
  gpointer _padding_dummy[30];
};


GType      cally_actor_get_type              (void);

AtkObject* cally_actor_new                   (ClutterActor *actor);
guint      cally_actor_add_action            (CallyActor   *cally_actor,
                                              const gchar *action_name,
                                              const gchar *action_description,
                                              const gchar *action_keybinding,
                                              CallyActionFunc action_func);

gboolean   cally_actor_remove_action         (CallyActor   *cally_actor,
                                              gint         action_id);

gboolean   cally_actor_remove_action_by_name (CallyActor   *cally_actor,
                                              const gchar *action_name);


G_END_DECLS

#endif /* __CALLY_ACTOR_H__ */
