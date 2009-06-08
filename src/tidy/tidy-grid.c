/* tidy-grid.h: Reflowing grid layout container for clutter.
 *
 * Copyright (C) 2008 Intel Corporation
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
 *
 * Written by: Øyvind Kolås <pippin@linux.intel.com>
 */

/* TODO:
 *
 * - Better names for properties.
 * - Caching layouted positions? (perhaps needed for huge collections)
 * - More comments / overall concept on how the layouting is done.
 * - Allow more layout directions than just row major / column major.
 */

#include <string.h>

#include "tidy-grid.h"

typedef struct _TidyGridActorData TidyGridActorData;

static void tidy_grid_dispose             (GObject *object);
static void tidy_grid_finalize            (GObject *object);

static void tidy_grid_finalize            (GObject *object);

static void tidy_grid_set_property        (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec);
static void tidy_grid_get_property        (GObject      *object,
                                           guint         prop_id,
                                           GValue       *value,
                                           GParamSpec   *pspec);

static void clutter_container_iface_init  (ClutterContainerIface *iface);

static void tidy_grid_real_add            (ClutterContainer *container,
                                           ClutterActor     *actor);
static void tidy_grid_real_remove         (ClutterContainer *container,
                                           ClutterActor     *actor);
static void tidy_grid_real_foreach        (ClutterContainer *container,
                                           ClutterCallback   callback,
                                           gpointer          user_data);
static void tidy_grid_real_raise          (ClutterContainer *container,
                                           ClutterActor     *actor,
                                           ClutterActor     *sibling);
static void tidy_grid_real_lower          (ClutterContainer *container,
                                           ClutterActor     *actor,
                                           ClutterActor     *sibling);
static void
tidy_grid_real_sort_depth_order (ClutterContainer *container);

static void
tidy_grid_free_actor_data (gpointer data);

static void tidy_grid_paint (ClutterActor *actor);

static void tidy_grid_pick (ClutterActor *actor,
                                       const ClutterColor *color);

static void
tidy_grid_get_preferred_width (ClutterActor *self,
			       gfloat        for_height,
			       gfloat       *min_width_p,
			       gfloat       *natural_width_p);

static void
tidy_grid_get_preferred_height (ClutterActor *self,
				gfloat        for_width,
				gfloat       *min_height_p,
				gfloat       *natural_height_p);

static void tidy_grid_allocate (ClutterActor          *self,
				const ClutterActorBox *box,
				ClutterAllocationFlags flags);

G_DEFINE_TYPE_WITH_CODE (TidyGrid, tidy_grid,
                         CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init));

#define TIDY_GRID_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_GRID, \
                                TidyGridPrivate))

struct _TidyGridPrivate
{
  gfloat      for_height,  for_width;
  gfloat      pref_width,  pref_height;
  gfloat      alloc_width, alloc_height;

  GHashTable *hash_table;
  GList      *list;

  gboolean    homogenous_rows;
  gboolean    homogenous_columns;
  gboolean    end_align;
  gfloat      column_gap, row_gap;
  gdouble     valign, halign;

  gboolean    column_major;

  gboolean    first_of_batch;
  gfloat      a_current_sum, a_wrap;
  gfloat      max_extent_a;
  gfloat      max_extent_b;
};

enum
{
  PROP_0,
  PROP_HOMOGENOUS_ROWS,
  PROP_HOMOGENOUS_COLUMNS,
  PROP_ROW_GAP,
  PROP_COLUMN_GAP,
  PROP_VALIGN,
  PROP_HALIGN,
  PROP_END_ALIGN,
  PROP_COLUMN_MAJOR,
};

struct _TidyGridActorData
{
  gboolean    xpos_set,   ypos_set;
  gfloat      xpos,       ypos;
  gfloat      pref_width, pref_height;
};

static void
tidy_grid_class_init (TidyGridClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;

  gobject_class->dispose = tidy_grid_dispose;
  gobject_class->finalize = tidy_grid_finalize;

  gobject_class->set_property = tidy_grid_set_property;
  gobject_class->get_property = tidy_grid_get_property;

  actor_class->paint                = tidy_grid_paint;
  actor_class->pick                 = tidy_grid_pick;
  actor_class->get_preferred_width  = tidy_grid_get_preferred_width;
  actor_class->get_preferred_height = tidy_grid_get_preferred_height;
  actor_class->allocate             = tidy_grid_allocate;

  g_type_class_add_private (klass, sizeof (TidyGridPrivate));


  g_object_class_install_property
                   (gobject_class,
                    PROP_ROW_GAP,
                    g_param_spec_float ("row-gap",
					"Row gap",
					"gap between rows in the layout",
					0.0, G_MAXFLOAT,
					0.0,
					G_PARAM_READWRITE|G_PARAM_CONSTRUCT));

  g_object_class_install_property
                   (gobject_class,
                    PROP_COLUMN_GAP,
                    g_param_spec_float ("column-gap",
					"Column gap",
					"gap between columns in the layout",
					0.0, G_MAXFLOAT,
					0.0,
					G_PARAM_READWRITE|G_PARAM_CONSTRUCT));


  g_object_class_install_property
                   (gobject_class,
                    PROP_HOMOGENOUS_ROWS,
                    g_param_spec_boolean ("homogenous-rows",
                                          "homogenous rows",
                                          "Should all rows have the same height?",
                                          FALSE,
                                          G_PARAM_READWRITE|G_PARAM_CONSTRUCT));

  g_object_class_install_property
                   (gobject_class,
                    PROP_HOMOGENOUS_COLUMNS,
                    g_param_spec_boolean ("homogenous-columns",
                                          "homogenous columns",
                                          "Should all columns have the same height?",
                                          FALSE,
                                          G_PARAM_READWRITE|G_PARAM_CONSTRUCT));

  g_object_class_install_property
                   (gobject_class,
                    PROP_COLUMN_MAJOR,
                    g_param_spec_boolean ("column-major",
                                          "column-major",
                                          "Do a column filling first instead of row filling first",
                                          FALSE,
                                          G_PARAM_READWRITE|G_PARAM_CONSTRUCT));

  g_object_class_install_property
                   (gobject_class,
                    PROP_END_ALIGN,
                    g_param_spec_boolean ("end-align",
                                          "end-align",
                                          "Right/bottom aligned rows/columns",
                                          FALSE,
                                          G_PARAM_READWRITE|G_PARAM_CONSTRUCT));

  g_object_class_install_property
                   (gobject_class,
                    PROP_VALIGN,
                    g_param_spec_double ("valign",
                                         "Vertical align",
                                         "Vertical alignment of items within cells",
                                          0.0, 1.0, 0.0,
                                          G_PARAM_READWRITE|G_PARAM_CONSTRUCT));

  g_object_class_install_property
                   (gobject_class,
                    PROP_HALIGN,
                    g_param_spec_double ("halign",
                                         "Horizontal align",
                                         "Horizontal alignment of items within cells",
                                          0.0, 1.0, 0.0,
                                          G_PARAM_READWRITE|G_PARAM_CONSTRUCT));

}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add              = tidy_grid_real_add;
  iface->remove           = tidy_grid_real_remove;
  iface->foreach          = tidy_grid_real_foreach;
  iface->raise            = tidy_grid_real_raise;
  iface->lower            = tidy_grid_real_lower;
  iface->sort_depth_order = tidy_grid_real_sort_depth_order;
}

static void
tidy_grid_init (TidyGrid *self)
{
  TidyGridPrivate *priv;

  self->priv = priv = TIDY_GRID_GET_PRIVATE (self);

  /* do not unref in the hashtable, the reference is for now kept by the list
   * (double bookkeeping sucks)
   */
  priv->hash_table
    = g_hash_table_new_full (g_direct_hash,
                             g_direct_equal,
                             NULL,
                             tidy_grid_free_actor_data);
}

static void
tidy_grid_dispose (GObject *object)
{
  TidyGrid *self = (TidyGrid *) object;
  TidyGridPrivate *priv;
 
  priv = self->priv;

  /* Destroy all of the children. This will cause them to be removed
     from the container and unparented */
  clutter_container_foreach (CLUTTER_CONTAINER (object),
                             (ClutterCallback) clutter_actor_destroy,
                             NULL);

  G_OBJECT_CLASS (tidy_grid_parent_class)->dispose (object);
}

static void
tidy_grid_finalize (GObject *object)
{
  TidyGrid *self = (TidyGrid *) object;
  TidyGridPrivate *priv = self->priv;
  
  g_hash_table_destroy (priv->hash_table);

  G_OBJECT_CLASS (tidy_grid_parent_class)->finalize (object);
}


void
tidy_grid_set_end_align (TidyGrid *self,
                         gboolean  value)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  priv->end_align = value;
  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

gboolean
tidy_grid_get_end_align (TidyGrid *self)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  return priv->end_align;
}

void
tidy_grid_set_homogenous_rows (TidyGrid *self,
                               gboolean  value)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  priv->homogenous_rows = value;
  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

gboolean
tidy_grid_get_homogenous_rows (TidyGrid *self)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  return priv->homogenous_rows;
}


void
tidy_grid_set_homogenous_columns (TidyGrid *self,
                                  gboolean  value)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  priv->homogenous_columns = value;
  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}


gboolean
tidy_grid_get_homogenous_columns (TidyGrid *self)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  return priv->homogenous_columns;
}


void
tidy_grid_set_column_major (TidyGrid *self,
                            gboolean  value)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  priv->column_major = value;
  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

gboolean
tidy_grid_get_column_major (TidyGrid *self)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  return priv->column_major;
}

void
tidy_grid_set_column_gap (TidyGrid    *self,
                          gfloat       value)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  priv->column_gap = value;
  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

gfloat
tidy_grid_get_column_gap (TidyGrid *self)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  return priv->column_gap;
}



void
tidy_grid_set_row_gap (TidyGrid    *self,
                       gfloat       value)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  priv->row_gap = value;
  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

gfloat
tidy_grid_get_row_gap (TidyGrid *self)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  return priv->row_gap;
}


void
tidy_grid_set_valign (TidyGrid *self,
                      gdouble   value)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  priv->valign = value;
  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

gdouble
tidy_grid_get_valign (TidyGrid *self)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  return priv->valign;
}



void
tidy_grid_set_halign (TidyGrid *self,
                      gdouble   value)
                                 
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  priv->halign = value;
  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

gdouble
tidy_grid_get_halign (TidyGrid *self)
{
  TidyGridPrivate *priv = TIDY_GRID_GET_PRIVATE (self);
  return priv->halign;
}


static void
tidy_grid_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  TidyGrid *grid = TIDY_GRID (object);

  TidyGridPrivate *priv;

  priv = TIDY_GRID_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_END_ALIGN:
      tidy_grid_set_end_align (grid, g_value_get_boolean (value));
      break;
    case PROP_HOMOGENOUS_ROWS:
      tidy_grid_set_homogenous_rows (grid, g_value_get_boolean (value));
      break;
    case PROP_HOMOGENOUS_COLUMNS:
      tidy_grid_set_homogenous_columns (grid, g_value_get_boolean (value));
      break;
    case PROP_COLUMN_MAJOR:
      tidy_grid_set_column_major (grid, g_value_get_boolean (value));
      break;
    case PROP_COLUMN_GAP:
      tidy_grid_set_column_gap (grid, g_value_get_float (value));
      break;
    case PROP_ROW_GAP:
      tidy_grid_set_row_gap (grid, g_value_get_float (value));
      break;
    case PROP_VALIGN:
      tidy_grid_set_valign (grid, g_value_get_double (value));
      break;
    case PROP_HALIGN:
      tidy_grid_set_halign (grid, g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
tidy_grid_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  TidyGrid *grid = TIDY_GRID (object);

  TidyGridPrivate *priv;

  priv = TIDY_GRID_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_HOMOGENOUS_ROWS:
      g_value_set_boolean (value, tidy_grid_get_homogenous_rows (grid));
      break;
    case PROP_HOMOGENOUS_COLUMNS:
      g_value_set_boolean (value, tidy_grid_get_homogenous_columns (grid));
      break;
    case PROP_END_ALIGN:
      g_value_set_boolean (value, tidy_grid_get_end_align (grid));
      break;
    case PROP_COLUMN_MAJOR:
      g_value_set_boolean (value, tidy_grid_get_column_major (grid));
      break;
    case PROP_COLUMN_GAP:
      g_value_set_float (value, tidy_grid_get_column_gap (grid));
      break;
    case PROP_ROW_GAP:
      g_value_set_float (value, tidy_grid_get_row_gap (grid));
      break;
    case PROP_VALIGN:
      g_value_set_double (value, tidy_grid_get_valign (grid));
      break;
    case PROP_HALIGN:
      g_value_set_double (value, tidy_grid_get_halign (grid));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
tidy_grid_free_actor_data (gpointer data)
{
  g_slice_free (TidyGridActorData, data);
}

ClutterActor *
tidy_grid_new (void)
{
  ClutterActor *self = g_object_new (TIDY_TYPE_GRID, NULL);

  return self;
}

static void
tidy_grid_real_add (ClutterContainer *container,
                    ClutterActor     *actor)
{
  TidyGridPrivate *priv;
  TidyGridActorData *data;

  g_return_if_fail (TIDY_IS_GRID (container));

  priv = TIDY_GRID (container)->priv;

  g_object_ref (actor);

  clutter_actor_set_parent (actor, CLUTTER_ACTOR (container));

  data = g_slice_alloc0 (sizeof (TidyGridActorData));

  priv->list = g_list_append (priv->list, actor);
  g_hash_table_insert (priv->hash_table, actor, data);

  g_signal_emit_by_name (container, "actor-added", actor);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));

  g_object_unref (actor);
}

static void
tidy_grid_real_remove (ClutterContainer *container,
                       ClutterActor     *actor)
{
  TidyGrid *layout = TIDY_GRID (container);
  TidyGridPrivate *priv = layout->priv;

  g_object_ref (actor);
  
  if (g_hash_table_remove (priv->hash_table, actor))
    {
      clutter_actor_unparent (actor);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (layout));

      g_signal_emit_by_name (container, "actor-removed", actor);

      if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (layout)))
        clutter_actor_queue_redraw (CLUTTER_ACTOR (layout));
    }
  priv->list = g_list_remove (priv->list, actor);

  g_object_unref (actor);
}

static void
tidy_grid_real_foreach (ClutterContainer *container,
                                   ClutterCallback callback,
                                   gpointer user_data)
{
  TidyGrid *layout = TIDY_GRID (container);
  TidyGridPrivate *priv = layout->priv;

  g_list_foreach (priv->list, (GFunc) callback, user_data);
}

static void
tidy_grid_real_raise (ClutterContainer *container,
                                 ClutterActor *actor,
                                 ClutterActor *sibling)
{
  /* STUB */
}

static void
tidy_grid_real_lower (ClutterContainer *container,
                                 ClutterActor *actor,
                                 ClutterActor *sibling)
{
  /* STUB */
}

static void
tidy_grid_real_sort_depth_order (ClutterContainer *container)
{
  /* STUB */
}

static void
tidy_grid_paint (ClutterActor *actor)
{
  TidyGrid *layout = (TidyGrid *) actor;
  TidyGridPrivate *priv = layout->priv;
  GList *child_item;

  for (child_item = priv->list;
       child_item != NULL;
       child_item = child_item->next)
    {
      ClutterActor *child = child_item->data;

      g_assert (child != NULL);

      if (CLUTTER_ACTOR_IS_VISIBLE (child))
        clutter_actor_paint (child);
    }

}

static void
tidy_grid_pick (ClutterActor *actor,
                           const ClutterColor *color)
{
  /* Chain up so we get a bounding box pained (if we are reactive) */
  CLUTTER_ACTOR_CLASS (tidy_grid_parent_class)->pick (actor, color);

  /* Just forward to the paint call which in turn will trigger
   * the child actors also getting 'picked'.
   */
  if (CLUTTER_ACTOR_IS_VISIBLE (actor))
   tidy_grid_paint (actor);
}

static void
tidy_grid_get_preferred_width (ClutterActor *self,
                                          gfloat      for_height,
                                          gfloat      *min_width_p,
                                          gfloat      *natural_width_p)
{
  TidyGrid *layout = (TidyGrid *) self;
  TidyGridPrivate *priv = layout->priv;
  gfloat natural_width;

  natural_width = 200.0;
  if (min_width_p)
    *min_width_p = natural_width;
  if (natural_width_p)
    *natural_width_p = natural_width;

  priv->pref_width = natural_width;
}

static void
tidy_grid_get_preferred_height (ClutterActor *self,
                                gfloat      for_width,
                                gfloat      *min_height_p,
                                gfloat      *natural_height_p)
{
  TidyGrid *layout = (TidyGrid *) self;
  TidyGridPrivate *priv = layout->priv;
  gfloat natural_height;

  natural_height = 200.0;

  priv->for_width = for_width;
  priv->pref_height = natural_height;

  if (min_height_p)
    *min_height_p = natural_height;
  if (natural_height_p)
    *natural_height_p = natural_height;
}

static gfloat
compute_row_height (GList                    *siblings,
                    gfloat                    best_yet,
                    gfloat                    current_a,
                    TidyGridPrivate *priv)
{
  GList *l;

  gboolean homogenous_a;
  gboolean homogenous_b;
  gfloat gap;

  if (priv->column_major)
    {
      homogenous_b = priv->homogenous_columns;
      homogenous_a = priv->homogenous_rows;
      gap          = priv->row_gap;
    }
  else
    {
      homogenous_a = priv->homogenous_columns;
      homogenous_b = priv->homogenous_rows;
      gap          = priv->column_gap;
    }

  for (l = siblings; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      gfloat natural_width, natural_height;

      /* each child will get as much space as they require */
      clutter_actor_get_preferred_size (CLUTTER_ACTOR (child),
                                        NULL, NULL,
                                        &natural_width, &natural_height);

      if (priv->column_major)
        {
          gfloat temp = natural_height;
          natural_height = natural_width;
          natural_width = temp;
        }

      /* if the primary axis is homogenous, each additional item is the same
       * width */
      if (homogenous_a)
        natural_width = priv->max_extent_a; 

      if (natural_height > best_yet)
        best_yet = natural_height;

      /* if the child is overflowing, we wrap to next line */
      if (current_a + natural_width + gap > priv->a_wrap)
        {
          return best_yet;
        }
      current_a += natural_width + gap;
    }
  return best_yet;
}




static gfloat
compute_row_start (GList           *siblings,
                   gfloat           start_x,
                   TidyGridPrivate *priv)
{
  gfloat current_a = start_x;
  GList *l;

  gboolean homogenous_a;
  gboolean homogenous_b;
  gfloat gap;

  if (priv->column_major)
    {
      homogenous_b = priv->homogenous_columns;
      homogenous_a = priv->homogenous_rows;
      gap          = priv->row_gap;
    }
  else
    {
      homogenous_a = priv->homogenous_columns;
      homogenous_b = priv->homogenous_rows;
      gap          = priv->column_gap;
    }

  for (l = siblings; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      gfloat natural_width, natural_height;

      /* each child will get as much space as they require */
      clutter_actor_get_preferred_size (CLUTTER_ACTOR (child),
                                        NULL, NULL,
                                        &natural_width, &natural_height);


      if (priv->column_major)
        natural_width = natural_height;

      /* if the primary axis is homogenous, each additional item is the same width */
      if (homogenous_a)
        natural_width = priv->max_extent_a; 

      /* if the child is overflowing, we wrap to next line */
      if (current_a + natural_width + gap > priv->a_wrap)
        {
          if (current_a == start_x)
            return start_x;
          return (priv->a_wrap - current_a);
        }
      current_a += natural_width + gap;
    }
  return (priv->a_wrap - current_a);
}

static void
tidy_grid_allocate (ClutterActor          *self,
		    const ClutterActorBox *box,
		    ClutterAllocationFlags flags)
{
  TidyGrid *layout = (TidyGrid *) self;
  TidyGridPrivate *priv = layout->priv;

  gfloat current_a;
  gfloat current_b;
  gfloat next_b;
  gfloat agap;
  gfloat bgap;

  gboolean homogenous_a;
  gboolean homogenous_b;
  gdouble  aalign;
  gdouble  balign;
 
  current_a = current_b = next_b = 0;

  GList *iter;

  /* chain up to set actor->allocation */
  CLUTTER_ACTOR_CLASS (tidy_grid_parent_class)
    ->allocate (self, box, flags);

  priv->alloc_width = box->x2 - box->x1;
  priv->alloc_height = box->y2 - box->y1;

  /* Make sure we have calculated the preferred size */
  /* what does this do? */
  clutter_actor_get_preferred_size (self, NULL, NULL, NULL, NULL);


  if (priv->column_major)
    {
      priv->a_wrap = priv->alloc_height;
      homogenous_b = priv->homogenous_columns;
      homogenous_a = priv->homogenous_rows;
      aalign = priv->valign;
      balign = priv->halign;
      agap          = priv->row_gap;
      bgap          = priv->column_gap;
    }
  else
    {
      priv->a_wrap = priv->alloc_width;
      homogenous_a = priv->homogenous_columns;
      homogenous_b = priv->homogenous_rows;
      aalign = priv->halign;
      balign = priv->valign;
      agap          = priv->column_gap;
      bgap          = priv->row_gap;
    }

  priv->max_extent_a = 0;
  priv->max_extent_b = 0;

  priv->first_of_batch = TRUE;
  
  if (homogenous_a ||
      homogenous_b)
    {
      for (iter = priv->list; iter; iter = iter->next)
        {
          ClutterActor *child = iter->data;
          gfloat natural_width;
          gfloat natural_height;

          /* each child will get as much space as they require */
          clutter_actor_get_preferred_size (CLUTTER_ACTOR (child),
                                            NULL, NULL,
                                            &natural_width, &natural_height);
          if (natural_width > priv->max_extent_a)
            priv->max_extent_a = natural_width;
          if (natural_height > priv->max_extent_b)
            priv->max_extent_b = natural_width;
        }
    }

  if (priv->column_major)
    {
      gfloat temp = priv->max_extent_a;
      priv->max_extent_a = priv->max_extent_b;
      priv->max_extent_b = temp;
    }

  for (iter = priv->list; iter; iter=iter->next)
    {
      ClutterActor *child = iter->data;
      gfloat natural_a;
      gfloat natural_b;

      /* each child will get as much space as they require */
      clutter_actor_get_preferred_size (CLUTTER_ACTOR (child),
                                        NULL, NULL,
                                        &natural_a, &natural_b);

      if (priv->column_major) /* swap axes around if column is major */
        {
          gfloat temp = natural_a;
          natural_a = natural_b;
          natural_b = temp;
        }

      /* if the child is overflowing, we wrap to next line */
      if (current_a + natural_a > priv->a_wrap ||
          (homogenous_a && current_a + priv->max_extent_a > priv->a_wrap))
        {
          current_b = next_b + bgap;
          current_a = 0;
          next_b = current_b + bgap;
          priv->first_of_batch = TRUE;
        }

      if (priv->end_align &&
          priv->first_of_batch)
        {
          current_a = compute_row_start (iter, current_a, priv);
          priv->first_of_batch = FALSE;
        }

      if (next_b-current_b < natural_b)
          next_b = current_b + natural_b;

        {
          gfloat          row_height;
          ClutterActorBox child_box;

          if (homogenous_b)
            {
              row_height = priv->max_extent_b;
            }
          else
            {
              row_height = compute_row_height (iter, next_b-current_b,
                                               current_a, priv);
            }

          if (homogenous_a)
            {
              child_box.x1 = current_a + (priv->max_extent_a-natural_a) * aalign;
              child_box.x2 = child_box.x1 + natural_a;

            }
          else
            {
              child_box.x1 = current_a;
              child_box.x2 = child_box.x1 + natural_a;
            }

          child_box.y1 = current_b + (row_height-natural_b) * balign;
          child_box.y2 = child_box.y1 + natural_b;


          if (priv->column_major)
            {
              gfloat temp = child_box.x1;
              child_box.x1 = child_box.y1;
              child_box.y1 = temp;

              temp = child_box.x2;
              child_box.x2 = child_box.y2;
              child_box.y2 = temp;
            }

          /* update the allocation */
          clutter_actor_allocate (CLUTTER_ACTOR (child),
                                  &child_box,
                                  flags);

          if (homogenous_a)
            {
              current_a += priv->max_extent_a + agap;
            }
          else
            {
              current_a += natural_a + agap;
            }
        }
    }
}
