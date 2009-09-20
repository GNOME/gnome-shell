/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_THEME_NODE_H__
#define __SHELL_THEME_NODE_H__

#include <clutter/clutter.h>
#include "shell-border-image.h"

G_BEGIN_DECLS

typedef struct _ShellTheme          ShellTheme;
typedef struct _ShellThemeContext   ShellThemeContext;

typedef struct _ShellThemeNode      ShellThemeNode;
typedef struct _ShellThemeNodeClass ShellThemeNodeClass;

#define SHELL_TYPE_THEME_NODE              (shell_theme_node_get_type ())
#define SHELL_THEME_NODE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), SHELL_TYPE_THEME_NODE, ShellThemeNode))
#define SHELL_THEME_NODE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass),     SHELL_TYPE_THEME_NODE, ShellThemeNodeClass))
#define SHELL_IS_THEME_NODE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), SHELL_TYPE_THEME_NODE))
#define SHELL_IS_THEME_NODE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass),     SHELL_TYPE_THEME_NODE))
#define SHELL_THEME_NODE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj),     SHELL_TYPE_THEME_NODE, ShellThemeNodeClass))

typedef enum {
    SHELL_SIDE_TOP,
    SHELL_SIDE_RIGHT,
    SHELL_SIDE_BOTTOM,
    SHELL_SIDE_LEFT
} ShellSide;

typedef enum {
    SHELL_CORNER_TOPLEFT,
    SHELL_CORNER_TOPRIGHT,
    SHELL_CORNER_BOTTOMRIGHT,
    SHELL_CORNER_BOTTOMLEFT
} ShellCorner;

/* These are the CSS values; that doesn't mean we have to implement blink... */
typedef enum {
    SHELL_TEXT_DECORATION_UNDERLINE    = 1 << 0,
    SHELL_TEXT_DECORATION_OVERLINE     = 1 << 1,
    SHELL_TEXT_DECORATION_LINE_THROUGH = 1 << 2,
    SHELL_TEXT_DECORATION_BLINK        = 1 << 3
} ShellTextDecoration;

GType shell_theme_node_get_type (void) G_GNUC_CONST;

/* An element_type of G_TYPE_NONE means this style was created for the stage
 * actor and matches a selector element name of 'stage'
 */
ShellThemeNode *shell_theme_node_new (ShellThemeContext *context,
                                      ShellThemeNode    *parent_node,   /* can be null */
                                      ShellTheme        *theme,         /* can be null */
                                      GType              element_type,
                                      const char        *element_id,
                                      const char        *element_class,
                                      const char        *pseudo_class,
                                      const char        *inline_style);

ShellThemeNode *shell_theme_node_get_parent (ShellThemeNode *node);

ShellTheme *shell_theme_node_get_theme (ShellThemeNode *node);

GType       shell_theme_node_get_element_type  (ShellThemeNode *node);
const char *shell_theme_node_get_element_id    (ShellThemeNode *node);
const char *shell_theme_node_get_element_class (ShellThemeNode *node);
const char *shell_theme_node_get_pseudo_class  (ShellThemeNode *node);

/* Generic getters ... these are not cached so are less efficient. The other
 * reason for adding the more specific version is that we can handle the
 * details of the actual CSS rules, which can be complicated, especially
 * for fonts
 */
gboolean shell_theme_node_get_color (ShellThemeNode *node,
                                     const char     *property_name,
                                     gboolean        inherit,
                                     ClutterColor   *color);

gboolean shell_theme_node_get_double (ShellThemeNode *node,
                                      const char     *property_name,
                                      gboolean        inherit,
                                      double         *value);

/* The length here is already resolved to pixels
 */
gboolean shell_theme_node_get_length (ShellThemeNode *node,
                                      const char     *property_name,
                                      gboolean        inherit,
                                      gdouble        *length);

/* Specific getters for particular properties: cached
 */
void shell_theme_node_get_background_color (ShellThemeNode *node,
                                            ClutterColor   *color);
void shell_theme_node_get_foreground_color (ShellThemeNode *node,
                                            ClutterColor   *color);

const char *shell_theme_node_get_background_image (ShellThemeNode *node);

double shell_theme_node_get_border_width  (ShellThemeNode *node,
                                           ShellSide       side);
double shell_theme_node_get_border_radius (ShellThemeNode *node,
                                           ShellCorner     corner);
void   shell_theme_node_get_border_color  (ShellThemeNode *node,
                                           ShellSide       side,
                                           ClutterColor   *color);

double  shell_theme_node_get_padding      (ShellThemeNode *node,
                                           ShellSide       side);

ShellTextDecoration shell_theme_node_get_text_decoration (ShellThemeNode *node);

/* Font rule processing is pretty complicated, so we just hardcode it
 * under the standard font/font-family/font-size/etc names. This means
 * you can't have multiple separate styled fonts for a single item,
 * but that should be OK.
 */
const PangoFontDescription *shell_theme_node_get_font (ShellThemeNode *node);

ShellBorderImage *shell_theme_node_get_border_image (ShellThemeNode *node);

/* Helpers for get_preferred_width()/get_preferred_height() ClutterActor vfuncs */
void shell_theme_node_adjust_for_height       (ShellThemeNode  *node,
                                               float           *for_height);
void shell_theme_node_adjust_preferred_width  (ShellThemeNode  *node,
                                               float           *min_width_p,
                                               float           *natural_width_p);
void shell_theme_node_adjust_for_width        (ShellThemeNode  *node,
                                               float           *for_width);
void shell_theme_node_adjust_preferred_height (ShellThemeNode  *node,
                                               float           *min_height_p,
                                               float           *natural_height_p);

/* Helper for allocate() ClutterActor vfunc */
void shell_theme_node_get_content_box         (ShellThemeNode        *node,
                                               const ClutterActorBox *actor_box,
                                               ClutterActorBox       *content_box);

gboolean shell_theme_node_geometry_equal (ShellThemeNode *node,
                                          ShellThemeNode *other);

G_END_DECLS

#endif /* __SHELL_THEME_NODE_H__ */
