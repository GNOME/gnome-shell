/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef MUTTER_WINDOW_GROUP_H
#define MUTTER_WINDOW_GROUP_H

#include <clutter/clutter.h>

#include "screen.h"

/**
 * MutterWindowGroup:
 *
 * This class is a subclass of ClutterGroup with special handling for
 * MutterWindow when painting the group. When we are painting a stack
 * of 5-10 maximized windows, the standard bottom-to-top method of
 * drawing every actor results in a tremendous amount of overdraw
 * and can easily max out the available memory bandwidth on a low-end
 * graphics chipset. It's even worse if window textures are being accessed
 * over the AGP bus.
 *
 * The basic technique applied here is to do a pre-pass before painting
 * where we walk window from top to bottom and compute the visible area
 * at each step by subtracting out the windows above it. The visible
 * area is passed to MutterWindow which uses it to clip the portion of
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

#define MUTTER_TYPE_WINDOW_GROUP            (mutter_window_group_get_type ())
#define MUTTER_WINDOW_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MUTTER_TYPE_WINDOW_GROUP, MutterWindowGroup))
#define MUTTER_WINDOW_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MUTTER_TYPE_WINDOW_GROUP, MutterWindowGroupClass))
#define MUTTER_IS_WINDOW_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MUTTER_TYPE_WINDOW_GROUP))
#define MUTTER_IS_WINDOW_GROU_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MUTTER_TYPE_WINDOW_GROUP))
#define MUTTER_WINDOW_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MUTTER_TYPE_WINDOW_GROUP, MutterWindowGroupClass))

typedef struct _MutterWindowGroup        MutterWindowGroup;
typedef struct _MutterWindowGroupClass   MutterWindowGroupClass;
typedef struct _MutterWindowGroupPrivate MutterWindowGroupPrivate;

GType mutter_window_group_get_type (void);

ClutterActor *mutter_window_group_new (MetaScreen *screen);

#endif /* MUTTER_WINDOW_GROUP_H */
