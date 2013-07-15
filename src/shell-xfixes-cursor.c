/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#include <stdio.h>
#include <stdlib.h>

#include "shell-xfixes-cursor.h"

#include <gdk/gdkx.h>
#include <X11/extensions/Xfixes.h>
#include <meta/display.h>
#include <meta/screen.h>
#include <meta/util.h>

/**
 * SECTION:shell-xfixes-cursor
 * @short_description: Capture/manipulate system mouse cursor.
 *
 * The #ShellXFixesCursor object uses the XFixes extension to show/hide the
 * the system mouse pointer, to grab its image as it changes, and emit a
 * notification when its image changes.
 */

struct _ShellXFixesCursorClass
{
  GObjectClass parent_class;
};

struct _ShellXFixesCursor {
  GObject parent;

  MetaScreen *screen;

  gboolean have_xfixes;
  int xfixes_event_base;

  gboolean is_showing;

  CoglHandle *cursor_sprite;
  int cursor_hot_x;
  int cursor_hot_y;
};

static void xfixes_cursor_set_screen   (ShellXFixesCursor *xfixes_cursor,
                                        MetaScreen        *screen);

static void xfixes_cursor_reset_image (ShellXFixesCursor *xfixes_cursor);

enum {
  PROP_0,
  PROP_SCREEN,
};

G_DEFINE_TYPE(ShellXFixesCursor, shell_xfixes_cursor, G_TYPE_OBJECT);

enum {
    CURSOR_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
shell_xfixes_cursor_init (ShellXFixesCursor *xfixes_cursor)
{
  // (JS) Best (?) that can be assumed since XFixes doesn't provide a way of
  // detecting if the system mouse cursor is showing or not.
  xfixes_cursor->is_showing = TRUE;
}

static void
shell_xfixes_cursor_finalize (GObject  *object)
{
  ShellXFixesCursor *xfixes_cursor = SHELL_XFIXES_CURSOR (object);

  xfixes_cursor_set_screen (xfixes_cursor, NULL);
  if (xfixes_cursor->cursor_sprite != NULL)
    cogl_handle_unref (xfixes_cursor->cursor_sprite);

  G_OBJECT_CLASS (shell_xfixes_cursor_parent_class)->finalize (object);
}

static GdkFilterReturn
xfixes_cursor_event_filter (XEvent     *xev,
                            GdkEvent   *ev,
                            gpointer    data)
{
  ShellXFixesCursor *xfixes_cursor = data;

  if (xev->xany.window != meta_get_overlay_window (xfixes_cursor->screen))
    return GDK_FILTER_CONTINUE;

  if (xev->xany.type == xfixes_cursor->xfixes_event_base + XFixesCursorNotify)
    {
      XFixesCursorNotifyEvent *notify_event = (XFixesCursorNotifyEvent *)xev;
      if (notify_event->subtype == XFixesDisplayCursorNotify)
        xfixes_cursor_reset_image (xfixes_cursor);
    }

  return GDK_FILTER_CONTINUE;
}

static void
xfixes_cursor_set_screen (ShellXFixesCursor *xfixes_cursor,
                          MetaScreen        *screen)
{
  if (xfixes_cursor->screen == screen)
    return;

  if (xfixes_cursor->screen)
    {
      gdk_window_remove_filter (NULL, (GdkFilterFunc)xfixes_cursor_event_filter, xfixes_cursor);
    }

  xfixes_cursor->screen = screen;
  if (xfixes_cursor->screen)
    {
      int error_base;

      gdk_window_add_filter (NULL, (GdkFilterFunc)xfixes_cursor_event_filter, xfixes_cursor);

      xfixes_cursor->have_xfixes = XFixesQueryExtension (gdk_x11_get_default_xdisplay (),
                                                         &xfixes_cursor->xfixes_event_base,
                                                         &error_base);

      /* FIXME: this needs to be moved down to mutter as a whole */
      if (xfixes_cursor->have_xfixes && !meta_is_display_server())
        XFixesSelectCursorInput (gdk_x11_get_default_xdisplay (),
                                 meta_get_overlay_window (screen),
                                 XFixesDisplayCursorNotifyMask);

      xfixes_cursor_reset_image (xfixes_cursor);
    }
}

static void
xfixes_cursor_reset_image (ShellXFixesCursor *xfixes_cursor)
{
  XFixesCursorImage *cursor_image;
  CoglHandle sprite = COGL_INVALID_HANDLE;
  guint8 *cursor_data;
  gboolean free_cursor_data;

  if (!xfixes_cursor->have_xfixes)
    return;

  cursor_image = XFixesGetCursorImage (gdk_x11_get_default_xdisplay ());
  if (!cursor_image)
    return;

  /* Like all X APIs, XFixesGetCursorImage() returns arrays of 32-bit
   * quantities as arrays of long; we need to convert on 64 bit */
  if (sizeof(long) == 4)
    {
      cursor_data = (guint8 *)cursor_image->pixels;
      free_cursor_data = FALSE;
    }
  else
    {
      int i, j;
      guint32 *cursor_words;
      gulong *p;
      guint32 *q;

      cursor_words = g_new (guint32, cursor_image->width * cursor_image->height);
      cursor_data = (guint8 *)cursor_words;

      p = cursor_image->pixels;
      q = cursor_words;
      for (j = 0; j < cursor_image->height; j++)
        for (i = 0; i < cursor_image->width; i++)
          *(q++) = *(p++);

      free_cursor_data = TRUE;
    }

  sprite = cogl_texture_new_from_data (cursor_image->width,
                                       cursor_image->height,
                                       COGL_TEXTURE_NONE,
                                       CLUTTER_CAIRO_FORMAT_ARGB32,
                                       COGL_PIXEL_FORMAT_ANY,
                                       cursor_image->width * 4, /* stride */
                                       cursor_data);

  if (free_cursor_data)
    g_free (cursor_data);

  if (sprite != COGL_INVALID_HANDLE)
    {
      if (xfixes_cursor->cursor_sprite != NULL)
        cogl_handle_unref (xfixes_cursor->cursor_sprite);

      xfixes_cursor->cursor_sprite = sprite;
      xfixes_cursor->cursor_hot_x = cursor_image->xhot;
      xfixes_cursor->cursor_hot_y = cursor_image->yhot;
      g_signal_emit (xfixes_cursor, signals[CURSOR_CHANGED], 0);
    }
  XFree (cursor_image);
}

static void
shell_xfixes_cursor_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ShellXFixesCursor *xfixes_cursor = SHELL_XFIXES_CURSOR (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      xfixes_cursor_set_screen (xfixes_cursor, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_xfixes_cursor_get_property (GObject         *object,
                             guint            prop_id,
                             GValue          *value,
                             GParamSpec      *pspec)
{
  ShellXFixesCursor *xfixes_cursor = SHELL_XFIXES_CURSOR (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, G_OBJECT (xfixes_cursor->screen));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_xfixes_cursor_class_init (ShellXFixesCursorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = shell_xfixes_cursor_finalize;

  signals[CURSOR_CHANGED] = g_signal_new ("cursor-change",
                                       G_TYPE_FROM_CLASS (klass),
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL, NULL, NULL,
                                       G_TYPE_NONE, 0);

  gobject_class->get_property = shell_xfixes_cursor_get_property;
  gobject_class->set_property = shell_xfixes_cursor_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_SCREEN,
                                   g_param_spec_object ("screen",
                                                        "Screen",
                                                        "Screen for mouse cursor",
                                                        META_TYPE_SCREEN,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/**
 * shell_xfixes_cursor_get_for_screen:
 * @screen: (transfer none): The #MetaScreen to get the cursor for
 *
 * Return value: (transfer none): A #ShellXFixesCursor instance
 */
ShellXFixesCursor *
shell_xfixes_cursor_get_for_screen (MetaScreen *screen)
{
  ShellXFixesCursor *instance;
  static GQuark xfixes_cursor_quark;

  if (G_UNLIKELY (xfixes_cursor_quark == 0))
    xfixes_cursor_quark = g_quark_from_static_string ("gnome-shell-xfixes-cursor");

  instance = g_object_get_qdata (G_OBJECT (screen), xfixes_cursor_quark);

  if (instance == NULL)
    {
      instance = g_object_new (SHELL_TYPE_XFIXES_CURSOR,
                               "screen", screen,
                               NULL);
      g_object_set_qdata (G_OBJECT (screen), xfixes_cursor_quark, instance);
    }

  return instance;
}

/**
 * shell_xfixes_cursor_update_texture_image:
 * @xfixes_cursor:  the #ShellXFixesCursor
 * @texture:        ClutterTexture to update with the current sprite image.
 */
void
shell_xfixes_cursor_update_texture_image (ShellXFixesCursor *xfixes_cursor,
                                          ClutterTexture *texture)
{
    CoglHandle *old_sprite;
    g_return_if_fail (SHELL_IS_XFIXES_CURSOR (xfixes_cursor));

    if (texture == NULL)
        return;

    old_sprite = clutter_texture_get_cogl_texture (texture);
    if (xfixes_cursor->cursor_sprite == old_sprite)
        return;

    clutter_texture_set_cogl_texture (texture, xfixes_cursor->cursor_sprite);
}

/**
 * shell_xfixes_cursor_get_hot_x:
 * @xfixes_cursor: the #ShellXFixesCursor
 *
 * Returns: the current mouse cursor's hot x-coordinate.
 */
int
shell_xfixes_cursor_get_hot_x (ShellXFixesCursor *xfixes_cursor)
{
  g_return_val_if_fail (SHELL_IS_XFIXES_CURSOR (xfixes_cursor), 0);

  return xfixes_cursor->cursor_hot_x;
}

/**
 * shell_xfixes_cursor_get_hot_y:
 * @xfixes_cursor: the #ShellXFixesCursor
 *
 * Returns: the current mouse cursor's hot y-coordinate.
 */
int
shell_xfixes_cursor_get_hot_y (ShellXFixesCursor *xfixes_cursor)
{
  g_return_val_if_fail (SHELL_IS_XFIXES_CURSOR (xfixes_cursor), 0);

  return xfixes_cursor->cursor_hot_y;
}
