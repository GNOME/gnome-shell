#include <config.h>
#include <glib.h>
#include <stdlib.h>
#include <clutter/clutter.h>
#include <cogl/cogl.h>

/* Coglbox declaration
 *--------------------------------------------------*/

G_BEGIN_DECLS
  
#define TEST_TYPE_COGLBOX test_coglbox_get_type()

#define TEST_COGLBOX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TEST_TYPE_COGLBOX, TestCoglboxClass))

#define TEST_COGLBOX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TEST_TYPE_COGLBOX, TestCoglboxClass))

#define TEST_IS_COGLBOX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TEST_TYPE_COGLBOX))

#define TEST_IS_COGLBOX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TEST_TYPE_COGLBOX))

#define TEST_COGLBOX_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TEST_TYPE_COGLBOX, TestCoglboxClass))

typedef struct _TestCoglbox        TestCoglbox;
typedef struct _TestCoglboxClass   TestCoglboxClass;
typedef struct _TestCoglboxPrivate TestCoglboxPrivate;

struct _TestCoglbox
{
  ClutterActor           parent;

  /*< private >*/
  TestCoglboxPrivate *priv;
};

struct _TestCoglboxClass 
{
  ClutterActorClass parent_class;

  /* padding for future expansion */
  void (*_test_coglbox1) (void);
  void (*_test_coglbox2) (void);
  void (*_test_coglbox3) (void);
  void (*_test_coglbox4) (void);
};

GType test_coglbox_get_type (void) G_GNUC_CONST;

G_END_DECLS

/* Coglbox private declaration
 *--------------------------------------------------*/

G_DEFINE_TYPE (TestCoglbox, test_coglbox, CLUTTER_TYPE_ACTOR);

#define TEST_COGLBOX_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), TEST_TYPE_COGLBOX, TestCoglboxPrivate))

struct _TestCoglboxPrivate
{
  CoglHandle cogl_tex_id;
  gint       frame;
};

/* Coglbox implementation
 *--------------------------------------------------*/

static void
test_coglbox_fade_texture (CoglHandle tex_id,
			   ClutterFixed x1,
			   ClutterFixed y1,
			   ClutterFixed x2,
			   ClutterFixed y2,
			   ClutterFixed tx1,
			   ClutterFixed ty1,
			   ClutterFixed tx2,
			   ClutterFixed ty2)
{
  CoglTextureVertex vertices[4];
  int i;
  static const ClutterColor white = { 0xff, 0xff, 0xff, 0xff };
  
  vertices[0].x = x1;
  vertices[0].y = y1;
  vertices[0].z = 0;
  vertices[0].tx = tx1;
  vertices[0].ty = ty1;
  vertices[1].x = x1;
  vertices[1].y = y2;
  vertices[1].z = 0;
  vertices[1].tx = tx1;
  vertices[1].ty = ty2;
  vertices[2].x = x2;
  vertices[2].y = y2;
  vertices[2].z = 0;
  vertices[2].tx = tx2;
  vertices[2].ty = ty2;
  vertices[3].x = x2;
  vertices[3].y = y1;
  vertices[3].z = 0;
  vertices[3].tx = tx2;
  vertices[3].ty = ty1;

  for (i = 0; i < 4; i++)
    {
      vertices[i].color.red = 255;
      vertices[i].color.green = 255;
      vertices[i].color.blue = 255;
      vertices[i].color.alpha = ((i ^ (i >> 1)) & 1) ? 0 : 128;
    }

  cogl_texture_polygon (tex_id, 4, vertices, TRUE);

  cogl_color (&white);
}

static void
test_coglbox_triangle_texture (CoglHandle tex_id,
			       ClutterFixed x,
			       ClutterFixed y,
			       ClutterFixed tx1,
			       ClutterFixed ty1,
			       ClutterFixed tx2,
			       ClutterFixed ty2,
			       ClutterFixed tx3,
			       ClutterFixed ty3)
{
  CoglTextureVertex vertices[3];
  int tex_width = cogl_texture_get_width (tex_id);
  int tex_height = cogl_texture_get_height (tex_id);
  
  vertices[0].x = x + tx1 * tex_width;
  vertices[0].y = y + ty1 * tex_height;
  vertices[0].z = 0;
  vertices[0].tx = tx1;
  vertices[0].ty = ty1;

  vertices[1].x = x + tx2 * tex_width;
  vertices[1].y = y + ty2 * tex_height;
  vertices[1].z = 0;
  vertices[1].tx = tx2;
  vertices[1].ty = ty2;

  vertices[2].x = x + tx3 * tex_width;
  vertices[2].y = y + ty3 * tex_height;
  vertices[2].z = 0;
  vertices[2].tx = tx3;
  vertices[2].ty = ty3;

  cogl_texture_polygon (tex_id, 3, vertices, FALSE);
}

static void
test_coglbox_paint (ClutterActor *self)
{
  TestCoglboxPrivate *priv = TEST_COGLBOX_GET_PRIVATE (self);
  int tex_width = cogl_texture_get_width (priv->cogl_tex_id);
  int tex_height = cogl_texture_get_height (priv->cogl_tex_id);
  int i;

  for (i = 0; i < 2; i++)
    {
      cogl_texture_set_filters (priv->cogl_tex_id,
				i ? CGL_LINEAR : CGL_NEAREST,
				i ? CGL_LINEAR : CGL_NEAREST);

      if (i)
	{
	  cogl_push_matrix ();
	  cogl_translate (tex_width, tex_height, 0);
	}
      
      cogl_push_matrix ();
      cogl_translate (tex_width / 2, 0, 0);
      cogl_rotate (priv->frame, 0, 1, 0);
      cogl_translate (-tex_width / 2, 0, 0);

      /* Draw a hand and refect it */
      cogl_texture_rectangle (priv->cogl_tex_id,
			      0, 0,
			      CLUTTER_INT_TO_FIXED (tex_width),
			      CLUTTER_INT_TO_FIXED (tex_height),
			      0, 0, CFX_ONE, CFX_ONE);
      test_coglbox_fade_texture (priv->cogl_tex_id,
				 0, CLUTTER_INT_TO_FIXED (tex_height),
				 CLUTTER_INT_TO_FIXED (tex_width),
				 CLUTTER_INT_TO_FIXED (tex_height * 3 / 2),
				 0, CFX_ONE,
				 CFX_ONE, CFX_ONE / 2);

      cogl_pop_matrix ();

      cogl_push_matrix ();
      cogl_translate (tex_width * 3 / 2 + 60, 0, 0);
      cogl_rotate (priv->frame, 0, 1, 0);
      cogl_translate (-tex_width / 2 - 10, 0, 0);

      /* Draw the texture split into two triangles */
      test_coglbox_triangle_texture (priv->cogl_tex_id,
				     0, 0,
				     0, 0,
				     0, CFX_ONE,
				     CFX_ONE, CFX_ONE);
      test_coglbox_triangle_texture (priv->cogl_tex_id,
				     CLUTTER_INT_TO_FIXED (20), 0,
				     0, 0,
				     CFX_ONE, 0,
				     CFX_ONE, CFX_ONE);

      cogl_pop_matrix ();

      if (i)
	cogl_pop_matrix ();
    }
}

static void
test_coglbox_finalize (GObject *object)
{
  G_OBJECT_CLASS (test_coglbox_parent_class)->finalize (object);
}

static void
test_coglbox_dispose (GObject *object)
{
  TestCoglboxPrivate *priv;
  
  priv = TEST_COGLBOX_GET_PRIVATE (object);
  cogl_texture_unref (priv->cogl_tex_id);
  
  G_OBJECT_CLASS (test_coglbox_parent_class)->dispose (object);
}

static void
test_coglbox_init (TestCoglbox *self)
{
  TestCoglboxPrivate *priv;
  self->priv = priv = TEST_COGLBOX_GET_PRIVATE (self);
  
  priv->cogl_tex_id = cogl_texture_new_from_file ("redhand.png", 0,
						  COGL_PIXEL_FORMAT_ANY,
						  NULL);
  
  cogl_texture_set_filters (priv->cogl_tex_id,
			    CGL_LINEAR, CGL_LINEAR);
}

static void
test_coglbox_class_init (TestCoglboxClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class   = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->finalize     = test_coglbox_finalize;
  gobject_class->dispose      = test_coglbox_dispose;  
  actor_class->paint          = test_coglbox_paint;
  
  g_type_class_add_private (gobject_class, sizeof (TestCoglboxPrivate));
}

ClutterActor*
test_coglbox_new (void)
{
  return g_object_new (TEST_TYPE_COGLBOX, NULL);
}

static void
frame_cb (ClutterTimeline *timeline,
	  gint             frame_num,
	  gpointer         data)
{
  TestCoglboxPrivate *priv = TEST_COGLBOX_GET_PRIVATE (data);
  
  priv->frame = frame_num;
  clutter_actor_queue_redraw (CLUTTER_ACTOR (data));
}

int
main (int argc, char *argv[])
{
  ClutterActor     *stage;
  ClutterActor     *coglbox;
  ClutterTimeline  *timeline;
  ClutterColor      blue = { 0x30, 0x30, 0xff, 0xff };
  
  clutter_init (&argc, &argv);
  
  /* Stage */
  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &blue);
  clutter_actor_set_size (stage, 800, 600);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Cogl Test");
  
  /* Cogl Box */
  coglbox = test_coglbox_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), coglbox);
  
  /* Timeline for animation */
  timeline = clutter_timeline_new (360, 60); /* num frames, fps */
  g_object_set (timeline, "loop", TRUE, NULL);   /* have it loop */
  g_signal_connect (timeline, "new-frame", G_CALLBACK (frame_cb), coglbox);
  clutter_timeline_start (timeline);
  
  clutter_actor_show_all (stage);
  
  clutter_main ();
  
  return 0;
}
