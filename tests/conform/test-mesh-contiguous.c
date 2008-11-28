
#include <clutter/clutter.h>
#include <cogl/cogl.h>

#include "test-conform-common.h"

/* This test verifies that the simplest usage of the mesh API, where we add
 * contiguous (x,y) GLfloat vertices, and RGBA GLubyte color attributes to a
 * mesh object, submit, and draw.
 *
 * It also tries to verify that the enable/disable attribute APIs are working
 * too.
 *
 * If you want visual feedback of what this test paints for debugging purposes,
 * then remove the call to clutter_main_quit() in validate_result.
 */

typedef struct _TestState
{
  CoglHandle mesh;
  ClutterGeometry stage_geom;
  guint frame;
} TestState;

static void
validate_result (TestState *state)
{
  GLubyte pixel[4];
  GLint y_off = state->stage_geom.height - 90;

  /* NB: glReadPixels is done in GL screen space so y = 0 is at the bottom */
  if (g_test_verbose ())
    g_print ("y_off = %d\n", y_off);

  /* NB: We ignore the alpha, since we don't know if our render target is
   * RGB or RGBA */

#define RED 0
#define GREEN 1
#define BLUE 2

  /* Should see a blue pixel */
  glReadPixels (10, y_off, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
  if (g_test_verbose ())
    g_print ("pixel 0 = %x, %x, %x\n", pixel[RED], pixel[GREEN], pixel[BLUE]);
  g_assert (pixel[RED] == 0 && pixel[GREEN] == 0 && pixel[BLUE] != 0);
  
  /* Should see a red pixel */
  glReadPixels (110, y_off, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
  if (g_test_verbose ())
    g_print ("pixel 1 = %x, %x, %x\n", pixel[RED], pixel[GREEN], pixel[BLUE]);
  g_assert (pixel[RED] != 0 && pixel[GREEN] == 0 && pixel[BLUE] == 0);

  /* Should see a blue pixel */
  glReadPixels (210, y_off, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
  if (g_test_verbose ())
    g_print ("pixel 2 = %x, %x, %x\n", pixel[RED], pixel[GREEN], pixel[BLUE]);
  g_assert (pixel[RED] == 0 && pixel[GREEN] == 0 && pixel[BLUE] != 0);
  
#undef RED
#undef GREEN
#undef BLUE
  
  /* Comment this out if you want visual feedback of what this test
   * paints.
   */
  clutter_main_quit ();
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  /* Draw a faded blue triangle */
  cogl_mesh_enable_attribute (state->mesh, "gl_Color::blue");
  cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
  cogl_mesh_draw_arrays (state->mesh,
			 GL_TRIANGLE_STRIP, /* mode */
			 0, /* first */
			 3); /* count */
  
  /* Draw a red triangle */
  /* Here we are testing that the disable attribute works; if it doesn't
   * the triangle will remain faded blue */
  cogl_translate (100, 0, 0);
  cogl_mesh_disable_attribute (state->mesh, "gl_Color::blue");
  cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
  cogl_mesh_draw_arrays (state->mesh,
			 GL_TRIANGLE_STRIP, /* mode */
			 0, /* first */
			 3); /* count */

  /* Draw a faded blue triangle */
  /* Here we are testing that the re-enable works; if it doesn't
   * the triangle will remain red */
  cogl_translate (100, 0, 0);
  cogl_mesh_enable_attribute (state->mesh, "gl_Color::blue");
  cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
  cogl_mesh_draw_arrays (state->mesh,
			 GL_TRIANGLE_STRIP, /* mode */
			 0, /* first */
			 3); /* count */
  
  /* XXX: Experiments have shown that for some buggy drivers, when using
   * glReadPixels there is some kind of race, so we delay our test for a
   * few frames and a few seconds:
   */
  if (state->frame >= 2)
    validate_result (state);
  else
    g_usleep (G_USEC_PER_SEC);
  
  state->frame++;
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

void
test_mesh_contiguous (TestConformSimpleFixture *fixture,
		      gconstpointer data)
{
  TestState state;
  ClutterActor *stage;
  ClutterColor stage_clr = {0x0, 0x0, 0x0, 0xff};
  ClutterActor *group;
  guint idle_source;

  state.frame = 0;

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_clr);
  clutter_actor_get_geometry (stage, &state.stage_geom);

  group = clutter_group_new ();
  clutter_actor_set_size (group,
			  state.stage_geom.width,
			  state.stage_geom.height);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = g_idle_add (queue_redraw, stage);

  g_signal_connect (group, "paint", G_CALLBACK (on_paint), &state);
  
  {
    GLfloat triangle_verts[3][2] =
      {
	{0.0,	0.0},
	{100.0, 100.0},
	{0.0,	100.0}
      };
    GLbyte triangle_colors[3][4] =
      {
	{0x00, 0x00, 0xff, 0xff}, /* blue */
	{0x00, 0x00, 0xff, 0x00}, /* transparent blue */
	{0x00, 0x00, 0xff, 0x00}  /* transparent blue */
      };
    state.mesh = cogl_mesh_new (3 /* n vertices */);
    cogl_mesh_add_attribute (state.mesh,
			     "gl_Vertex",
			     2, /* n components */
			     GL_FLOAT,
			     FALSE, /* normalized */
			     0, /* stride */
			     triangle_verts);
    cogl_mesh_add_attribute (state.mesh,
			     "gl_Color::blue",
			     4, /* n components */
			     GL_UNSIGNED_BYTE,
			     FALSE, /* normalized */
			     0, /* stride */
			     triangle_colors);
    cogl_mesh_submit (state.mesh);
  }

  clutter_actor_show_all (stage);

  clutter_main ();

  cogl_mesh_unref (state.mesh);

  g_source_remove (idle_source);

  if (g_test_verbose ())
    g_print ("OK\n");
}

