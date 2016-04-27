#include <stdlib.h>
#include <gmodule.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter.h>

typedef struct _SolidContent {
  GObject parent_instance;

  double red;
  double green;
  double blue;
  double alpha;

  float padding;
} SolidContent;

typedef struct _SolidContentClass {
  GObjectClass parent_class;
} SolidContentClass;

static void clutter_content_iface_init (ClutterContentIface *iface);

G_DEFINE_TYPE_WITH_CODE (SolidContent, solid_content, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTENT,
                                                clutter_content_iface_init))

static void
solid_content_paint_content (ClutterContent   *content,
                             ClutterActor     *actor,
                             ClutterPaintNode *root)
{
  SolidContent *self = (SolidContent *) content;
  ClutterActorBox box, content_box;
  ClutterColor color;
  PangoLayout *layout;
  PangoRectangle logical;
  ClutterPaintNode *node;

#if 0
  g_debug ("Painting content [%p] "
           "{ r:%.2f, g:%.2f, b:%.2f, a:%.2f } "
           "for actor [%p] (context: [%p])",
           content,
           self->red,
           self->green,
           self->blue,
           self->alpha,
           actor, context);
#endif

  clutter_actor_get_content_box (actor, &content_box);

  box = content_box;
  box.x1 += self->padding;
  box.y1 += self->padding;
  box.x2 -= self->padding;
  box.y2 -= self->padding;

  color.alpha = self->alpha * 255;

  color.red = self->red * 255;
  color.green = self->green * 255;
  color.blue = self->blue * 255;

  node = clutter_color_node_new (&color);
  clutter_paint_node_add_rectangle (node, &box);
  clutter_paint_node_add_child (root, node);
  clutter_paint_node_unref (node);

  color.red = (1.0 - self->red) * 255;
  color.green = (1.0 - self->green) * 255;
  color.blue = (1.0 - self->blue) * 255;

  layout = clutter_actor_create_pango_layout (actor, "A");
  pango_layout_get_pixel_extents (layout, NULL, &logical);

  node = clutter_text_node_new (layout, &color);

  /* top-left */
  box.x1 = clutter_actor_box_get_x (&content_box);
  box.y1 = clutter_actor_box_get_y (&content_box);
  box.x2 = box.x1 + logical.width;
  box.y2 = box.y1 + logical.height;
  clutter_paint_node_add_rectangle (node, &box);

  /* top-right */
  box.x1 = clutter_actor_box_get_x (&content_box)
         + clutter_actor_box_get_width (&content_box)
         - logical.width;
  box.y1 = clutter_actor_box_get_y (&content_box);
  box.x2 = box.x1 + logical.width;
  box.y2 = box.y1 + logical.height;
  clutter_paint_node_add_rectangle (node, &box);

  /* bottom-right */
  box.x1 = clutter_actor_box_get_x (&content_box)
         + clutter_actor_box_get_width (&content_box)
         - logical.width;
  box.y1 = clutter_actor_box_get_y (&content_box)
         + clutter_actor_box_get_height (&content_box)
         - logical.height;
  box.x2 = box.x1 + logical.width;
  box.y2 = box.y1 + logical.height;
  clutter_paint_node_add_rectangle (node, &box);

  /* bottom-left */
  box.x1 = clutter_actor_box_get_x (&content_box);
  box.y1 = clutter_actor_box_get_y (&content_box)
         + clutter_actor_box_get_height (&content_box)
         - logical.height;
  box.x2 = box.x1 + logical.width;
  box.y2 = box.y1 + logical.height;
  clutter_paint_node_add_rectangle (node, &box);

  /* center */
  box.x1 = clutter_actor_box_get_x (&content_box)
         + (clutter_actor_box_get_width (&content_box) - logical.width) / 2.0;
  box.y1 = clutter_actor_box_get_y (&content_box)
         + (clutter_actor_box_get_height (&content_box) - logical.height) / 2.0;
  box.x2 = box.x1 + logical.width;
  box.y2 = box.y1 + logical.height;
  clutter_paint_node_add_rectangle (node, &box);

  clutter_paint_node_add_child (root, node);
  clutter_paint_node_unref (node);

  g_object_unref (layout);
}

static void
clutter_content_iface_init (ClutterContentIface *iface)
{
  iface->paint_content = solid_content_paint_content;
}

static void
solid_content_class_init (SolidContentClass *klass)
{
}

static void
solid_content_init (SolidContent *self)
{
}

static ClutterContent *
solid_content_new (double red,
                   double green,
                   double blue,
                   double alpha,
                   float  padding)
{
  SolidContent *self = g_object_new (solid_content_get_type (), NULL);

  self->red = red;
  self->green = green;
  self->blue = blue;
  self->alpha = alpha;
  self->padding = padding;

  return (ClutterContent *) self;
}

G_MODULE_EXPORT const char *
test_image_describe (void)
{
  return "A test with image content.";
}

G_MODULE_EXPORT int
test_image_main (int argc, char *argv[])
{
  ClutterActor *stage, *grid;
  ClutterContent *color, *image;
  GdkPixbuf *pixbuf;
  int i, n_rects;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  stage = clutter_stage_new ();
  clutter_actor_set_name (stage, "Stage");
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Content");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);
  clutter_actor_show (stage);

  grid = clutter_actor_new ();
  clutter_actor_set_name (grid, "Grid");
  clutter_actor_set_margin_top (grid, 12);
  clutter_actor_set_margin_right (grid, 12);
  clutter_actor_set_margin_bottom (grid, 12);
  clutter_actor_set_margin_left (grid, 12);
  clutter_actor_set_layout_manager (grid, clutter_flow_layout_new (CLUTTER_FLOW_HORIZONTAL));
  clutter_actor_add_constraint (grid, clutter_bind_constraint_new (stage, CLUTTER_BIND_SIZE, 0.0));
  clutter_actor_add_child (stage, grid);

  color = solid_content_new (g_random_double_range (0.0, 1.0),
                             g_random_double_range (0.0, 1.0),
                             g_random_double_range (0.0, 1.0),
                             1.0,
                             2.0);

  pixbuf = gdk_pixbuf_new_from_file (TESTS_DATADIR G_DIR_SEPARATOR_S "redhand.png", NULL);
  image = clutter_image_new ();
  clutter_image_set_data (CLUTTER_IMAGE (image),
                          gdk_pixbuf_get_pixels (pixbuf),
                          gdk_pixbuf_get_has_alpha (pixbuf)
                            ? COGL_PIXEL_FORMAT_RGBA_8888
                            : COGL_PIXEL_FORMAT_RGB_888,
                          gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf),
                          gdk_pixbuf_get_rowstride (pixbuf),
                          NULL);
  g_object_unref (pixbuf);

  n_rects = g_random_int_range (12, 24);
  for (i = 0; i < n_rects; i++)
    {
      ClutterActor *box = clutter_actor_new ();
      ClutterColor bg_color = {
        g_random_int_range (0, 255),
        g_random_int_range (0, 255),
        g_random_int_range (0, 255),
        255
      };
      char *name, *str;

      str = clutter_color_to_string (&bg_color);
      name = g_strconcat ("Box <", color, ">", NULL);
      clutter_actor_set_name (box, name);

      g_free (name);
      g_free (str);

      if ((i % 2) == 0)
        clutter_actor_set_content (box, color);
      else
        clutter_actor_set_content (box, image);

      clutter_actor_set_size (box, 64, 64);

      clutter_actor_add_child (grid, box);
    }

  clutter_main ();

  g_object_unref (color);
  g_object_unref (image);

  return EXIT_SUCCESS;
}
