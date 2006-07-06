/* HACK HACK. HACK.  
 * This is just a quick hack to see if a 3D type video cube
 * would be possible with clutter. It needs many hacks cleaning.
 */

#include <clutter/clutter.h>
#include <glib-object.h>

#include <math.h> 		/* for M_PI */

#define WINWIDTH 800
#define WINHEIGHT 600

/* lazy globals */
static float xrot, yrot, zrot;

/* Avoid needing GLUT perspective call */
static void
frustum (GLfloat left,
	 GLfloat right,
	 GLfloat bottom,
	 GLfloat top,
	 GLfloat nearval,
	 GLfloat farval)
{
  GLfloat x, y, a, b, c, d;
  GLfloat m[16];

  x = (2.0 * nearval) / (right - left);
  y = (2.0 * nearval) / (top - bottom);
  a = (right + left) / (right - left);
  b = (top + bottom) / (top - bottom);
  c = -(farval + nearval) / ( farval - nearval);
  d = -(2.0 * farval * nearval) / (farval - nearval);

#define M(row,col)  m[col*4+row]
  M(0,0) = x;     M(0,1) = 0.0F;  M(0,2) = a;      M(0,3) = 0.0F;
  M(1,0) = 0.0F;  M(1,1) = y;     M(1,2) = b;      M(1,3) = 0.0F;
  M(2,0) = 0.0F;  M(2,1) = 0.0F;  M(2,2) = c;      M(2,3) = d;
  M(3,0) = 0.0F;  M(3,1) = 0.0F;  M(3,2) = -1.0F;  M(3,3) = 0.0F;
#undef M

  glMultMatrixf (m);
}

static void
perspective (GLfloat fovy,
	     GLfloat aspect,
	     GLfloat zNear,
	     GLfloat zFar)
{
  GLfloat xmin, xmax, ymin, ymax;

  ymax = zNear * tan (fovy * M_PI / 360.0);
  ymin = -ymax;
  xmin = ymin * aspect;
  xmax = ymax * aspect;

  frustum (xmin, xmax, ymin, ymax, zNear, zFar);
}


/* video texture subclass */

#define CLUTTER_TYPE_VIDEO_TEXTURE_CUBE clutter_video_texture_cube_get_type()

#define CLUTTER_VIDEO_TEXTURE_CUBE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_VIDEO_TEXTURE_CUBE, ClutterVideoTextureCube))

#define CLUTTER_VIDEO_TEXTURE_CUBE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_VIDEO_TEXTURE_CUBE, ClutterVideoTextureCubeClass))

#define CLUTTER_IS_VIDEO_TEXTURE_CUBE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_VIDEO_TEXTURE_CUBE))

#define CLUTTER_IS_VIDEO_TEXTURE_CUBE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_VIDEO_TEXTURE_CUBE))

#define CLUTTER_VIDEO_TEXTURE_CUBE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_VIDEO_TEXTURE_CUBE, ClutterVideoTextureCubeClass))

typedef struct ClutterVideoTextureCubePrivate ClutterVideoTextureCubePrivate ;

typedef struct ClutterVideoTextureCube
{
  ClutterVideoTexture             parent;
  ClutterVideoTextureCubePrivate *priv;

} 
ClutterVideoTextureCube;

typedef struct ClutterVideoTextureCubeClass 
{
  ClutterVideoTextureClass parent_class;
} 
ClutterVideoTextureCubeClass;

GType clutter_video_texture_cube_get_type (void);

G_DEFINE_TYPE (ClutterVideoTextureCube, clutter_video_texture_cube, CLUTTER_TYPE_VIDEO_TEXTURE);


static void
clutter_video_texture_cube_paint (ClutterActor *self)
{
  if (clutter_texture_get_pixbuf (CLUTTER_TEXTURE(self)) == NULL)
    return;

  if (!CLUTTER_ACTOR_IS_REALIZED (CLUTTER_ACTOR(self)))
      clutter_actor_realize (CLUTTER_ACTOR(self));

  if (!clutter_texture_has_generated_tiles (CLUTTER_TEXTURE(self)))
    return;

  /* HACK: sets up a 3D tranform matrix other than regular 2D one */
  /* FIXME: figure out how to nicely combine both within clutter */
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  perspective (45.0f, 
	       (GLfloat)WINWIDTH/(GLfloat)WINHEIGHT, 
	       0.1f,
	       100.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  /* back camera out a little */
  glTranslatef(0.0f,0.0f,-3.0f);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);

  glShadeModel(GL_SMOOTH);
  glClearDepth(1.0f);
  glDepthFunc(GL_LEQUAL);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  glRotatef(xrot,1.0f,0.0f,0.0f);
  glRotatef(yrot,0.0f,1.0f,0.0f);
  glRotatef(zrot,0.0f,0.0f,1.0f);

  /* HACK: Cheat as just bind to first tiled as squared   */
  clutter_texture_bind_tile (CLUTTER_TEXTURE(self), 0);

  glBegin(GL_QUADS);
  // Front Face
  glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
  glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
  glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
  glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
  // Back Face
  glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
  glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
  glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
  glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
  // Top Face
  glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
  glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
  glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
  glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
  // Bottom Face
  glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
  glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
  glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
  glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
  // Right face
  glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
  glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
  glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
  glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
  // Left Face
  glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
  glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
  glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
  glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
  glEnd();


  /* HACK: reset to regular transform */
#if 0
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (0, WINWIDTH, WINHEIGHT, 0, -1, 1);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
#endif

  /* rotate */
  xrot+=1.0f;
  yrot+=1.0f;
  zrot+=1.0f;
}

static void
clutter_video_texture_cube_class_init (ClutterVideoTextureCubeClass *klass)
{
  GObjectClass        *gobject_class;
  ClutterActorClass *actor_class;

  gobject_class = (GObjectClass*)klass;
  actor_class = (ClutterActorClass*)klass;

  actor_class->paint = clutter_video_texture_cube_paint;
}

static void
clutter_video_texture_cube_init (ClutterVideoTextureCube *self)
{

}

ClutterActor*
clutter_video_texture_cube_new (GError **err)
{
  return CLUTTER_ACTOR(g_object_new (CLUTTER_TYPE_VIDEO_TEXTURE_CUBE, 
				       /* "tiled", FALSE, */
				       NULL));
}


int
main (int argc, char *argv[])
{
  ClutterActor      *label, *texture, *vtexture; 
  ClutterActor      *stage;
  GdkPixbuf         *pixbuf;
  GError            *err = NULL;

  if (argc < 2)
    g_error("%s <video file>", argv[0]);

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  g_signal_connect (stage, "key-press-event",
		    G_CALLBACK (clutter_main_quit), NULL);

  pixbuf = gdk_pixbuf_new_from_file ("clutter-logo-800x600.png", NULL);

  if (!pixbuf)
    g_error("pixbuf load failed");

  clutter_actor_set_size (stage, 
			    WINWIDTH, WINHEIGHT);

  texture = clutter_texture_new_from_pixbuf (pixbuf);

  vtexture = clutter_video_texture_cube_new (&err);

  if (vtexture == NULL || err != NULL)
    {
      g_error("failed to create vtexture, err: %s", err->message);
    }

  clutter_media_set_filename (CLUTTER_MEDIA(vtexture), argv[1]);

  clutter_group_add (CLUTTER_GROUP (stage), texture);
  clutter_group_add (CLUTTER_GROUP (stage), vtexture);
  clutter_group_show_all (CLUTTER_GROUP (stage));

  clutter_media_set_playing(CLUTTER_MEDIA(vtexture), TRUE);

  clutter_main();

  return 0;
}
