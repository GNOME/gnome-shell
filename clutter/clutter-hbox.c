#include "config.h"

#include "clutter-hbox.h"

#include "clutter-box.h"
#include "clutter-container.h"
#include "clutter-layout.h"
#include "clutter-debug.h"
#include "clutter-units.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-private.h"

/**
 * SECTION:clutter-hbox
 * @short_description: Simple horizontal box
 *
 * FIXME
 *
 * #ClutterHBox is available since Clutter 0.4.
 */

enum
{
  PROP_0,

  PROP_LAYOUT_FLAGS
};

static void clutter_layout_iface_init (ClutterLayoutIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterHBox,
                         clutter_hbox,
                         CLUTTER_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_LAYOUT,
                                                clutter_layout_iface_init));

static void
clutter_hbox_query_coords (ClutterActor    *actor,
                           ClutterActorBox *coords)
{
  ClutterBox *box = CLUTTER_BOX (actor);
  GList *l;
  gint width, height;
  guint spacing;

  spacing = clutter_box_get_spacing (box);
  width = height = 0;
  
  for (l = box->children; l; l = l->next)
    {
      ClutterBoxChild *child = l->data;
      
      if (CLUTTER_ACTOR_IS_VISIBLE (child->actor))
        {
          guint child_width, child_height;

          clutter_actor_get_size (child->actor, &child_width, &child_height);

          width += child_width + (spacing * 2);
          height = MAX (child_height, height);
        }
    }

  coords->x2 = coords->x1 + CLUTTER_UNITS_FROM_INT (width);
  coords->y2 = coords->y1 + CLUTTER_UNITS_FROM_INT (height);
}

static void
clutter_hbox_request_coords (ClutterActor    *actor,
                             ClutterActorBox *coords)
{
  /* FIXME */
}

static void
clutter_hbox_pack_child (ClutterBox      *box,
                         ClutterBoxChild *child)
{
  ClutterGeometry box_geom, child_geom;
  guint spacing;

  spacing = clutter_box_get_spacing (box);

  clutter_actor_get_geometry (CLUTTER_ACTOR (box), &box_geom);
  clutter_actor_get_geometry (child->actor, &child_geom);

  child_geom.x = box_geom.x + box_geom.width + spacing;
  child_geom.y = box_geom.y;

  clutter_actor_set_geometry (child->actor, &child_geom);
}

static void
clutter_hbox_unpack_child (ClutterBox      *box,
                           ClutterBoxChild *child)
{
  /* no need to do anything */
}

static ClutterLayoutFlags
clutter_hbox_get_layout_flags (ClutterLayout *layout)
{
  return CLUTTER_LAYOUT_HEIGHT_FOR_WIDTH;
}

static void
clutter_hbox_height_for_width (ClutterLayout *layout,
                               gint           width,
                               gint          *height)
{

}

static void
clutter_hbox_get_property (GObject    *gobject,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_LAYOUT_FLAGS:
      g_value_set_enum (value, CLUTTER_LAYOUT_HEIGHT_FOR_WIDTH);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_layout_iface_init (ClutterLayoutIface *iface)
{
  iface->get_layout_flags = clutter_hbox_get_layout_flags;
  iface->height_for_width = clutter_hbox_height_for_width;
}

static void
clutter_hbox_class_init (ClutterHBoxClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterBoxClass *box_class = CLUTTER_BOX_CLASS (klass);

  gobject_class->get_property = clutter_hbox_get_property;

  actor_class->query_coords = clutter_hbox_query_coords;
  actor_class->request_coords = clutter_hbox_request_coords;

  box_class->pack_child = clutter_hbox_pack_child;
  box_class->unpack_child = clutter_hbox_unpack_child;

  g_object_class_override_property (gobject_class,
                                    PROP_LAYOUT_FLAGS,
                                    "layout-flags");
}

static void
clutter_hbox_init (ClutterHBox *box)
{

}

ClutterActor *
clutter_hbox_new (void)
{
  return g_object_new (CLUTTER_TYPE_HBOX, NULL);
}
