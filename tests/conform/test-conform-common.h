
/* Stuff you put in here is setup once in main() and gets passed around to
 * all test functions and fixture setup/teardown functions in the data
 * argument */
typedef struct _TestConformSharedState
{
  int	 *argc_addr;
  char ***argv_addr;
} TestConformSharedState;


/* This fixture structure is allocated by glib, and before running each test
 * the test_conform_simple_fixture_setup func (see below) is called to
 * initialise it, and test_conform_simple_fixture_teardown is called when
 * the test is finished. */
typedef struct _TestConformSimpleFixture
{
  /**/
  int dummy;
} TestConformSimpleFixture;

typedef struct _TestConformTodo
{
  gchar *name;
  void (* func) (TestConformSimpleFixture *, gconstpointer);
} TestConformTodo;

typedef struct _TestConformGLFunctions
{
  const GLubyte * (* glGetString) (GLenum name);
  void (* glGetIntegerv) (GLenum pname, GLint *params);
  void (* glPixelStorei) (GLenum pname, GLint param);
  void (* glBindTexture) (GLenum target, GLuint texture);
  void (* glGenTextures) (GLsizei n, GLuint *textures);
  GLenum (* glGetError) (void);
  void (* glDeleteTextures) (GLsizei n, const GLuint *textures);
  void (* glTexImage2D) (GLenum target, GLint level,
                         GLint internalFormat,
                         GLsizei width, GLsizei height,
                         GLint border, GLenum format, GLenum type,
                         const GLvoid *pixels);
  void (* glTexParameteri) (GLenum target, GLenum pname, GLint param);
} TestConformGLFunctions;

void test_conform_get_gl_functions (TestConformGLFunctions *functions);

void test_conform_simple_fixture_setup (TestConformSimpleFixture *fixture,
					gconstpointer data);
void test_conform_simple_fixture_teardown (TestConformSimpleFixture *fixture,
					   gconstpointer data);

gchar *clutter_test_get_data_file (const gchar *filename);
