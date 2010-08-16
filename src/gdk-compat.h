#ifndef __GDK_COMPAT_H__
#define __GDK_COMPAT_H__

#include <gdk/gdk.h>

/* Provide a compatibility layer for accessor function introduced
 * in GTK+ 2.22 which we need to build without deprecated GDK symbols.
 * That way it is still possible to build with GTK+ 2.18 when not
 * using GDK_DISABLE_DEPRECATED.
 */

#if !GTK_CHECK_VERSION (2, 21, 1)

#define gdk_visual_get_depth(v)           GDK_VISUAL(v)->depth

#endif /*GTK_CHECK_VERSION */

#endif /* __GDK_COMPAT_H__ */
