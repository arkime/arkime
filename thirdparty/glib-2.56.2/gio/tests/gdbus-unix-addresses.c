/* GLib testing framework examples and tests
 *
 * Copyright © 2015 Collabora Ltd.
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
 */

#include <glib.h>

#ifndef G_OS_UNIX
#error This is a Unix-specific test
#endif

#include <errno.h>

#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>

static void
print_address (void)
{
  GError *error = NULL;
  gchar *addr;

  addr = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION, NULL,
      &error);

  g_assert_no_error (error);
  g_assert_nonnull (addr);
  g_print ("%s\n", addr);
  g_free (addr);
}

static GSocket *mock_bus = NULL;
static gchar *mock_bus_path = NULL;
/* this is deliberately something that needs escaping */
static gchar tmpdir[] = "/tmp/gdbus,unix,test.XXXXXX";

static void
set_up_mock_xdg_runtime_dir (void)
{
  GError *error = NULL;
  GSocketAddress *addr;

  mock_bus = g_socket_new (G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM, 0,
      &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_SOCKET (mock_bus));

  /* alters tmpdir in-place */
  if (g_mkdtemp_full (tmpdir, 0700) == NULL)
    {
      int errsv = errno;
      g_error ("g_mkdtemp_full: %s", g_strerror (errsv));
    }

  mock_bus_path = g_strconcat (tmpdir, "/bus", NULL);
  addr = g_unix_socket_address_new (mock_bus_path);
  g_socket_bind (mock_bus, addr, FALSE, &error);
  g_assert_no_error (error);
  g_object_unref (addr);

  g_setenv ("XDG_RUNTIME_DIR", tmpdir, TRUE);
}

static void
tear_down_mock_xdg_runtime_dir (void)
{
  GError *error = NULL;

  g_socket_close (mock_bus, &error);
  g_assert_no_error (error);

  if (g_unlink (mock_bus_path) < 0)
    {
      int errsv = errno;
      g_error ("g_unlink(\"%s\"): %s", mock_bus_path, g_strerror (errsv));
    }

  if (g_rmdir (tmpdir) < 0)
    {
      int errsv = errno;
      g_error ("g_rmdir(\"%s\"): %s", tmpdir, g_strerror (errsv));
    }

  g_clear_object (&mock_bus);
  g_clear_pointer (&mock_bus_path, g_free);
}

static gchar *path = NULL;

static void
set_up_mock_dbus_launch (void)
{
  path = g_strconcat (g_test_get_dir (G_TEST_BUILT), ":",
      g_getenv ("PATH"), NULL);
  g_debug ("PATH=%s", path);
  g_setenv ("PATH", path, TRUE);

  /* libdbus won't even try X11 autolaunch if DISPLAY is unset; GDBus
   * does the same in Debian derivatives (proposed upstream in
   * GNOME#723506) */
  g_setenv ("DISPLAY", "an unrealistic mock X11 display", TRUE);
}

static void
tear_down_mock_dbus_launch (void)
{
  g_clear_pointer (&path, g_free);
}

static void
test_x11_autolaunch (void)
{
  if (g_test_subprocess ())
    {
      g_unsetenv ("DISPLAY");
      g_unsetenv ("DBUS_SESSION_BUS_ADDRESS");
      g_unsetenv ("XDG_RUNTIME_DIR");
      set_up_mock_dbus_launch ();

      print_address ();

      tear_down_mock_dbus_launch ();
      return;
    }

  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_stderr_unmatched ("?*");
  g_test_trap_assert_stdout ("hello:this=address-is-from-the,mock=dbus-launch\n");
  g_test_trap_assert_passed ();
}

static void
test_xdg_runtime (void)
{
  if (g_test_subprocess ())
    {
      g_unsetenv ("DISPLAY");
      g_unsetenv ("DBUS_SESSION_BUS_ADDRESS");
      set_up_mock_xdg_runtime_dir ();
      set_up_mock_dbus_launch ();

      print_address ();

      tear_down_mock_dbus_launch ();
      tear_down_mock_xdg_runtime_dir ();
      return;
    }

  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_stderr_unmatched ("?*");
  g_test_trap_assert_stdout ("unix:path=/tmp/gdbus%2Cunix%2Ctest.*/bus\n");
  g_test_trap_assert_passed ();
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gdbus/x11-autolaunch", test_x11_autolaunch);
  g_test_add_func ("/gdbus/xdg-runtime", test_xdg_runtime);

  return g_test_run();
}
