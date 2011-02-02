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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:cally-actor
 * @Title: CallyActor
 * @short_description: Implementation of the ATK interfaces for #ClutterActor
 * @see_also: #ClutterActor
 *
 * #CallyActor implements the required ATK interfaces of #ClutterActor
 * exposing the common elements on each actor (position, extents, etc).
 */

/*
 *
 * IMPLEMENTATION NOTES:
 *
 * ####
 *
 * Focus: clutter hasn't got the focus concept in the same way that GTK, but it
 * has a key focus managed by the stage. Basically any actor can be focused using
 * clutter_stage_set_key_focus. So, we will use this approach: all actors are
 * focusable, and we get the currently focused using clutter_stage_get_key_focus
 * This affects focus related stateset and some atk_componenet focus methods (like
 * grab focus)
 *
 * ####
 *
 * #ClutterContainer : cally_actor implements some of his methods based on
 * #ClutterContainer interface, although there are the posibility to not
 * implement it. Could be strange to do that but:
 *   * Some methods (like get_n_children and ref_child) are easily implemented using
 *     this interface so a general implementation can be done
 *   * #ClutterContainer is a popular interface, so several classes will implement
 *     that.
 *   * So we can implement a a11y class similar to GailContainer for each clutter
 *     object implementing that, and their clutter subclasses will have a proper
 *     a11y class, but not if they are parallel classes (ie: #ClutterGroup,
 *     #TinyFrame, #TinyScrollView)
 *   * So, on all this objects, will be required to reimplement again some methods
 *     on the objects.
 *   * A auxiliar object (a kind of a11y specific #ClutterContainer implementation)
 *     could be used to implement this methods in only a place, anyway, this will
 *     require some code on each concrete class to manage it.
 *   * So this implementation is based in that is better to manage a interface
 *     on the top abstract object, instead that C&P some code, with the minor
 *     problem that we need to check if we are implementing or not the interface.
 *
 * This methods can be reimplemented, in concrete cases that we can get ways more
 * efficient to implement that. Take a look to #CallyGroup as a example of this.
 *
 * Anyway, there are several examples of behaviour changes depending of the current
 * type of the object you are granting access.
 *
 * TODO,FIXME: check if an option would be to use a dynamic type, as
 * it has been done on the webkit a11y implementation:
 *      See: https://bugs.webkit.org/show_bug.cgi?id=21546
 *
 * ###
 *
 * #AtkAction implementation: on previous releases ClutterActor added
 * the actions "press", "release" and "click", as at that time some
 * general-purpose actors like textures were directly used as buttons.
 *
 * But now, new toolkits appeared, providing high-level widgets, like
 * buttons. So in this environment, it doesn't make sense to keep
 * adding them as default.
 *
 * Anyway, current implementation of AtkAction is done at CallyActor
 * providing methods to add and remove actions. This is based on the
 * one used at gailcell, and proposed as a change on #AtkAction
 * interface:
 *
 *  https://bugzilla.gnome.org/show_bug.cgi?id=649804
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <clutter/clutter.h>

#ifdef HAVE_CLUTTER_GLX
#include <clutter/x11/clutter-x11.h>
#endif

#include <math.h>

#include "cally-actor.h"
#include "cally-actor-private.h"

typedef struct _CallyActorActionInfo CallyActorActionInfo;

/*< private >
 * CallyActorActionInfo:
 * @name: name of the action
 * @description: description of the action
 * @keybinding: keybinding related to the action
 * @do_action_func: callback
 * @user_data: data to be passed to @do_action_func
 * @notify: function to be called when removing the action
 *
 * Utility structure to maintain the different actions added to the
 * #CallyActor
 */
struct _CallyActorActionInfo
{
  gchar *name;
  gchar *description;
  gchar *keybinding;

  CallyActionCallback do_action_func;
  gpointer user_data;
  GDestroyNotify notify;
};

static void cally_actor_class_init (CallyActorClass *klass);
static void cally_actor_init       (CallyActor *cally_actor);
static void cally_actor_initialize (AtkObject *obj,
                                   gpointer   data);
static void cally_actor_finalize   (GObject *obj);

/* AtkObject.h */
static AtkObject*            cally_actor_get_parent          (AtkObject *obj);
static gint                  cally_actor_get_index_in_parent (AtkObject *obj);
static AtkStateSet*          cally_actor_ref_state_set       (AtkObject *obj);
static const gchar*          cally_actor_get_name            (AtkObject *obj);
static gint                  cally_actor_get_n_children      (AtkObject *obj);
static AtkObject*            cally_actor_ref_child           (AtkObject *obj,
                                                             gint       i);
static AtkAttributeSet *     cally_actor_get_attributes      (AtkObject *obj);

static gboolean             _cally_actor_all_parents_visible (ClutterActor *actor);

/* ClutterContainer */
static gint cally_actor_add_actor          (ClutterActor *container,
                                           ClutterActor *actor,
                                           gpointer      data);
static gint cally_actor_remove_actor      (ClutterActor *container,
                                          ClutterActor *actor,
                                          gpointer      data);
static gint cally_actor_real_add_actor    (ClutterActor *container,
                                          ClutterActor *actor,
                                          gpointer      data);
static gint cally_actor_real_remove_actor (ClutterActor *container,
                                          ClutterActor *actor,
                                          gpointer      data);

/* AtkComponent.h */
static void     cally_actor_component_interface_init (AtkComponentIface *iface);
static void     cally_actor_get_extents              (AtkComponent *component,
                                                     gint         *x,
                                                     gint         *y,
                                                     gint         *width,
                                                     gint         *height,
                                                     AtkCoordType  coord_type);
static gint     cally_actor_get_mdi_zorder           (AtkComponent *component);
static gboolean cally_actor_grab_focus               (AtkComponent *component);
static guint    cally_actor_add_focus_handler        (AtkComponent *component,
                                                      AtkFocusHandler handler);
static void     cally_actor_remove_focus_handler     (AtkComponent *component,
                                                      guint handler_id);
static void     cally_actor_focus_event              (AtkObject   *obj,
                                                      gboolean    focus_in);
static gboolean _is_actor_on_screen                 (ClutterActor *actor);

/* AtkAction.h */
static void                  cally_actor_action_interface_init  (AtkActionIface *iface);
static gboolean              cally_actor_action_do_action       (AtkAction *action,
                                                                gint       i);
static gboolean              idle_do_action                    (gpointer data);
static gint                  cally_actor_action_get_n_actions   (AtkAction *action);
static const gchar*          cally_actor_action_get_description (AtkAction *action,
                                                                gint       i);
static const gchar*          cally_actor_action_get_keybinding  (AtkAction *action,
                                                                gint       i);
static const gchar*          cally_actor_action_get_name        (AtkAction *action,
                                                                gint       i);
static gboolean              cally_actor_action_set_description (AtkAction   *action,
                                                                gint         i,
                                                                const gchar *desc);
static void                  _cally_actor_destroy_action_info   (gpointer      action_info,
                                                                gpointer      user_data);
static void                  _cally_actor_clean_action_list     (CallyActor *cally_actor);

static CallyActorActionInfo*  _cally_actor_get_action_info       (CallyActor *cally_actor,
                                                                gint       index);
/* Misc functions */
static void cally_actor_notify_clutter          (GObject    *obj,
                                                GParamSpec *pspec);
static void cally_actor_real_notify_clutter     (GObject    *obj,
                                                 GParamSpec *pspec);
static gboolean cally_actor_focus_clutter       (ClutterActor *actor,
                                                 gpointer      data);
static gboolean cally_actor_real_focus_clutter  (ClutterActor *actor,
                                                 gpointer      data);

G_DEFINE_TYPE_WITH_CODE (CallyActor,
                         cally_actor,
                         ATK_TYPE_GOBJECT_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT,
                                                cally_actor_component_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION,
                                                cally_actor_action_interface_init));

#define CALLY_ACTOR_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CALLY_TYPE_ACTOR, CallyActorPrivate))


struct _CallyActorPrivate
{
  GQueue *action_queue;
  guint   action_idle_handler;
  GList  *action_list;

  GList *children;
};

/**
 * cally_actor_new:
 * @actor: a #ClutterActor
 *
 * Creates a new #CallyActor for the given @actor
 *
 * Return value: the newly created #AtkObject
 *
 * Since: 1.4
 */
AtkObject *
cally_actor_new (ClutterActor *actor)
{
  gpointer   object;
  AtkObject *atk_object;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);

  object = g_object_new (CALLY_TYPE_ACTOR, NULL);

  atk_object = ATK_OBJECT (object);
  atk_object_initialize (atk_object, actor);

  return atk_object;
}

static void
cally_actor_initialize (AtkObject *obj,
                        gpointer   data)
{
  CallyActor        *self  = NULL;
  CallyActorPrivate *priv  = NULL;
  ClutterActor     *actor = NULL;
  guint             handler_id;

  ATK_OBJECT_CLASS (cally_actor_parent_class)->initialize (obj, data);

  self = CALLY_ACTOR(obj);
  priv = self->priv;
  actor = CLUTTER_ACTOR (data);

  g_signal_connect_after (actor,
                          "key-focus-in",
                          G_CALLBACK (cally_actor_focus_clutter),
                          GINT_TO_POINTER (TRUE));
  g_signal_connect_after (actor,
                          "key-focus-out",
                          G_CALLBACK (cally_actor_focus_clutter),
                          GINT_TO_POINTER (FALSE));
  g_signal_connect (actor,
                    "notify",
                    G_CALLBACK (cally_actor_notify_clutter),
                    NULL);
  atk_component_add_focus_handler (ATK_COMPONENT (self),
                                   cally_actor_focus_event);

  g_object_set_data (G_OBJECT (obj), "atk-component-layer",
                     GINT_TO_POINTER (ATK_LAYER_MDI));

  /* Depends if the object implement ClutterContainer */
  if (CLUTTER_IS_CONTAINER(actor))
    {
      priv->children = clutter_container_get_children (CLUTTER_CONTAINER (actor));

      /*
       * We store the handler ids for these signals in case some objects
       * need to remove these handlers.
       */
      handler_id = g_signal_connect (actor,
                                     "actor-added",
                                     G_CALLBACK (cally_actor_add_actor),
                                     obj);
      g_object_set_data (G_OBJECT (obj), "cally-add-handler-id",
                         GUINT_TO_POINTER (handler_id));
      handler_id = g_signal_connect (actor,
                                     "actor-removed",
                                     G_CALLBACK (cally_actor_remove_actor),
                                     obj);
      g_object_set_data (G_OBJECT (obj), "cally-remove-handler-id",
                         GUINT_TO_POINTER (handler_id));

      obj->role = ATK_ROLE_PANEL; /* typically objects implementing ClutterContainer
                                     interface would be a panel */
    }
  else
    {
      priv->children = NULL;
      obj->role = ATK_ROLE_UNKNOWN;
    }
}

static void
cally_actor_class_init (CallyActorClass *klass)
{
  AtkObjectClass *class         = ATK_OBJECT_CLASS (klass);
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);

  klass->focus_clutter  = cally_actor_real_focus_clutter;
  klass->notify_clutter = cally_actor_real_notify_clutter;
  klass->add_actor      = cally_actor_real_add_actor;
  klass->remove_actor   = cally_actor_real_remove_actor;

  /* GObject */
  gobject_class->finalize = cally_actor_finalize;

  /* AtkObject */
  class->get_name            = cally_actor_get_name;
  class->get_parent          = cally_actor_get_parent;
  class->get_index_in_parent = cally_actor_get_index_in_parent;
  class->ref_state_set       = cally_actor_ref_state_set;
  class->initialize          = cally_actor_initialize;
  class->get_n_children      = cally_actor_get_n_children;
  class->ref_child           = cally_actor_ref_child;
  class->get_attributes      = cally_actor_get_attributes;

  g_type_class_add_private (gobject_class, sizeof (CallyActorPrivate));
}

static void
cally_actor_init (CallyActor *cally_actor)
{
  CallyActorPrivate *priv = CALLY_ACTOR_GET_PRIVATE (cally_actor);

  cally_actor->priv = priv;

  priv->action_queue        = NULL;
  priv->action_idle_handler = 0;

  priv->action_list = NULL;

  priv->children = NULL;
}


static void
cally_actor_finalize (GObject *obj)
{
  CallyActor        *cally_actor = NULL;
  CallyActorPrivate *priv       = NULL;

  cally_actor = CALLY_ACTOR (obj);
  priv = cally_actor->priv;

  _cally_actor_clean_action_list (cally_actor);

  if (priv->action_idle_handler)
    {
      g_source_remove (priv->action_idle_handler);
      priv->action_idle_handler = 0;
    }

  if (priv->action_queue)
    {
      g_queue_free (priv->action_queue);
    }

  if (priv->children)
    {
      g_list_free (priv->children);
      priv->children = NULL;
    }

  G_OBJECT_CLASS (cally_actor_parent_class)->finalize (obj);
}

/* AtkObject */

static const gchar*
cally_actor_get_name (AtkObject *obj)
{
  const gchar* name = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (obj), NULL);

  name = ATK_OBJECT_CLASS (cally_actor_parent_class)->get_name (obj);
  if (name == NULL)
    {
      CallyActor *cally_actor = NULL;
      ClutterActor *actor = NULL;

      cally_actor = CALLY_ACTOR (obj);
      actor = CALLY_GET_CLUTTER_ACTOR (cally_actor);
      if (actor == NULL) /* State is defunct */
        name = NULL;
      else
        name = clutter_actor_get_name (actor);
    }
  return name;
}

static AtkObject *
cally_actor_get_parent (AtkObject *obj)
{
  ClutterActor *parent_actor = NULL;
  AtkObject    *parent       = NULL;
  ClutterActor *actor        = NULL;
  CallyActor    *cally_actor   = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (obj), NULL);

  /* Check if we have and assigned parent */
  if (obj->accessible_parent)
    return obj->accessible_parent;

  /* Try to get it from the clutter parent */
  cally_actor = CALLY_ACTOR (obj);
  actor = CALLY_GET_CLUTTER_ACTOR (cally_actor);
  if (actor == NULL)  /* Object is defunct */
    return NULL;

  parent_actor = clutter_actor_get_parent (actor);
  if (parent_actor == NULL)
    return NULL;

  parent = clutter_actor_get_accessible (parent_actor);

  /* FIXME: I need to review the clutter-embed, to check if in this case I
   * should get the widget accessible
   */

  return parent;
}

static gint
cally_actor_get_index_in_parent (AtkObject *obj)
{
  CallyActor    *cally_actor   = NULL;
  ClutterActor *actor        = NULL;
  ClutterActor *parent_actor = NULL;
  GList        *children     = NULL;
  gint          index        = -1;

  g_return_val_if_fail (CALLY_IS_ACTOR (obj), -1);

  if (obj->accessible_parent)
    {
      gint n_children, i;
      gboolean found = FALSE;

      n_children = atk_object_get_n_accessible_children (obj->accessible_parent);
      for (i = 0; i < n_children; i++)
        {
          AtkObject *child;

          child = atk_object_ref_accessible_child (obj->accessible_parent, i);
          if (child == obj)
            found = TRUE;

          g_object_unref (child);
          if (found)
            return i;
        }
      return -1;
    }

  cally_actor = CALLY_ACTOR (obj);
  actor = CALLY_GET_CLUTTER_ACTOR (cally_actor);
  if (actor == NULL) /* Object is defunct */
    return -1;

  parent_actor = clutter_actor_get_parent(actor);
  if ((parent_actor == NULL)||(!CLUTTER_IS_CONTAINER(parent_actor)))
    return -1;

  children = clutter_container_get_children(CLUTTER_CONTAINER(parent_actor));

  index = g_list_index (children, actor);
  g_list_free (children);

  return index;
}

static AtkStateSet*
cally_actor_ref_state_set (AtkObject *obj)
{
  ClutterActor         *actor = NULL;
  AtkStateSet          *state_set = NULL;
  ClutterStage         *stage = NULL;
  ClutterActor         *focus_actor = NULL;
  CallyActor            *cally_actor = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (obj), NULL);
  cally_actor = CALLY_ACTOR (obj);

  state_set = ATK_OBJECT_CLASS (cally_actor_parent_class)->ref_state_set (obj);

  actor = CALLY_GET_CLUTTER_ACTOR (cally_actor);

  if (actor == NULL) /* Object is defunct */
    {
      atk_state_set_add_state (state_set, ATK_STATE_DEFUNCT);
    }
  else
    {
      if (CLUTTER_ACTOR_IS_REACTIVE (actor))
        {
          atk_state_set_add_state (state_set, ATK_STATE_SENSITIVE);
          atk_state_set_add_state (state_set, ATK_STATE_ENABLED);
        }

      if (CLUTTER_ACTOR_IS_VISIBLE (actor))
        {
          atk_state_set_add_state (state_set, ATK_STATE_VISIBLE);

          if (_is_actor_on_screen (actor) &&
              _cally_actor_all_parents_visible (actor))
            atk_state_set_add_state (state_set, ATK_STATE_SHOWING);
        }

      /* See focus section on implementation notes */
      atk_state_set_add_state (state_set, ATK_STATE_FOCUSABLE);

      stage = CLUTTER_STAGE (clutter_actor_get_stage (actor));
      /* If for any reason this actor doesn't have a stage
         associated, we try the default one as fallback */
      if (stage == NULL)
          stage = CLUTTER_STAGE (clutter_stage_get_default ());

      focus_actor = clutter_stage_get_key_focus (stage);
      if (focus_actor == actor)
        atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
    }

  return state_set;
}

static gint
cally_actor_get_n_children (AtkObject *obj)
{
  ClutterActor     *actor    = NULL;
  GList            *children = NULL;
  gint              num      = 0;

  g_return_val_if_fail (CALLY_IS_ACTOR (obj), 0);

  actor = CALLY_GET_CLUTTER_ACTOR (obj);

  if (actor == NULL) /* State is defunct */
    return 0;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), 0);

  if (CLUTTER_IS_CONTAINER (actor))
    {
      children = clutter_container_get_children (CLUTTER_CONTAINER (actor));
      num = g_list_length (children);

      g_list_free (children);
    }
  else
    {
      num = 0;
    }

  return num;
}

static AtkObject*
cally_actor_ref_child (AtkObject *obj,
                      gint       i)
{
  ClutterActor     *actor    = NULL;
  ClutterActor     *child    = NULL;
  GList            *children = NULL;
  AtkObject        *result   = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (obj), NULL);

  actor = CALLY_GET_CLUTTER_ACTOR (obj);

  if (actor == NULL) /* State is defunct */
    {
      return NULL;
    }

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);

  if (CLUTTER_IS_CONTAINER (actor))
    {
      children = clutter_container_get_children (CLUTTER_CONTAINER (actor));
      child = g_list_nth_data (children, i);

      result = clutter_actor_get_accessible (child);

      g_object_ref (result);
      g_list_free (children);
    }
  else
    {
      result = NULL;
    }

  return result;
}

static AtkAttributeSet *
cally_actor_get_attributes (AtkObject *obj)
{
  AtkAttributeSet *attributes;
  AtkAttribute *toolkit;

  toolkit = g_new (AtkAttribute, 1);
  toolkit->name = g_strdup ("toolkit");
  toolkit->value = g_strdup ("clutter");

  attributes = g_slist_append (NULL, toolkit);

  return attributes;
}

/* ClutterContainer */
static gint
cally_actor_add_actor (ClutterActor *container,
                      ClutterActor *actor,
                      gpointer      data)
{
  CallyActor *cally_actor = CALLY_ACTOR (data);
  CallyActorClass *klass = NULL;

  klass = CALLY_ACTOR_GET_CLASS (cally_actor);

  if (klass->add_actor)
    return klass->add_actor (container, actor, data);
  else
    return 1;
}

static gint
cally_actor_remove_actor (ClutterActor *container,
                         ClutterActor *actor,
                         gpointer      data)
{
  CallyActor      *cally_actor = CALLY_ACTOR (data);
  CallyActorClass *klass      = NULL;

  klass = CALLY_ACTOR_GET_CLASS (cally_actor);

  if (klass->remove_actor)
    return klass->remove_actor (container, actor, data);
  else
    return 1;
}


static gint
cally_actor_real_add_actor (ClutterActor *container,
                           ClutterActor *actor,
                           gpointer      data)
{
  AtkObject        *atk_parent = ATK_OBJECT (data);
  AtkObject        *atk_child  = clutter_actor_get_accessible (actor);
  CallyActor        *cally_actor = CALLY_ACTOR (atk_parent);
  CallyActorPrivate *priv       = cally_actor->priv;
  gint              index;

  g_return_val_if_fail (CLUTTER_IS_CONTAINER (container), 0);
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), 0);

  g_object_notify (G_OBJECT (atk_child), "accessible_parent");

  g_list_free (priv->children);

  priv->children =
    clutter_container_get_children (CLUTTER_CONTAINER(container));

  index = g_list_index (priv->children, actor);
  g_signal_emit_by_name (atk_parent, "children_changed::add",
                         index, atk_child, NULL);

  return 1;
}

static gint
cally_actor_real_remove_actor (ClutterActor *container,
                              ClutterActor *actor,
                              gpointer      data)
{
  AtkPropertyValues  values      = { NULL };
  AtkObject*         atk_parent  = NULL;
  AtkObject         *atk_child   = NULL;
  CallyActorPrivate  *priv        = NULL;
  gint               index;

  g_return_val_if_fail (CLUTTER_IS_CONTAINER (container), 0);
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), 0);

  atk_parent = ATK_OBJECT (data);
  atk_child = clutter_actor_get_accessible (actor);

  if (atk_child)
    {
      g_value_init (&values.old_value, G_TYPE_POINTER);
      g_value_set_pointer (&values.old_value, atk_parent);

      values.property_name = "accessible-parent";

      g_object_ref (atk_child);
      g_signal_emit_by_name (atk_child,
                             "property_change::accessible-parent", &values, NULL);
      g_object_unref (atk_child);
    }

  priv = CALLY_ACTOR (atk_parent)->priv;
  index = g_list_index (priv->children, actor);
  g_list_free (priv->children);
  priv->children = clutter_container_get_children (CLUTTER_CONTAINER(container));

  if (index >= 0 && index <= g_list_length (priv->children))
    g_signal_emit_by_name (atk_parent, "children_changed::remove",
                           index, atk_child, NULL);

  return 1;
}

/* AtkComponent implementation */
static void
cally_actor_component_interface_init (AtkComponentIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_extents    = cally_actor_get_extents;
  iface->get_mdi_zorder = cally_actor_get_mdi_zorder;

  /* focus management */
  iface->grab_focus           = cally_actor_grab_focus;
  iface->add_focus_handler    = cally_actor_add_focus_handler;
  iface->remove_focus_handler = cally_actor_remove_focus_handler;
}

static void
cally_actor_get_extents (AtkComponent *component,
                        gint         *x,
                        gint         *y,
                        gint         *width,
                        gint         *height,
                        AtkCoordType coord_type)
{
  CallyActor   *cally_actor = NULL;
  ClutterActor *actor      = NULL;
  gint          top_level_x, top_level_y;
  gfloat        f_width, f_height;
  ClutterVertex verts[4];

  g_return_if_fail (CALLY_IS_ACTOR (component));

  cally_actor = CALLY_ACTOR (component);
  actor = CALLY_GET_CLUTTER_ACTOR (cally_actor);

  if (actor == NULL) /* actor is defunct */
    return;

  clutter_actor_get_abs_allocation_vertices (actor, verts);
  clutter_actor_get_transformed_size (actor, &f_width, &f_height);

  *x = verts[0].x;
  *y = verts[0].y;
  *width = ceilf (f_width);
  *height = ceilf (f_height);

  /* In the ATK_XY_WINDOW case, we consider the stage as the
   * "top-level-window"
   *
   * http://library.gnome.org/devel/atk/stable/AtkUtil.html#AtkCoordType
   */

  if (coord_type == ATK_XY_SCREEN)
    {
      _cally_actor_get_top_level_origin (actor, &top_level_x, &top_level_y);

      *x += top_level_x;
      *y += top_level_y;
    }

  return;
}

static gint
cally_actor_get_mdi_zorder (AtkComponent *component)
{
  CallyActor    *cally_actor = NULL;
  ClutterActor *actor = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (component), G_MININT);

  cally_actor = CALLY_ACTOR(component);
  actor = CALLY_GET_CLUTTER_ACTOR (cally_actor);

  return clutter_actor_get_depth (actor);
}

static gboolean
cally_actor_grab_focus (AtkComponent    *component)
{
  ClutterActor *actor      = NULL;
  ClutterActor *stage      = NULL;
  CallyActor    *cally_actor = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (component), FALSE);

  /* See focus section on implementation notes */
  cally_actor = CALLY_ACTOR(component);
  actor = CALLY_GET_CLUTTER_ACTOR (cally_actor);
  stage = clutter_actor_get_stage (actor);

  clutter_stage_set_key_focus (CLUTTER_STAGE (stage),
                               actor);

  return TRUE;
}

/*
 * These methods are basically taken from gail, as I don't see any
 * reason to modify it. It makes me wonder why it is really required
 * to be implemented in the toolkit
 */
static guint
cally_actor_add_focus_handler (AtkComponent *component,
                               AtkFocusHandler handler)
{
  GSignalMatchType match_type;
  gulong ret;
  guint signal_id;

  match_type = G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_FUNC;
  signal_id = g_signal_lookup ("focus-event", ATK_TYPE_OBJECT);

  ret = g_signal_handler_find (component, match_type, signal_id, 0, NULL,
                               (gpointer) handler, NULL);
  if (!ret)
    {
      return g_signal_connect_closure_by_id (component,
                                             signal_id, 0,
                                             g_cclosure_new (G_CALLBACK (handler), NULL,
                                                             (GClosureNotify) NULL),
                                             FALSE);
    }
  else
    {
      return 0;
    }
}

static void
cally_actor_remove_focus_handler (AtkComponent *component,
                                  guint handler_id)
{
  g_signal_handler_disconnect (component, handler_id);
}

/* This method should check if the actor is currently on screen */
static gboolean
_is_actor_on_screen (ClutterActor *actor)
{
  /* FIXME: FILL ME!!
   * You could get some ideas from clutter_actor_is_on_stage, a private clutter
   * function (note: it doesn't exists in the last versions of clutter)
   * A occlusion check could be a good idea too
   */

  return TRUE;
}

/*
 *
 * This gets the top level origin, it is, the position of the stage in
 * the global screen. You can see it as the absolute display position
 * of the stage.
 *
 * FIXME: only the case with x11 is implemented, other backends are
 * required
 *
 */
void
_cally_actor_get_top_level_origin (ClutterActor *actor,
                                   gint         *xp,
                                   gint         *yp)
{
  /* default values */
  gint x = 0;
  gint y = 0;

#ifdef HAVE_CLUTTER_GLX
  {
    ClutterActor *stage      = NULL;
    Display      *display    = NULL;
    Window        root_window;
    Window        stage_window;
    Window        child;
    gint          return_val = 0;

    stage = clutter_actor_get_stage (actor);

    /* FIXME: what happens if you use another display with
       clutter_backend_x11_set_display ?*/
    display = clutter_x11_get_default_display ();
    root_window = clutter_x11_get_root_window ();
    stage_window = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

    return_val = XTranslateCoordinates (display, stage_window, root_window,
                                        0, 0, &x, &y,
                                        &child);

    if (!return_val)
      g_warning ("[x11] We were not able to get proper absolute "
                 "position of the stage");
  }
#else
  {
    static gboolean yet_warned = FALSE;

    if (!yet_warned)
      {
        yet_warned = TRUE;

        g_warning ("Using a clutter backend not supported. "
                   "atk_component_get_extents using ATK_XY_SCREEN "
                   "could return a wrong screen position");
      }
  }
#endif

  if (xp)
      *xp = x;

  if (yp)
      *yp = y;
}

/* AtkAction implementation */
static void
cally_actor_action_interface_init (AtkActionIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->do_action       = cally_actor_action_do_action;
  iface->get_n_actions   = cally_actor_action_get_n_actions;
  iface->get_description = cally_actor_action_get_description;
  iface->get_keybinding  = cally_actor_action_get_keybinding;
  iface->get_name        = cally_actor_action_get_name;
  iface->set_description = cally_actor_action_set_description;
}

static gboolean
cally_actor_action_do_action (AtkAction *action,
                             gint       index)
{
  CallyActor           *cally_actor = NULL;
  AtkStateSet          *set         = NULL;
  CallyActorPrivate    *priv        = NULL;
  CallyActorActionInfo *info        = NULL;

  cally_actor = CALLY_ACTOR (action);
  priv = cally_actor->priv;

  set = atk_object_ref_state_set (ATK_OBJECT (cally_actor));

  if (atk_state_set_contains_state (set, ATK_STATE_DEFUNCT))
    return FALSE;

  if (!atk_state_set_contains_state (set, ATK_STATE_SENSITIVE) ||
      !atk_state_set_contains_state (set, ATK_STATE_SHOWING))
    return FALSE;

  g_object_unref (set);

  info = _cally_actor_get_action_info (cally_actor, index);

  if (info == NULL)
    return FALSE;

  if (info->do_action_func == NULL)
    return FALSE;

  if (!priv->action_queue)
    priv->action_queue = g_queue_new ();

  g_queue_push_head (priv->action_queue, info);

  if (!priv->action_idle_handler)
    priv->action_idle_handler = g_idle_add (idle_do_action, cally_actor);

  return TRUE;
}

static gboolean
idle_do_action (gpointer data)
{
  CallyActor        *cally_actor = NULL;
  CallyActorPrivate *priv       = NULL;
  ClutterActor     *actor      = NULL;

  cally_actor = CALLY_ACTOR (data);
  priv = cally_actor->priv;
  actor = CALLY_GET_CLUTTER_ACTOR (cally_actor);
  priv->action_idle_handler = 0;

  if (actor == NULL) /* state is defunct*/
    return FALSE;

  while (!g_queue_is_empty (priv->action_queue))
    {
      CallyActorActionInfo *info = NULL;

      info = (CallyActorActionInfo *) g_queue_pop_head (priv->action_queue);

      info->do_action_func (cally_actor, info->user_data);
    }

  return FALSE;
}

static gint
cally_actor_action_get_n_actions (AtkAction *action)
{
  CallyActor        *cally_actor = NULL;
  CallyActorPrivate *priv       = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (action), 0);

  cally_actor = CALLY_ACTOR (action);
  priv       = cally_actor->priv;

  return g_list_length (priv->action_list);
}

static const gchar*
cally_actor_action_get_name (AtkAction *action,
                            gint       i)
{
  CallyActor           *cally_actor = NULL;
  CallyActorActionInfo *info       = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (action), NULL);
  cally_actor = CALLY_ACTOR (action);
  info = _cally_actor_get_action_info (cally_actor, i);

  if (info == NULL)
    return NULL;

  return info->name;
}

static const gchar*
cally_actor_action_get_description (AtkAction *action,
                                   gint       i)
{
  CallyActor           *cally_actor = NULL;
  CallyActorActionInfo *info       = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (action), NULL);
  cally_actor = CALLY_ACTOR (action);
  info = _cally_actor_get_action_info (cally_actor, i);

  if (info == NULL)
    return NULL;

  return info->description;
}

static gboolean
cally_actor_action_set_description (AtkAction   *action,
                                   gint         i,
                                   const gchar *desc)
{
  CallyActor           *cally_actor = NULL;
  CallyActorActionInfo *info       = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (action), FALSE);
  cally_actor = CALLY_ACTOR (action);
  info = _cally_actor_get_action_info (cally_actor, i);

  if (info == NULL)
      return FALSE;

  g_free (info->description);
  info->description = g_strdup (desc);

  return TRUE;
}

static const gchar*
cally_actor_action_get_keybinding (AtkAction *action,
                                  gint       i)
{
  CallyActor           *cally_actor = NULL;
  CallyActorActionInfo *info       = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (action), NULL);
  cally_actor = CALLY_ACTOR (action);
  info = _cally_actor_get_action_info (cally_actor, i);

  if (info == NULL)
    return NULL;

  return info->keybinding;
}

/* Misc functions */

/*
 * Checks if the parent actor, and his parent, etc is all visible
 * Used to check the showing property
 *
 * FIXME: the same functionality is implemented on clutter since version 0.8.4
 * by clutter_actor_get_paint_visibility, so we should change this function
 * if a clutter version update is made
 */
static gboolean
_cally_actor_all_parents_visible (ClutterActor *actor)
{
  ClutterActor *iter_parent = NULL;
  gboolean      result      = TRUE;
  ClutterActor *stage       = NULL;

  stage = clutter_actor_get_stage (actor);

  for (iter_parent = clutter_actor_get_parent(actor); iter_parent;
       iter_parent = clutter_actor_get_parent(iter_parent))
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE (iter_parent))
        {
          /* stage parent */
          if (iter_parent != stage)
            result = FALSE;
          else
            result = TRUE;

          break;
        }
    }

  return result;
}

/*
 * This function is a signal handler for key_focus_in and
 * key_focus_out signal which gets emitted on a ClutterActor
 */
static gboolean
cally_actor_focus_clutter (ClutterActor *actor,
                           gpointer      data)
{
  CallyActor      *cally_actor = NULL;
  CallyActorClass *klass       = NULL;

  cally_actor = CALLY_ACTOR (clutter_actor_get_accessible (actor));
  klass = CALLY_ACTOR_GET_CLASS (cally_actor);
  if (klass->focus_clutter)
    return klass->focus_clutter (actor, data);
  else
    return FALSE;
}

static gboolean
cally_actor_real_focus_clutter (ClutterActor *actor,
                                gpointer      data)
{
  CallyActor *cally_actor = NULL;
  gboolean return_val = FALSE;
  gboolean in = FALSE;

  in = GPOINTER_TO_INT (data);
  cally_actor = CALLY_ACTOR (clutter_actor_get_accessible (actor));

  g_signal_emit_by_name (cally_actor, "focus_event", in, &return_val);
  atk_focus_tracker_notify (ATK_OBJECT (cally_actor));

  return FALSE;
}

/*
 * This function is a signal handler for notify signal which gets emitted
 * when a property changes value on the ClutterActor associated with the object.
 *
 * It calls a function for the CallyActor type
 */
static void
cally_actor_notify_clutter (GObject    *obj,
                            GParamSpec *pspec)
{
  CallyActor      *cally_actor = NULL;
  CallyActorClass *klass      = NULL;

  cally_actor = CALLY_ACTOR (clutter_actor_get_accessible (CLUTTER_ACTOR (obj)));
  klass = CALLY_ACTOR_GET_CLASS (cally_actor);

  if (klass->notify_clutter)
    klass->notify_clutter (obj, pspec);
}

/*
 * This function is a signal handler for notify signal which gets emitted
 * when a property changes value on the ClutterActor associated with a CallyActor
 *
 * It constructs an AtkPropertyValues structure and emits a "property_changed"
 * signal which causes the user specified AtkPropertyChangeHandler
 * to be called.
 */
static void
cally_actor_real_notify_clutter (GObject    *obj,
                                GParamSpec *pspec)
{
  ClutterActor* actor   = CLUTTER_ACTOR (obj);
  AtkObject*    atk_obj = clutter_actor_get_accessible (CLUTTER_ACTOR(obj));
  AtkState      state;
  gboolean      value;

  if (g_strcmp0 (pspec->name, "visible") == 0)
    {
      state = ATK_STATE_VISIBLE;
      value = CLUTTER_ACTOR_IS_VISIBLE (actor);
    }
  else if (g_strcmp0 (pspec->name, "reactive") == 0)
    {
      state = ATK_STATE_SENSITIVE;
      value = CLUTTER_ACTOR_IS_REACTIVE (actor);
    }
  else
    return;

  atk_object_notify_state_change (atk_obj, state, value);
}

static void
cally_actor_focus_event (AtkObject   *obj,
                         gboolean    focus_in)
{
  atk_object_notify_state_change (obj, ATK_STATE_FOCUSED, focus_in);
}


static void
_cally_actor_clean_action_list (CallyActor *cally_actor)
{
  CallyActorPrivate *priv = NULL;

  priv = cally_actor->priv;

  if (priv->action_list)
    {
      g_list_foreach (priv->action_list,
                      (GFunc) _cally_actor_destroy_action_info,
                      NULL);
      g_list_free (priv->action_list);
      priv->action_list = NULL;
    }
}

static CallyActorActionInfo *
_cally_actor_get_action_info (CallyActor *cally_actor,
                             gint       index)
{
  CallyActorPrivate *priv = NULL;
  GList            *node = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (cally_actor), NULL);

  priv = cally_actor->priv;

  if (priv->action_list == NULL)
    return NULL;

  node = g_list_nth (priv->action_list, index);

  if (node == NULL)
    return NULL;

  return (CallyActorActionInfo *)(node->data);
}

/**
 * cally_actor_add_action: (skip)
 * @cally_actor: a #CallyActor
 * @action_name: the action name
 * @action_description: the action description
 * @action_keybinding: the action keybinding
 * @action_func: the callback of the action, to be executed with do_action
 *
 * Adds a new action to be accessed with the #AtkAction interface.
 *
 * Return value: added action id, or -1 if failure
 *
 * Since: 1.4
 */
guint
cally_actor_add_action (CallyActor      *cally_actor,
                        const gchar     *action_name,
                        const gchar     *action_description,
                        const gchar     *action_keybinding,
                        CallyActionFunc  action_func)
{
  return cally_actor_add_action_full (cally_actor,
                                      action_name,
                                      action_description,
                                      action_keybinding,
                                      (CallyActionCallback) action_func,
                                      NULL, NULL);
}

/**
 * cally_actor_add_action_full:
 * @cally_actor: a #CallyActor
 * @action_name: the action name
 * @action_description: the action description
 * @action_keybinding: the action keybinding
 * @callback: (scope notified): the callback of the action
 * @user_data: (closure): data to be passed to @callback
 * @notify: function to be called when removing the action
 *
 * Adds a new action to be accessed with the #AtkAction interface.
 *
 * Return value: added action id, or -1 if failure
 *
 * Rename to: cally_actor_add_action
 *
 * Since: 1.6
 */
guint
cally_actor_add_action_full (CallyActor          *cally_actor,
                             const gchar         *action_name,
                             const gchar         *action_description,
                             const gchar         *action_keybinding,
                             CallyActionCallback  callback,
                             gpointer             user_data,
                             GDestroyNotify       notify)
{
  CallyActorActionInfo *info = NULL;
  CallyActorPrivate *priv = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (cally_actor), -1);
  g_return_val_if_fail (callback != NULL, -1);

  priv = cally_actor->priv;

  info = g_slice_new (CallyActorActionInfo);
  info->name = g_strdup (action_name);
  info->description = g_strdup (action_description);
  info->keybinding = g_strdup (action_keybinding);
  info->do_action_func = callback;
  info->user_data = user_data;
  info->notify = notify;

  priv->action_list = g_list_append (priv->action_list, info);

  return g_list_length (priv->action_list);
}

/**
 * cally_actor_remove_action:
 * @cally_actor: a #CallyActor
 * @action_id: the action id
 *
 * Removes a action, using the @action_id returned by cally_actor_add_action()
 *
 * Return value: %TRUE if the operation was succesful, %FALSE otherwise
 *
 * Since: 1.4
 */
gboolean
cally_actor_remove_action (CallyActor *cally_actor,
                           gint        action_id)
{
  GList            *list_node = NULL;
  CallyActorPrivate *priv      = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (cally_actor), FALSE);
  priv = cally_actor->priv;

  list_node = g_list_nth (priv->action_list, action_id - 1);

  if (!list_node)
    return FALSE;

  _cally_actor_destroy_action_info (list_node->data, NULL);

  priv->action_list = g_list_remove_link (priv->action_list, list_node);

  return TRUE;
}

/**
 * cally_actor_remove_action_by_name:
 * @cally_actor: a #CallyActor
 * @action_name: the name of the action to remove
 *
 * Removes an action, using the @action_name used when the action was added
 * with cally_actor_add_action()
 *
 * Return value: %TRUE if the operation was succesful, %FALSE otherwise
 *
 * Since: 1.4
 */
gboolean
cally_actor_remove_action_by_name (CallyActor  *cally_actor,
                                   const gchar *action_name)
{
  GList            *node         = NULL;
  gboolean          action_found = FALSE;
  CallyActorPrivate *priv         = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (cally_actor), FALSE);
  priv = CALLY_ACTOR (cally_actor)->priv;

  for (node = priv->action_list; node && !action_found;
       node = node->next)
    {
      CallyActorActionInfo *ainfo = node->data;

      if (!g_ascii_strcasecmp (ainfo->name, action_name))
	{
	  action_found = TRUE;
	  break;
	}
    }
  if (!action_found)
    return FALSE;

  _cally_actor_destroy_action_info (node->data, NULL);
  priv->action_list = g_list_remove_link (priv->action_list, node);

  return TRUE;
}


static void
_cally_actor_destroy_action_info (gpointer action_info,
                                  gpointer user_data)
{
  CallyActorActionInfo *info = action_info;

  g_assert (info != NULL);

  g_free (info->name);
  g_free (info->description);
  g_free (info->keybinding);

  if (info->notify)
    info->notify (info->user_data);

  g_slice_free (CallyActorActionInfo, info);
}
