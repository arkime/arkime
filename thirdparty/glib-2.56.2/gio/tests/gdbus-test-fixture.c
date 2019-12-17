
#include "gdbus-object-manager-example/gdbus-example-objectmanager-generated.h"

/* ---------------------------------------------------------------------------------------------------- */

/* The fixture contains a GTestDBus object and
 * a proxy to the service we're going to be testing.
 */
typedef struct {
  GTestDBus *dbus;
  GDBusObjectManager *manager;
} TestFixture;

static void
fixture_setup (TestFixture *fixture, gconstpointer unused)
{
  GError *error = NULL;

  /* Create the global dbus-daemon for this test suite
   */
  fixture->dbus = g_test_dbus_new (G_TEST_DBUS_NONE);

  /* Add the private directory with our in-tree service files, 
   * TEST_SERVICES is defined by the build system to point
   * to the right directory.
   */
  g_test_dbus_add_service_dir (fixture->dbus, TEST_SERVICES);

  /* Start the private D-Bus daemon
   */
  g_test_dbus_up (fixture->dbus);

  /* Create the proxy that we're going to test
   */
  fixture->manager =
    example_object_manager_client_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                    G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                    "org.gtk.GDBus.Examples.ObjectManager",
                                                    "/example/Animals",
                                                    NULL, /* GCancellable */
                                                    &error);
  if (fixture->manager == NULL)
    g_error ("Error getting object manager client: %s", error->message);
}

static void
fixture_teardown (TestFixture *fixture, gconstpointer unused)
{
  /* Tear down the proxy
   */
  if (fixture->manager)
    g_object_unref (fixture->manager);

  /* Stop the private D-Bus daemon
   */
  g_test_dbus_down (fixture->dbus);
  g_object_unref (fixture->dbus);
}

/* The gdbus-example-objectmanager-server exports 10 objects,
 * to test the server has actually activated, let's ensure
 * that 10 objects exist.
 */
static void
test_gtest_dbus (TestFixture *fixture, gconstpointer unused)
{
  GList *objects;

  objects = g_dbus_object_manager_get_objects (fixture->manager);

  g_assert_cmpint (g_list_length (objects), ==, 10);
  g_list_free_full (objects, g_object_unref);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  /* This test simply ensures that we can bring the GTestDBus up and down a hand
   * full of times in a row, each time successfully activating the in-tree service
   */
  g_test_add ("/GTestDBus/Cycle1", TestFixture, NULL,
  	      fixture_setup, test_gtest_dbus, fixture_teardown);
  g_test_add ("/GTestDBus/Cycle2", TestFixture, NULL,
  	      fixture_setup, test_gtest_dbus, fixture_teardown);
  g_test_add ("/GTestDBus/Cycle3", TestFixture, NULL,
  	      fixture_setup, test_gtest_dbus, fixture_teardown);
  g_test_add ("/GTestDBus/Cycle4", TestFixture, NULL,
  	      fixture_setup, test_gtest_dbus, fixture_teardown);
  g_test_add ("/GTestDBus/Cycle5", TestFixture, NULL,
  	      fixture_setup, test_gtest_dbus, fixture_teardown);
  
  return g_test_run ();
}
