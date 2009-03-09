#include <config.h>
#include <glib.h>
#include <gmodule.h>
#include <stdlib.h>
#include <math.h>
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

static GType test_coglbox_get_type (void) G_GNUC_CONST;

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
test_coglbox_paint (ClutterActor *self)
{
  TestCoglboxPrivate *priv = TEST_COGLBOX_GET_PRIVATE (self);
  gfloat texcoords[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
  gfloat sin_frame, cos_frame;
  gfloat frac_frame;
  gint pingpong_frame;
  gint t;

  sin_frame = sinf ((float) priv->frame);
  cos_frame = cosf ((float) priv->frame);
  
  pingpong_frame = (priv->frame <= 180 ? priv->frame : 360 - priv->frame);
  frac_frame = (float) pingpong_frame / 180.0;
  frac_frame += 0.5;
  frac_frame *= 2;
  
  for (t=0; t<4; t+=2)
    {
      texcoords[t]   += cos_frame;
      texcoords[t+1] += sin_frame;
      
      texcoords[t]   = (texcoords[t] * frac_frame);
      texcoords[t+1] = (texcoords[t+1] * frac_frame);
    }
  
  priv = TEST_COGLBOX_GET_PRIVATE (self);
  
  cogl_push_matrix ();

  cogl_set_source_color4ub (0x66, 0x66, 0xdd, 0xff);
  cogl_rectangle (0, 0, 400, 400);
  
  cogl_translate (100, 100, 0);
  cogl_set_source_texture (priv->cogl_tex_id);
  cogl_rectangle_with_texture_coords (0, 0, 200, 213,
                                      texcoords[0], texcoords[1],
                                      texcoords[2], texcoords[3]);
  
  cogl_pop_matrix();
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
  self->priv = priv = TEST_COGLBOX_GET_PRIVATE(self);
  
  priv->cogl_tex_id = cogl_texture_new_from_file ("redhand.png", 0,
                                                  COGL_TEXTURE_NONE,
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

static ClutterActor*
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

G_MODULE_EXPORT int
test_cogl_tex_tile_main (int argc, char *argv[])
{
  ClutterActor     *stage;
  ClutterActor     *coglbox;
  ClutterTimeline  *timeline;
  
  clutter_init(&argc, &argv);
  
  /* Stage */
  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 400, 400);
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
