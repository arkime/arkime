#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gdbus-tests.h"
#include "gdbus-sessionbus.h"

#if 0
/* These tests are racy -- there is no guarantee about the order of data
 * arriving over D-Bus.
 *
 * They're also a bit ridiculous -- GApplication was never meant to be
 * abused in this way...
 *
 * We need new tests.
 */
static gint outstanding_watches;
static GMainLoop *main_loop;

typedef struct
{
  gchar *expected_stdout;
  gint stdout_pipe;
  gchar *expected_stderr;
  gint stderr_pipe;
} ChildData;

static void
check_data (gint fd, const gchar *expected)
{
  gssize len, actual;
  gchar *buffer;
  
  len = strlen (expected);
  buffer = g_alloca (len + 100);
  actual = read (fd, buffer, len + 100);

  g_assert_cmpint (actual, >=, 0);

  if (actual != len ||
      memcmp (buffer, expected, len) != 0)
    {
      buffer[MIN(len + 100, actual)] = '\0';

      g_error ("\nExpected\n-----\n%s-----\nGot (%s)\n-----\n%s-----\n",
               expected,
               (actual > len) ? "truncated" : "full", buffer);
    }
}

static void
child_quit (GPid     pid,
            gint     status,
            gpointer data)
{
  ChildData *child = data;

  g_assert_cmpint (status, ==, 0);

  if (--outstanding_watches == 0)
    g_main_loop_quit (main_loop);

  check_data (child->stdout_pipe, child->expected_stdout);
  close (child->stdout_pipe);
  g_free (child->expected_stdout);

  if (child->expected_stderr)
    {
      check_data (child->stderr_pipe, child->expected_stderr);
      close (child->stderr_pipe);
      g_free (child->expected_stderr);
    }

  g_slice_free (ChildData, child);
}

static void
spawn (const gchar *expected_stdout,
       const gchar *expected_stderr,
       const gchar *first_arg,
       ...)
{
  GError *error = NULL;
  const gchar *arg;
  GPtrArray *array;
  ChildData *data;
  gchar **args;
  va_list ap;
  GPid pid;
  GPollFD fd;
  gchar **env;

  va_start (ap, first_arg);
  array = g_ptr_array_new ();
  g_ptr_array_add (array, g_test_build_filename (G_TEST_BUILT, "basic-application", NULL));
  for (arg = first_arg; arg; arg = va_arg (ap, const gchar *))
    g_ptr_array_add (array, g_strdup (arg));
  g_ptr_array_add (array, NULL);
  args = (gchar **) g_ptr_array_free (array, FALSE);
  va_end (ap);

  env = g_environ_setenv (g_get_environ (), "TEST", "1", TRUE);

  data = g_slice_new (ChildData);
  data->expected_stdout = g_strdup (expected_stdout);
  data->expected_stderr = g_strdup (expected_stderr);

  g_spawn_async_with_pipes (NULL, args, env,
                            G_SPAWN_DO_NOT_REAP_CHILD,
                            NULL, NULL, &pid, NULL,
                            &data->stdout_pipe,
                            expected_stderr ? &data->stderr_pipe : NULL,
                            &error);
  g_assert_no_error (error);

  g_strfreev (env);

  g_child_watch_add (pid, child_quit, data);
  outstanding_watches++;

  /* we block until the children write to stdout to make sure
   * they have started, as they need to be executed in order;
   * see https://bugzilla.gnome.org/show_bug.cgi?id=664627
   */
  fd.fd = data->stdout_pipe;
  fd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
  g_poll (&fd, 1, -1);
}

static void
basic (void)
{
  GDBusConnection *c;

  g_assert (outstanding_watches == 0);

  session_bus_up ();
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  main_loop = g_main_loop_new (NULL, 0);

  /* spawn the master */
  spawn ("activated\n"
         "open file:///a file:///b\n"
         "exit status: 0\n", NULL,
         "./app", NULL);

  /* send it some files */
  spawn ("exit status: 0\n", NULL,
         "./app", "/a", "/b", NULL);

  g_main_loop_run (main_loop);

  g_object_unref (c);
  session_bus_down ();

  g_main_loop_unref (main_loop);
}

static void
test_remote_command_line (void)
{
  GDBusConnection *c;
  GFile *file;
  gchar *replies;
  gchar *cwd;

  g_assert (outstanding_watches == 0);

  session_bus_up ();
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  main_loop = g_main_loop_new (NULL, 0);

  file = g_file_new_for_commandline_arg ("foo");
  cwd = g_get_current_dir ();

  replies = g_strconcat ("got ./cmd 0\n",
                         "got ./cmd 1\n",
                         "cmdline ./cmd echo --abc -d\n",
                         "environment TEST=1\n",
                         "getenv TEST=1\n",
                         "file ", g_file_get_path (file), "\n",
                         "properties ok\n",
                         "cwd ", cwd, "\n",
                         "busy\n",
                         "idle\n",
                         "stdin ok\n",        
                         "exit status: 0\n",
                         NULL);
  g_object_unref (file);

  /* spawn the master */
  spawn (replies, NULL,
         "./cmd", NULL);

  g_free (replies);

  /* send it a few commandlines */
  spawn ("exit status: 0\n", NULL,
         "./cmd", NULL);

  spawn ("exit status: 0\n", NULL,
         "./cmd", "echo", "--abc", "-d", NULL);

  spawn ("exit status: 0\n", NULL,
         "./cmd", "env", NULL);

  spawn ("exit status: 0\n", NULL,
         "./cmd", "getenv", NULL);

  spawn ("print test\n"
         "exit status: 0\n", NULL,
         "./cmd", "print", "test", NULL);

  spawn ("exit status: 0\n", "printerr test\n",
         "./cmd", "printerr", "test", NULL);

  spawn ("exit status: 0\n", NULL,
         "./cmd", "file", "foo", NULL);

  spawn ("exit status: 0\n", NULL,
         "./cmd", "properties", NULL);

  spawn ("exit status: 0\n", NULL,
         "./cmd", "cwd", NULL);

  spawn ("exit status: 0\n", NULL,
         "./cmd", "busy", NULL);

  spawn ("exit status: 0\n", NULL,
         "./cmd", "idle", NULL);

  spawn ("exit status: 0\n", NULL,
         "./cmd", "stdin", NULL);

  g_main_loop_run (main_loop);

  g_object_unref (c);
  session_bus_down ();

  g_main_loop_unref (main_loop);
}

static void
test_remote_actions (void)
{
  GDBusConnection *c;

  g_assert (outstanding_watches == 0);

  session_bus_up ();
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  main_loop = g_main_loop_new (NULL, 0);

  /* spawn the master */
  spawn ("got ./cmd 0\n"
         "activate action1\n"
         "change action2 1\n"
         "exit status: 0\n", NULL,
         "./cmd", NULL);

  spawn ("actions quit new action1 action2\n"
         "exit status: 0\n", NULL,
         "./actions", "list", NULL);

  spawn ("exit status: 0\n", NULL,
         "./actions", "activate", NULL);

  spawn ("exit status: 0\n", NULL,
         "./actions", "set-state", NULL);

  g_main_loop_run (main_loop);

  g_object_unref (c);
  session_bus_down ();

  g_main_loop_unref (main_loop);
}
#endif

#if 0
/* Now that we register non-unique apps on the bus we need to fix the
 * following test not to assume that it's safe to create multiple instances
 * of the same app in one process.
 *
 * See https://bugzilla.gnome.org/show_bug.cgi?id=647986 for the patch that
 * introduced this problem.
 */

static GApplication *recently_activated;
static GMainLoop *loop;

static void
nonunique_activate (GApplication *application)
{
  recently_activated = application;

  if (loop != NULL)
    g_main_loop_quit (loop);
}

static GApplication *
make_app (gboolean non_unique)
{
  GApplication *app;
  gboolean ok;

  app = g_application_new ("org.gtk.Test-Application",
                           non_unique ? G_APPLICATION_NON_UNIQUE : 0);
  g_signal_connect (app, "activate", G_CALLBACK (nonunique_activate), NULL);
  ok = g_application_register (app, NULL, NULL);
  if (!ok)
    {
      g_object_unref (app);
      return NULL;
    }

  g_application_activate (app);

  return app;
}

static void
test_nonunique (void)
{
  GApplication *first, *second, *third, *fourth;

  session_bus_up ();

  first = make_app (TRUE);
  /* non-remote because it is non-unique */
  g_assert (!g_application_get_is_remote (first));
  g_assert (recently_activated == first);
  recently_activated = NULL;

  second = make_app (FALSE);
  /* non-remote because it is first */
  g_assert (!g_application_get_is_remote (second));
  g_assert (recently_activated == second);
  recently_activated = NULL;

  third = make_app (TRUE);
  /* non-remote because it is non-unique */
  g_assert (!g_application_get_is_remote (third));
  g_assert (recently_activated == third);
  recently_activated = NULL;

  fourth = make_app (FALSE);
  /* should have failed to register due to being
   * unable to register the object paths
   */
  g_assert (fourth == NULL);
  g_assert (recently_activated == NULL);

  g_object_unref (first);
  g_object_unref (second);
  g_object_unref (third);

  session_bus_down ();
}
#endif

static void
properties (void)
{
  GDBusConnection *c;
  GObject *app;
  gchar *id;
  GApplicationFlags flags;
  gboolean registered;
  guint timeout;
  gboolean remote;
  gboolean ret;
  GError *error = NULL;

  session_bus_up ();
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  app = g_object_new (G_TYPE_APPLICATION,
                      "application-id", "org.gtk.TestApplication",
                      NULL);

  g_object_get (app,
                "application-id", &id,
                "flags", &flags,
                "is-registered", &registered,
                "inactivity-timeout", &timeout,
                NULL);

  g_assert_cmpstr (id, ==, "org.gtk.TestApplication");
  g_assert_cmpint (flags, ==, G_APPLICATION_FLAGS_NONE);
  g_assert (!registered);
  g_assert_cmpint (timeout, ==, 0);

  ret = g_application_register (G_APPLICATION (app), NULL, &error);
  g_assert (ret);
  g_assert_no_error (error);

  g_object_get (app,
                "is-registered", &registered,
                "is-remote", &remote,
                NULL);

  g_assert (registered);
  g_assert (!remote);

  g_object_set (app,
                "inactivity-timeout", 1000,
                NULL);

  g_application_quit (G_APPLICATION (app));

  g_object_unref (c);
  g_object_unref (app);
  g_free (id);

  session_bus_down ();
}

static void
appid (void)
{
  gchar *id;

  g_assert_false (g_application_id_is_valid (""));
  g_assert_false (g_application_id_is_valid ("."));
  g_assert_false (g_application_id_is_valid ("a"));
  g_assert_false (g_application_id_is_valid ("abc"));
  g_assert_false (g_application_id_is_valid (".abc"));
  g_assert_false (g_application_id_is_valid ("abc."));
  g_assert_false (g_application_id_is_valid ("a..b"));
  g_assert_false (g_application_id_is_valid ("a/b"));
  g_assert_false (g_application_id_is_valid ("a\nb"));
  g_assert_false (g_application_id_is_valid ("a\nb"));
  g_assert_false (g_application_id_is_valid ("emoji_picker"));
  g_assert_false (g_application_id_is_valid ("emoji-picker"));
  g_assert_false (g_application_id_is_valid ("emojipicker"));
  g_assert_false (g_application_id_is_valid ("my.Terminal.0123"));
  id = g_new0 (gchar, 261);
  memset (id, 'a', 260);
  id[1] = '.';
  id[260] = 0;
  g_assert_false (g_application_id_is_valid (id));
  g_free (id);

  g_assert_true (g_application_id_is_valid ("a.b"));
  g_assert_true (g_application_id_is_valid ("A.B"));
  g_assert_true (g_application_id_is_valid ("A-.B"));
  g_assert_true (g_application_id_is_valid ("a_b.c-d"));
  g_assert_true (g_application_id_is_valid ("_a.b"));
  g_assert_true (g_application_id_is_valid ("-a.b"));
  g_assert_true (g_application_id_is_valid ("org.gnome.SessionManager"));
  g_assert_true (g_application_id_is_valid ("my.Terminal._0123"));
  g_assert_true (g_application_id_is_valid ("com.example.MyApp"));
  g_assert_true (g_application_id_is_valid ("com.example.internal_apps.Calculator"));
  g_assert_true (g_application_id_is_valid ("org._7_zip.Archiver"));
}

static gboolean nodbus_activated;

static gboolean
release_app (gpointer user_data)
{
  g_application_release (user_data);
  return G_SOURCE_REMOVE;
}

static void
nodbus_activate (GApplication *app)
{
  nodbus_activated = TRUE;
  g_application_hold (app);

  g_assert (g_application_get_dbus_connection (app) == NULL);
  g_assert (g_application_get_dbus_object_path (app) == NULL);

  g_idle_add (release_app, app);
}

static void
test_nodbus (void)
{
  char *binpath = g_test_build_filename (G_TEST_BUILT, "unimportant", NULL);
  gchar *argv[] = { binpath, NULL };
  GApplication *app;

  app = g_application_new ("org.gtk.Unimportant", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (nodbus_activate), NULL);
  g_application_run (app, 1, argv);
  g_object_unref (app);

  g_assert (nodbus_activated);
  g_free (binpath);
}

static gboolean noappid_activated;

static void
noappid_activate (GApplication *app)
{
  noappid_activated = TRUE;
  g_application_hold (app);

  g_assert (g_application_get_flags (app) & G_APPLICATION_NON_UNIQUE);

  g_idle_add (release_app, app);
}

/* test that no appid -> non-unique */
static void
test_noappid (void)
{
  char *binpath = g_test_build_filename (G_TEST_BUILT, "unimportant", NULL);
  gchar *argv[] = { binpath, NULL };
  GApplication *app;

  app = g_application_new (NULL, G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (noappid_activate), NULL);
  g_application_run (app, 1, argv);
  g_object_unref (app);

  g_assert (noappid_activated);
  g_free (binpath);
}

static gboolean activated;
static gboolean quitted;

static gboolean
quit_app (gpointer user_data)
{
  quitted = TRUE;
  g_application_quit (user_data);
  return G_SOURCE_REMOVE;
}

static void
quit_activate (GApplication *app)
{
  activated = TRUE;
  g_application_hold (app);

  g_assert (g_application_get_dbus_connection (app) != NULL);
  g_assert (g_application_get_dbus_object_path (app) != NULL);

  g_idle_add (quit_app, app);
}

static void
test_quit (void)
{
  GDBusConnection *c;
  char *binpath = g_test_build_filename (G_TEST_BUILT, "unimportant", NULL);
  gchar *argv[] = { binpath, NULL };
  GApplication *app;

  session_bus_up ();
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  app = g_application_new ("org.gtk.Unimportant",
                           G_APPLICATION_FLAGS_NONE);
  activated = FALSE;
  quitted = FALSE;
  g_signal_connect (app, "activate", G_CALLBACK (quit_activate), NULL);
  g_application_run (app, 1, argv);
  g_object_unref (app);
  g_object_unref (c);

  g_assert (activated);
  g_assert (quitted);

  session_bus_down ();
  g_free (binpath);
}

static void
on_activate (GApplication *app)
{
  gchar **actions;
  GAction *action;
  GVariant *state;

  g_assert (!g_application_get_is_remote (app));

  actions = g_action_group_list_actions (G_ACTION_GROUP (app));
  g_assert (g_strv_length (actions) == 0);
  g_strfreev (actions);

  action = (GAction*)g_simple_action_new_stateful ("test", G_VARIANT_TYPE_BOOLEAN, g_variant_new_boolean (FALSE));
  g_action_map_add_action (G_ACTION_MAP (app), action);

  actions = g_action_group_list_actions (G_ACTION_GROUP (app));
  g_assert (g_strv_length (actions) == 1);
  g_strfreev (actions);

  g_action_group_change_action_state (G_ACTION_GROUP (app), "test", g_variant_new_boolean (TRUE));
  state = g_action_group_get_action_state (G_ACTION_GROUP (app), "test");
  g_assert (g_variant_get_boolean (state) == TRUE);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "test");
  g_assert (action != NULL);

  g_action_map_remove_action (G_ACTION_MAP (app), "test");

  actions = g_action_group_list_actions (G_ACTION_GROUP (app));
  g_assert (g_strv_length (actions) == 0);
  g_strfreev (actions);
}

static void
test_local_actions (void)
{
  char *binpath = g_test_build_filename (G_TEST_BUILT, "unimportant", NULL);
  gchar *argv[] = { binpath, NULL };
  GApplication *app;

  app = g_application_new ("org.gtk.Unimportant",
                           G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);
  g_application_run (app, 1, argv);
  g_object_unref (app);
  g_free (binpath);
}

typedef GApplication TestLocCmdApp;
typedef GApplicationClass TestLocCmdAppClass;

static GType test_loc_cmd_app_get_type (void);
G_DEFINE_TYPE (TestLocCmdApp, test_loc_cmd_app, G_TYPE_APPLICATION)

static void
test_loc_cmd_app_init (TestLocCmdApp *app)
{
}

static void
test_loc_cmd_app_startup (GApplication *app)
{
  g_assert_not_reached ();
}

static void
test_loc_cmd_app_shutdown (GApplication *app)
{
  g_assert_not_reached ();
}

static gboolean
test_loc_cmd_app_local_command_line (GApplication   *application,
                                     gchar        ***arguments,
                                     gint           *exit_status)
{
  return TRUE;
}

static void
test_loc_cmd_app_class_init (TestLocCmdAppClass *klass)
{
  G_APPLICATION_CLASS (klass)->startup = test_loc_cmd_app_startup;
  G_APPLICATION_CLASS (klass)->shutdown = test_loc_cmd_app_shutdown;
  G_APPLICATION_CLASS (klass)->local_command_line = test_loc_cmd_app_local_command_line;
}

static void
test_local_command_line (void)
{
  char *binpath = g_test_build_filename (G_TEST_BUILT, "unimportant", NULL);
  gchar *argv[] = { binpath, "-invalid", NULL };
  GApplication *app;

  app = g_object_new (test_loc_cmd_app_get_type (),
                      "application-id", "org.gtk.Unimportant",
                      "flags", G_APPLICATION_FLAGS_NONE,
                      NULL);
  g_application_run (app, 1, argv);
  g_object_unref (app);
  g_free (binpath);
}

static void
test_resource_path (void)
{
  GApplication *app;

  app = g_application_new ("x.y.z", 0);
  g_assert_cmpstr (g_application_get_resource_base_path (app), ==, "/x/y/z");

  /* this should not change anything */
  g_application_set_application_id (app, "a.b.c");
  g_assert_cmpstr (g_application_get_resource_base_path (app), ==, "/x/y/z");

  /* but this should... */
  g_application_set_resource_base_path (app, "/x");
  g_assert_cmpstr (g_application_get_resource_base_path (app), ==, "/x");

  /* ... and this */
  g_application_set_resource_base_path (app, NULL);
  g_assert_cmpstr (g_application_get_resource_base_path (app), ==, NULL);

  g_object_unref (app);

  /* Make sure that overriding at construction time works properly */
  app = g_object_new (G_TYPE_APPLICATION, "application-id", "x.y.z", "resource-base-path", "/a", NULL);
  g_assert_cmpstr (g_application_get_resource_base_path (app), ==, "/a");
  g_object_unref (app);

  /* ... particularly if we override to NULL */
  app = g_object_new (G_TYPE_APPLICATION, "application-id", "x.y.z", "resource-base-path", NULL, NULL);
  g_assert_cmpstr (g_application_get_resource_base_path (app), ==, NULL);
  g_object_unref (app);
}

static gint
test_help_command_line (GApplication            *app,
                        GApplicationCommandLine *command_line,
                        gpointer                 user_data)
{
  gboolean *called = user_data;

  *called = TRUE;

  return 0;
}

/* Test whether --help is handled when HANDLES_COMMND_LINE is set and
 * options have been added.
 */
static void
test_help (void)
{
  if (g_test_subprocess ())
    {
      char *binpath = g_test_build_filename (G_TEST_BUILT, "unimportant", NULL);
      gchar *argv[] = { binpath, "--help", NULL };
      GApplication *app;
      gboolean called = FALSE;
      int status;

      app = g_application_new ("org.gtk.TestApplication", G_APPLICATION_HANDLES_COMMAND_LINE);
      g_application_add_main_option (app, "foo", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, "", "");
      g_signal_connect (app, "command-line", G_CALLBACK (test_help_command_line), &called);

      status = g_application_run (app, G_N_ELEMENTS (argv) -1, argv);
      g_assert (called == TRUE);
      g_assert_cmpint (status, ==, 0);

      g_object_unref (app);
      g_free (binpath);
      return;
    }

  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*Application options*");
}

static void
test_busy (void)
{
  GApplication *app;

  /* use GSimpleAction to bind to the busy state, because it's easy to
   * create and has an easily modifiable boolean property */
  GSimpleAction *action1;
  GSimpleAction *action2;

  session_bus_up ();

  app = g_application_new ("org.gtk.TestApplication", G_APPLICATION_NON_UNIQUE);
  g_assert (g_application_register (app, NULL, NULL));

  g_assert (!g_application_get_is_busy (app));
  g_application_mark_busy (app);
  g_assert (g_application_get_is_busy (app));
  g_application_unmark_busy (app);
  g_assert (!g_application_get_is_busy (app));

  action1 = g_simple_action_new ("action", NULL);
  g_application_bind_busy_property (app, action1, "enabled");
  g_assert (g_application_get_is_busy (app));

  g_simple_action_set_enabled (action1, FALSE);
  g_assert (!g_application_get_is_busy (app));

  g_application_mark_busy (app);
  g_assert (g_application_get_is_busy (app));

  action2 = g_simple_action_new ("action", NULL);
  g_application_bind_busy_property (app, action2, "enabled");
  g_assert (g_application_get_is_busy (app));

  g_application_unmark_busy (app);
  g_assert (g_application_get_is_busy (app));

  g_object_unref (action2);
  g_assert (!g_application_get_is_busy (app));

  g_simple_action_set_enabled (action1, TRUE);
  g_assert (g_application_get_is_busy (app));

  g_application_mark_busy (app);
  g_assert (g_application_get_is_busy (app));

  g_application_unbind_busy_property (app, action1, "enabled");
  g_assert (g_application_get_is_busy (app));

  g_application_unmark_busy (app);
  g_assert (!g_application_get_is_busy (app));

  g_object_unref (action1);
  g_object_unref (app);

  session_bus_down ();
}

/*
 * Test that handle-local-options works as expected
 */

static gint
test_local_options (GApplication *app,
                    GVariantDict *options,
                    gpointer      data)
{
  gboolean *called = data;

  *called = TRUE;

  if (g_variant_dict_contains (options, "success"))
    return 0;
  else if (g_variant_dict_contains (options, "failure"))
    return 1;
  else
    return -1;
}

static gint
second_handler (GApplication *app,
                GVariantDict *options,
                gpointer      data)
{
  gboolean *called = data;

  *called = TRUE;

  return 2;
}

static void
test_handle_local_options_success (void)
{
  if (g_test_subprocess ())
    {
      char *binpath = g_test_build_filename (G_TEST_BUILT, "unimportant", NULL);
      gchar *argv[] = { binpath, "--success", NULL };
      GApplication *app;
      gboolean called = FALSE;
      gboolean called2 = FALSE;
      int status;

      app = g_application_new ("org.gtk.TestApplication", 0);
      g_application_add_main_option (app, "success", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, "", "");
      g_application_add_main_option (app, "failure", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, "", "");
      g_signal_connect (app, "handle-local-options", G_CALLBACK (test_local_options), &called);
      g_signal_connect (app, "handle-local-options", G_CALLBACK (second_handler), &called2);

      status = g_application_run (app, G_N_ELEMENTS (argv) -1, argv);
      g_assert (called);
      g_assert (!called2);
      g_assert_cmpint (status, ==, 0);

      g_object_unref (app);
      g_free (binpath);
      return;
    }

  g_test_trap_subprocess (NULL, 0, G_TEST_SUBPROCESS_INHERIT_STDOUT | G_TEST_SUBPROCESS_INHERIT_STDERR);
  g_test_trap_assert_passed ();
}

static void
test_handle_local_options_failure (void)
{
  if (g_test_subprocess ())
    {
      char *binpath = g_test_build_filename (G_TEST_BUILT, "unimportant", NULL);
      gchar *argv[] = { binpath, "--failure", NULL };
      GApplication *app;
      gboolean called = FALSE;
      gboolean called2 = FALSE;
      int status;

      app = g_application_new ("org.gtk.TestApplication", 0);
      g_application_add_main_option (app, "success", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, "", "");
      g_application_add_main_option (app, "failure", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, "", "");
      g_signal_connect (app, "handle-local-options", G_CALLBACK (test_local_options), &called);
      g_signal_connect (app, "handle-local-options", G_CALLBACK (second_handler), &called2);

      status = g_application_run (app, G_N_ELEMENTS (argv) -1, argv);
      g_assert (called);
      g_assert (!called2);
      g_assert_cmpint (status, ==, 1);

      g_object_unref (app);
      g_free (binpath);
      return;
    }

  g_test_trap_subprocess (NULL, 0, G_TEST_SUBPROCESS_INHERIT_STDOUT | G_TEST_SUBPROCESS_INHERIT_STDERR);
  g_test_trap_assert_passed ();
}

static void
test_handle_local_options_passthrough (void)
{
  if (g_test_subprocess ())
    {
      char *binpath = g_test_build_filename (G_TEST_BUILT, "unimportant", NULL);
      gchar *argv[] = { binpath, NULL };
      GApplication *app;
      gboolean called = FALSE;
      gboolean called2 = FALSE;
      int status;

      app = g_application_new ("org.gtk.TestApplication", 0);
      g_application_add_main_option (app, "success", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, "", "");
      g_application_add_main_option (app, "failure", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, "", "");
      g_signal_connect (app, "handle-local-options", G_CALLBACK (test_local_options), &called);
      g_signal_connect (app, "handle-local-options", G_CALLBACK (second_handler), &called2);

      status = g_application_run (app, G_N_ELEMENTS (argv) -1, argv);
      g_assert (called);
      g_assert (called2);
      g_assert_cmpint (status, ==, 2);

      g_object_unref (app);
      g_free (binpath);
      return;
    }

  g_test_trap_subprocess (NULL, 0, G_TEST_SUBPROCESS_INHERIT_STDOUT | G_TEST_SUBPROCESS_INHERIT_STDERR);
  g_test_trap_assert_passed ();
}

static void
test_api (void)
{
  GApplication *app;
  GSimpleAction *action;

  app = g_application_new ("org.gtk.TestApplication", 0);

  /* add an action without a name */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*failed*");
  action = g_simple_action_new (NULL, NULL);
  g_assert (action == NULL);
  g_test_assert_expected_messages ();

  /* also, gapplication shouldn't accept actions without names */
  action = g_object_new (G_TYPE_SIMPLE_ACTION, NULL);
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*action has no name*");
  g_action_map_add_action (G_ACTION_MAP (app), G_ACTION (action));
  g_test_assert_expected_messages ();

  g_object_unref (action);
  g_object_unref (app);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_dbus_unset ();

  g_test_add_func ("/gapplication/no-dbus", test_nodbus);
/*  g_test_add_func ("/gapplication/basic", basic); */
  g_test_add_func ("/gapplication/no-appid", test_noappid);
/*  g_test_add_func ("/gapplication/non-unique", test_nonunique); */
  g_test_add_func ("/gapplication/properties", properties);
  g_test_add_func ("/gapplication/app-id", appid);
  g_test_add_func ("/gapplication/quit", test_quit);
  g_test_add_func ("/gapplication/local-actions", test_local_actions);
/*  g_test_add_func ("/gapplication/remote-actions", test_remote_actions); */
  g_test_add_func ("/gapplication/local-command-line", test_local_command_line);
/*  g_test_add_func ("/gapplication/remote-command-line", test_remote_command_line); */
  g_test_add_func ("/gapplication/resource-path", test_resource_path);
  g_test_add_func ("/gapplication/test-help", test_help);
  g_test_add_func ("/gapplication/test-busy", test_busy);
  g_test_add_func ("/gapplication/test-handle-local-options1", test_handle_local_options_success);
  g_test_add_func ("/gapplication/test-handle-local-options2", test_handle_local_options_failure);
  g_test_add_func ("/gapplication/test-handle-local-options3", test_handle_local_options_passthrough);
  g_test_add_func ("/gapplication/api", test_api);

  return g_test_run ();
}
