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

#include "cally-stage.h"
#include "cally-actor-private.h"

enum {
  ACTIVATE,
  CREATE,
  DEACTIVATE,
  DESTROY,
  LAST_SIGNAL
};

static guint cally_stage_signals [LAST_SIGNAL] = { 0, };

static void cally_stage_class_init (CallyStageClass *klass);
static void cally_stage_init       (CallyStage      *stage);

/* AtkObject.h */
static void                  cally_stage_real_initialize (AtkObject *obj,
                                                          gpointer   data);
static AtkStateSet*          cally_stage_ref_state_set   (AtkObject *obj);

/* Auxiliar */
static void                  cally_stage_activate_cb     (ClutterStage *stage,
                                                          gpointer      data);
static void                  cally_stage_deactivate_cb   (ClutterStage *stage,
                                                          gpointer      data);


G_DEFINE_TYPE (CallyStage, cally_stage, CALLY_TYPE_GROUP);

#define CALLY_STAGE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CALLY_TYPE_STAGE, CallyStagePrivate))

struct _CallyStagePrivate
{
  gboolean active;
};

static void
cally_stage_class_init (CallyStageClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class         = ATK_OBJECT_CLASS (klass);
/*   CallyActorClass *cally_class  = CALLY_ACTOR_CLASS (klass); */

  /* AtkObject */
  class->initialize = cally_stage_real_initialize;
  class->ref_state_set = cally_stage_ref_state_set;

  g_type_class_add_private (gobject_class, sizeof (CallyStagePrivate));

  /**
   * CallyStage::activate:
   * @cally_actor: the object which received the signal
   *
   * The ::activate signal is emitted when the stage receives the key
   * focus from the underlying window system.
   *
   * Toolkit implementation note: it is used when anyone adds a global
   * event listener to "window:activate"
   *
   * Since: 1.4
   */
  cally_stage_signals [ACTIVATE] =
    g_signal_new ("activate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * CallyStage::create:
   * @cally_actor: the object which received the signal
   *
   * The ::create signal is emitted when the stage is created.
   *
   * Toolkit implementation note: it is used when anyone adds a global
   * event listener to "window:create"
   *
   * Since: 1.4
   */
  cally_stage_signals [CREATE] =
    g_signal_new ("create",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * CallyStage::deactivate:
   * @cally_actor: the object which received the signal
   *
   * The ::deactivate signal is emitted when the stage loses key focus
   * from the underlying window system.
   *
   * Toolkit implementation note: it is used when anyone adds a global
   * event listener to "window:deactivate"
   *
   * Since: 1.4
   */
  cally_stage_signals [DEACTIVATE] =
    g_signal_new ("deactivate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * CallyStage::destroy:
   * @cally_actor: the object which received the signal
   *
   * The ::destroy signal is emitted when the stage is destroyed.
   *
   * Toolkit implementation note: it is used when anyone adds a global
   * event listener to "window:destroy"
   *
   * Since: 1.4
   */
  cally_stage_signals [DESTROY] =
    g_signal_new ("destroy",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
cally_stage_init (CallyStage *cally_stage)
{
  CallyStagePrivate *priv = CALLY_STAGE_GET_PRIVATE (cally_stage);

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
cally_stage_real_initialize (AtkObject *obj,
                             gpointer  data)
{
  ClutterStage *stage = NULL;

  g_return_if_fail (CALLY_IS_STAGE (obj));

  ATK_OBJECT_CLASS (cally_stage_parent_class)->initialize (obj, data);

  stage = CLUTTER_STAGE (CALLY_GET_CLUTTER_ACTOR (obj));

  g_signal_connect (stage, "activate", G_CALLBACK (cally_stage_activate_cb), obj);
  g_signal_connect (stage, "deactivate", G_CALLBACK (cally_stage_deactivate_cb), obj);

  obj->role = ATK_ROLE_CANVAS;
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

  g_signal_emit (cally_stage, cally_stage_signals [ACTIVATE], 0);
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

  g_signal_emit (cally_stage, cally_stage_signals [DEACTIVATE], 0);
}
