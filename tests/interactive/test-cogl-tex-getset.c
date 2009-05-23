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
  CoglHandle cogl_tex_id[4];
};

/* Coglbox implementation
 *--------------------------------------------------*/

static void
test_coglbox_paint(ClutterActor *self)
{
  TestCoglboxPrivate *priv = TEST_COGLBOX_GET_PRIVATE (self);
  gfloat texcoords[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

  priv = TEST_COGLBOX_GET_PRIVATE (self);
  
  cogl_set_source_color4ub (0x66, 0x66, 0xdd, 0xff);
  cogl_rectangle (0, 0, 400, 400);

  cogl_push_matrix ();

  cogl_translate (100, 100, 0);
  cogl_set_source_texture (priv->cogl_tex_id[1]);
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
  cogl_handle_unref (priv->cogl_tex_id);
  
  G_OBJECT_CLASS (test_coglbox_parent_class)->dispose (object);
}

static void
test_coglbox_init (TestCoglbox *self)
{
  TestCoglboxPrivate *priv;
  guint            width;
  guint            height;
  guint            rowstride;
  CoglPixelFormat  format;
  gint             size;
  guchar          *data;
  gint             x,y,t;
  guchar          *pixel;
  
  self->priv = priv = TEST_COGLBOX_GET_PRIVATE(self);
  
  /* Load image from file */
  
  priv->cogl_tex_id[0] =
    cogl_texture_new_from_file ("redhand.png",
                                COGL_TEXTURE_NONE,
                                COGL_PIXEL_FORMAT_ANY, NULL);
  
  if (priv->cogl_tex_id[0] == COGL_INVALID_HANDLE)
    {
      printf ("Failed loading redhand.png image!\n");
      return;
    }
  
  printf("Texture loaded from file.\n");
  
  /* Obtain pixel data */
  
  format = cogl_texture_get_format (priv->cogl_tex_id[0]);
  g_assert(format == COGL_PIXEL_FORMAT_RGBA_8888 ||
           format == COGL_PIXEL_FORMAT_ARGB_8888);

  width = cogl_texture_get_width (priv->cogl_tex_id[0]);
  height = cogl_texture_get_height (priv->cogl_tex_id[0]);
  size = cogl_texture_get_data (priv->cogl_tex_id[0],
				format, 0, NULL);
  
  printf("size: %dx%d\n", width, height);
  printf("format: 0x%x\n", format);
  printf("bytesize: %d\n", size);
  
  data = (guchar*) g_malloc (sizeof(guchar) * size);
  
  cogl_texture_get_data (priv->cogl_tex_id[0],
			 format, 0, data);
  rowstride = cogl_texture_get_rowstride (priv->cogl_tex_id[0]);
  
  /* Create new texture from modified data */
  
  priv->cogl_tex_id[1] =
    cogl_texture_new_from_data (width, height,
                                COGL_TEXTURE_NONE,
                                format, format,
				rowstride, data);
  
  if (priv->cogl_tex_id[1] == COGL_INVALID_HANDLE)
    {
      printf ("Failed creating image from data!\n");
      return;
    }
  
  printf ("Texture created from data.\n");
  
  /* Modify data (swap red and green) */
  
  for (y=0; y<height; ++y)
    {
      for (x=0; x<width; ++x)
	{
	  pixel = data + y * rowstride + x * 4;
	  if (format == COGL_PIXEL_FORMAT_RGBA_8888)
	    {
	      t = pixel[0];
	      pixel[0] = pixel[1];
	      pixel[1] = t;
	    }
	  else
	    {
	      t = pixel[1];
	      pixel[1] = pixel[2];
	      pixel[2] = t;
	    }
	}
    }
  
  
  cogl_texture_set_region (priv->cogl_tex_id[1],
			   0, 0, 0, 0,
			   100, 100, width, height,
			   format, 0, data);
  
  cogl_texture_set_region (priv->cogl_tex_id[1],
			   100, 100, 100, 100,
			   100, 100, width, height,
			   format, 0, data);
  
  printf ("Subregion data updated.\n");
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

G_MODULE_EXPORT int
test_cogl_tex_getset_main (int argc, char *argv[])
{
  ClutterActor     *stage;
  ClutterActor     *coglbox;
  
  clutter_init(&argc, &argv);
  
  /* Stage */
  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Cogl Test");
  
  /* Cogl Box */
  coglbox = test_coglbox_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), coglbox);
  
  clutter_actor_show_all (stage);
  
  clutter_main ();
  
  return 0;
}
