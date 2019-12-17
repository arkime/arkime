/* Testcase for bug in GIO function g_file_query_filesystem_info()
 * Author: Nelson Benítez León
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gunixmounts.h>

static void
test_filesystem_readonly (gconstpointer with_mount_monitor)
{
  GFileInfo *file_info;
  GFile *mounted_file;
  GUnixMountMonitor *mount_monitor;
  gchar *bindfs, *fusermount;
  gchar *command_mount, *command_mount_ro, *command_umount;
  gchar *curdir, *dir_to_mount, *dir_mountpoint;
  gchar *file_in_mount, *file_in_mountpoint;

  /* installed by package 'bindfs' in Fedora */
  bindfs = g_find_program_in_path ("bindfs");

  /* installed by package 'fuse' in Fedora */
  fusermount = g_find_program_in_path ("fusermount");

  if (bindfs == NULL || fusermount == NULL)
    {
      /* We need these because "mount --bind" requires root privileges */
      g_test_skip ("'bindfs' and 'fusermount' commands are needed to run this test");
      return;
    }

  curdir = g_get_current_dir ();
  dir_to_mount = g_strdup_printf ("%s/dir_bindfs_to_mount", curdir);
  file_in_mount = g_strdup_printf ("%s/example.txt", dir_to_mount);
  dir_mountpoint = g_strdup_printf ("%s/dir_bindfs_mountpoint", curdir);

  g_mkdir (dir_to_mount, 0777);
  g_mkdir (dir_mountpoint, 0777);
  if (! g_file_set_contents (file_in_mount, "Example", -1, NULL))
    {
      g_test_skip ("Failed to create file needed to proceed further with the test");
      return;
    }

  if (with_mount_monitor)
    {
      mount_monitor = g_unix_mount_monitor_get ();
    }

  /* Use bindfs, which does not need root privileges, to mount the contents of one dir
   * into another dir (and do the mount as readonly as per passed '-o ro' option) */
  command_mount_ro = g_strdup_printf ("%s -n -o ro '%s' '%s'", bindfs, dir_to_mount, dir_mountpoint);
  g_spawn_command_line_sync (command_mount_ro, NULL, NULL, NULL, NULL);

  /* Let's check now, that the file is in indeed in a readonly filesystem */
  file_in_mountpoint = g_strdup_printf ("%s/example.txt", dir_mountpoint);
  mounted_file = g_file_new_for_path (file_in_mountpoint);

  if (with_mount_monitor)
    {
     /* Let UnixMountMonitor process its 'mounts-changed'
      * signal triggered by mount operation above */
      while (g_main_context_iteration (NULL, FALSE));
    }

  file_info = g_file_query_filesystem_info (mounted_file,
                                            G_FILE_ATTRIBUTE_FILESYSTEM_READONLY, NULL, NULL);
  if (! g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_FILESYSTEM_READONLY))
    {
      g_test_skip ("Failed to create readonly file needed to proceed further with the test");
      return;
    }

  /* Now we unmount, and mount again but this time rw (not readonly) */
  command_umount = g_strdup_printf ("%s -u '%s'", fusermount, dir_mountpoint);
  g_spawn_command_line_sync (command_umount, NULL, NULL, NULL, NULL);
  command_mount = g_strdup_printf ("%s -n '%s' '%s'", bindfs, dir_to_mount, dir_mountpoint);
  g_spawn_command_line_sync (command_mount, NULL, NULL, NULL, NULL);

  if (with_mount_monitor)
    {
     /* Let UnixMountMonitor process its 'mounts-changed' signal
      * triggered by mount/umount operations above */
      while (g_main_context_iteration (NULL, FALSE));
    }

  /* Now let's test if GIO will report the new filesystem state */
  g_clear_object (&file_info);
  g_clear_object (&mounted_file);
  mounted_file = g_file_new_for_path (file_in_mountpoint);
  file_info = g_file_query_filesystem_info (mounted_file,
                                            G_FILE_ATTRIBUTE_FILESYSTEM_READONLY, NULL, NULL);

  if (g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_FILESYSTEM_READONLY))
    {
      /* ¡¡ GIO still reports filesystem as being Readonly !!
       * Let's check if that's true by trying to write to file */
      GFileOutputStream *write_stream;
      write_stream = g_file_append_to (mounted_file, G_FILE_CREATE_NONE, NULL, NULL);
      if (write_stream != NULL)
        {
          /* The file has been opened for writing without error, so ¡¡ GIO IS WRONG !! */
          g_object_unref (write_stream);
          g_test_fail (); /* Marking test as FAILED */
        }
    }

  /* Clean up */
  if (with_mount_monitor)
    g_clear_object (&mount_monitor);

  g_clear_object (&file_info);
  g_clear_object (&mounted_file);
  g_spawn_command_line_sync (command_umount, NULL, NULL, NULL, NULL); /* unmount */

  g_remove (file_in_mount);
  g_remove (dir_to_mount);
  g_remove (dir_mountpoint);

  g_free (bindfs);
  g_free (fusermount);
  g_free (curdir);
  g_free (dir_to_mount);
  g_free (dir_mountpoint);
  g_free (command_mount);
  g_free (command_mount_ro);
  g_free (command_umount);
  g_free (file_in_mount);
  g_free (file_in_mountpoint);
}

int
main (int argc, char *argv[])
{
  /* To avoid unnecessary D-Bus calls, see http://goo.gl/ir56j2 */
  g_setenv ("GIO_USE_VFS", "local", FALSE);

  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");
  g_test_bug ("787731");

  g_test_add_data_func ("/g-file-info-filesystem-readonly/test-fs-ro",
                        GINT_TO_POINTER (FALSE), test_filesystem_readonly);

  /* This second test is using a running GUnixMountMonitor, so the calls to:
   *  g_unix_mount_get(&time_read) - To fill the time_read parameter
   *  g_unix_mounts_changed_since()
   *
   * made from inside g_file_query_filesystem_info() will use the mount_poller_time
   * from the monitoring of /proc/self/mountinfo , while in the previous test new
   * created timestamps are returned from those g_unix_mount* functions. */
  g_test_add_data_func ("/g-file-info-filesystem-readonly/test-fs-ro-with-mount-monitor",
                        GINT_TO_POINTER (TRUE), test_filesystem_readonly);

  return g_test_run ();
}
