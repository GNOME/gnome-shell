#ifndef __CLUTTER_EVENT_TRANSLATOR_H__
#define __CLUTTER_EVENT_TRANSLATOR_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_EVENT_TRANSLATOR           (_clutter_event_translator_get_type ())
#define CLUTTER_EVENT_TRANSLATOR(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_EVENT_TRANSLATOR, ClutterEventTranslator))
#define CLUTTER_IS_EVENT_TRANSLATOR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_EVENT_TRANSLATOR))
#define CLUTTER_EVENT_TRANSLATOR_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CLUTTER_TYPE_EVENT_TRANSLATOR, ClutterEventTranslatorIface))

typedef struct _ClutterEventTranslator          ClutterEventTranslator;
typedef struct _ClutterEventTranslatorIface     ClutterEventTranslatorIface;

typedef enum {
  CLUTTER_TRANSLATE_CONTINUE,
  CLUTTER_TRANSLATE_REMOVE,
  CLUTTER_TRANSLATE_QUEUE
} ClutterTranslateReturn;

struct _ClutterEventTranslatorIface
{
  GTypeInterface g_iface;

  ClutterTranslateReturn (* translate_event) (ClutterEventTranslator *translator,
                                              gpointer                native,
                                              ClutterEvent           *translated);
};

GType _clutter_event_translator_get_type (void) G_GNUC_CONST;

ClutterTranslateReturn _clutter_event_translator_translate_event (ClutterEventTranslator *translator,
                                                                  gpointer                native,
                                                                  ClutterEvent           *translated);

G_END_DECLS

#endif /* __CLUTTER_EVENT_TRANSLATOR_H__ */
