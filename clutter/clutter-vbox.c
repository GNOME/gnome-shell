#include "config.h"

#include "clutter-vbox.h"

#include "clutter-box.h"
#include "clutter-container.h"
#include "clutter-layout.h"
#include "clutter-debug.h"
#include "clutter-units.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-private.h"

/**
 * SECTION:clutter-vbox
 * @short_description: Simple horizontal box
 *
 * FIXME
 *
 * #ClutterVBox is available since Clutter 0.4.
 */

enum
{
  PROP_0,

  PROP_LAYOUT_FLAGS
};

static void clutter_layout_iface_init (ClutterLayoutIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterVBox,
                         clutter_vbox,
                         CLUTTER_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_LAYOUT,
                                                clutter_layout_iface_init));

static void
clutter_vbox_query_coords (ClutterActor    *actor,
                           ClutterActorBox *coords)
{
  ClutterBox *box = CLUTTER_BOX (actor);
  ClutterMargin box_margin;
  GList *l;
  gint width, height;

  if (box->allocation.x2 != -1 && box->allocation.y2 != -1)
    {
      coords->x2 = box->allocation.x2;
      coords->y2 = box->allocation.y2;
      return;
    }
  
  clutter_box_get_margin (box, &box_margin);

  width = CLUTTER_UNITS_TO_INT (box_margin.left);
  height = CLUTTER_UNITS_TO_INT (box_margin.top);

  for (l = box->children; l; l = l->next)
    {
      ClutterBoxChild *child = l->data;
      
      if (CLUTTER_ACTOR_IS_VISIBLE (child->actor))
        {
          guint child_width, child_height;

          clutter_actor_get_size (child->actor, &child_width, &child_height);

          height = height
                   + CLUTTER_UNITS_TO_INT (child->padding.top)
                   + child_height
                   + CLUTTER_UNITS_TO_INT (child->padding.bottom);

          width = MAX ((child_width
                         + CLUTTER_UNITS_TO_INT (child->padding.left)
                         + CLUTTER_UNITS_TO_INT (child->padding.right)),
                        width);
        }
    }

  width += CLUTTER_UNITS_TO_INT (box_margin.right);
  height += CLUTTER_UNITS_TO_INT (box_margin.bottom);

  box->allocation.x2 = coords->x2 = 
        coords->x1 + CLUTTER_UNITS_FROM_INT (width);
  box->allocation.y2 = coords->y2 = 
        coords->y1 + CLUTTER_UNITS_FROM_INT (height);
}

static void
clutter_vbox_request_coords (ClutterActor    *actor,
                             ClutterActorBox *coords)
{
  ClutterBox *box = CLUTTER_BOX (actor);

  /* we reset the allocation here */
  box->allocation.x1 = coords->x1;
  box->allocation.y1 = coords->y1;
  box->allocation.x2 = -1;
  box->allocation.y2 = -1;
}

static void
clutter_vbox_pack_child (ClutterBox      *box,
                         ClutterBoxChild *child)
{
  ClutterGeometry box_geom, child_geom;
  ClutterMargin box_margin;

  /* reset the saved allocation */
  box->allocation.x2 = box->allocation.y2 = -1;

  clutter_actor_get_geometry (CLUTTER_ACTOR (box), &box_geom);
  clutter_actor_get_geometry (child->actor, &child_geom);

  clutter_box_get_margin (box, &box_margin);
  
  if (child->pack_type == CLUTTER_PACK_START)
    {
      child_geom.x = CLUTTER_UNITS_TO_INT (child->padding.left);
      child_geom.y = box_geom.height
                     + CLUTTER_UNITS_TO_INT (child->padding.top);
    }
  else if (child->pack_type == CLUTTER_PACK_END)
    {
      child_geom.x = CLUTTER_UNITS_TO_INT (child->padding.left);
      child_geom.y = box_geom.height - child_geom.height
                     - CLUTTER_UNITS_TO_INT (child->padding.bottom);
    }

  child->child_coords.x1 = CLUTTER_UNITS_FROM_INT (child_geom.x);
  child->child_coords.y1 = CLUTTER_UNITS_FROM_INT (child_geom.y);
  child->child_coords.x2 = CLUTTER_UNITS_FROM_INT (child_geom.x)
                           + CLUTTER_UNITS_FROM_INT (child_geom.width);
  child->child_coords.y2 = CLUTTER_UNITS_FROM_INT (child_geom.y)
                           + CLUTTER_UNITS_FROM_INT (child_geom.height);

  clutter_actor_set_geometry (child->actor, &child_geom);
}

static void
clutter_vbox_unpack_child (ClutterBox      *box,
                           ClutterBoxChild *child)
{
  /* no need to do anything */
}

static ClutterLayoutFlags
clutter_vbox_get_layout_flags (ClutterLayout *layout)
{
  return CLUTTER_LAYOUT_WIDTH_FOR_HEIGHT;
}

static void
clutter_vbox_width_for_height (ClutterLayout *layout,
                               gint          *width,
                               gint           height)
{

}

static void
clutter_vbox_get_property (GObject    *gobject,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_LAYOUT_FLAGS:
      g_value_set_enum (value, CLUTTER_LAYOUT_WIDTH_FOR_HEIGHT);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_layout_iface_init (ClutterLayoutIface *iface)
{
  iface->get_layout_flags = clutter_vbox_get_layout_flags;
  iface->width_for_height = clutter_vbox_width_for_height;
}

static void
clutter_vbox_class_init (ClutterVBoxClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterBoxClass *box_class = CLUTTER_BOX_CLASS (klass);

  gobject_class->get_property = clutter_vbox_get_property;

  actor_class->query_coords = clutter_vbox_query_coords;
  actor_class->request_coords = clutter_vbox_request_coords;

  box_class->pack_child = clutter_vbox_pack_child;
  box_class->unpack_child = clutter_vbox_unpack_child;

  g_object_class_override_property (gobject_class,
                                    PROP_LAYOUT_FLAGS,
                                    "layout-flags");
}

static void
clutter_vbox_init (ClutterVBox *box)
{

}

/**
 * clutter_vbox_new:
 *
 * Creates a new vertical layout box.
 *
 * Return value: the newly created #ClutterVBox
 *
 * Since: 0.4
 */
ClutterActor *
clutter_vbox_new (void)
{
  return g_object_new (CLUTTER_TYPE_VBOX, NULL);
}
