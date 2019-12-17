/* GLib testing framework examples and tests
 *
 * Copyright © 2017 Endless Mobile, Inc.
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
#include <locale.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gunixmounts.h>

static void
test_is_system_fs_type (void)
{
  g_assert_true (g_unix_is_system_fs_type ("tmpfs"));
  g_assert_false (g_unix_is_system_fs_type ("ext4"));

  /* Check that some common network file systems aren’t considered ‘system’. */
  g_assert_false (g_unix_is_system_fs_type ("cifs"));
  g_assert_false (g_unix_is_system_fs_type ("nfs"));
  g_assert_false (g_unix_is_system_fs_type ("nfs4"));
  g_assert_false (g_unix_is_system_fs_type ("smbfs"));
}

static void
test_is_system_device_path (void)
{
  g_assert_true (g_unix_is_system_device_path ("devpts"));
  g_assert_false (g_unix_is_system_device_path ("/"));
}

int
main (int   argc,
      char *argv[])
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/unix-mounts/is-system-fs-type", test_is_system_fs_type);
  g_test_add_func ("/unix-mounts/is-system-device-path", test_is_system_device_path);

  return g_test_run ();
}
