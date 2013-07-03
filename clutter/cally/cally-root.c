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
 * SECTION:cally-root
 * @short_description: Root object for the Cally toolkit
 * @see_also: #ClutterStage
 *
 * #CallyRoot is the root object of the accessibility tree-like
 * hierarchy, exposing the application level.
 *
 * Somewhat equivalent to #GailTopLevel. We consider that this class
 * expose the a11y information of the #ClutterStageManager, as the
 * children of this object are the different ClutterStage managed (so
 * the #GObject used in the atk_object_initialize() is the
 * #ClutterStageManager).
 */

#include <clutter/clutter.h>
#include "cally-root.h"

/* GObject */
static void cally_root_finalize   (GObject *object);

/* AtkObject.h */
static void             cally_root_initialize           (AtkObject *accessible,
                                                         gpointer   data);
static gint             cally_root_get_n_children       (AtkObject *obj);
static AtkObject *      cally_root_ref_child            (AtkObject *obj,
                                                         gint i);
static AtkObject *      cally_root_get_parent           (AtkObject *obj);
static const char *     cally_root_get_name             (AtkObject *obj);

/* Private */
static void             cally_util_stage_added_cb       (ClutterStageManager *stage_manager,
                                                         ClutterStage *stage,
                                                         gpointer data);
static void             cally_util_stage_removed_cb     (ClutterStageManager *stage_manager,
                                                         ClutterStage *stage,
                                                         gpointer data);

struct _CallyRootPrivate
{
/* We save the CallyStage objects. Other option could save the stage
 * list, and then just get the a11y object on the ref_child, etc. But
 * the ref_child is more common that the init and the stage-add,
 * stage-remove, so we avoid getting the accessible object
 * constantly
 */
  GSList *stage_list;

  /* signals id */
  guint stage_added_id;
  guint stage_removed_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (CallyRoot, cally_root,  ATK_TYPE_GOBJECT_ACCESSIBLE)

static void
cally_root_class_init (CallyRootClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  gobject_class->finalize = cally_root_finalize;

  /* AtkObject */
  class->get_n_children = cally_root_get_n_children;
  class->ref_child = cally_root_ref_child;
  class->get_parent = cally_root_get_parent;
  class->initialize = cally_root_initialize;
  class->get_name = cally_root_get_name;
}

static void
cally_root_init (CallyRoot *root)
{
  root->priv = cally_root_get_instance_private (root);

  root->priv->stage_list = NULL;
  root->priv->stage_added_id = 0;
  root->priv->stage_removed_id = 0;
}

/**
 * cally_root_new:
 *
 * Creates a new #CallyRoot object.
 *
 * Return value: the newly created #AtkObject
 *
 * Since: 1.4
 */
AtkObject*
cally_root_new (void)
{
  GObject *object = NULL;
  AtkObject *accessible = NULL;
  ClutterStageManager *stage_manager = NULL;

  object = g_object_new (CALLY_TYPE_ROOT, NULL);

  accessible = ATK_OBJECT (object);
  stage_manager = clutter_stage_manager_get_default ();

  atk_object_initialize (accessible, stage_manager);

  return accessible;
}

static void
cally_root_finalize (GObject *object)
{
  CallyRoot *root = CALLY_ROOT (object);
  GObject *stage_manager = NULL;

  g_return_if_fail (CALLY_IS_ROOT (object));

  if (root->priv->stage_list)
    {
      g_slist_free (root->priv->stage_list);
      root->priv->stage_list = NULL;
    }

  stage_manager = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (root));

  g_signal_handler_disconnect (stage_manager,
                               root->priv->stage_added_id);

  g_signal_handler_disconnect (stage_manager,
                               root->priv->stage_added_id);

  G_OBJECT_CLASS (cally_root_parent_class)->finalize (object);
}

/* AtkObject.h */
static void
cally_root_initialize (AtkObject              *accessible,
                       gpointer                data)
{
  ClutterStageManager *stage_manager = NULL;
  const GSList        *iter          = NULL;
  const GSList        *stage_list    = NULL;
  ClutterStage        *clutter_stage = NULL;
  AtkObject           *cally_stage   = NULL;
  CallyRoot           *root          = NULL;

  accessible->role = ATK_ROLE_APPLICATION;
  accessible->accessible_parent = NULL;

  /* children initialization */
  root = CALLY_ROOT (accessible);
  stage_manager = CLUTTER_STAGE_MANAGER (data);
  stage_list = clutter_stage_manager_peek_stages (stage_manager);

  for (iter = stage_list; iter != NULL; iter = g_slist_next (iter))
    {
      clutter_stage = CLUTTER_STAGE (iter->data);
      cally_stage = clutter_actor_get_accessible (CLUTTER_ACTOR (clutter_stage));

      atk_object_set_parent (cally_stage, ATK_OBJECT (root));

      root->priv->stage_list = g_slist_append (root->priv->stage_list,
                                               cally_stage);
    }

  root->priv->stage_added_id =
    g_signal_connect (G_OBJECT (stage_manager), "stage-added",
                      G_CALLBACK (cally_util_stage_added_cb), root);

  root->priv->stage_removed_id =
    g_signal_connect (G_OBJECT (stage_manager), "stage-removed",
                      G_CALLBACK (cally_util_stage_removed_cb), root);

  ATK_OBJECT_CLASS (cally_root_parent_class)->initialize (accessible, data);
}


static gint
cally_root_get_n_children (AtkObject *obj)
{
  CallyRoot *root = CALLY_ROOT (obj);

  return g_slist_length (root->priv->stage_list);
}

static AtkObject*
cally_root_ref_child (AtkObject *obj,
                     gint i)
{
  CallyRoot *cally_root = NULL;
  GSList *stage_list = NULL;
  gint num = 0;
  AtkObject *item = NULL;

  cally_root = CALLY_ROOT (obj);
  stage_list = cally_root->priv->stage_list;
  num = g_slist_length (stage_list);

  g_return_val_if_fail ((i < num)&&(i >= 0), NULL);

  item = g_slist_nth_data (stage_list, i);
  if (!item)
    {
      return NULL;
    }

  g_object_ref (item);

  return item;
}

static AtkObject*
cally_root_get_parent (AtkObject *obj)
{
  return NULL;
}

static const char *
cally_root_get_name (AtkObject *obj)
{
  return g_get_prgname ();
}

/* -------------------------------- PRIVATE --------------------------------- */

static void
cally_util_stage_added_cb (ClutterStageManager *stage_manager,
                           ClutterStage *stage,
                           gpointer data)
{
  CallyRoot *root = CALLY_ROOT (data);
  AtkObject *cally_stage = NULL;
  gint index = -1;

  cally_stage = clutter_actor_get_accessible (CLUTTER_ACTOR (stage));

  atk_object_set_parent (cally_stage, ATK_OBJECT (root));

  root->priv->stage_list = g_slist_append (root->priv->stage_list,
                                           cally_stage);

  index = g_slist_index (root->priv->stage_list, cally_stage);
  g_signal_emit_by_name (root, "children_changed::add",
                         index, cally_stage, NULL);
  g_signal_emit_by_name (cally_stage, "create", 0);
}

static void
cally_util_stage_removed_cb (ClutterStageManager *stage_manager,
                             ClutterStage *stage,
                             gpointer data)
{
  CallyRoot *root = CALLY_ROOT (data);
  AtkObject *cally_stage = NULL;
  gint index = -1;

  cally_stage = clutter_actor_get_accessible (CLUTTER_ACTOR (stage));

  index = g_slist_index (root->priv->stage_list, cally_stage);

  root->priv->stage_list = g_slist_remove (root->priv->stage_list,
                                           cally_stage);

  index = g_slist_index (root->priv->stage_list, cally_stage);
  g_signal_emit_by_name (root, "children_changed::remove",
                         index, cally_stage, NULL);
  g_signal_emit_by_name (cally_stage, "destroy", 0);
}
