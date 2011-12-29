#include <clutter/clutter.h>

int
main (int argc, char **argv)
{
  ClutterActor *stage, *image, *sub_image;
  CoglHandle texture, sub_texture;
  gfloat image_width, image_height;

  /* Initialize Clutter */
  if (clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
    return 1;

  /* Get the default stage */
  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Sub-texture");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* Create a new ClutterTexture that shows smiley.png */
  image = clutter_texture_new_from_file ("smiley.png", NULL);
  clutter_actor_get_size (image, &image_width, &image_height);
  clutter_actor_set_size (stage,
                          image_width * 3 / 2 + 30,
                          image_height + 20);

  /* Grab the CoglHandle of the underlying Cogl texture */
  texture = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (image));

  /* Create a new Cogl texture from the handle above. That new texture is a
   * rectangular region from image, more precisely the northwest corner
   * of the image */
  sub_texture = cogl_texture_new_from_sub_texture (texture,
                                                   0, 0,
                                                   image_width / 2,
                                                   image_height / 2);

  /* Finally, use the newly created Cogl texture to feed a new ClutterTexture
   * and thus create a new actor that displays sub_texture */
   sub_image = clutter_texture_new ();
   clutter_texture_set_cogl_texture (CLUTTER_TEXTURE (sub_image), sub_texture);

  /*
   * You could have used the more straightforward g_object_new() function that
   * can create an object and set some properties on it at the same time:
   * sub_image = g_object_new (CLUTTER_TYPE_TEXTURE,
   *                           "cogl-texture", sub_texture,
   *                           NULL);
   */

  /* Put the original image at (10,10) and the new sub image next to it */
  clutter_actor_set_position (image, 10, 10);
  clutter_actor_set_position (sub_image, 20 + image_width, 10);

  /* Add both ClutterTexture to the stage */
  clutter_container_add (CLUTTER_CONTAINER (stage), image, sub_image, NULL);

  clutter_actor_show_all (stage);

  clutter_main ();

  return 0;
}
