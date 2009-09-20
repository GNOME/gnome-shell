/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __ST_THEME_CONTEXT_H__
#define __ST_THEME_CONTEXT_H__

#include <clutter/clutter.h>
#include <pango/pango.h>
#include "st-theme-node.h"

G_BEGIN_DECLS

typedef struct _StThemeContextClass StThemeContextClass;

#define ST_TYPE_THEME_CONTEXT             (st_theme_context_get_type ())
#define ST_THEME_CONTEXT(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), ST_TYPE_THEME_CONTEXT, StThemeContext))
#define ST_THEME_CONTEXT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ST_TYPE_THEME_CONTEXT, StThemeContextClass))
#define ST_IS_THEME_CONTEXT(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), ST_TYPE_THEME_CONTEXT))
#define ST_IS_THEME_CONTEXT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ST_TYPE_THEME_CONTEXT))
#define ST_THEME_CONTEXT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), ST_TYPE_THEME_CONTEXT, StThemeContextClass))

GType st_theme_context_get_type (void) G_GNUC_CONST;

StThemeContext *st_theme_context_new           (void);
StThemeContext *st_theme_context_get_for_stage (ClutterStage *stage);

void                        st_theme_context_set_theme      (StThemeContext             *context,
                                                             StTheme                    *theme);
StTheme *                   st_theme_context_get_theme      (StThemeContext             *context);

void                        st_theme_context_set_resolution (StThemeContext             *context,
                                                             gdouble                     resolution);
double                      st_theme_context_get_resolution (StThemeContext             *context);
void                        st_theme_context_set_font       (StThemeContext             *context,
                                                             const PangoFontDescription *font);
const PangoFontDescription *st_theme_context_get_font       (StThemeContext             *context);

StThemeNode *               st_theme_context_get_root_node  (StThemeContext             *context);

G_END_DECLS

#endif /* __ST_THEME_CONTEXT_H__ */
