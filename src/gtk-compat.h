#ifndef __GTK_COMPAT_H__
#define __GTK_COMPAT_H__

#include <gtk/gtk.h>

/* Provide a compatibility layer for accessor function introduces
 * in GTK+ 2.20 which we need to build with GSEAL_ENABLE.
 * That way it is still possible to build with GTK+ 2.18 when not
 * using GSEAL_ENABLE
 */

#if !GTK_CHECK_VERSION (2, 20, 0)

#define gtk_widget_get_realized(w) GTK_WIDGET_REALIZED (w)
#define gtk_widget_get_requisition(w,r) (*r = GTK_WIDGET (w)->requisition)
#define gtk_widget_set_mapped(w,m)            \
  G_STMT_START {                              \
    if (m)                                    \
      GTK_WIDGET_SET_FLAGS (w, GTK_MAPPED);   \
    else                                      \
      GTK_WIDGET_UNSET_FLAGS (w, GTK_MAPPED); \
  } G_STMT_END

#endif /* GTK_CHECK_VERSION */

#endif /* __GTK_COMPAT_H__ */
