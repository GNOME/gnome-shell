/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_SCREEN_GRABBER_H__
#define __SHELL_SCREEN_GRABBER_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * SECTION:shell-screen-grabber
 * @short_description: Grab pixel data from the screen
 *
 * The #ShellScreenGrabber object is used to download previous drawn
 * content to the screen. It internally uses pixel-buffer objects if
 * available, otherwise falls back to cogl_read_pixels().
 *
 * If you are repeatedly grabbing images of the same size from the
 * screen, it makes sense to create one #ShellScreenGrabber and keep
 * it around. Otherwise, it's fine to simply create one as needed and
 * then get rid of it.
 */

typedef struct _ShellScreenGrabber      ShellScreenGrabber;
typedef struct _ShellScreenGrabberClass ShellScreenGrabberClass;

#define SHELL_TYPE_SCREEN_GRABBER              (shell_screen_grabber_get_type ())
#define SHELL_SCREEN_GRABBER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), SHELL_TYPE_SCREEN_GRABBER, ShellScreenGrabber))
#define SHELL_SCREEN_GRABBER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_SCREEN_GRABBER, ShellScreenGrabberClass))
#define SHELL_IS_SCREEN_GRABBER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), SHELL_TYPE_SCREEN_GRABBER))
#define SHELL_IS_SCREEN_GRABBER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_SCREEN_GRABBER))
#define SHELL_SCREEN_GRABBER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_SCREEN_GRABBER, ShellScreenGrabberClass))

GType shell_screen_grabber_get_type (void) G_GNUC_CONST;

ShellScreenGrabber *shell_screen_grabber_new  (void);
guchar *            shell_screen_grabber_grab (ShellScreenGrabber *grabber,
                                               int                 x,
                                               int                 y,
                                               int                 width,
                                               int                 height);

G_END_DECLS

#endif /* __SHELL_SCREEN_GRABBER_H__ */
