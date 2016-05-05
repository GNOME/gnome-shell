#include "clutter-build-config.h"

#include "clutter-event-translator.h"

#include "clutter-backend.h"
#include "clutter-private.h"

#define clutter_event_translator_get_type       _clutter_event_translator_get_type

typedef ClutterEventTranslatorIface     ClutterEventTranslatorInterface;

G_DEFINE_INTERFACE (ClutterEventTranslator, clutter_event_translator, G_TYPE_OBJECT);

static ClutterTranslateReturn
default_translate_event (ClutterEventTranslator *translator,
                         gpointer                native,
                         ClutterEvent           *event)
{
  return CLUTTER_TRANSLATE_CONTINUE;
}

static void
clutter_event_translator_default_init (ClutterEventTranslatorIface *iface)
{
  iface->translate_event = default_translate_event;
}

ClutterTranslateReturn
_clutter_event_translator_translate_event (ClutterEventTranslator *translator,
                                           gpointer                native,
                                           ClutterEvent           *translated)
{
  ClutterEventTranslatorIface *iface;

  iface = CLUTTER_EVENT_TRANSLATOR_GET_IFACE (translator);

  return iface->translate_event (translator, native, translated);
}
