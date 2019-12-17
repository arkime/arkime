#include <glib.h>

static void
test_dir_read (void)
{
  GDir *dir;
  GError *error;
  gchar *first;
  const gchar *name;

  error = NULL;
  dir = g_dir_open (".", 0, &error);
  g_assert_no_error (error);

  first = NULL;
  while ((name = g_dir_read_name (dir)) != NULL)
    {
      if (first == NULL)
        first = g_strdup (name);
      g_assert_cmpstr (name, !=, ".");
      g_assert_cmpstr (name, !=, "..");
    }

  g_dir_rewind (dir);
  g_assert_cmpstr (g_dir_read_name (dir), ==, first);

  g_free (first);
  g_dir_close (dir);
}

static void
test_dir_nonexisting (void)
{
  GDir *dir;
  GError *error;

  error = NULL;
  dir = g_dir_open ("/pfrkstrf", 0, &error);
  g_assert (dir == NULL);
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT);
  g_error_free (error);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/dir/read", test_dir_read);
  g_test_add_func ("/dir/nonexisting", test_dir_nonexisting);

  return g_test_run ();
}
