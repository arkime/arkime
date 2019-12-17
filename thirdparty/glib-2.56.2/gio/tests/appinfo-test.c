#include <stdlib.h>
#include <gio/gio.h>

int
main (int argc, char *argv[])
{
  const gchar *envvar;

  g_test_init (&argc, &argv, NULL);

  envvar = g_getenv ("GIO_LAUNCHED_DESKTOP_FILE");
  if (envvar != NULL)
    {
      gchar *expected;
      gint pid_from_env;

      expected = g_test_build_filename (G_TEST_DIST, "appinfo-test.desktop", NULL);
      g_assert_cmpstr (envvar, ==, expected);
      g_free (expected);

      envvar = g_getenv ("GIO_LAUNCHED_DESKTOP_FILE_PID");
      g_assert (envvar != NULL);
      pid_from_env = atoi (envvar);
      g_assert_cmpint (pid_from_env, ==, getpid ());
    }

  return 0;
}

