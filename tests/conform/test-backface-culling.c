
#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

#ifdef CLUTTER_COGL_HAS_GL

/* Size the texture so that it is just off a power of two to enourage
   it so use software tiling when NPOTs aren't available */
#define TEXTURE_SIZE        257

#else /* CLUTTER_COGL_HAS_GL */

/* We can't use the funny-sized texture on GL ES because it will break
   cogl_texture_polygon. However there is only one code path for
   rendering quads so there is no need */
#define TEXTURE_SIZE        32

#endif /* CLUTTER_COGL_HAS_GL */

/* Amount of pixels to skip off the top, bottom, left and right of the
   texture when reading back the stage */
#define TEST_INSET          4

/* Size to actually render the texture at */
#define TEXTURE_RENDER_SIZE 32

typedef struct _TestState
{
  CoglHandle texture;
  CoglHandle offscreen;
  CoglHandle offscreen_tex;
} TestState;

static gboolean
validate_part (int xnum, int ynum, gboolean shown)
{
  guchar *pixels, *p;
  gboolean ret = TRUE;

  pixels = g_malloc0 ((TEXTURE_RENDER_SIZE - TEST_INSET * 2)
                      * (TEXTURE_RENDER_SIZE - TEST_INSET * 2) * 4);

  /* Read the appropriate part but skip out a few pixels around the
     edges */
  cogl_read_pixels (xnum * TEXTURE_RENDER_SIZE + TEST_INSET,
                    ynum * TEXTURE_RENDER_SIZE + TEST_INSET,
                    TEXTURE_RENDER_SIZE - TEST_INSET * 2,
                    TEXTURE_RENDER_SIZE - TEST_INSET * 2,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    pixels);

  /* Make sure every pixels is the appropriate color */
  for (p = pixels;
       p < pixels + ((TEXTURE_RENDER_SIZE - TEST_INSET * 2)
                     * (TEXTURE_RENDER_SIZE - TEST_INSET * 2));
       p += 4)
    {
      if (p[0] != (shown ? 255 : 0))
        ret = FALSE;
      if (p[1] !=  0)
        ret = FALSE;
      if (p[2] != 0)
        ret = FALSE;
    }

  g_free (pixels);

  return ret;
}

static void
do_test_backface_culling (TestState *state)
{
  int i;
  CoglHandle material = cogl_material_new ();

  cogl_material_set_layer_filters (material, 0,
                                   COGL_MATERIAL_FILTER_NEAREST,
                                   COGL_MATERIAL_FILTER_NEAREST);

  cogl_set_backface_culling_enabled (TRUE);

  cogl_push_matrix ();

  /* Render the scene twice - once with backface culling enabled and
     once without. The second time is translated so that it is below
     the first */
  for (i = 0; i < 2; i++)
    {
      float x1 = 0, x2, y1 = 0, y2 = (float)(TEXTURE_RENDER_SIZE);
      CoglTextureVertex verts[4];

      cogl_set_source (material);

      memset (verts, 0, sizeof (verts));

      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a front-facing texture */
      cogl_material_set_layer (material, 0, state->texture);
      cogl_rectangle (x1, y1, x2, y2);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a front-facing texture with flipped texcoords */
      cogl_material_set_layer (material, 0, state->texture);
      cogl_rectangle_with_texture_coords (x1, y1, x2, y2,
                                          1.0, 0.0, 0.0, 1.0);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a back-facing texture */
      cogl_material_set_layer (material, 0, state->texture);
      cogl_rectangle (x2, y1, x1, y2);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a front-facing texture polygon */
      verts[0].x = x1;    verts[0].y = y2;
      verts[1].x = x2;    verts[1].y = y2;
      verts[2].x = x2;    verts[2].y = y1;
      verts[3].x = x1;    verts[3].y = y1;
      verts[0].tx = 0;    verts[0].ty = 0;
      verts[1].tx = 1.0;  verts[1].ty = 0;
      verts[2].tx = 1.0;  verts[2].ty = 1.0;
      verts[3].tx = 0;    verts[3].ty = 1.0;
      cogl_material_set_layer (material, 0, state->texture);
      cogl_polygon (verts, 4, FALSE);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a back-facing texture polygon */
      verts[0].x = x1;    verts[0].y = y1;
      verts[1].x = x2;    verts[1].y = y1;
      verts[2].x = x2;    verts[2].y = y2;
      verts[3].x = x1;    verts[3].y = y2;
      verts[0].tx = 0;    verts[0].ty = 0;
      verts[1].tx = 1.0;  verts[1].ty = 0;
      verts[2].tx = 1.0;  verts[2].ty = 1.0;
      verts[3].tx = 0;    verts[3].ty = 1.0;
      cogl_material_set_layer (material, 0, state->texture);
      cogl_polygon (verts, 4, FALSE);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a regular rectangle (this should always show) */
      cogl_set_source_color4f (1.0, 0, 0, 1.0);
      cogl_rectangle (x1, y1, x2, y2);

      /* The second time round draw beneath the first with backface
         culling disabled */
      cogl_translate (0, TEXTURE_RENDER_SIZE, 0);
      cogl_set_backface_culling_enabled (FALSE);
    }

  cogl_handle_unref (material);

  cogl_pop_matrix ();

  /* Front-facing texture */
  g_assert (validate_part (0, 0, TRUE));
  /* Front-facing texture with flipped tex coords */
  g_assert (validate_part (1, 0, TRUE));
  /* Back-facing texture */
  g_assert (validate_part (2, 0, FALSE));
  /* Front-facing texture polygon */
  g_assert (validate_part (3, 0, TRUE));
  /* Back-facing texture polygon */
  g_assert (validate_part (4, 0, FALSE));
  /* Regular rectangle */
  g_assert (validate_part (5, 0, TRUE));

  /* Backface culling disabled - everything should be shown */

  /* Front-facing texture */
  g_assert (validate_part (0, 1, TRUE));
  /* Front-facing texture with flipped tex coords */
  g_assert (validate_part (1, 1, TRUE));
  /* Back-facing texture */
  g_assert (validate_part (2, 1, TRUE));
  /* Front-facing texture polygon */
  g_assert (validate_part (3, 1, TRUE));
  /* Back-facing texture polygon */
  g_assert (validate_part (4, 1, TRUE));
  /* Regular rectangle */
  g_assert (validate_part (5, 1, TRUE));

}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  CoglColor clr;
  float stage_viewport[4];
  CoglMatrix stage_projection;
  CoglMatrix stage_modelview;

  cogl_color_set_from_4ub (&clr, 0x00, 0x00, 0x00, 0xff);

  do_test_backface_culling (state);

  /* Since we are going to repeat the test rendering offscreen we clear the
   * stage, just to minimize the chance of a some other bug causing us
   * mistakenly reading back the results from the stage and giving a false
   * posistive. */
  cogl_clear (&clr, COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_STENCIL);

  /*
   * Now repeat the test but rendered to an offscreen framebuffer...
   */

  cogl_get_viewport (stage_viewport);
  cogl_get_projection_matrix (&stage_projection);
  cogl_get_modelview_matrix (&stage_modelview);

  cogl_push_framebuffer (state->offscreen);

  cogl_clear (&clr, COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_STENCIL);

  cogl_set_viewport (stage_viewport[0],
                     stage_viewport[1],
                     stage_viewport[2],
                     stage_viewport[3]);
  cogl_set_projection_matrix (&stage_projection);
  cogl_set_modelview_matrix (&stage_modelview);

  do_test_backface_culling (state);

  cogl_pop_framebuffer ();

  /* Incase we want feedback of what was drawn offscreen we draw it
   * to the stage... */
  cogl_set_source_texture (state->offscreen_tex);
  cogl_rectangle (0, 0, stage_viewport[2], stage_viewport[3]);

  /* Comment this out if you want visual feedback of what this test
   * paints.
   */
  clutter_main_quit ();
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

static CoglHandle
make_texture (void)
{
  guchar *tex_data, *p;
  CoglHandle tex;

  tex_data = g_malloc (TEXTURE_SIZE * TEXTURE_SIZE * 4);

  for (p = tex_data + TEXTURE_SIZE * TEXTURE_SIZE * 4; p > tex_data;)
    {
      *(--p) = 255;
      *(--p) = 0;
      *(--p) = 0;
      *(--p) = 255;
    }

  tex = cogl_texture_new_from_data (TEXTURE_SIZE,
                                    TEXTURE_SIZE,
                                    COGL_TEXTURE_NONE,
                                    COGL_PIXEL_FORMAT_RGBA_8888,
                                    COGL_PIXEL_FORMAT_ANY,
                                    TEXTURE_SIZE * 4,
                                    tex_data);

  g_free (tex_data);

  return tex;
}

void
test_backface_culling (TestConformSimpleFixture *fixture,
                       gconstpointer data)
{
  TestState state;
  CoglHandle tex;
  ClutterActor *stage;
  float stage_width;
  float stage_height;
  const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };
  ClutterActor *group;
  guint idle_source;

  stage = clutter_stage_get_default ();
  clutter_actor_get_size (stage, &stage_width, &stage_height);

  state.offscreen = COGL_INVALID_HANDLE;

  state.texture = make_texture ();

  tex = cogl_texture_new_with_size (stage_width, stage_height,
                                    COGL_TEXTURE_NO_SLICING,
                                    COGL_PIXEL_FORMAT_ANY); /* internal fmt */
  state.offscreen = cogl_offscreen_new_to_texture (tex);
  state.offscreen_tex = tex;

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

  cogl_handle_unref (state.offscreen);
  cogl_handle_unref (state.offscreen_tex);
  cogl_handle_unref (state.texture);

  if (g_test_verbose ())
    g_print ("OK\n");
}

