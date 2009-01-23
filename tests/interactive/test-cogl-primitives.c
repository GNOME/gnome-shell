#include <config.h>
#include <glib.h>
#include <gmodule.h>
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

static GType test_coglbox_get_type (void) G_GNUC_CONST;

G_END_DECLS

/* Coglbox private declaration
 *--------------------------------------------------*/

G_DEFINE_TYPE (TestCoglbox, test_coglbox, CLUTTER_TYPE_ACTOR);

#define TEST_COGLBOX_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), TEST_TYPE_COGLBOX, TestCoglboxPrivate))

struct _TestCoglboxPrivate
{
  void (*_test_coglbox_priv1) (void);
};

/* Coglbox implementation
 *--------------------------------------------------*/

typedef void (*PaintFunc) (void);

static void
test_paint_line ()
{
  cogl_path_line (CLUTTER_INT_TO_FIXED (-50),
                  CLUTTER_INT_TO_FIXED (-25),
                  CLUTTER_INT_TO_FIXED (50),
                  CLUTTER_INT_TO_FIXED (25));
}

static void
test_paint_rect ()
{
  cogl_path_rectangle (CLUTTER_INT_TO_FIXED (-50),
                       CLUTTER_INT_TO_FIXED (-25),
                       CLUTTER_INT_TO_FIXED (100),
                       CLUTTER_INT_TO_FIXED (50));
}

static void
test_paint_rndrect()
{
  cogl_path_round_rectangle (CLUTTER_INT_TO_FIXED (-50),
                             CLUTTER_INT_TO_FIXED (-25),
                             CLUTTER_INT_TO_FIXED (100),
                             CLUTTER_INT_TO_FIXED (50),
                             CLUTTER_INT_TO_FIXED (10),
                             5);
}

static void
test_paint_polyl ()
{
  ClutterFixed poly_coords[] = {
    CLUTTER_INT_TO_FIXED (-50),
    CLUTTER_INT_TO_FIXED (-50),
    CLUTTER_INT_TO_FIXED (+50),
    CLUTTER_INT_TO_FIXED (-30),
    CLUTTER_INT_TO_FIXED (+30),
    CLUTTER_INT_TO_FIXED (+30),
    CLUTTER_INT_TO_FIXED (-30),
    CLUTTER_INT_TO_FIXED (+40)
  };
  
  cogl_path_polyline (poly_coords, 4);
}

static void
test_paint_polyg ()
{
  gfloat poly_coords[] = {
    CLUTTER_INT_TO_FIXED (-50),
    CLUTTER_INT_TO_FIXED (-50),
    CLUTTER_INT_TO_FIXED (+50),
    CLUTTER_INT_TO_FIXED (-30),
    CLUTTER_INT_TO_FIXED (+30),
    CLUTTER_INT_TO_FIXED (+30),
    CLUTTER_INT_TO_FIXED (-30),
    CLUTTER_INT_TO_FIXED (+40)
  };
  
  cogl_path_polygon (poly_coords, 4);
}

static void
test_paint_elp ()
{
  cogl_path_ellipse (0, 0,
                     CLUTTER_INT_TO_FIXED (60),
                     CLUTTER_INT_TO_FIXED (40));
}

static void
test_paint_curve ()
{
  cogl_path_move_to (CLUTTER_INT_TO_FIXED (-50),
                     CLUTTER_INT_TO_FIXED (+50));
  
  cogl_path_curve_to (CLUTTER_INT_TO_FIXED (+100),
                      CLUTTER_INT_TO_FIXED (-50),
                      CLUTTER_INT_TO_FIXED (-100),
                      CLUTTER_INT_TO_FIXED (-50),
                      CLUTTER_INT_TO_FIXED (+50),
                      CLUTTER_INT_TO_FIXED (+50));
}

static PaintFunc paint_func []=
{
  test_paint_line,
  test_paint_rect,
  test_paint_rndrect,
  test_paint_polyl,
  test_paint_polyg,
  test_paint_elp,
  test_paint_curve
};

static void
test_coglbox_paint(ClutterActor *self)
{
  TestCoglboxPrivate *priv;
  
  static GTimer *timer = NULL;
  static gint paint_index = 0;
  
  gint NUM_PAINT_FUNCS;
 
  NUM_PAINT_FUNCS = G_N_ELEMENTS (paint_func);
  
  priv = TEST_COGLBOX_GET_PRIVATE (self);
  
  
  if (!timer)
    {
      timer = g_timer_new ();
      g_timer_start (timer);
    }
  
  if (g_timer_elapsed (timer, NULL) >= 1)
    {
      paint_index += 1;
      paint_index = paint_index % NUM_PAINT_FUNCS;
      g_timer_start (timer);
    }
 
  cogl_push_matrix ();
  
  paint_func[paint_index] ();

  cogl_translate (100, 100, 0);
  cogl_set_source_color4ub (0, 160, 0, 255);
  cogl_path_stroke_preserve ();

  cogl_translate (150, 0, 0);
  cogl_set_source_color4ub (200, 0, 0, 255);
  cogl_path_fill ();
  
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
  
  G_OBJECT_CLASS (test_coglbox_parent_class)->dispose (object);
}

static void
test_coglbox_init (TestCoglbox *self)
{
  TestCoglboxPrivate *priv;
  self->priv = priv = TEST_COGLBOX_GET_PRIVATE(self);
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

#define SPIN()   while (g_main_context_pending (NULL)) \
                     g_main_context_iteration (NULL, FALSE);

G_MODULE_EXPORT int
test_cogl_primitives_main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *coglbox;
  
  clutter_init(&argc, &argv);
  
  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Cogl Test");
  
  coglbox = test_coglbox_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), coglbox);

  clutter_actor_set_rotation (coglbox, CLUTTER_Y_AXIS, -30, 200, 0, 0);
  clutter_actor_set_position (coglbox, 0, 100);
  
  clutter_actor_show_all (stage);
  
  while (1)
    {
      clutter_actor_hide (coglbox);
      clutter_actor_show (coglbox);
      SPIN();
    }
  
  return 0;
}
