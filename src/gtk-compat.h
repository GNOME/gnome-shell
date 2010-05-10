#ifndef __GTK_COMPAT_H__
#define __GTK_COMPAT_H__

#include <gtk/gtk.h>

/* Provide a compatibility layer for accessor functions introduced
 * in GTK+ 2.20 which we need to build with GSEAL_ENABLE.
 * That way it is still possible to build with GTK+ 2.18 when not
 * using GSEAL_ENABLE
 */

#if !GTK_CHECK_VERSION(2, 20, 0)

#define gtk_widget_get_realized(w) GTK_WIDGET_REALIZED (w)
#define gtk_widget_get_mapped(w)   GTK_WIDGET_MAPPED (w)

#endif /* GTK_CHECK_VERSION(2, 20, 0) */

#endif /* __GTK_COMPAT_H__ */
