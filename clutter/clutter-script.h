/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_SCRIPT_H__
#define __CLUTTER_SCRIPT_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_SCRIPT             (clutter_script_get_type ())
#define CLUTTER_SCRIPT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_SCRIPT, ClutterScript))
#define CLUTTER_IS_SCRIPT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_SCRIPT))
#define CLUTTER_SCRIPT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_SCRIPT, ClutterScriptClass))
#define CLUTTER_IS_SCRIPT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_SCRIPT))
#define CLUTTER_SCRIPT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_SCRIPT, ClutterScriptClass))

typedef struct _ClutterScript           ClutterScript;
typedef struct _ClutterScriptPrivate    ClutterScriptPrivate;
typedef struct _ClutterScriptClass      ClutterScriptClass;

/**
 * ClutterScriptConnectFunc:
 * @script: a #ClutterScript
 * @object: the object to connect
 * @signal_name: the name of the signal
 * @handler_name: the name of the signal handler
 * @connect_object: the object to connect the signal to, or %NULL
 * @flags: signal connection flags
 * @user_data: user data to pass to the signal handler
 *
 * This is the signature of a function used to connect signals.  It is used
 * by the clutter_script_connect_signals_full() function.  It is mainly
 * intended for interpreted language bindings, but could be useful where the
 * programmer wants more control over the signal connection process.
 *
 * Since: 0.6
 */
typedef void (* ClutterScriptConnectFunc) (ClutterScript *script,
                                           GObject       *object,
                                           const gchar   *signal_name,
                                           const gchar   *handler_name,
                                           GObject       *connect_object,
                                           GConnectFlags  flags,
                                           gpointer       user_data);

/**
 * ClutterScriptError:
 * @CLUTTER_SCRIPT_ERROR_INVALID_TYPE_FUNCTION: Type function not found
 *   or invalid
 * @CLUTTER_SCRIPT_ERROR_INVALID_PROPERTY: Property not found or invalid
 * @CLUTTER_SCRIPT_ERROR_INVALID_VALUE: Invalid value
 *
 * #ClutterScript error enumeration.
 *
 * Since: 0.6
 */
typedef enum {
  CLUTTER_SCRIPT_ERROR_INVALID_TYPE_FUNCTION,
  CLUTTER_SCRIPT_ERROR_INVALID_PROPERTY,
  CLUTTER_SCRIPT_ERROR_INVALID_VALUE
} ClutterScriptError;

/**
 * CLUTTER_SCRIPT_ERROR:
 *
 * Error domain for the #ClutterScript errors
 *
 * Since: 0.6
 */
#define CLUTTER_SCRIPT_ERROR    (clutter_script_error_quark ())
CLUTTER_AVAILABLE_IN_ALL
GQuark clutter_script_error_quark (void);

/**
 * ClutterScript:
 *
 * The #ClutterScript structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 0.6
 */
struct _ClutterScript
{
  /*< private >*/
  GObject parent_instance;

  ClutterScriptPrivate *priv;
};

/**
 * ClutterScriptClass:
 * @get_type_from_name: virtual function used to map a type name
 *   to a #GType. This function should only be overridden by
 *   language bindings in order to map native types to #GType.
 *   The default implementation is equivalent to g_type_from_name()
 *
 * The #ClutterScriptClass structure contains only private data
 *
 * Since: 0.6
 */
struct _ClutterScriptClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  GType (* get_type_from_name) (ClutterScript *script,
                                const gchar   *type_name);

  /*< private >*/
  /* padding, for future expansion */
  void (*_clutter_reserved1) (void);
  void (*_clutter_reserved2) (void);
  void (*_clutter_reserved3) (void);
  void (*_clutter_reserved4) (void);
  void (*_clutter_reserved5) (void);
  void (*_clutter_reserved6) (void);
  void (*_clutter_reserved7) (void);
  void (*_clutter_reserved8) (void);
};

CLUTTER_AVAILABLE_IN_ALL
GType clutter_script_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_ALL
ClutterScript * clutter_script_new                      (void);
CLUTTER_AVAILABLE_IN_ALL
guint           clutter_script_load_from_file           (ClutterScript             *script,
                                                         const gchar               *filename,
                                                         GError                   **error);
CLUTTER_AVAILABLE_IN_ALL
guint           clutter_script_load_from_data           (ClutterScript             *script,
                                                         const gchar               *data,
                                                         gssize                     length,
                                                         GError                   **error);
CLUTTER_AVAILABLE_IN_1_10
guint           clutter_script_load_from_resource       (ClutterScript             *script,
                                                         const gchar               *resource_path,
                                                         GError                   **error);

CLUTTER_AVAILABLE_IN_ALL
GObject *       clutter_script_get_object               (ClutterScript             *script,
                                                         const gchar               *name);
CLUTTER_AVAILABLE_IN_ALL
gint            clutter_script_get_objects              (ClutterScript             *script,
                                                         const gchar               *first_name,
                                                         ...) G_GNUC_NULL_TERMINATED;
CLUTTER_AVAILABLE_IN_ALL
GList *         clutter_script_list_objects             (ClutterScript             *script);
CLUTTER_AVAILABLE_IN_ALL
void            clutter_script_unmerge_objects          (ClutterScript             *script,
                                                         guint                      merge_id);
CLUTTER_AVAILABLE_IN_ALL
void            clutter_script_ensure_objects           (ClutterScript             *script);

CLUTTER_DEPRECATED_IN_1_12
void            clutter_script_add_states               (ClutterScript             *script,
                                                         const gchar               *name,
                                                         ClutterState              *state);

CLUTTER_DEPRECATED_IN_1_12
ClutterState *  clutter_script_get_states               (ClutterScript             *script,
                                                         const gchar               *name);

CLUTTER_AVAILABLE_IN_ALL
void            clutter_script_connect_signals          (ClutterScript             *script,
                                                         gpointer                   user_data);
CLUTTER_AVAILABLE_IN_ALL
void            clutter_script_connect_signals_full     (ClutterScript             *script,
                                                         ClutterScriptConnectFunc   func,
                                                         gpointer                   user_data);

CLUTTER_AVAILABLE_IN_ALL
void            clutter_script_add_search_paths         (ClutterScript             *script,
                                                         const gchar * const        paths[],
                                                         gsize                      n_paths);
CLUTTER_AVAILABLE_IN_ALL
gchar *         clutter_script_lookup_filename          (ClutterScript             *script,
                                                         const gchar               *filename) G_GNUC_MALLOC;
CLUTTER_AVAILABLE_IN_ALL
GType           clutter_script_get_type_from_name       (ClutterScript             *script,
                                                         const gchar               *type_name);

CLUTTER_AVAILABLE_IN_1_10
void            clutter_script_set_translation_domain   (ClutterScript             *script,
                                                         const gchar               *domain);
CLUTTER_AVAILABLE_IN_1_10
const gchar *   clutter_script_get_translation_domain   (ClutterScript             *script);

CLUTTER_AVAILABLE_IN_ALL
const gchar *   clutter_get_script_id                   (GObject                   *gobject);

G_END_DECLS

#endif /* __CLUTTER_SCRIPT_H__ */
