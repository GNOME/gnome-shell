
#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

#ifdef CLUTTER_COGL_HAS_GL

/* Size the texture so that it is just off a power of two to enourage
   it so use software tiling when NPOTs aren't available */
#define TEXTURE_SIZE 33

#else /* CLUTTER_COGL_HAS_GL */

/* We can't use the funny-sized texture on GL ES because it will break
   cogl_texture_polygon. However there is only one code path for
   rendering quads so there is no need */
#define TEXTURE_SIZE 32

#endif /* CLUTTER_COGL_HAS_GL */

/* Amount of pixels to skip off the top, bottom, left and right of the
   texture when reading back the stage */
#define TEST_INSET   4

typedef struct _TestState
{
  guint frame;
  CoglHandle texture;
} TestState;

static gboolean
validate_part (int xnum, int ynum, gboolean shown)
{
  guchar *pixels, *p;
  ClutterActor *stage = clutter_stage_get_default ();
  gboolean ret = TRUE;

  /* Read the appropriate part but skip out a few pixels around the
     edges */
  pixels = clutter_stage_read_pixels (CLUTTER_STAGE (stage),
                                      xnum * TEXTURE_SIZE + TEST_INSET,
                                      ynum * TEXTURE_SIZE + TEST_INSET,
                                      TEXTURE_SIZE - TEST_INSET * 2,
                                      TEXTURE_SIZE - TEST_INSET * 2);

  /* Make sure every pixels is the appropriate color */
  for (p = pixels;
       p < pixels + ((TEXTURE_SIZE - TEST_INSET * 2)
                     * (TEXTURE_SIZE - TEST_INSET * 2));
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
validate_result (TestState *state)
{
  /* Front-facing texture */
  g_assert (validate_part (0, 0, TRUE));
  /* Back-facing texture */
  g_assert (validate_part (1, 0, FALSE));
  /* Front-facing texture polygon */
  g_assert (validate_part (2, 0, TRUE));
  /* Back-facing texture polygon */
  g_assert (validate_part (3, 0, FALSE));
  /* Regular rectangle */
  g_assert (validate_part (4, 0, TRUE));

  /* Backface culling disabled - everything should be shown */

  /* Front-facing texture */
  g_assert (validate_part (0, 1, TRUE));
  /* Back-facing texture */
  g_assert (validate_part (1, 1, TRUE));
  /* Front-facing texture polygon */
  g_assert (validate_part (2, 1, TRUE));
  /* Back-facing texture polygon */
  g_assert (validate_part (3, 1, TRUE));
  /* Regular rectangle */
  g_assert (validate_part (4, 1, TRUE));

  /* Comment this out if you want visual feedback of what this test
   * paints.
   */
  clutter_main_quit ();
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  int i;
  int frame_num;

  cogl_enable_backface_culling (TRUE);

  cogl_push_matrix ();

  /* Render the scene twice - once with backface culling enabled and
     once without. The second time is translated so that it is below
     the first */
  for (i = 0; i < 2; i++)
    {
      float x1 = 0, x2, y1 = 0, y2 = (float)(TEXTURE_SIZE);
      CoglTextureVertex verts[4];

      memset (verts, 0, sizeof (verts));

      /* Set the color to white so that all the textures will be drawn
         at their own color */
      cogl_set_source_color4f (1.0, 1.0,
                               1.0, 1.0);

      x2 = x1 + (float)(TEXTURE_SIZE);

      /* Draw a front-facing texture */
      cogl_texture_rectangle (state->texture,
                              x1, y1, x2, y2,
                              0, 0, 1.0, 1.0);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_SIZE);

      /* Draw a back-facing texture */
      cogl_texture_rectangle (state->texture,
                              x2, y1, x1, y2,
                              0, 0, 1.0, 1.0);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_SIZE);

      /* Draw a front-facing texture polygon */
      verts[0].x = x1;             verts[0].y = y2;
      verts[1].x = x2;             verts[1].y = y2;
      verts[2].x = x2;             verts[2].y = y1;
      verts[3].x = x1;             verts[3].y = y1;
      verts[0].tx = 0;             verts[0].ty = 0;
      verts[1].tx = 1.0;  verts[1].ty = 0;
      verts[2].tx = 1.0;  verts[2].ty = 1.0;
      verts[3].tx = 0;             verts[3].ty = 1.0;
      cogl_texture_polygon (state->texture, 4,
                            verts, FALSE);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_SIZE);

      /* Draw a back-facing texture polygon */
      verts[0].x = x1;             verts[0].y = y1;
      verts[1].x = x2;             verts[1].y = y1;
      verts[2].x = x2;             verts[2].y = y2;
      verts[3].x = x1;             verts[3].y = y2;
      verts[0].tx = 0;             verts[0].ty = 0;
      verts[1].tx = 1.0;  verts[1].ty = 0;
      verts[2].tx = 1.0;  verts[2].ty = 1.0;
      verts[3].tx = 0;             verts[3].ty = 1.0;
      cogl_texture_polygon (state->texture, 4,
                            verts, FALSE);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_SIZE);

      /* Draw a regular rectangle (this should always show) */
      cogl_set_source_color4f (1.0, 0, 0, 1.0);
      cogl_rectangle ( (x1),  (y1),
                       (x2 - x1),  (y2 - y1));

      /* The second time round draw beneath the first with backface
         culling disabled */
      cogl_translate (0, TEXTURE_SIZE, 0);
      cogl_enable_backface_culling (FALSE);
    }

  cogl_pop_matrix ();

  /* XXX: Experiments have shown that for some buggy drivers, when using
   * glReadPixels there is some kind of race, so we delay our test for a
   * few frames and a few seconds:
   */
  /* Need to increment frame first because clutter_stage_read_pixels
     fires a redraw */
  frame_num = state->frame++;
  if (frame_num == 2)
    validate_result (state);
  else if (frame_num < 2)
    g_usleep (G_USEC_PER_SEC);
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
                                    8,
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
  ClutterActor *stage;
  ClutterActor *group;
  guint idle_source;

  state.frame = 0;

  state.texture = make_texture ();

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

  cogl_texture_unref (state.texture);

  if (g_test_verbose ())
    g_print ("OK\n");
}

