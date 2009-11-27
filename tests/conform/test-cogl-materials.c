
#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

#define QUAD_WIDTH 20

#define RED 0
#define GREEN 1
#define BLUE 2
#define ALPHA 3

#define MASK_RED(COLOR)   ((COLOR & 0xff000000) >> 24);
#define MASK_GREEN(COLOR) ((COLOR & 0xff0000) >> 16);
#define MASK_BLUE(COLOR)  ((COLOR & 0xff00) >> 8);
#define MASK_ALPHA(COLOR) (COLOR & 0xff);

#define SKIP_FRAMES 2

typedef struct _TestState
{
  guint frame;
  ClutterGeometry stage_geom;
} TestState;


static void
check_pixel (TestState *state, int x, int y, guint32 color)
{
  GLint y_off;
  GLint x_off;
  GLubyte pixel[4];
  guint8 r = MASK_RED (color);
  guint8 g = MASK_GREEN (color);
  guint8 b = MASK_BLUE (color);
  guint8 a = MASK_ALPHA (color);

  /* See what we got... */

  /* NB: glReadPixels is done in GL screen space so y = 0 is at the bottom */
  y_off = y * QUAD_WIDTH + (QUAD_WIDTH / 2);
  x_off = x * QUAD_WIDTH + (QUAD_WIDTH / 2);

  /* XXX:
   * We haven't always had good luck with GL drivers implementing glReadPixels
   * reliably and skipping the first two frames improves our chances... */
  if (state->frame <= SKIP_FRAMES)
    return;

  cogl_read_pixels (x_off, y_off, 1, 1,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    pixel);
  if (g_test_verbose ())
    g_print ("  result = %02x, %02x, %02x, %02x\n",
             pixel[RED], pixel[GREEN], pixel[BLUE], pixel[ALPHA]);

  if (g_test_verbose ())
    g_print ("  expected = %x, %x, %x, %x\n",
             r, g, b, a);
  /* FIXME - allow for hardware in-precision */
  g_assert (pixel[RED] == r);
  g_assert (pixel[GREEN] == g);
  g_assert (pixel[BLUE] == b);

  /* FIXME
   * We ignore the alpha, since we don't know if our render target is
   * RGB or RGBA */
  /* g_assert (pixel[ALPHA] == a); */
}

static void
test_invalid_texture_layers (TestState *state, int x, int y)
{
  CoglHandle        material = cogl_material_new ();
  CoglTextureVertex verts[4] = {
    { .x = 0,          .y = 0,          .z = 0 },
    { .x = 0,          .y = QUAD_WIDTH, .z = 0 },
    { .x = QUAD_WIDTH, .y = QUAD_WIDTH, .z = 0 },
    { .x = QUAD_WIDTH, .y = 0,          .z = 0 },
  };
  CoglHandle vbo;

  cogl_push_matrix ();

  cogl_translate (x * QUAD_WIDTH, y * QUAD_WIDTH, 0);

  /* explicitly create a layer with an invalid handle. This may be desireable
   * if the user also sets a texture combine string that e.g. refers to a
   * constant color. */
  cogl_material_set_layer (material, 0, COGL_INVALID_HANDLE);

  cogl_set_source (material);
  cogl_rectangle (0, 0, QUAD_WIDTH, QUAD_WIDTH);

  cogl_translate (0, QUAD_WIDTH, 0);
  cogl_polygon (verts, 4, FALSE);

  cogl_translate (0, QUAD_WIDTH, 0);
  vbo = cogl_vertex_buffer_new (4);
  cogl_vertex_buffer_add (vbo,
                          "gl_Vertex",
                          2, /* n components */
                          COGL_ATTRIBUTE_TYPE_FLOAT,
                          FALSE, /* normalized */
                          sizeof (CoglTextureVertex), /* stride */
                          verts);
  cogl_vertex_buffer_draw (vbo,
			   COGL_VERTICES_MODE_TRIANGLE_FAN,
			   0, /* first */
			   4); /* count */
  cogl_handle_unref (vbo);

  cogl_pop_matrix ();

  cogl_handle_unref (material);

  /* We expect a white fallback material to be used */
  check_pixel (state, x, y,   0xffffffff);
  check_pixel (state, x, y+1, 0xffffffff);
  check_pixel (state, x, y+2, 0xffffffff);
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  int frame_num;

  test_invalid_texture_layers (state,
                               0, 0 /* position */
                               );

  /* XXX: Experiments have shown that for some buggy drivers, when using
   * glReadPixels there is some kind of race, so we delay our test for a
   * few frames and a few seconds:
   */
  frame_num = state->frame++;
  if (frame_num < SKIP_FRAMES)
    g_usleep (G_USEC_PER_SEC);

  /* Comment this out if you want visual feedback for what this test paints */
#if 1
  if (frame_num > SKIP_FRAMES)
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
test_cogl_materials (TestConformSimpleFixture *fixture,
                     gconstpointer data)
{
  TestState state;
  ClutterActor *stage;
  ClutterActor *group;
  guint idle_source;

  state.frame = 0;

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_get_geometry (stage, &state.stage_geom);

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

