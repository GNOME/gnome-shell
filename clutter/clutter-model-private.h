#ifndef __CLUTTER_MODEL_PRIVATE_H__
#define __CLUTTER_MODEL_PRIVATE_H__

#include <glib.h>
#include "clutter-model.h"

G_BEGIN_DECLS

void     clutter_model_set_n_columns   (ClutterModel *model,
                                        gint          n_columns,
                                        gboolean      set_types,
                                        gboolean      set_names);
gboolean clutter_model_check_type      (GType         gtype);

void     clutter_model_set_column_type (ClutterModel *model,
                                        gint          column,
                                        GType         gtype);
void     clutter_model_set_column_name (ClutterModel *model,
                                        gint          column,
                                        const gchar  *name);

void    clutter_model_iter_set_row (ClutterModelIter *iter,
                                    guint             row);

G_END_DECLS

#endif /* __CLUTTER_MODEL_PRIVATE_H__ */
