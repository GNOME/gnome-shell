/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011  Intel Corporation.
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
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-content
 * @Title: ClutterContent
 * @Short_Description: Delegate for painting the content of an actor
 *
 * #ClutterContent is an interface to implement types responsible for
 * painting the content of a #ClutterActor.
 *
 * Multiple actors can use the same #ClutterContent instance, in order
 * to share the resources associated with painting the same content.
 *
 * #ClutterContent is available since Clutter 1.10.
 */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include "clutter-content-private.h"

#include "clutter-debug.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

typedef struct _ClutterContentIface     ClutterContentInterface;

enum
{
  ATTACHED,
  DETACHED,

  LAST_SIGNAL
};

static GQuark quark_content_actors = 0;

static guint content_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_INTERFACE (ClutterContent, clutter_content, G_TYPE_OBJECT)

static gboolean
clutter_content_real_get_preferred_size (ClutterContent *content,
                                         gfloat         *width,
                                         gfloat         *height)
{
  if (width != NULL)
    *width = 0.f;

  if (height != NULL)
    *height = 0.f;

  return FALSE;
}

static void
clutter_content_real_attached (ClutterContent *content,
                               ClutterActor   *actor)
{
}

static void
clutter_content_real_detached (ClutterContent *content,
                               ClutterActor   *actor)
{
}

static void
clutter_content_real_invalidate (ClutterContent *content)
{
}

static void
clutter_content_real_paint_content (ClutterContent   *content,
                                    ClutterActor     *actor,
                                    ClutterPaintNode *context)
{
}

static void
clutter_content_default_init (ClutterContentInterface *iface)
{
  quark_content_actors = g_quark_from_static_string ("-clutter-content-actors");

  iface->get_preferred_size = clutter_content_real_get_preferred_size;
  iface->paint_content = clutter_content_real_paint_content;
  iface->attached = clutter_content_real_attached;
  iface->detached = clutter_content_real_detached;
  iface->invalidate = clutter_content_real_invalidate;

  /**
   * ClutterContent::attached:
   * @content: the object that emitted the signal
   * @actor: a #ClutterActor
   *
   * This signal is emitted each time a #ClutterContent implementation is
   * assigned to a #ClutterActor.
   *
   * Since: 1.10
   */
  content_signals[ATTACHED] =
    g_signal_new (I_("attached"),
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ClutterContentIface, attached),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);

  /**
   * ClutterContent::detached:
   * @content: the object that emitted the signal
   * @actor: a #ClutterActor
   *
   * This signal is emitted each time a #ClutterContent implementation is
   * removed from a #ClutterActor.
   *
   * Since: 1.10
   */
  content_signals[DETACHED] =
    g_signal_new (I_("detached"),
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ClutterContentIface, detached),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);
}

/**
 * clutter_content_invalidate:
 * @content: a #ClutterContent
 *
 * Invalidates a #ClutterContent.
 *
 * This function should be called by #ClutterContent implementations when
 * they change the way a the content should be painted regardless of the
 * actor state.
 *
 * Since: 1.10
 */
void
clutter_content_invalidate (ClutterContent *content)
{
  GHashTable *actors;
  GHashTableIter iter;
  gpointer key_p, value_p;

  g_return_if_fail (CLUTTER_IS_CONTENT (content));

  CLUTTER_CONTENT_GET_IFACE (content)->invalidate (content);

  actors = g_object_get_qdata (G_OBJECT (content), quark_content_actors);
  if (actors == NULL)
    return;

  g_hash_table_iter_init (&iter, actors);
  while (g_hash_table_iter_next (&iter, &key_p, &value_p))
    {
      ClutterActor *actor = key_p;

      g_assert (actor != NULL);

      clutter_actor_queue_redraw (actor);
    }
}

/*< private >
 * _clutter_content_attached:
 * @content: a #ClutterContent
 * @actor: a #ClutterActor
 *
 * Attaches @actor to the @content.
 *
 * This function should be used internally every time a #ClutterActor
 * is associated to a #ClutterContent, to set up a backpointer from
 * the @content to the @actor.
 *
 * This function will invoke the #ClutterContentIface.attached() virtual
 * function.
 */
void
_clutter_content_attached (ClutterContent *content,
                           ClutterActor   *actor)
{
  GObject *obj = G_OBJECT (content);
  GHashTable *actors;

  actors = g_object_get_qdata (obj, quark_content_actors);
  if (actors == NULL)
    {
      actors = g_hash_table_new (NULL, NULL);
      g_object_set_qdata_full (obj, quark_content_actors,
                               actors,
                               (GDestroyNotify) g_hash_table_unref);
    }

  g_hash_table_insert (actors, actor, actor);

  g_signal_emit (content, content_signals[ATTACHED], 0, actor);
}

/*< private >
 * _clutter_content_detached:
 * @content: a #ClutterContent
 * @actor: a #ClutterActor
 *
 * Detaches @actor from @content.
 *
 * This function should be used internally every time a #ClutterActor
 * removes the association with a #ClutterContent.
 *
 * This function will invoke the #ClutterContentIface.detached() virtual
 * function.
 */
void
_clutter_content_detached (ClutterContent *content,
                           ClutterActor   *actor)
{
  GObject *obj = G_OBJECT (content);
  GHashTable *actors;

  actors = g_object_get_qdata (obj, quark_content_actors);
  g_assert (actors != NULL);

  g_hash_table_remove (actors, actor);

  if (g_hash_table_size (actors) == 0)
    g_object_set_qdata (obj, quark_content_actors, NULL);

  g_signal_emit (content, content_signals[DETACHED], 0, actor);
}

/*< private >
 * _clutter_content_paint_content:
 * @content: a #ClutterContent
 * @actor: a #ClutterActor
 * @context: a #ClutterPaintNode
 *
 * Creates the render tree for the @content and @actor.
 *
 * This function will invoke the #ClutterContentIface.paint_content()
 * virtual function.
 */
void
_clutter_content_paint_content (ClutterContent   *content,
                                ClutterActor     *actor,
                                ClutterPaintNode *node)
{
  CLUTTER_CONTENT_GET_IFACE (content)->paint_content (content, actor, node);
}

/**
 * clutter_content_get_preferred_size:
 * @content: a #ClutterContent
 * @width: (out): return location for the natural width of the content
 * @height: (out): return location for the natural height of the content
 *
 * Retrieves the natural size of the @content, if any.
 *
 * The natural size of a #ClutterContent is defined as the size the content
 * would have regardless of the allocation of the actor that is painting it,
 * for instance the size of an image data.
 *
 * Return value: %TRUE if the content has a preferred size, and %FALSE
 *   otherwise
 *
 * Since: 1.10
 */
gboolean
clutter_content_get_preferred_size (ClutterContent *content,
                                    gfloat         *width,
                                    gfloat         *height)
{
  g_return_val_if_fail (CLUTTER_IS_CONTENT (content), FALSE);

  return CLUTTER_CONTENT_GET_IFACE (content)->get_preferred_size (content,
                                                                  width,
                                                                  height);
}
