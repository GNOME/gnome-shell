# C Coding Style

The coding style used is primarily the GNU flavor of the [GNOME coding
style][gnome-coding-style], with some additions described below.

## General

 * Use this code style on new code. When changing old code with a different
   code style, feel free to also adjust it to use this code style.

 * Use regular C types and `stdint.h` types instead of GLib fundamental
   types, except for `gboolean`, and `guint`/`gulong` for GSource IDs and
   signal handler IDs. That means e.g. `uint64_t` instead of `guint64`, `int`
   instead of `gint`, `unsigned int` instead of `guint` if unsignedness
   is of importance, `uint8_t` instead of `guchar`, and so on.

 * Try to to limit line length to 80 characters, although it's not a
   strict limit.

 * Usage of `g_autofree` and `g_autoptr` is encouraged. The style to use is

    ```c
    g_autofree char *text = NULL;
    g_autoptr (StSomeThing) thing = NULL;

    text = g_strdup_printf ("The text: %d", a_number);
    thing = g_object_new (ST_TYPE_SOME_THING,
                          "text", text,
                          NULL);
    thinger_use_thing (rocket, thing);
    ```

 * Declare variables at the top of the block they are used, but avoid
   non-trivial logic among variable declarations. Non-trivial logic can be
   getting a pointer that may be `NULL`, any kind of math, or anything
   that may have side effects.

 * Instead of boolean arguments in functions, prefer enums or flags when
   they're more expressive. The naming convention for flags is

    ```c
    typedef _StSomeThingFlags
    {
      ST_SOME_THING_FLAG_NONE = 0,
      ST_SOME_THING_FLAG_ALTER_REALITY = 1 << 0,
      ST_SOME_THING_FLAG_MANIPULATE_PERCEPTION = 1 << 1,
    } StSomeThingFlags;
    ```

 * Use `g_new0 ()` etc. instead of `g_slice_new0 ()`.

 * Initialize and assign floating point variables (i.e. `float` or
   `double`) using the form `floating_point = 3.14159` or `ratio = 2.0`.

## Naming conventions

 * For object instance pointers, use a descriptive name instead of `self`, e.g.

```c
G_DEFINE_TYPE (StPlaceholder, st_placeholder, G_TYPE_OBJECT)

...

void
st_placeholder_hold_place (StPlaceholder *placeholder)
{
  ...
}
```

## Header (.h) files

 * The return type and `*` are separated by a space.
 * Function name starts one space after the last `*`.
 * Parenthesis comes one space after the function name.

As an example, this is how functions in a header file should look like:

```c
gboolean st_icon_theme_has_icon (StIconTheme *icon_theme,
                                 const char  *icon_name);

GList * st_icon_theme_list_icons (StIconTheme *icon_theme,
                                  const char  *context);

StIconInfo * st_icon_info_new_for_pixbuf (StIconTheme *icon_theme,
                                          GdkPixbuf   *pixbuf);
```

## Source code

Keep functions in the following order in source files:

  1. GPL header
  2. Include header files
  3. Enums
  4. Structures
  5. Function prototypes
  6. `G_DEFINE_TYPE()`
  7. Static variables
  8. Auxiliary functions
  9. Callbacks
  10. Interface implementations
  11. Parent vfunc overrides
  12. class_init and init
  13. Public API

### Include header files

Source files should use the header include order of the following example:

* `st-example.c`:
```c
#include "config.h"

#include "st-example-private.h"

#include <glib-object.h>
#include <stdint.h>

#ifdef HAVE_MALLINFO2
#include <malloc.h>
#endif

#include "st-private.h"

#include "st-dbus-file-generated-by-gdbus-codegen.h"
```

### Structures

Each structure field has a space after their type name. Structure fields aren't
aligned. For example:

```c
struct _StFooBar
{
  StFoo parent;

  StBar *bar;
  StSomething *something;
};
```

### Function Prototypes

Function prototypes must be formatted just like in header files.

### Overrides

When overriding parent class vfuncs, or implementing an interface, vfunc
overrides should be named as a composition of the current class prefix,
followed by the vfunc name. For example:


```c
static void
st_bar_spawn_unicorn (StParent *parent)
{
  /* ... */
}

static void
st_bar_dispose (GObject *object)
{
  /* ... */
}

static void
st_bar_finalize (GObject *object)
{
  /* ... */
}

static void
st_bar_class_init (StBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  StParentClass *parent_class = ST_PARENT_CLASS (klass);

  object_class->dispose = st_bar_dispose;
  object_class->finalize = st_bar_finalize;

  parent_class->spawn_unicorn = st_bar_spawn_unicorn;
}
```

### Interface Implementations

When implementing interfaces, two groups of functions are involved: the init
function, and the overrides.

The interface init function is named after the interface type in snake case,
followed by the `_iface_init` suffix. For example:


```c
static void st_foo_iface_init (StFooInterface *foo_iface);

G_DEFINE_TYPE_WITH_CODE (StBar, st_bar, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ST_TYPE_FOO,
                                                st_foo_iface_init));
```

Then, when implementing each vfunc of the interface, follow the same pattern
of the [Overrides](###Overrides) section. Here's an example:

```c
static void
st_bar_do_something (StFoo *foo)
{
  /* ... */
}

static void
st_foo_iface_init (StFooInterface *foo_iface)
{
  foo_iface->do_something = st_bar_do_something;
}
```

### Auxiliary Functions

Auxiliary functions are above every other functions to minimize the number of
function prototypes in the file. These functions often grow when factoring out
the same code between two or more functions:

```c
static void
do_something_on_data (Foo *data,
                      Bar *bar)
{
  /* ... */
}

static void
random_function (Foo *foo)
{
  do_something_on_data (foo, bar);
}

static void
another_random_function (Foo *foo)
{
  do_something_on_data (foo, bar);
}
```

Sometimes, however, auxiliary functions are created to break down otherwise
large functions - in this case, it is appropriate to keep these auxiliary
functions close to the function they are tightly related to.

Auxiliary function names must have a verb in the imperative form, and should
always perform an action over something. They usually don't have the class
prefix (`st_` or `shell_`). For example:

```c
static void
do_something_on_data (Foo *data,
                      Bar *bar)
{
  /* ... */
}
```

Exceptionally, when converting between types, auxiliary function names may
have the class prefix to this rule. For example:

```c
static StFoo *
st_foo_from_bar (Bar *bar)
{
  /* ... */
}
```

### Callback Functions

Callback function names should have the name of the action in imperative
form. They don't have any prefix, but have a `_func` suffix. For example:

```c
static void
filter_something_func (Foo      *foo,
                       Bar      *bar,
                       gpointer  user_data)
{
  /* ... */
}
```

### Signal Callbacks

Signal callbacks generally have the signal name. They should be prefixed with
`on_`, or suffixed with `_cb`, but not both. For example:

```c
static void
on_realize (ClutterActor *actor,
            gpointer      user_data)
{
  /* ... */
}

static void
destroy_cb (ClutterActor *actor,
            gpointer      user_data)
{
  /* ... */
}
```

When the callback is named after the object that generated it, and the signal,
then passive voice is used. For example:

```c
static void
click_action_clicked_cb (ClutterClickAction *click_action,
                         ClutterActor       *actor,
                         gpointer            user_data)
{
  /* ... */
}
```

[gnome-coding-style]: https://developer.gnome.org/documentation/guidelines/programming/coding-style.html
