/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2008 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
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

/**
 * SECTION:cally-stage
 * @Title: CallyStage
 * @short_description: Implementation of the ATK interfaces for a #ClutterStage
 * @see_also: #ClutterStage
 *
 * #CallyStage implements the required ATK interfaces for #ClutterStage
 *
 * Some implementation details: at this moment #CallyStage is used as
 * the most similar Window object in this toolkit (ie: emitting window
 * related signals), although the real purpose of #ClutterStage is
 * being a canvas. Anyway, this is required for applications using
 * just clutter, or directly #ClutterStage
 */
#include "config.h"

#include "cally-stage.h"
#include "cally-actor-private.h"

/* AtkObject.h */
static void                  cally_stage_real_initialize (AtkObject *obj,
                                                          gpointer   data);
static AtkStateSet*          cally_stage_ref_state_set   (AtkObject *obj);

/* AtkWindow */
static void                  cally_stage_window_interface_init (AtkWindowIface *iface);

/* Auxiliar */
static void                  cally_stage_activate_cb     (ClutterStage *stage,
                                                          gpointer      data);
static void                  cally_stage_deactivate_cb   (ClutterStage *stage,
                                                          gpointer      data);

struct _CallyStagePrivate
{
  /* NULL means that the stage will receive the focus */
  ClutterActor *key_focus;

  gboolean active;
};

G_DEFINE_TYPE_WITH_CODE (CallyStage,
                         cally_stage,
                         CALLY_TYPE_GROUP,
                         G_ADD_PRIVATE (CallyStage)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_WINDOW,
                                                cally_stage_window_interface_init));

static void
cally_stage_class_init (CallyStageClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  /* AtkObject */
  class->initialize = cally_stage_real_initialize;
  class->ref_state_set = cally_stage_ref_state_set;
}

static void
cally_stage_init (CallyStage *cally_stage)
{
  CallyStagePrivate *priv = cally_stage_get_instance_private (cally_stage);

  cally_stage->priv = priv;

  priv->active = FALSE;
}

/**
 * cally_stage_new:
 * @actor: a #ClutterActor
 *
 * Creates a new #CallyStage for the given @actor. @actor should be a
 * #ClutterStage.
 *
 * Return value: the newly created #AtkObject
 *
 * Since: 1.4
 */
AtkObject*
cally_stage_new (ClutterActor *actor)
{
  GObject   *object     = NULL;
  AtkObject *accessible = NULL;

  g_return_val_if_fail (CLUTTER_IS_STAGE (actor), NULL);

  object = g_object_new (CALLY_TYPE_STAGE, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, actor);

  return accessible;
}

static void
cally_stage_notify_key_focus_cb (ClutterStage *stage,
                                 GParamSpec   *pspec,
                                 CallyStage   *self)
{
  ClutterActor *key_focus = NULL;
  AtkObject *new = NULL;

  if (self->priv->active == FALSE)
    return;

  key_focus = clutter_stage_get_key_focus (stage);

  if (key_focus != self->priv->key_focus)
    {
      AtkObject *old = NULL;

      if (self->priv->key_focus != NULL)
        {
          g_object_remove_weak_pointer (G_OBJECT (self->priv->key_focus),
                                        (gpointer *) &self->priv->key_focus);
          old = clutter_actor_get_accessible (self->priv->key_focus);
        }
      else
        old = clutter_actor_get_accessible (CLUTTER_ACTOR (stage));

      atk_object_notify_state_change (old,
                                      ATK_STATE_FOCUSED,
                                      FALSE);
    }

  /* we keep notifying the focus gain without checking previous
   * key-focus to avoid some missing events due timing
   */
  self->priv->key_focus = key_focus;

  if (key_focus != NULL)
    {
      /* ensure that if the key focus goes away, the field inside
       * CallyStage is reset. see bug:
       *
       * https://bugzilla.gnome.org/show_bug.cgi?id=692706
       *
       * we remove the weak pointer above.
       */
      g_object_add_weak_pointer (G_OBJECT (self->priv->key_focus),
                                 (gpointer *) &self->priv->key_focus);

      new = clutter_actor_get_accessible (key_focus);
    }
  else
    new = clutter_actor_get_accessible (CLUTTER_ACTOR (stage));

  atk_object_notify_state_change (new,
                                  ATK_STATE_FOCUSED,
                                  TRUE);
}

static void
cally_stage_real_initialize (AtkObject *obj,
                             gpointer  data)
{
  ClutterStage *stage = NULL;

  g_return_if_fail (CALLY_IS_STAGE (obj));

  ATK_OBJECT_CLASS (cally_stage_parent_class)->initialize (obj, data);

  stage = CLUTTER_STAGE (CALLY_GET_CLUTTER_ACTOR (obj));

  g_signal_connect (stage, "activate", G_CALLBACK (cally_stage_activate_cb), obj);
  g_signal_connect (stage, "deactivate", G_CALLBACK (cally_stage_deactivate_cb), obj);
  g_signal_connect (stage, "notify::key-focus",
                    G_CALLBACK (cally_stage_notify_key_focus_cb), obj);

  atk_object_set_role (obj, ATK_ROLE_WINDOW);
}

static AtkStateSet*
cally_stage_ref_state_set   (AtkObject *obj)
{
  CallyStage   *cally_stage = NULL;
  AtkStateSet  *state_set   = NULL;
  ClutterStage *stage       = NULL;

  g_return_val_if_fail (CALLY_IS_STAGE (obj), NULL);
  cally_stage = CALLY_STAGE (obj);

  state_set = ATK_OBJECT_CLASS (cally_stage_parent_class)->ref_state_set (obj);
  stage = CLUTTER_STAGE (CALLY_GET_CLUTTER_ACTOR (cally_stage));

  if (stage == NULL)
    return state_set;

  if (cally_stage->priv->active)
    atk_state_set_add_state (state_set, ATK_STATE_ACTIVE);

  return state_set;
}

/* AtkWindow */
static void
cally_stage_window_interface_init (AtkWindowIface *iface)
{
  /* At this moment AtkWindow is just about signals */
}

/* Auxiliar */
static void
cally_stage_activate_cb     (ClutterStage *stage,
                             gpointer      data)
{
  CallyStage *cally_stage = NULL;

  g_return_if_fail (CALLY_IS_STAGE (data));

  cally_stage = CALLY_STAGE (data);

  cally_stage->priv->active = TRUE;

  atk_object_notify_state_change (ATK_OBJECT (cally_stage),
                                  ATK_STATE_ACTIVE, TRUE);

  g_signal_emit_by_name (cally_stage, "activate", 0);
}

static void
cally_stage_deactivate_cb   (ClutterStage *stage,
                             gpointer      data)
{
  CallyStage *cally_stage = NULL;

  g_return_if_fail (CALLY_IS_STAGE (data));

  cally_stage = CALLY_STAGE (data);

  cally_stage->priv->active = FALSE;

  atk_object_notify_state_change (ATK_OBJECT (cally_stage),
                                  ATK_STATE_ACTIVE, FALSE);

  g_signal_emit_by_name (cally_stage, "deactivate", 0);
}
