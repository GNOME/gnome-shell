#include <stdlib.h>
#include <clutter/clutter.h>

#define STAGE_SIDE 400
#define RECTANGLE_SIDE STAGE_SIDE * 0.5
#define TEXTURE_SIZE_MAX STAGE_SIDE * 0.9
#define TEXTURE_SIZE_MIN STAGE_SIDE * 0.1
#define TEXTURE_SIZE_STEP 0.2
#define OVERLAY_OPACITY_OFF 0
#define OVERLAY_OPACITY_ON 100

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor overlay_color = { 0xaa, 0x99, 0x00, 0xff };

/* change the texture size with +/- */
static gboolean
key_press_cb (ClutterActor *actor,
              ClutterEvent *event,
              gpointer      user_data)
{
  ClutterActor *texture;
  gfloat texture_width, texture_height;
  guint key_pressed;

  texture = CLUTTER_ACTOR (user_data);
  clutter_actor_get_size (texture, &texture_width, &texture_height);

  key_pressed = clutter_event_get_key_symbol (event);

  if (key_pressed == CLUTTER_KEY_plus)
    {
      texture_width *= 1.0 + TEXTURE_SIZE_STEP;
      texture_height *= 1.0 + TEXTURE_SIZE_STEP;
    }
  else if (key_pressed == CLUTTER_KEY_minus)
    {
      texture_width *= 1.0 - TEXTURE_SIZE_STEP;
      texture_height *= 1.0 - TEXTURE_SIZE_STEP;
    }

  if (texture_width <= TEXTURE_SIZE_MAX && texture_width >= TEXTURE_SIZE_MIN)
    clutter_actor_animate (texture, CLUTTER_EASE_OUT_CUBIC, 500,
                           "width", texture_width,
                           "height", texture_height,
                           NULL);

  return TRUE;
}

/* turn overlay opacity on/off */
static void
click_cb (ClutterClickAction *action,
          ClutterActor       *actor,
          gpointer            user_data)
{
  ClutterActor *overlay = CLUTTER_ACTOR (user_data);
  guint8 opacity = clutter_actor_get_opacity (overlay);

  if (opacity < OVERLAY_OPACITY_ON)
    opacity = OVERLAY_OPACITY_ON;
  else
    opacity = OVERLAY_OPACITY_OFF;

  clutter_actor_set_opacity (overlay, opacity);
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *texture;
  ClutterActor *overlay;
  ClutterAction *click;
  GError *error = NULL;

  const gchar *filename = "redhand.png";

  if (argc > 1)
    filename = argv[1];

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, STAGE_SIDE, STAGE_SIDE);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  texture = clutter_texture_new ();
  clutter_texture_set_keep_aspect_ratio (CLUTTER_TEXTURE (texture), TRUE);
  clutter_actor_set_reactive (texture, TRUE);
  clutter_actor_set_size (texture, RECTANGLE_SIDE, RECTANGLE_SIDE);
  clutter_actor_add_constraint (texture, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (texture, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5));

  clutter_texture_set_from_file (CLUTTER_TEXTURE (texture),
                                 filename,
                                 &error);

  if (error != NULL)
    {
      g_warning ("Error loading %s\n%s", filename, error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  /* overlay is 10px wider and taller than the texture, and centered on it;
   * initially, it is transparent; but it is made semi-opaque when the
   * texture is clicked
   */
  overlay = clutter_rectangle_new_with_color (&overlay_color);
  clutter_actor_set_opacity (overlay, OVERLAY_OPACITY_OFF);
  clutter_actor_add_constraint (overlay, clutter_bind_constraint_new (texture, CLUTTER_BIND_WIDTH, 10));
  clutter_actor_add_constraint (overlay, clutter_bind_constraint_new (texture, CLUTTER_BIND_HEIGHT, 10));
  clutter_actor_add_constraint (overlay, clutter_align_constraint_new (texture, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (overlay, clutter_align_constraint_new (texture, CLUTTER_ALIGN_Y_AXIS, 0.5));

  click = clutter_click_action_new ();
  clutter_actor_add_action (texture, click);

  clutter_container_add (CLUTTER_CONTAINER (stage), texture, overlay, NULL);
  clutter_actor_raise_top (overlay);

  g_signal_connect (click, "clicked", G_CALLBACK (click_cb), overlay);

  g_signal_connect (stage,
                    "key-press-event",
                    G_CALLBACK (key_press_cb),
                    texture);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
