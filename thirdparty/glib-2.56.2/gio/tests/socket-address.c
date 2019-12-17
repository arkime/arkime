#include <gio/gunixsocketaddress.h>

static void
test_unix_socket_address_construct (void)
{
  GUnixSocketAddress *a;

  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS, NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_PATH);
  g_object_unref (a);

  /* Try passing some default values for the arguments explicitly and
   * make sure it makes no difference.
   */
  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS, "address-type", G_UNIX_SOCKET_ADDRESS_PATH, NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_PATH);
  g_object_unref (a);

  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS, "abstract", FALSE, NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_PATH);
  g_object_unref (a);

  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                    "abstract", FALSE,
                    "address-type", G_UNIX_SOCKET_ADDRESS_PATH,
                    NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_PATH);
  g_object_unref (a);

  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                    "address-type", G_UNIX_SOCKET_ADDRESS_PATH,
                    "abstract", FALSE,
                    NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_PATH);
  g_object_unref (a);

  /* Try explicitly setting abstract to TRUE */
  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                    "abstract", TRUE,
                    NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED);
  g_object_unref (a);

  /* Try explicitly setting a different kind of address */
  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                    "address-type", G_UNIX_SOCKET_ADDRESS_ANONYMOUS,
                    NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_ANONYMOUS);
  g_object_unref (a);

  /* Now try explicitly setting a different type of address after
   * setting abstract to FALSE.
   */
  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                    "abstract", FALSE,
                    "address-type", G_UNIX_SOCKET_ADDRESS_ANONYMOUS,
                    NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_ANONYMOUS);
  g_object_unref (a);

  /* And the other way around */
  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                    "address-type", G_UNIX_SOCKET_ADDRESS_ANONYMOUS,
                    "abstract", FALSE,
                    NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_ANONYMOUS);
  g_object_unref (a);
}

static void
test_unix_socket_address_to_string (void)
{
  GSocketAddress *addr = NULL;
  gchar *str = NULL;

  /* ADDRESS_PATH. */
  addr = g_unix_socket_address_new_with_type ("/some/path", -1,
                                              G_UNIX_SOCKET_ADDRESS_PATH);
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (addr));
  g_assert_cmpstr (str, ==, "/some/path");
  g_free (str);
  g_object_unref (addr);

  /* ADDRESS_ANONYMOUS. */
  addr = g_unix_socket_address_new_with_type ("", 0,
                                              G_UNIX_SOCKET_ADDRESS_ANONYMOUS);
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (addr));
  g_assert_cmpstr (str, ==, "anonymous");
  g_free (str);
  g_object_unref (addr);

  /* ADDRESS_ABSTRACT. */
  addr = g_unix_socket_address_new_with_type ("abstract-path\0✋", 17,
                                              G_UNIX_SOCKET_ADDRESS_ABSTRACT);
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (addr));
  g_assert_cmpstr (str, ==, "abstract-path\\x00\\xe2\\x9c\\x8b");
  g_free (str);
  g_object_unref (addr);

  /* ADDRESS_ABSTRACT_PADDED. */
  addr = g_unix_socket_address_new_with_type ("abstract-path\0✋", 17,
                                              G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED);
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (addr));
  g_assert_cmpstr (str, ==, "abstract-path\\x00\\xe2\\x9c\\x8b");
  g_free (str);
  g_object_unref (addr);
}

int
main (int    argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/socket/address/unix/construct", test_unix_socket_address_construct);
  g_test_add_func ("/socket/address/unix/to-string", test_unix_socket_address_to_string);

  return g_test_run ();
}
