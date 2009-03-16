/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen  <jorn@openedhand.com>
 *             Emmanuele Bassi  <ebassi@openedhand.com>
 *             Tomas Frydrych <tf@openedhand.com>
 *             Øyvind Kolås <ok@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
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

#ifndef __CLUTTER_CHILD_META_H__
#define __CLUTTER_CHILD_META_H__

#include <glib-object.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_CHILD_META                 (clutter_child_meta_get_type ())
#define CLUTTER_CHILD_META(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_CHILD_META, ClutterChildMeta))
#define CLUTTER_CHILD_META_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_CHILD_META, ClutterChildMetaClass))
#define CLUTTER_IS_CHILD_META(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_CHILD_META))
#define CLUTTER_IS_CHILD_META_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_CHILD_META))
#define CLUTTER_CHILD_META_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_CHILD_META, ClutterChildMetaClass))

typedef struct _ClutterChildMetaClass           ClutterChildMetaClass;

/**
 * ClutterChildMeta:
 * @container: the container handling this data
 * @actor: the actor wrapped by this data
 * 
 * Base interface for container specific state for child actors. A child
 * data is meant to be used when you need to keep track of information
 * about each individual child added to a container.
 *
 * In order to use it you should create your own subclass of
 * #ClutterChildMeta and set the #ClutterContainerIface::child_meta_type
 * interface member to your subclass type, like:
 *
 * |[
 * static void
 * my_container_iface_init (ClutterContainerIface *iface)
 * {
 *   /&ast; set the rest of the #ClutterContainer vtable &ast;/
 *
 *   container_iface->child_meta_type  = MY_TYPE_CHILD_META;
 * }
 * ]|
 *
 * This will automatically create a #ClutterChildMeta of type
 * MY_TYPE_CHILD_META for every actor that is added to the container.
 *
 * The child data for an actor can be retrieved using the
 * clutter_container_get_child_meta() function.
 * 
 * The properties of the data and your subclass can be manipulated with
 * clutter_container_child_set() and clutter_container_child_get() which
 * act like g_object_set() and g_object_get().
 *
 * You can provide hooks for your own storage as well as control the
 * instantiation by overriding #ClutterContainerIface::create_child_meta,
 * #ClutterContainerIface::destroy_child_meta and
 * #ClutterContainerIface::get_child_meta.
 *
 * Since: 0.8
 */
struct _ClutterChildMeta
{
  /*< private >*/
  GObject parent_instance;

  /*< public >*/
  ClutterContainer *container;
  ClutterActor *actor;
};

/**
 * ClutterChildMetaClass:
 *
 * The #ClutterChildMetaClass contains only private data
 *
 * Since: 0.8
 */
struct _ClutterChildMetaClass
{
  /*< private >*/
  GObjectClass parent_class;
}; 

GType             clutter_child_meta_get_type      (void) G_GNUC_CONST;

ClutterContainer *clutter_child_meta_get_container (ClutterChildMeta *data);
ClutterActor     *clutter_child_meta_get_actor     (ClutterChildMeta *data);

G_END_DECLS

#endif /* __CLUTTER_CHILD_META_H__ */
