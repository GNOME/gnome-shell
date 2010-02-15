#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

#define QUAD_WIDTH 20

#define RED   0
#define GREEN 1
#define BLUE  2
#define ALPHA 3

typedef struct _TestState
{
  guint frame;
} TestState;

static void
assert_region_color (int x,
                     int y,
                     int width,
                     int height,
                     guint8 red,
                     guint8 green,
                     guint8 blue,
                     guint8 alpha)
{
  guint8 *data = g_malloc0 (width * height * 4);
  cogl_read_pixels (x, y, width, height,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    data);
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
        guint8 *pixel = &data[y * width * 4 + x * 4];
#if 1
        g_assert (pixel[RED] == red &&
                  pixel[GREEN] == green &&
                  pixel[BLUE] == blue &&
                  pixel[ALPHA] == alpha);
#endif
      }
  g_free (data);
}

/* Creates a texture divided into 4 quads with colors arranged as follows:
 * (The same value are used in all channels for each texel)
 *
 * |-----------|
 * |0x11 |0x00 |
 * |+ref |     |
 * |-----------|
 * |0x00 |0x33 |
 * |     |+ref |
 * |-----------|
 *
 *
 */
static CoglHandle
make_texture (guchar ref)
{
  int x;
  int y;
  guchar *tex_data, *p;
  CoglHandle tex;
  guchar val;

  tex_data = g_malloc (QUAD_WIDTH * QUAD_WIDTH * 16);

  for (y = 0; y < QUAD_WIDTH * 2; y++)
    for (x = 0; x < QUAD_WIDTH * 2; x++)
      {
        p = tex_data + (QUAD_WIDTH * 8 * y) + x * 4;
        if (x < QUAD_WIDTH && y < QUAD_WIDTH)
          val = 0x11 + ref;
        else if (x >= QUAD_WIDTH && y >= QUAD_WIDTH)
          val = 0x33 + ref;
        else
          val = 0x00;
        p[0] = p[1] = p[2] = p[3] = val;
      }

  /* Note: we don't use COGL_PIXEL_FORMAT_ANY for the internal format here
   * since we don't want to allow Cogl to premultiply our data. */
  tex = cogl_texture_new_from_data (QUAD_WIDTH * 2,
                                    QUAD_WIDTH * 2,
                                    COGL_TEXTURE_NONE,
                                    COGL_PIXEL_FORMAT_RGBA_8888,
                                    COGL_PIXEL_FORMAT_RGBA_8888,
                                    QUAD_WIDTH * 8,
                                    tex_data);

  g_free (tex_data);

  return tex;
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  CoglHandle tex0, tex1;
  CoglHandle material;
  gboolean status;
  GError *error = NULL;
  float tex_coords[] = {
    0, 0, 0.5, 0.5, /* tex0 */
    0.5, 0.5, 1, 1 /* tex1 */
  };

  /* XXX:
   * We haven't always had good luck with GL drivers implementing glReadPixels
   * reliably and skipping the first two frames improves our chances... */
  if (state->frame++ <= 2)
    {
      g_usleep (G_USEC_PER_SEC);
      return;
    }

  tex0 = make_texture (0x00);
  tex1 = make_texture (0x11);

  material = cogl_material_new ();

  /* An arbitrary color which should be replaced by the first texture layer */
  cogl_material_set_color4ub (material, 0x80, 0x80, 0x80, 0x80);
  cogl_material_set_blend (material, "RGBA = ADD (SRC_COLOR, 0)", NULL);

  cogl_material_set_layer (material, 0, tex0);
  cogl_material_set_layer_combine (material, 0,
                                   "RGBA = REPLACE (TEXTURE)", NULL);

  cogl_material_set_layer (material, 1, tex1);
  status = cogl_material_set_layer_combine (material, 1,
                                            "RGBA = ADD (PREVIOUS, TEXTURE)",
                                            &error);
  if (!status)
    {
      /* It's not strictly a test failure; you need a more capable GPU or
       * driver to test this texture combine string. */
      g_debug ("Failed to setup texture combine string "
               "RGBA = ADD (PREVIOUS, TEXTURE): %s",
               error->message);
    }

  cogl_set_source (material);
  cogl_rectangle_with_multitexture_coords (0, 0, QUAD_WIDTH, QUAD_WIDTH,
                                           tex_coords, 8);

  cogl_handle_unref (material);
  cogl_handle_unref (tex0);
  cogl_handle_unref (tex1);

  /* See what we got... */

  assert_region_color (0, 0, QUAD_WIDTH, QUAD_WIDTH,
                       0x55, 0x55, 0x55, 0x55);

  /* Comment this out if you want visual feedback for what this test paints */
#if 1
  clutter_main_quit ();
#endif
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

void
test_cogl_multitexture (TestConformSimpleFixture *fixture,
                        gconstpointer data)
{
  TestState state;
  ClutterActor *stage;
  ClutterActor *group;
  guint idle_source;

  state.frame = 0;

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  group = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = g_idle_add (queue_redraw, stage);

  g_signal_connect (group, "paint", G_CALLBACK (on_paint), &state);

  clutter_actor_show_all (stage);

  clutter_main ();

  g_source_remove (idle_source);

  if (g_test_verbose ())
    g_print ("OK\n");
}
