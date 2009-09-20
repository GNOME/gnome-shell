/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_THEME_IMAGE_H__
#define __SHELL_THEME_IMAGE_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* A ShellThemeImage encapsulates an image with specified unscaled borders on each edge.
 */
typedef struct _ShellThemeImage      ShellThemeImage;
typedef struct _ShellThemeImageClass ShellThemeImageClass;

#define SHELL_TYPE_THEME_IMAGE             (shell_theme_image_get_type ())
#define SHELL_THEME_IMAGE(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), SHELL_TYPE_THEME_IMAGE, ShellThemeImage))
#define SHELL_THEME_IMAGE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_THEME_IMAGE, ShellThemeImageClass))
#define SHELL_IS_THEME_IMAGE(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), SHELL_TYPE_THEME_IMAGE))
#define SHELL_IS_THEME_IMAGE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_THEME_IMAGE))
#define SHELL_THEME_IMAGE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_THEME_IMAGE, ShellThemeImageClass))

GType             shell_theme_image_get_type          (void) G_GNUC_CONST;

ShellThemeImage *shell_theme_image_new (const char *filename,
                                        int         border_top,
                                        int         border_right,
                                        int         border_bottom,
                                        int         border_left);

const char *shell_theme_image_get_filename (ShellThemeImage *image);
void        shell_theme_image_get_borders  (ShellThemeImage *image,
                                            int             *border_top,
                                            int             *border_right,
                                            int             *border_bottom,
                                            int             *border_left);

G_END_DECLS

#endif /* __SHELL_THEME_IMAGE_H__ */
