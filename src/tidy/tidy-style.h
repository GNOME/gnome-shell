#ifndef __TIDY_STYLE_H__
#define __TIDY_STYLE_H__

#include <glib-object.h>
#include <clutter/clutter-actor.h>
#include <clutter/clutter-alpha.h>
#include <clutter/clutter-timeline.h>

G_BEGIN_DECLS

#define TIDY_TYPE_STYLE                (tidy_style_get_type ())
#define TIDY_STYLE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), TIDY_TYPE_STYLE, TidyStyle))
#define TIDY_IS_STYLE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TIDY_TYPE_STYLE))
#define TIDY_STYLE_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), TIDY_TYPE_STYLE, TidyStyleClass))
#define TIDY_IS_STYLE_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), TIDY_TYPE_STYLE))
#define TIDY_STYLE_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), TIDY_TYPE_STYLE, TidyStyleClass))

/* Default properties */
#define TIDY_FONT_NAME                  "TidyActor::font-name"
#define TIDY_BACKGROUND_COLOR           "TidyActor::bg-color"
#define TIDY_ACTIVE_COLOR               "TidyActor::active-color"
#define TIDY_TEXT_COLOR                 "TidyActor::text-color"

typedef struct _TidyStyle              TidyStyle;
typedef struct _TidyStylePrivate       TidyStylePrivate;
typedef struct _TidyStyleClass         TidyStyleClass;

struct _TidyStyle
{
  GObject parent_instance;

  TidyStylePrivate *priv;
};

struct _TidyStyleClass
{
  GObjectClass parent_class;

  void (* changed) (TidyStyle *style);
};

GType            tidy_style_get_type     (void) G_GNUC_CONST;

TidyStyle *      tidy_style_get_default  (void);
TidyStyle *      tidy_style_new          (void);

gboolean         tidy_style_has_property (TidyStyle        *style,
                                          const gchar      *property_name);
gboolean         tidy_style_has_effect   (TidyStyle        *style,
                                          const gchar      *effect_name);
void             tidy_style_add_property (TidyStyle        *style,
                                          const gchar      *property_name,
                                          GType             property_type);
void             tidy_style_add_effect   (TidyStyle        *style,
                                          const gchar      *effect_name);

void             tidy_style_get_property (TidyStyle        *style,
                                          const gchar      *property_name,
                                          GValue           *value);
void             tidy_style_set_property (TidyStyle        *style,
                                          const gchar      *property_name,
                                          const GValue     *value);

ClutterTimeline *tidy_style_get_effect   (TidyStyle        *style,
                                          const gchar      *effect_name,
                                          ClutterActor     *actor);
void             tidy_style_set_effectv  (TidyStyle        *style,
                                          const gchar      *effect_name,
                                          guint             duration,
                                          GType             behaviour_type,
                                          ClutterAlphaFunc  alpha_func,
                                          guint             n_parameters,
                                          GParameter       *parameters);
void             tidy_style_set_effect   (TidyStyle        *style,
                                          const gchar      *effect_name,
                                          guint             duration,
                                          GType             behaviour_type,
                                          ClutterAlphaFunc  alpha_func,
                                          const gchar      *first_property_name,
                                          ...) G_GNUC_NULL_TERMINATED;
                                          

G_END_DECLS

#endif /* __TIDY_STYLE_H__ */
