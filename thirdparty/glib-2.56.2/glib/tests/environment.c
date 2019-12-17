#include <glib.h>

static void
test_listenv (void)
{
  GHashTable *table;
  gchar **list;
  gint i;

  table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                 g_free, g_free);

  list = g_get_environ ();
  for (i = 0; list[i]; i++)
    {
      gchar **parts;

      parts = g_strsplit (list[i], "=", 2);
      g_assert (g_hash_table_lookup (table, parts[0]) == NULL);
      if (g_strcmp0 (parts[0], ""))
        g_hash_table_insert (table, parts[0], parts[1]);
      g_free (parts);
    }
  g_strfreev (list);

  g_assert_cmpint (g_hash_table_size (table), >, 0);

  list = g_listenv ();
  for (i = 0; list[i]; i++)
    {
      const gchar *expected;
      const gchar *value;

      expected = g_hash_table_lookup (table, list[i]);
      value = g_getenv (list[i]);
      g_assert_cmpstr (value, ==, expected);
      g_hash_table_remove (table, list[i]);
    }
  g_assert_cmpint (g_hash_table_size (table), ==, 0);
  g_hash_table_unref (table);
  g_strfreev (list);
}

static void
test_setenv (void)
{
  const gchar *var, *value;

  var = "NOSUCHENVVAR";
  value = "value1";

  g_assert (g_getenv (var) == NULL);
  g_setenv (var, value, FALSE);
  g_assert_cmpstr (g_getenv (var), ==, value);
  g_assert (g_setenv (var, "value2", FALSE));
  g_assert_cmpstr (g_getenv (var), ==, value);
  g_assert (g_setenv (var, "value2", TRUE));
  g_assert_cmpstr (g_getenv (var), ==, "value2");
  g_unsetenv (var);
  g_assert (g_getenv (var) == NULL);
}

static void
test_environ_array (void)
{
  gchar **env;
  const gchar *value;

  env = g_new (gchar *, 1);
  env[0] = NULL;

  value = g_environ_getenv (env, "foo");
  g_assert (value == NULL);

  env = g_environ_setenv (env, "foo", "bar", TRUE);
  value = g_environ_getenv (env, "foo");
  g_assert_cmpstr (value, ==, "bar");

  env = g_environ_setenv (env, "foo2", "bar2", FALSE);
  value = g_environ_getenv (env, "foo");
  g_assert_cmpstr (value, ==, "bar");
  value = g_environ_getenv (env, "foo2");
  g_assert_cmpstr (value, ==, "bar2");

  env = g_environ_setenv (env, "foo", "x", FALSE);
  value = g_environ_getenv (env, "foo");
  g_assert_cmpstr (value, ==, "bar");

  env = g_environ_setenv (env, "foo", "x", TRUE);
  value = g_environ_getenv (env, "foo");
  g_assert_cmpstr (value, ==, "x");

  env = g_environ_unsetenv (env, "foo2");
  value = g_environ_getenv (env, "foo2");
  g_assert (value == NULL);

  g_strfreev (env);
}

static void
test_environ_null (void)
{
  gchar **env;
  const gchar *value;

  env = NULL;

  value = g_environ_getenv (env, "foo");
  g_assert (value == NULL);

  env = g_environ_setenv (NULL, "foo", "bar", TRUE);
  g_assert (env != NULL);
  g_strfreev (env);

  env = g_environ_unsetenv (NULL, "foo");
  g_assert (env == NULL);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/environ/listenv", test_listenv);
  g_test_add_func ("/environ/setenv", test_setenv);
  g_test_add_func ("/environ/array", test_environ_array);
  g_test_add_func ("/environ/null", test_environ_null);

  return g_test_run ();
}
