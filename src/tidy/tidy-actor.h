/* tidy-actor.h: Base class for Tidy actors
 *
 * Copyright (C) 2007 OpenedHand
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

#ifndef __TIDY_ACTOR_H__
#define __TIDY_ACTOR_H__

#include <clutter/clutter-actor.h>
#include <tidy/tidy-types.h>

G_BEGIN_DECLS

#define TIDY_TYPE_ACTOR                 (tidy_actor_get_type ())
#define TIDY_ACTOR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), TIDY_TYPE_ACTOR, TidyActor))
#define TIDY_IS_ACTOR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TIDY_TYPE_ACTOR))
#define TIDY_ACTOR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), TIDY_TYPE_ACTOR, TidyActorClass))
#define TIDY_IS_ACTOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), TIDY_TYPE_ACTOR))
#define TIDY_ACTOR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), TIDY_TYPE_ACTOR, TidyActorClass))

typedef struct _TidyActor               TidyActor;
typedef struct _TidyActorPrivate        TidyActorPrivate;
typedef struct _TidyActorClass          TidyActorClass;

/**
 * TidyActor:
 *
 * Base class for stylable actors. The contents of the #TidyActor
 * structure are private and should only be accessed through the
 * public API.
 */
struct _TidyActor
{
  /*< private >*/
  ClutterActor parent_instance;

  TidyActorPrivate *priv;
};

/**
 * TidyActorClass:
 *
 * Base class for stylable actors.
 */
struct _TidyActorClass
{
  /*< private >*/
  ClutterActorClass parent_class;
};

GType      tidy_actor_get_type       (void) G_GNUC_CONST;

void       tidy_actor_set_padding    (TidyActor         *actor,
                                      const TidyPadding *padding);
void       tidy_actor_get_padding    (TidyActor         *actor,
                                      TidyPadding       *padding);

void       tidy_actor_set_alignment  (TidyActor         *actor,
                                      gdouble            x_align,
                                      gdouble            y_align);
void       tidy_actor_get_alignment  (TidyActor         *actor,
                                      gdouble           *x_align,
                                      gdouble           *y_align);
void       tidy_actor_set_alignmentx (TidyActor         *actor,
                                      ClutterFixed       x_align,
                                      ClutterFixed       y_align);
void       tidy_actor_get_alignmentx (TidyActor         *actor,
                                      ClutterFixed      *x_align,
                                      ClutterFixed      *y_align);

G_END_DECLS

#endif /* __TIDY_ACTOR_H__ */
