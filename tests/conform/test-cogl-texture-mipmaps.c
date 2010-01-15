#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff };

#define TEX_SIZE 64

typedef struct _TestState
{
  guint frame;
} TestState;

/* Creates a texture where the pixels are evenly divided between
   selecting just one of the R,G and B components */
static CoglHandle
make_texture (void)
{
  guchar *tex_data = g_malloc (TEX_SIZE * TEX_SIZE * 3), *p = tex_data;
  CoglHandle tex;
  int x, y;

  for (y = 0; y < TEX_SIZE; y++)
    for (x = 0; x < TEX_SIZE; x++)
      {
        memset (p, 0, 3);
        /* Set one of the components to full. The components should be
           evenly represented so that each gets a third of the
           texture */
        p[(p - tex_data) / (TEX_SIZE * TEX_SIZE * 3 / 3)] = 255;
        p += 3;
      }

  tex = cogl_texture_new_from_data (TEX_SIZE, TEX_SIZE, COGL_TEXTURE_NONE,
                                    COGL_PIXEL_FORMAT_RGB_888,
                                    COGL_PIXEL_FORMAT_ANY,
                                    TEX_SIZE * 3,
                                    tex_data);

  g_free (tex_data);

  return tex;
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  CoglHandle tex;
  CoglHandle material;
  guint8 pixels[8];

  /* XXX:
   * We haven't always had good luck with GL drivers implementing glReadPixels
   * reliably and skipping the first two frames improves our chances... */
  if (state->frame++ <= 2)
    {
      g_usleep (G_USEC_PER_SEC);
      return;
    }

  tex = make_texture ();
  material = cogl_material_new ();
  cogl_material_set_layer (material, 0, tex);
  cogl_handle_unref (tex);

  /* Render a 1x1 pixel quad without mipmaps */
  cogl_set_source (material);
  cogl_material_set_layer_filters (material, 0,
                                   COGL_MATERIAL_FILTER_NEAREST,
                                   COGL_MATERIAL_FILTER_NEAREST);
  cogl_rectangle (0, 0, 1, 1);
  /* Then with mipmaps */
  cogl_material_set_layer_filters (material, 0,
                                   COGL_MATERIAL_FILTER_NEAREST_MIPMAP_NEAREST,
                                   COGL_MATERIAL_FILTER_NEAREST);
  cogl_rectangle (1, 0, 2, 1);

  cogl_material_unref (material);

  /* Read back the two pixels we rendered */
  cogl_read_pixels (0, 0, 2, 1,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    pixels);

  /* The first pixel should be just one of the colors from the
     texture. It doesn't matter which one */
  g_assert ((pixels[0] == 255 && pixels[1] == 0 && pixels[2] == 0) ||
            (pixels[0] == 0 && pixels[1] == 255 && pixels[2] == 0) ||
            (pixels[0] == 0 && pixels[1] == 0 && pixels[2] == 255));
  /* The second pixel should be more or less the average of all of the
     pixels in the texture. Each component gets a third of the image
     so each component should be approximately 255/3 */
  g_assert (ABS (pixels[4] - 255 / 3) <= 3 &&
            ABS (pixels[5] - 255 / 3) <= 3 &&
            ABS (pixels[6] - 255 / 3) <= 3);

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
test_cogl_texture_mipmaps (TestConformSimpleFixture *fixture,
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
