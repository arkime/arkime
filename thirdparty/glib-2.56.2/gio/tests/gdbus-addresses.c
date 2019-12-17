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

#ifdef G_OS_UNIX
#include <gio/gunixsocketaddress.h>
#endif

/* ---------------------------------------------------------------------------------------------------- */

static void
test_empty_address (void)
{
  GError *error;
  error = NULL;
  g_dbus_address_get_stream_sync ("",
                                  NULL,
                                  NULL,
                                  &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_error_free (error);
}

static void
assert_is_supported_address (const gchar *address)
{
  GError *error = NULL;

  g_assert_true (g_dbus_is_supported_address (address, NULL));
  g_assert_true (g_dbus_is_supported_address (address, &error));
  g_assert_no_error (error);
}

static void
assert_not_supported_address (const gchar *address)
{
  GError *error = NULL;

  g_assert_false (g_dbus_is_supported_address (address, NULL));
  g_assert_false (g_dbus_is_supported_address (address, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_clear_error (&error);
}

#ifdef G_OS_UNIX
static void
test_unix_address (void)
{
  assert_not_supported_address ("some-imaginary-transport:foo=bar");
  assert_is_supported_address ("unix:path=/tmp/dbus-test");
  assert_is_supported_address ("unix:abstract=/tmp/dbus-another-test");
  g_assert (g_dbus_is_address ("unix:foo=bar"));
  assert_not_supported_address ("unix:foo=bar");
  g_assert (!g_dbus_is_address ("unix:path=/foo;abstract=/bar"));
  assert_not_supported_address ("unix:path=/foo;abstract=/bar");
  assert_is_supported_address ("unix:path=/tmp/concrete;unix:abstract=/tmp/abstract");
  g_assert (g_dbus_is_address ("some-imaginary-transport:foo=bar"));

  g_assert (g_dbus_is_address ("some-imaginary-transport:foo=bar;unix:path=/this/is/valid"));
  assert_not_supported_address ("some-imaginary-transport:foo=bar;unix:path=/this/is/valid");
}
#endif

static void
test_nonce_tcp_address (void)
{
  assert_is_supported_address ("nonce-tcp:host=localhost,port=42,noncefile=/foo/bar");
  assert_is_supported_address ("nonce-tcp:host=localhost,port=42,noncefile=/foo/bar,family=ipv6");
  assert_is_supported_address ("nonce-tcp:host=localhost,port=42,noncefile=/foo/bar,family=ipv4");

  assert_not_supported_address ("nonce-tcp:host=localhost,port=42,noncefile=/foo/bar,family=blah");
  assert_not_supported_address ("nonce-tcp:host=localhost,port=420000,noncefile=/foo/bar,family=ipv4");
  assert_not_supported_address ("nonce-tcp:host=,port=x42,noncefile=/foo/bar,family=ipv4");
  assert_not_supported_address ("nonce-tcp:host=,port=42x,noncefile=/foo/bar,family=ipv4");
  assert_not_supported_address ("nonce-tcp:host=,port=420000,noncefile=/foo/bar,family=ipv4");
}

static void
test_tcp_address (void)
{
  assert_is_supported_address ("tcp:host=localhost");
  assert_not_supported_address ("tcp:host=localhost,noncefile=/tmp/foo");
  assert_is_supported_address ("tcp:host=localhost,port=42");
  assert_not_supported_address ("tcp:host=localhost,port=-1");
  assert_not_supported_address ("tcp:host=localhost,port=420000");
  assert_not_supported_address ("tcp:host=localhost,port=42x");
  assert_is_supported_address ("tcp:host=localhost,port=42,family=ipv4");
  assert_is_supported_address ("tcp:host=localhost,port=42,family=ipv6");
  assert_not_supported_address ("tcp:host=localhost,port=42,family=sopranos");
}

static void
test_autolaunch_address (void)
{
  assert_is_supported_address ("autolaunch:");
}

static void
test_mixed_address (void)
{
  assert_is_supported_address ("unix:path=/tmp/dbus1;unix:path=/tmp/dbus2");
  assert_is_supported_address ("tcp:host=localhost,port=42;autolaunch:");
  assert_not_supported_address ("tcp:host=localhost,port=42;tcp:family=bla");
}

static const struct { const char *before; const char *after; } escaping[] = {
      { "foo", "foo" },
      { "/.\\-_", "/.\\-_" },
      { "<=>", "%3C%3D%3E" },
      { "/foo~1", "/foo%7E1" },
      { "\xe2\x98\xad\xff", "%E2%98%AD%FF" },
      { NULL, NULL }
};

static void
test_escape_address (void)
{
  gsize i;

  for (i = 0; escaping[i].before != NULL; i++)
    {
      gchar *s = g_dbus_address_escape_value (escaping[i].before);

      g_assert_cmpstr (s, ==, escaping[i].after);
      g_free (s);
    }
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gdbus/empty-address", test_empty_address);
#ifdef G_OS_UNIX
  g_test_add_func ("/gdbus/unix-address", test_unix_address);
#endif
  g_test_add_func ("/gdbus/nonce-tcp-address", test_nonce_tcp_address);
  g_test_add_func ("/gdbus/tcp-address", test_tcp_address);
  g_test_add_func ("/gdbus/autolaunch-address", test_autolaunch_address);
  g_test_add_func ("/gdbus/mixed-address", test_mixed_address);
  g_test_add_func ("/gdbus/escape-address", test_escape_address);

  return g_test_run();
}

