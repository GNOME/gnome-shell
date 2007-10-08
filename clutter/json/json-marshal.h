
#ifndef ___json_marshal_MARSHAL_H__
#define ___json_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:VOID (./json-marshal.list:1) */
#define _json_marshal_VOID__VOID	g_cclosure_marshal_VOID__VOID

/* VOID:BOXED (./json-marshal.list:2) */
#define _json_marshal_VOID__BOXED	g_cclosure_marshal_VOID__BOXED

/* VOID:BOXED,STRING (./json-marshal.list:3) */
extern void _json_marshal_VOID__BOXED_STRING (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* VOID:BOXED,INT (./json-marshal.list:4) */
extern void _json_marshal_VOID__BOXED_INT (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);

/* VOID:POINTER (./json-marshal.list:5) */
#define _json_marshal_VOID__POINTER	g_cclosure_marshal_VOID__POINTER

G_END_DECLS

#endif /* ___json_marshal_MARSHAL_H__ */

