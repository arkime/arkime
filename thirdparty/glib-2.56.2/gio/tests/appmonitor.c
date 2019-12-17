#include <gio/gio.h>
#include <gstdio.h>

typedef struct
{
  gchar *data_dir;
  gchar *applications_dir;
} Fixture;

static void
setup (Fixture       *fixture,
       gconstpointer  user_data)
{
  GError *error = NULL;

  fixture->data_dir = g_dir_make_tmp ("gio-test-app-monitor_XXXXXX", &error);
  g_assert_no_error (error);

  fixture->applications_dir = g_build_filename (fixture->data_dir, "applications", NULL);
  g_assert_cmpint (g_mkdir (fixture->applications_dir, 0755), ==, 0);

  g_setenv ("XDG_DATA_DIRS", fixture->data_dir, TRUE);
  g_setenv ("XDG_DATA_HOME", fixture->data_dir, TRUE);

  g_test_message ("Using data directory: %s", fixture->data_dir);
}

static void
teardown (Fixture       *fixture,
          gconstpointer  user_data)
{
  g_assert_cmpint (g_rmdir (fixture->applications_dir), ==, 0);
  g_clear_pointer (&fixture->applications_dir, g_free);

  g_assert_cmpint (g_rmdir (fixture->data_dir), ==, 0);
  g_clear_pointer (&fixture->data_dir, g_free);
}

static gboolean
create_app (gpointer data)
{
  const gchar *path = data;
  GError *error = NULL;
  const gchar *contents = 
    "[Desktop Entry]\n"
    "Name=Application\n"
    "Version=1.0\n"
    "Type=Application\n"
    "Exec=true\n";

  g_file_set_contents (path, contents, -1, &error);
  g_assert_no_error (error);

  return G_SOURCE_REMOVE;
}

static void
delete_app (gpointer data)
{
  const gchar *path = data;

  g_remove (path);
}

static gboolean changed_fired;

static void
changed_cb (GAppInfoMonitor *monitor, GMainLoop *loop)
{
  changed_fired = TRUE;
  g_main_loop_quit (loop);
}

static gboolean
quit_loop (gpointer data)
{
  GMainLoop *loop = data;

  if (g_main_loop_is_running (loop))
    g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static void
test_app_monitor (Fixture       *fixture,
                  gconstpointer  user_data)
{
  gchar *app_path;
  GAppInfoMonitor *monitor;
  GMainLoop *loop;

  app_path = g_build_filename (fixture->applications_dir, "app.desktop", NULL);

  /* FIXME: this shouldn't be required */
  g_list_free_full (g_app_info_get_all (), g_object_unref);

  monitor = g_app_info_monitor_get ();
  loop = g_main_loop_new (NULL, FALSE);

  g_signal_connect (monitor, "changed", G_CALLBACK (changed_cb), loop);

  g_idle_add (create_app, app_path);
  g_timeout_add_seconds (3, quit_loop, loop);

  g_main_loop_run (loop);
  g_assert (changed_fired);
  changed_fired = FALSE;

  /* FIXME: this shouldn't be required */
  g_list_free_full (g_app_info_get_all (), g_object_unref);

  g_timeout_add_seconds (3, quit_loop, loop);

  delete_app (app_path);

  g_main_loop_run (loop);

  g_assert (changed_fired);

  g_main_loop_unref (loop);
  g_remove (app_path);

  g_object_unref (monitor);

  g_free (app_path);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/monitor/app", Fixture, NULL, setup, test_app_monitor, teardown);

  return g_test_run ();
}
