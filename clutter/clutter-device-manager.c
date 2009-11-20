#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-debug.h"
#include "clutter-device-manager.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

#define CLUTTER_DEVICE_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_DEVICE_MANAGER, ClutterDeviceManagerClass))
#define CLUTTER_IS_DEVICE_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_DEVICE_MANAGER))
#define CLUTTER_DEVICE_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_DEVICE_MANAGER, ClutterDeviceManagerClass))

typedef struct _ClutterDeviceManagerClass       ClutterDeviceManagerClass;

struct _ClutterDeviceManagerClass
{
  GObjectClass parent_instance;
};

enum
{
  DEVICE_ADDED,
  DEVICE_REMOVED,

  LAST_SIGNAL
};

static ClutterDeviceManager *default_manager = NULL;

static guint manager_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (ClutterDeviceManager, clutter_device_manager, G_TYPE_OBJECT);

static void
clutter_device_manager_class_init (ClutterDeviceManagerClass *klass)
{
  manager_signals[DEVICE_ADDED] =
    g_signal_new (I_("device-added"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  clutter_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);

  manager_signals[DEVICE_REMOVED] =
    g_signal_new (I_("device-removed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  clutter_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);
}

static void
clutter_device_manager_init (ClutterDeviceManager *self)
{
}

ClutterDeviceManager *
clutter_device_manager_get_default (void)
{
  if (G_UNLIKELY (default_manager == NULL))
    default_manager = g_object_new (CLUTTER_TYPE_DEVICE_MANAGER, NULL);

  return default_manager;
}

GSList *
clutter_device_manager_list_devices (ClutterDeviceManager *device_manager)
{
  g_return_val_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager), NULL);

  return g_slist_copy (device_manager->devices);
}

const GSList *
clutter_device_manager_peek_devices (ClutterDeviceManager *device_manager)
{
  g_return_val_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager), NULL);

  return device_manager->devices;
}

ClutterInputDevice *
clutter_device_manager_get_device (ClutterDeviceManager *device_manager,
                                   gint                  device_id)
{
  GSList *l;

  g_return_val_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager), NULL);

  for (l = device_manager->devices; l != NULL; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (device->id == device_id)
        return device;
    }

  return NULL;
}

static gint
input_device_cmp (gconstpointer a,
                  gconstpointer b)
{
  const ClutterInputDevice *device_a = a;
  const ClutterInputDevice *device_b = b;

  if (device_a->id < device_b->id)
    return -1;

  if (device_a->id > device_b->id)
    return 1;

  return 0;
}

void
_clutter_device_manager_add_device (ClutterDeviceManager *device_manager,
                                    ClutterInputDevice   *device)
{
  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager));

  device_manager->devices = g_slist_insert_sorted (device_manager->devices,
                                                   device,
                                                   input_device_cmp);
}

void
_clutter_device_manager_remove_device (ClutterDeviceManager *device_manager,
                                       ClutterInputDevice   *device)
{
  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER (device_manager));

  if (g_slist_find (device_manager->devices, device) == NULL)
    return;

  device_manager->devices = g_slist_remove (device_manager->devices, device);
}
