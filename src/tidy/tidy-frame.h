/* tidy-frame.h: Simple container with a background
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
 *
 * Written by: Emmanuele Bassi <ebassi@openedhand.com>
 */

#ifndef __TIDY_FRAME_H__
#define __TIDY_FRAME_H__

#include <clutter/clutter-actor.h>
#include <tidy/tidy-actor.h>

G_BEGIN_DECLS

#define TIDY_TYPE_FRAME                 (tidy_frame_get_type ())
#define TIDY_FRAME(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), TIDY_TYPE_FRAME, TidyFrame))
#define TIDY_IS_FRAME(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TIDY_TYPE_FRAME))
#define TIDY_FRAME_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), TIDY_TYPE_FRAME, TidyFrameClass))
#define TIDY_IS_FRAME_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), TIDY_TYPE_FRAME))
#define TIDY_FRAME_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), TIDY_TYPE_FRAME, TidyFrameClass))

typedef struct _TidyFrame               TidyFrame;
typedef struct _TidyFramePrivate        TidyFramePrivate;
typedef struct _TidyFrameClass          TidyFrameClass;

struct _TidyFrame
{
  TidyActor parent_instance;

  TidyFramePrivate *priv;
};

struct _TidyFrameClass
{
  TidyActorClass parent_class;
};

GType         tidy_frame_get_type    (void) G_GNUC_CONST;

ClutterActor *tidy_frame_new         (void);
ClutterActor *tidy_frame_get_child   (TidyFrame    *frame);
void          tidy_frame_set_texture (TidyFrame    *frame,
                                      ClutterActor *actor);
ClutterActor *tidy_frame_get_texture (TidyFrame    *frame);

G_END_DECLS

#endif /* __TIDY_FRAME_H__ */
