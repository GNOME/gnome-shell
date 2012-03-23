
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
  const guint8 * (* glGetString) (guint name);
  void (* glGetIntegerv) (guint pname, int *params);
  void (* glPixelStorei) (guint pname, int param);
  void (* glBindTexture) (guint target, guint texture);
  void (* glGenTextures) (int n, guint *textures);
  guint (* glGetError) (void);
  void (* glDeleteTextures) (int n, const guint *textures);
  void (* glTexImage2D) (guint target, int level,
                         int internalFormat,
                         int width, int height,
                         int border, guint format, guint type,
                         const void *pixels);
  void (* glTexParameteri) (guint target, guint pname, int param);
} TestConformGLFunctions;

void test_conform_get_gl_functions (TestConformGLFunctions *functions);

void test_conform_simple_fixture_setup (TestConformSimpleFixture *fixture,
					gconstpointer data);
void test_conform_simple_fixture_teardown (TestConformSimpleFixture *fixture,
					   gconstpointer data);

gchar *clutter_test_get_data_file (const gchar *filename);
