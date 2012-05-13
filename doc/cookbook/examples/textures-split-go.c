#include <clutter/clutter.h>

/* Context will be used to carry interesting variables between functions */
typedef struct
{
  ClutterActor *sub_nw, *sub_ne, *sub_sw, *sub_se;
  gfloat image_width, image_height;
} Context;

/* Here, we animate the texture to go way by giving the new coordinates
 * outside of the stage. We rotate the sub-textures around their anchor
 * point (set in setup_sub() as well, it looks cool. */
static gboolean
go_away (gpointer data)
{
  Context *context = data;

  clutter_actor_animate (context->sub_nw, CLUTTER_EASE_OUT_CUBIC, 1500,
                         "x", -context->image_width,
                         "y", -context->image_height,
                         "rotation-angle-z", 2000.,
                         NULL);
  clutter_actor_animate (context->sub_ne, CLUTTER_EASE_OUT_CUBIC, 1500,
                         "x", +context->image_width,
                         "y", -context->image_height,
                         "rotation-angle-z", 2000.,
                         NULL);
  clutter_actor_animate (context->sub_sw, CLUTTER_EASE_OUT_CUBIC, 1500,
                         "x", -context->image_width,
                         "y", +context->image_height,
                         "rotation-angle-z", 2000.,
                         NULL);
  clutter_actor_animate (context->sub_se, CLUTTER_EASE_OUT_CUBIC, 1500,
                         "x", -context->image_width,
                         "y", +context->image_height,
                         "rotation-angle-z", 2000.,
                         NULL);
  return G_SOURCE_REMOVE; /* remove the timeout source */
}

/* We split the four sub-textures faking to be the big texture, moving them
 * away by 10 pixels in each direction */
static gboolean
split (gpointer data)
{
  Context *context = data;
  gfloat x, y;

  clutter_actor_get_position (context->sub_nw, &x, &y);
  clutter_actor_animate (context->sub_nw, CLUTTER_EASE_OUT_CUBIC, 300,
                         "x", x - 10,
                         "y", y - 10,
                         NULL);
  clutter_actor_get_position (context->sub_ne, &x, &y);
  clutter_actor_animate (context->sub_ne, CLUTTER_EASE_OUT_CUBIC, 300,
                         "x", x + 10,
                         "y", y - 10,
                         NULL);
  clutter_actor_get_position (context->sub_sw, &x, &y);
  clutter_actor_animate (context->sub_sw, CLUTTER_EASE_OUT_CUBIC, 300,
                         "x", x - 10,
                         "y", y + 10,
                         NULL);
  clutter_actor_get_position (context->sub_se, &x, &y);
  clutter_actor_animate (context->sub_se, CLUTTER_EASE_OUT_CUBIC, 300,
                         "x", x + 10,
                         "y", y + 10,
                         NULL);

  /* In 500ms the textures will flee! */
  clutter_threads_add_timeout (500, go_away, context);

  return G_SOURCE_REMOVE; /* remove the timeout source */
}

static ClutterActor *
setup_sub (CoglHandle texture,
           gint       image_width,
           gint       image_height,
           gint       t_x,
           gint       t_y,
           gint       t_width,
           gint       t_height)
{
  CoglHandle sub_texture;
  ClutterActor *sub_image;

  /* Create a new sub-texture from textures */
  sub_texture = cogl_texture_new_from_sub_texture (texture,
                                                   t_x, t_y,
                                                   t_width, t_height);

  /* Create the corresponding ClutterTexture */
  sub_image = g_object_new (CLUTTER_TYPE_TEXTURE,
                            "cogl-texture", sub_texture,
                            NULL);

  /* Set the anchor point in the middle of each sub_image so the position and
   * rotation of the textures are relative to that point */
  clutter_actor_set_anchor_point (sub_image, image_width / 4, image_height / 4);

  return sub_image;
}

#define IMAGE "smiley.png"

int
main (int    argc,
      char **argv)
{
  gfloat image_width, image_height, stage_width, stage_height;
  ClutterActor *stage, *image;
  GError *error = NULL;
  CoglHandle texture;
  Context context;

  if (clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_get_size (stage, &stage_width, &stage_height);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Animate sub-textures");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* Load smiley.png, creating a new ClutterTexture, get its size and the
   * Cogl texture handle */
  image = clutter_texture_new_from_file (IMAGE, &error);
  if (error != NULL)
    {
      g_warning ("Could not load " IMAGE ": %s", error->message);
      g_clear_error (&error);
      return 1;
    }
  clutter_actor_get_size (image, &image_width, &image_height);
  texture = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (image));

  /* Create four sub-textures from image, actually splitting the image in
   * four */
  context.sub_nw = setup_sub (texture, image_width, image_height,
                              0, 0, image_width / 2 , image_height / 2);
  context.sub_ne = setup_sub (texture, image_width, image_height,
                              image_width / 2 , 0,
                              image_width / 2, image_height / 2);
  context.sub_sw = setup_sub (texture, image_width, image_height,
                              0.f, image_height / 2,
                              image_width / 2, image_height / 2);
  context.sub_se = setup_sub (texture, image_width, image_height,
                              image_width / 2, image_height / 2,
                              image_width / 2, image_height / 2);

  /* We don't need the image anymore as we won't display it and as
   * cogl_texture_new_from_sub_texture() keeps a reference to the underlying
   * texture ressource */
  g_object_unref (image);

  /* Position the sub-texures in the middle of the screen, recreating the
   * original texture */
  clutter_actor_set_position (context.sub_nw,
                              stage_width / 2 - image_width / 4,
                              stage_height / 2 - image_height / 4);
  clutter_actor_set_position (context.sub_ne,
                              stage_width / 2 + image_width / 4,
                              stage_height / 2 - image_height / 4);
  clutter_actor_set_position (context.sub_sw,
                              stage_width / 2 - image_width / 4,
                              stage_height / 2 + image_height / 4);
  clutter_actor_set_position (context.sub_se,
                              stage_width / 2 + image_width / 4,
                              stage_height / 2 + image_height / 4);

  /* Add the four sub-textures to the stage */
  clutter_container_add (CLUTTER_CONTAINER (stage), context.sub_nw,
                         context.sub_ne, context.sub_sw, context.sub_se, NULL);

  clutter_actor_show_all (stage);

  context.image_width = image_width;
  context.image_height = image_height;

  /* In two seconds, we'll split the texture! */
  clutter_threads_add_timeout (2000, split, &context);

  clutter_main ();

  return 0;
}
