
#include "gdbus-object-manager-example/gdbus-example-objectmanager-generated.h"

/* ---------------------------------------------------------------------------------------------------- */

static GDBusObjectManagerServer *manager = NULL;

static gboolean
on_animal_poke (ExampleAnimal          *animal,
                GDBusMethodInvocation  *invocation,
                gboolean                make_sad,
                gboolean                make_happy,
                gpointer                user_data)
{
  if ((make_sad && make_happy) || (!make_sad && !make_happy))
    {
      g_dbus_method_invocation_return_dbus_error (invocation,
                                                  "org.gtk.GDBus.Examples.ObjectManager.Error.Failed",
                                                  "Exactly one of make_sad or make_happy must be TRUE");
      goto out;
    }

  if (make_sad)
    {
      if (g_strcmp0 (example_animal_get_mood (animal), "Sad") == 0)
        {
          g_dbus_method_invocation_return_dbus_error (invocation,
                                                      "org.gtk.GDBus.Examples.ObjectManager.Error.SadAnimalIsSad",
                                                      "Sad animal is already sad");
          goto out;
        }

      example_animal_set_mood (animal, "Sad");
      example_animal_complete_poke (animal, invocation);
      goto out;
    }

  if (make_happy)
    {
      if (g_strcmp0 (example_animal_get_mood (animal), "Happy") == 0)
        {
          g_dbus_method_invocation_return_dbus_error (invocation,
                                                      "org.gtk.GDBus.Examples.ObjectManager.Error.HappyAnimalIsHappy",
                                                      "Happy animal is already happy");
          goto out;
        }

      example_animal_set_mood (animal, "Happy");
      example_animal_complete_poke (animal, invocation);
      goto out;
    }

  g_assert_not_reached ();

 out:
  return TRUE; /* to indicate that the method was handled */
}


static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  ExampleObjectSkeleton *object;
  guint n;

  g_print ("Acquired a message bus connection\n");

  /* Create a new org.freedesktop.DBus.ObjectManager rooted at /example/Animals */
  manager = g_dbus_object_manager_server_new ("/example/Animals");

  for (n = 0; n < 10; n++)
    {
      gchar *s;
      ExampleAnimal *animal;

      /* Create a new D-Bus object at the path /example/Animals/N where N is 000..009 */
      s = g_strdup_printf ("/example/Animals/%03d", n);
      object = example_object_skeleton_new (s);
      g_free (s);

      /* Make the newly created object export the interface
       * org.gtk.GDBus.Example.ObjectManager.Animal (note
       * that @object takes its own reference to @animal).
       */
      animal = example_animal_skeleton_new ();
      example_animal_set_mood (animal, "Happy");
      example_object_skeleton_set_animal (object, animal);
      g_object_unref (animal);

      /* Cats are odd animals - so some of our objects implement the
       * org.gtk.GDBus.Example.ObjectManager.Cat interface in addition
       * to the .Animal interface
       */
      if (n % 2 == 1)
        {
          ExampleCat *cat;
          cat = example_cat_skeleton_new ();
          example_object_skeleton_set_cat (object, cat);
          g_object_unref (cat);
        }

      /* Handle Poke() D-Bus method invocations on the .Animal interface */
      g_signal_connect (animal,
                        "handle-poke",
                        G_CALLBACK (on_animal_poke),
                        NULL); /* user_data */

      /* Export the object (@manager takes its own reference to @object) */
      g_dbus_object_manager_server_export (manager, G_DBUS_OBJECT_SKELETON (object));
      g_object_unref (object);
    }

  /* Export all objects */
  g_dbus_object_manager_server_set_connection (manager, connection);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  g_print ("Acquired the name %s\n", name);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  g_print ("Lost the name %s\n", name);
}


gint
main (gint argc, gchar *argv[])
{
  GMainLoop *loop;
  guint id;

  loop = g_main_loop_new (NULL, FALSE);

  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       "org.gtk.GDBus.Examples.ObjectManager",
                       G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                       G_BUS_NAME_OWNER_FLAGS_REPLACE,
                       on_bus_acquired,
                       on_name_acquired,
                       on_name_lost,
                       loop,
                       NULL);

  g_main_loop_run (loop);

  g_bus_unown_name (id);
  g_main_loop_unref (loop);

  return 0;
}
