#ifndef __TIDY_STYLABLE_H__
#define __TIDY_STYLABLE_H__

#include <glib-object.h>
#include <tidy/tidy-style.h>

G_BEGIN_DECLS

#define TIDY_TYPE_STYLABLE              (tidy_stylable_get_type ())
#define TIDY_STYLABLE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), TIDY_TYPE_STYLABLE, TidyStylable))
#define TIDY_IS_STYLABLE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TIDY_TYPE_STYLABLE))
#define TIDY_STYLABLE_IFACE(iface)      (G_TYPE_CHECK_CLASS_CAST ((iface), TIDY_TYPE_STYLABLE, TidyStylableIface))
#define TIDY_IS_STYLABLE_IFACE(iface)   (G_TYPE_CHECK_CLASS_TYPE ((iface), TIDY_TYPE_STYLABLE))
#define TIDY_STYLABLE_GET_IFACE(obj)    (G_TYPE_INSTANCE_GET_INTERFACE ((obj), TIDY_TYPE_STYLABLE, TidyStylableIface))

typedef struct _TidyStylable            TidyStylable; /* dummy typedef */
typedef struct _TidyStylableIface       TidyStylableIface;

struct _TidyStylableIface
{
  GTypeInterface g_iface;

  /* virtual functions */
  TidyStyle *(* get_style) (TidyStylable *stylable);
  void       (* set_style) (TidyStylable *stylable,
                            TidyStyle    *style);

  /* signals, not vfuncs */
  void (* style_notify) (TidyStylable *stylable,
                         GParamSpec   *pspec);

  void (* style_set)    (TidyStylable *stylable,
                         TidyStyle    *old_style);
};

GType        tidy_stylable_get_type               (void) G_GNUC_CONST;

void         tidy_stylable_iface_install_property (TidyStylableIface *iface,
                                                   GType              owner_type,
                                                   GParamSpec        *pspec);

void         tidy_stylable_freeze_notify          (TidyStylable      *stylable);
void         tidy_stylable_notify                 (TidyStylable      *stylable,
                                                   const gchar       *property_name);
void         tidy_stylable_thaw_notify            (TidyStylable      *stylable);
GParamSpec **tidy_stylable_list_properties        (TidyStylable      *stylable,
                                                   guint             *n_props);
GParamSpec * tidy_stylable_find_property          (TidyStylable      *stylable,
                                                   const gchar       *property_name);
void         tidy_stylable_set_style              (TidyStylable      *stylable,
                                                   TidyStyle         *style);
TidyStyle *  tidy_stylable_get_style              (TidyStylable      *stylable);

void         tidy_stylable_set                    (TidyStylable      *stylable,
                                                   const gchar       *first_property_name,
                                                   ...) G_GNUC_NULL_TERMINATED;
void         tidy_stylable_get                    (TidyStylable      *stylable,
                                                   const gchar       *first_property_name,
                                                   ...) G_GNUC_NULL_TERMINATED;
void         tidy_stylable_set_property           (TidyStylable      *stylable,
                                                   const gchar       *property_name,
                                                   const GValue      *value);
void         tidy_stylable_get_property           (TidyStylable      *stylable,
                                                   const gchar       *property_name,
                                                   GValue            *value);

G_END_DECLS

#endif /* __TIDY_STYLABLE_H__ */
