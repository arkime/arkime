
#include "gdbus-object-manager-example/gdbus-example-objectmanager-generated.h"

/* ---------------------------------------------------------------------------------------------------- */

static void
print_objects (GDBusObjectManager *manager)
{
  GList *objects;
  GList *l;

  g_print ("Object manager at %s\n", g_dbus_object_manager_get_object_path (manager));
  objects = g_dbus_object_manager_get_objects (manager);
  for (l = objects; l != NULL; l = l->next)
    {
      ExampleObject *object = EXAMPLE_OBJECT (l->data);
      GList *interfaces;
      GList *ll;
      g_print (" - Object at %s\n", g_dbus_object_get_object_path (G_DBUS_OBJECT (object)));

      interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));
      for (ll = interfaces; ll != NULL; ll = ll->next)
        {
          GDBusInterface *interface = G_DBUS_INTERFACE (ll->data);
          g_print ("   - Interface %s\n", g_dbus_interface_get_info (interface)->name);

          /* Note that @interface is really a GDBusProxy instance - and additionally also
           * an ExampleAnimal or ExampleCat instance - either of these can be used to
           * invoke methods on the remote object. For example, the generated function
           *
           *  void example_animal_call_poke_sync (ExampleAnimal  *proxy,
           *                                      gboolean        make_sad,
           *                                      gboolean        make_happy,
           *                                      GCancellable   *cancellable,
           *                                      GError        **error);
           *
           * can be used to call the Poke() D-Bus method on the .Animal interface.
           * Additionally, the generated function
           *
           *  const gchar *example_animal_get_mood (ExampleAnimal *object);
           *
           * can be used to get the value of the :Mood property.
           */
        }
      g_list_free_full (interfaces, g_object_unref);
    }
  g_list_free_full (objects, g_object_unref);
}

static void
on_object_added (GDBusObjectManager *manager,
                 GDBusObject        *object,
                 gpointer            user_data)
{
  gchar *owner;
  owner = g_dbus_object_manager_client_get_name_owner (G_DBUS_OBJECT_MANAGER_CLIENT (manager));
  g_print ("Added object at %s (owner %s)\n", g_dbus_object_get_object_path (object), owner);
  g_free (owner);
}

static void
on_object_removed (GDBusObjectManager *manager,
                   GDBusObject        *object,
                   gpointer            user_data)
{
  gchar *owner;
  owner = g_dbus_object_manager_client_get_name_owner (G_DBUS_OBJECT_MANAGER_CLIENT (manager));
  g_print ("Removed object at %s (owner %s)\n", g_dbus_object_get_object_path (object), owner);
  g_free (owner);
}

static void
on_notify_name_owner (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (object);
  gchar *name_owner;

  name_owner = g_dbus_object_manager_client_get_name_owner (manager);
  g_print ("name-owner: %s\n", name_owner);
  g_free (name_owner);
}

static void
on_interface_proxy_properties_changed (GDBusObjectManagerClient *manager,
                                       GDBusObjectProxy         *object_proxy,
                                       GDBusProxy               *interface_proxy,
                                       GVariant                 *changed_properties,
                                       const gchar *const       *invalidated_properties,
                                       gpointer                  user_data)
{
  GVariantIter iter;
  const gchar *key;
  GVariant *value;
  gchar *s;

  g_print ("Properties Changed on %s:\n", g_dbus_object_get_object_path (G_DBUS_OBJECT (object_proxy)));
  g_variant_iter_init (&iter, changed_properties);
  while (g_variant_iter_next (&iter, "{&sv}", &key, &value))
    {
      s = g_variant_print (value, TRUE);
      g_print ("  %s -> %s\n", key, s);
      g_variant_unref (value);
      g_free (s);
    }
}

gint
main (gint argc, gchar *argv[])
{
  GDBusObjectManager *manager;
  GMainLoop *loop;
  GError *error;
  gchar *name_owner;

  manager = NULL;
  loop = NULL;

  loop = g_main_loop_new (NULL, FALSE);

  error = NULL;
  manager = example_object_manager_client_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                            G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                            "org.gtk.GDBus.Examples.ObjectManager",
                                                            "/example/Animals",
                                                            NULL, /* GCancellable */
                                                            &error);
  if (manager == NULL)
    {
      g_printerr ("Error getting object manager client: %s", error->message);
      g_error_free (error);
      goto out;
    }

  name_owner = g_dbus_object_manager_client_get_name_owner (G_DBUS_OBJECT_MANAGER_CLIENT (manager));
  g_print ("name-owner: %s\n", name_owner);
  g_free (name_owner);

  print_objects (manager);

  g_signal_connect (manager,
                    "notify::name-owner",
                    G_CALLBACK (on_notify_name_owner),
                    NULL);
  g_signal_connect (manager,
                    "object-added",
                    G_CALLBACK (on_object_added),
                    NULL);
  g_signal_connect (manager,
                    "object-removed",
                    G_CALLBACK (on_object_removed),
                    NULL);
  g_signal_connect (manager,
                    "interface-proxy-properties-changed",
                    G_CALLBACK (on_interface_proxy_properties_changed),
                    NULL);

  g_main_loop_run (loop);

 out:
  if (manager != NULL)
    g_object_unref (manager);
  if (loop != NULL)
    g_main_loop_unref (loop);

  return 0;
}
