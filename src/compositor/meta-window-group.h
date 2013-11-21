/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef META_WINDOW_GROUP_H
#define META_WINDOW_GROUP_H

#include <clutter/clutter.h>

#include <meta/screen.h>

/**
 * MetaWindowGroup:
 *
 * This class is a subclass of ClutterActor with special handling for
 * #MetaCullable when painting children. It uses code similar to
 * meta_cullable_cull_out_children(), but also has additional special
 * cases for the undirected window, and similar.
 */

#define META_TYPE_WINDOW_GROUP            (meta_window_group_get_type ())
#define META_WINDOW_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_WINDOW_GROUP, MetaWindowGroup))
#define META_WINDOW_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_WINDOW_GROUP, MetaWindowGroupClass))
#define META_IS_WINDOW_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_WINDOW_GROUP))
#define META_IS_WINDOW_GROUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_WINDOW_GROUP))
#define META_WINDOW_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_WINDOW_GROUP, MetaWindowGroupClass))

typedef struct _MetaWindowGroup        MetaWindowGroup;
typedef struct _MetaWindowGroupClass   MetaWindowGroupClass;
typedef struct _MetaWindowGroupPrivate MetaWindowGroupPrivate;

GType meta_window_group_get_type (void);

ClutterActor *meta_window_group_new (MetaScreen *screen);

gboolean meta_window_group_actor_is_untransformed (ClutterActor *actor,
                                                   int          *x_origin,
                                                   int          *y_origin);
#endif /* META_WINDOW_GROUP_H */
