/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_BORDER_IMAGE_H__
#define __SHELL_BORDER_IMAGE_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* A ShellBorderImage encapsulates an image with specified unscaled borders on each edge.
 */
typedef struct _ShellBorderImage      ShellBorderImage;
typedef struct _ShellBorderImageClass ShellBorderImageClass;

#define SHELL_TYPE_BORDER_IMAGE             (shell_border_image_get_type ())
#define SHELL_BORDER_IMAGE(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), SHELL_TYPE_BORDER_IMAGE, ShellBorderImage))
#define SHELL_BORDER_IMAGE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_BORDER_IMAGE, ShellBorderImageClass))
#define SHELL_IS_BORDER_IMAGE(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), SHELL_TYPE_BORDER_IMAGE))
#define SHELL_IS_BORDER_IMAGE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_BORDER_IMAGE))
#define SHELL_BORDER_IMAGE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_BORDER_IMAGE, ShellBorderImageClass))

GType             shell_border_image_get_type          (void) G_GNUC_CONST;

ShellBorderImage *shell_border_image_new (const char *filename,
                                          int         border_top,
                                          int         border_right,
                                          int         border_bottom,
                                          int         border_left);

const char *shell_border_image_get_filename (ShellBorderImage *image);
void        shell_border_image_get_borders  (ShellBorderImage *image,
                                             int             *border_top,
                                             int             *border_right,
                                             int             *border_bottom,
                                             int             *border_left);

G_END_DECLS

#endif /* __SHELL_BORDER_IMAGE_H__ */
