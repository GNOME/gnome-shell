#include <glib.h>
#include <gmodule.h>
#include <stdlib.h>
#include <clutter/clutter.h>
#include <cogl/cogl.h>

#ifndef GL_UNPACK_ALIGNMENT
#define GL_UNPACK_ALIGNMENT 0x0CF5
#endif
#ifndef GL_TEXTURE_BINDING_2D
#define GL_TEXTURE_BINDING_2D 0x8069
#endif
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D 0x0DE1
#endif
#ifndef GL_RGB
#define GL_RGB 0x1907
#endif
#ifndef GL_UNSIGNED_BYTE
#define GL_UNSIGNED_BYTE 0x1401
#endif
#ifndef GL_TEXTURE_MAG_FILTER
#define GL_TEXTURE_MAG_FILTER 0x2800
#endif
#ifndef GL_LINEAR
#define GL_LINEAR 0x1208
#endif
#ifndef GL_TEXTURE_MIN_FILTER
#define GL_TEXTURE_MIN_FILTER 0x2801
#endif

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
  guint      gl_handle;
  CoglHandle cogl_handle;

  void
  (* glGetIntegerv) (guint pname, int *params);
  void
  (* glPixelStorei) (guint pname, int param);
  void
  (* glTexParameteri) (guint target, guint pname, int param);
  void
  (* glTexImage2D) (guint target, int level,
                    int internalFormat,
                    int width, int height,
                    int border, guint format, guint type,
                    const void *pixels);
  void
  (* glGenTextures) (int n, guint *textures);
  void
  (* glDeleteTextures) (int n, const guint *textures);
  void
  (* glBindTexture) (guint target, guint texture);
};

/* Coglbox implementation
 *--------------------------------------------------*/

static void
test_coglbox_paint(ClutterActor *self)
{
  TestCoglboxPrivate *priv = TEST_COGLBOX_GET_PRIVATE (self);
  gfloat texcoords[4] = { 0.3f, 0.3f, 0.7f, 0.7f };
  
  cogl_set_source_color4ub (0x66, 0x66, 0xdd, 0xff);
  cogl_rectangle (0,0,400,400);
  
  cogl_push_matrix ();
  
  cogl_translate (100,100,0);
  cogl_set_source_texture (priv->cogl_handle);
  cogl_rectangle_with_texture_coords (0, 0, 200, 200,
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
  
  cogl_handle_unref (priv->cogl_handle);
  priv->glDeleteTextures (1, &priv->gl_handle);
  
  G_OBJECT_CLASS (test_coglbox_parent_class)->dispose (object);
}

static void
test_coglbox_init (TestCoglbox *self)
{
  TestCoglboxPrivate *priv;
  guchar              data[12];
  int prev_unpack_alignment;
  int prev_2d_texture_binding;
  
  self->priv = priv = TEST_COGLBOX_GET_PRIVATE(self);
  
  /* Prepare a 2x2 pixels texture */
  
  data[0] = 255; data[1]  =   0; data[2]  =   0;
  data[3] =   0; data[4]  = 255; data[5]  =   0;
  data[6] =   0; data[7]  =   0; data[8]  = 255;
  data[9] =   0; data[10] =   0; data[11] =   0;

  priv->glGetIntegerv = (void *) cogl_get_proc_address ("glGetIntegerv");
  priv->glPixelStorei = (void *) cogl_get_proc_address ("glPixelStorei");
  priv->glTexParameteri = (void *) cogl_get_proc_address ("glTexParameteri");
  priv->glTexImage2D = (void *) cogl_get_proc_address ("glTexImage2D");
  priv->glGenTextures = (void *) cogl_get_proc_address ("glGenTextures");
  priv->glDeleteTextures = (void *) cogl_get_proc_address ("glDeleteTextures");
  priv->glBindTexture = (void *) cogl_get_proc_address ("glBindTexture");

  /* We are about to use OpenGL directly to create a TEXTURE_2D
   * texture so we need to save the state that we modify so we can
   * restore it afterwards and be sure not to interfere with any state
   * caching that Cogl may do internally.
   */
  priv->glGetIntegerv (GL_UNPACK_ALIGNMENT, &prev_unpack_alignment);
  priv->glGetIntegerv (GL_TEXTURE_BINDING_2D, &prev_2d_texture_binding);

  priv->glGenTextures (1, &priv->gl_handle);
  priv->glBindTexture (GL_TEXTURE_2D, priv->gl_handle);
  
  priv->glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  priv->glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB,
		2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

  /* Now restore the original GL state as Cogl had left it */
  priv->glPixelStorei (GL_UNPACK_ALIGNMENT, prev_unpack_alignment);
  priv->glBindTexture (GL_TEXTURE_2D, prev_2d_texture_binding);
  
  priv->glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  priv->glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  
  /* Create texture from foreign */
  
  priv->cogl_handle =
    cogl_texture_new_from_foreign (priv->gl_handle,
				   GL_TEXTURE_2D,
				   2, 2, 0, 0,
				   COGL_PIXEL_FORMAT_RGB_888);
  
  if (priv->cogl_handle == COGL_INVALID_HANDLE)
    {
      printf ("Failed creating texture from foreign!\n");
      return;
    }
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
test_cogl_tex_foreign_main (int argc, char *argv[])
{
  ClutterActor     *stage;
  ClutterActor     *coglbox;
  
  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;
  
  /* Stage */
  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Cogl Foreign Textures");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* Cogl Box */
  coglbox = test_coglbox_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), coglbox);
  
  clutter_actor_show_all (stage);
  
  clutter_main ();
  
  return 0;
}

G_MODULE_EXPORT const char *
test_cogl_tex_foreign_describe (void)
{
  return "Foreign textures support in Cogl.";
}
