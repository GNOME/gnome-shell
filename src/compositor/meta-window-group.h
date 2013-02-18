/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef META_WINDOW_GROUP_H
#define META_WINDOW_GROUP_H

#include <clutter/clutter.h>

#include <meta/screen.h>

/**
 * MetaWindowGroup:
 *
 * This class is a subclass of ClutterGroup with special handling for
 * MetaWindowActor when painting the group. When we are painting a stack
 * of 5-10 maximized windows, the standard bottom-to-top method of
 * drawing every actor results in a tremendous amount of overdraw
 * and can easily max out the available memory bandwidth on a low-end
 * graphics chipset. It's even worse if window textures are being accessed
 * over the AGP bus.
 *
 * The basic technique applied here is to do a pre-pass before painting
 * where we walk window from top to bottom and compute the visible area
 * at each step by subtracting out the windows above it. The visible
 * area is passed to MetaWindowActor which uses it to clip the portion of
 * the window which drawn and avoid redrawing the shadow if it is completely
 * obscured.
 *
 * A caveat is that this is ineffective if applications are using ARGB
 * visuals, since we have no way of knowing whether a window obscures
 * the windows behind it or not. Alternate approaches using the depth
 * or stencil buffer rather than client side regions might be able to
 * handle alpha windows, but the combination of glAlphaFunc and stenciling
 * tends not to be efficient except on newer cards. (And on newer cards
 * we have lots of memory and bandwidth.)
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
