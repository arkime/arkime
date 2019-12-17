/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include <gio/gio.h>
#include <unistd.h>
#include <string.h>

/* ---------------------------------------------------------------------------------------------------- */
/* Test that registered errors are properly mapped */
/* ---------------------------------------------------------------------------------------------------- */

static void
check_registered_error (const gchar *given_dbus_error_name,
                        GQuark       error_domain,
                        gint         error_code)
{
  GError *error;
  gchar *dbus_error_name;

  error = g_dbus_error_new_for_dbus_error (given_dbus_error_name, "test message");
  g_assert_error (error, error_domain, error_code);
  g_assert (g_dbus_error_is_remote_error (error));
  g_assert (g_dbus_error_strip_remote_error (error));
  g_assert_cmpstr (error->message, ==, "test message");
  dbus_error_name = g_dbus_error_get_remote_error (error);
  g_assert_cmpstr (dbus_error_name, ==, given_dbus_error_name);
  g_free (dbus_error_name);
  g_error_free (error);
}

static void
test_registered_errors (void)
{
  /* Here we check that we are able to map to GError and back for registered
   * errors.
   *
   * For example, if "org.freedesktop.DBus.Error.AddressInUse" is
   * associated with (G_DBUS_ERROR, G_DBUS_ERROR_DBUS_FAILED), check
   * that
   *
   *  - Creating a GError for e.g. "org.freedesktop.DBus.Error.AddressInUse"
   *    has (error_domain, code) == (G_DBUS_ERROR, G_DBUS_ERROR_DBUS_FAILED)
   *
   *  - That it is possible to recover e.g. "org.freedesktop.DBus.Error.AddressInUse"
   *    as the D-Bus error name when dealing with an error with (error_domain, code) ==
   *    (G_DBUS_ERROR, G_DBUS_ERROR_DBUS_FAILED)
   *
   * We just check a couple of well-known errors.
   */
  check_registered_error ("org.freedesktop.DBus.Error.Failed",
                          G_DBUS_ERROR,
                          G_DBUS_ERROR_FAILED);
  check_registered_error ("org.freedesktop.DBus.Error.AddressInUse",
                          G_DBUS_ERROR,
                          G_DBUS_ERROR_ADDRESS_IN_USE);
  check_registered_error ("org.freedesktop.DBus.Error.UnknownMethod",
                          G_DBUS_ERROR,
                          G_DBUS_ERROR_UNKNOWN_METHOD);
  check_registered_error ("org.freedesktop.DBus.Error.UnknownObject",
                          G_DBUS_ERROR,
                          G_DBUS_ERROR_UNKNOWN_OBJECT);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
check_unregistered_error (const gchar *given_dbus_error_name)
{
  GError *error;
  gchar *dbus_error_name;

  error = g_dbus_error_new_for_dbus_error (given_dbus_error_name, "test message");
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_DBUS_ERROR);
  g_assert (g_dbus_error_is_remote_error (error));
  dbus_error_name = g_dbus_error_get_remote_error (error);
  g_assert_cmpstr (dbus_error_name, ==, given_dbus_error_name);
  g_free (dbus_error_name);

  /* strip the message */
  g_assert (g_dbus_error_strip_remote_error (error));
  g_assert_cmpstr (error->message, ==, "test message");

  /* check that we can no longer recover the D-Bus error name */
  g_assert (g_dbus_error_get_remote_error (error) == NULL);

  g_error_free (error);

}

static void
test_unregistered_errors (void)
{
  /* Here we check that we are able to map to GError and back for unregistered
   * errors.
   *
   * For example, if "com.example.Error.Failed" is not registered, then check
   *
   *  - Creating a GError for e.g. "com.example.Error.Failed" has (error_domain, code) ==
   *    (G_IO_ERROR, G_IO_ERROR_DBUS_ERROR)
   *
   *  - That it is possible to recover e.g. "com.example.Error.Failed" from that
   *    GError.
   *
   * We just check a couple of random errors.
   */

  check_unregistered_error ("com.example.Error.Failed");
  check_unregistered_error ("foobar.buh");
}

/* ---------------------------------------------------------------------------------------------------- */

static void
check_transparent_gerror (GQuark error_domain,
                          gint   error_code)
{
  GError *error;
  gchar *given_dbus_error_name;
  gchar *dbus_error_name;

  error = g_error_new (error_domain, error_code, "test message");
  given_dbus_error_name = g_dbus_error_encode_gerror (error);
  g_assert (g_str_has_prefix (given_dbus_error_name, "org.gtk.GDBus.UnmappedGError.Quark"));
  g_error_free (error);

  error = g_dbus_error_new_for_dbus_error (given_dbus_error_name, "test message");
  g_assert_error (error, error_domain, error_code);
  g_assert (g_dbus_error_is_remote_error (error));
  dbus_error_name = g_dbus_error_get_remote_error (error);
  g_assert_cmpstr (dbus_error_name, ==, given_dbus_error_name);
  g_free (dbus_error_name);
  g_free (given_dbus_error_name);

  /* strip the message */
  g_assert (g_dbus_error_strip_remote_error (error));
  g_assert_cmpstr (error->message, ==, "test message");

  /* check that we can no longer recover the D-Bus error name */
  g_assert (g_dbus_error_get_remote_error (error) == NULL);

  g_error_free (error);
}

static void
test_transparent_gerror (void)
{
  /* Here we check that we are able to transparent pass unregistered GError's
   * over the wire.
   *
   * For example, if G_IO_ERROR_FAILED is not registered, then check
   *
   *  - g_dbus_error_encode_gerror() returns something of the form
   *    org.gtk.GDBus.UnmappedGError.Quark_HEXENCODED_QUARK_NAME_.Code_ERROR_CODE
   *
   *  - mapping back the D-Bus error name gives us G_IO_ERROR_FAILED
   *
   *  - That it is possible to recover the D-Bus error name from the
   *    GError.
   *
   * We just check a couple of random errors.
   */

  check_transparent_gerror (G_IO_ERROR, G_IO_ERROR_FAILED);
  check_transparent_gerror (G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_PARSE);
}

typedef enum
{
  TEST_ERROR_FAILED,
  TEST_ERROR_BLA
} TestError;

GDBusErrorEntry test_error_entries[] =
{
  { TEST_ERROR_FAILED, "org.gtk.test.Error.Failed" },
  { TEST_ERROR_BLA,    "org.gtk.test.Error.Bla"    }
};

static void
test_register_error (void)
{
  gsize test_error_quark = 0;
  gboolean res;
  gchar *msg;
  GError *error;

  g_dbus_error_register_error_domain ("test-error-quark",
                                      &test_error_quark,
                                      test_error_entries,
                                      G_N_ELEMENTS (test_error_entries));
  g_assert_cmpint (test_error_quark, !=, 0);

  error = g_dbus_error_new_for_dbus_error ("org.gtk.test.Error.Failed", "Failed");
  g_assert_error (error, test_error_quark, TEST_ERROR_FAILED);
  res = g_dbus_error_is_remote_error (error);
  msg = g_dbus_error_get_remote_error (error);
  g_assert (res);
  g_assert_cmpstr (msg, ==, "org.gtk.test.Error.Failed");
  res = g_dbus_error_strip_remote_error (error);
  g_assert (res);
  g_assert_cmpstr (error->message, ==, "Failed");
  g_clear_error (&error);
  g_free (msg);

  g_dbus_error_set_dbus_error (&error, "org.gtk.test.Error.Failed", "Failed again", "Prefix %d", 1);
  res = g_dbus_error_is_remote_error (error);
  msg = g_dbus_error_get_remote_error (error);
  g_assert (res);
  g_assert_cmpstr (msg, ==, "org.gtk.test.Error.Failed");
  res = g_dbus_error_strip_remote_error (error);
  g_assert (res);
  g_assert_cmpstr (error->message, ==, "Prefix 1: Failed again");
  g_clear_error (&error);
  g_free (msg);

  error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_NOT_EMPTY, "Not Empty");
  res = g_dbus_error_is_remote_error (error);
  msg = g_dbus_error_get_remote_error (error);
  g_assert (!res);
  g_assert_cmpstr (msg, ==, NULL);
  res = g_dbus_error_strip_remote_error (error);
  g_assert (!res);
  g_assert_cmpstr (error->message, ==, "Not Empty");
  g_clear_error (&error);

  error = g_error_new_literal (test_error_quark, TEST_ERROR_BLA, "Bla");
  msg = g_dbus_error_encode_gerror (error);
  g_assert_cmpstr (msg, ==, "org.gtk.test.Error.Bla");
  g_free (msg);
  g_clear_error (&error);

  res = g_dbus_error_unregister_error (test_error_quark,
                                       TEST_ERROR_BLA, "org.gtk.test.Error.Bla");
  g_assert (res);
}


/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gdbus/registered-errors", test_registered_errors);
  g_test_add_func ("/gdbus/unregistered-errors", test_unregistered_errors);
  g_test_add_func ("/gdbus/transparent-gerror", test_transparent_gerror);
  g_test_add_func ("/gdbus/register-error", test_register_error);

  return g_test_run();
}
