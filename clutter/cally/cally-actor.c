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
 * grab focus).
 *
 * In the same way, we will manage the focus state change management
 * on the cally-stage object. The reason is avoid missing a focus
 * state change event if the object is focused just before the
 * accessibility object being created.
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

static void cally_actor_initialize (AtkObject *obj,
                                   gpointer   data);
static void cally_actor_finalize   (GObject *obj);

/* AtkObject.h */
static AtkObject*            cally_actor_get_parent          (AtkObject *obj);
static gint                  cally_actor_get_index_in_parent (AtkObject *obj);
static AtkStateSet*          cally_actor_ref_state_set       (AtkObject *obj);
static gint                  cally_actor_get_n_children      (AtkObject *obj);
static AtkObject*            cally_actor_ref_child           (AtkObject *obj,
                                                             gint       i);
static AtkAttributeSet *     cally_actor_get_attributes      (AtkObject *obj);

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

struct _CallyActorPrivate
{
  GQueue *action_queue;
  guint   action_idle_handler;
  GList  *action_list;

  GList *children;
};

G_DEFINE_TYPE_WITH_CODE (CallyActor,
                         cally_actor,
                         ATK_TYPE_GOBJECT_ACCESSIBLE,
                         G_ADD_PRIVATE (CallyActor)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT,
                                                cally_actor_component_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION,
                                                cally_actor_action_interface_init));

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

  g_signal_connect (actor,
                    "notify",
                    G_CALLBACK (cally_actor_notify_clutter),
                    NULL);

  g_object_set_data (G_OBJECT (obj), "atk-component-layer",
                     GINT_TO_POINTER (ATK_LAYER_MDI));

  priv->children = clutter_actor_get_children (actor);

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

static void
cally_actor_class_init (CallyActorClass *klass)
{
  AtkObjectClass *class         = ATK_OBJECT_CLASS (klass);
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);

  klass->notify_clutter = cally_actor_real_notify_clutter;
  klass->add_actor      = cally_actor_real_add_actor;
  klass->remove_actor   = cally_actor_real_remove_actor;

  /* GObject */
  gobject_class->finalize = cally_actor_finalize;

  /* AtkObject */
  class->get_parent          = cally_actor_get_parent;
  class->get_index_in_parent = cally_actor_get_index_in_parent;
  class->ref_state_set       = cally_actor_ref_state_set;
  class->initialize          = cally_actor_initialize;
  class->get_n_children      = cally_actor_get_n_children;
  class->ref_child           = cally_actor_ref_child;
  class->get_attributes      = cally_actor_get_attributes;
}

static void
cally_actor_init (CallyActor *cally_actor)
{
  CallyActorPrivate *priv = cally_actor_get_instance_private (cally_actor);

  cally_actor->priv = priv;

  priv->action_queue = NULL;
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
  CallyActor *cally_actor = NULL;
  ClutterActor *actor = NULL;
  ClutterActor *parent_actor = NULL;
  ClutterActor *iter;
  gint index = -1;

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

  index = 0;
  parent_actor = clutter_actor_get_parent (actor);
  if (parent_actor == NULL)
    return -1;

  for (iter = clutter_actor_get_first_child (parent_actor);
       iter != NULL && iter != actor;
       iter = clutter_actor_get_next_sibling (iter))
    {
      index += 1;
    }

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

          /* It would be good to also check if the actor is on screen,
             like the old and removed clutter_actor_is_on_stage*/
          if (clutter_actor_get_paint_visibility (actor))
            atk_state_set_add_state (state_set, ATK_STATE_SHOWING);

        }

      /* See focus section on implementation notes */
      atk_state_set_add_state (state_set, ATK_STATE_FOCUSABLE);

      stage = CLUTTER_STAGE (clutter_actor_get_stage (actor));
      if (stage != NULL)
        {
          focus_actor = clutter_stage_get_key_focus (stage);
          if (focus_actor == actor)
            atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
        }
    }

  return state_set;
}

static gint
cally_actor_get_n_children (AtkObject *obj)
{
  ClutterActor *actor = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (obj), 0);

  actor = CALLY_GET_CLUTTER_ACTOR (obj);

  if (actor == NULL) /* State is defunct */
    return 0;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), 0);

  return clutter_actor_get_n_children (actor);
}

static AtkObject*
cally_actor_ref_child (AtkObject *obj,
                       gint       i)
{
  ClutterActor *actor = NULL;
  ClutterActor *child = NULL;

  g_return_val_if_fail (CALLY_IS_ACTOR (obj), NULL);

  actor = CALLY_GET_CLUTTER_ACTOR (obj);
  if (actor == NULL) /* State is defunct */
    return NULL;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);

  if (i >= clutter_actor_get_n_children (actor))
    return NULL;

  child = clutter_actor_get_child_at_index (actor, i);
  if (child == NULL)
    return NULL;

  return g_object_ref (clutter_actor_get_accessible (child));
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

  priv->children = clutter_actor_get_children (CLUTTER_ACTOR (container));

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

  priv->children = clutter_actor_get_children (CLUTTER_ACTOR (container));

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
  ClutterActor  *stage = NULL;

  g_return_if_fail (CALLY_IS_ACTOR (component));

  cally_actor = CALLY_ACTOR (component);
  actor = CALLY_GET_CLUTTER_ACTOR (cally_actor);

  if (actor == NULL) /* actor is defunct */
    return;

  /* If the actor is not placed in any stage, we can't compute the
   * extents */
  stage = clutter_actor_get_stage (actor);
  if (stage == NULL)
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

  return clutter_actor_get_z_position (actor);
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
  else if (g_strcmp0 (pspec->name, "mapped") == 0)
    {
      state = ATK_STATE_SHOWING;
      value = CLUTTER_ACTOR_IS_MAPPED (actor);
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
