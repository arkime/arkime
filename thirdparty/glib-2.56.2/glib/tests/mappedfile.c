#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <glib.h>
#include <string.h>
#include <glib/gstdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif

static void
test_basic (void)
{
  GMappedFile *file;
  GError *error;

  error = NULL;
  file = g_mapped_file_new (g_test_get_filename (G_TEST_DIST, "empty", NULL), FALSE, &error);
  g_assert_no_error (error);

  g_mapped_file_ref (file);
  g_mapped_file_unref (file);

  g_mapped_file_unref (file);
}

static void
test_empty (void)
{
  GMappedFile *file;
  GError *error;

  error = NULL;
  file = g_mapped_file_new (g_test_get_filename (G_TEST_DIST, "empty", NULL), FALSE, &error);
  g_assert_no_error (error);

  g_assert (g_mapped_file_get_contents (file) == NULL);

  g_mapped_file_free (file);
}

#ifdef G_OS_UNIX
static void
test_device (void)
{
  GError *error = NULL;
  GMappedFile *file;

  file = g_mapped_file_new ("/dev/null", FALSE, &error);
  g_assert (g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_INVAL) ||
            g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NODEV) ||
            g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM));
  g_assert (file == NULL);
  g_error_free (error);
}
#endif

static void
test_nonexisting (void)
{
  GMappedFile *file;
  GError *error;

  error = NULL;
  file = g_mapped_file_new ("no-such-file", FALSE, &error);
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT);
  g_clear_error (&error);
  g_assert (file == NULL);
}

static void
test_writable (void)
{
  GMappedFile *file;
  GError *error = NULL;
  gchar *contents;
  gsize len;
  const gchar *old = "MMMMMMMMMMMMMMMMMMMMMMMMM";
  const gchar *new = "abcdefghijklmnopqrstuvxyz";
  gchar *tmp_copy_path;

  tmp_copy_path = g_build_filename (g_get_tmp_dir (), "glib-test-4096-random-bytes", NULL);

  g_file_get_contents (g_test_get_filename (G_TEST_DIST, "4096-random-bytes", NULL), &contents, &len, &error);
  g_assert_no_error (error);
  g_file_set_contents (tmp_copy_path, contents, len, &error);
  g_assert_no_error (error);
  
  g_free (contents);

  file = g_mapped_file_new (tmp_copy_path, TRUE, &error);
  g_assert_no_error (error);

  contents = g_mapped_file_get_contents (file);
  g_assert (strncmp (contents, old, strlen (old)) == 0);

  memcpy (contents, new, strlen (new));
  g_assert (strncmp (contents, new, strlen (new)) == 0);

  g_mapped_file_free (file);

  error = NULL;
  file = g_mapped_file_new (tmp_copy_path, FALSE, &error);
  g_assert_no_error (error);

  contents = g_mapped_file_get_contents (file);
  g_assert (strncmp (contents, old, strlen (old)) == 0);

  g_mapped_file_free (file);

  g_free (tmp_copy_path);
}

static void
test_writable_fd (void)
{
  GMappedFile *file;
  GError *error = NULL;
  gchar *contents;
  const gchar *old = "MMMMMMMMMMMMMMMMMMMMMMMMM";
  const gchar *new = "abcdefghijklmnopqrstuvxyz";
  gsize len;
  int fd;
  gchar *tmp_copy_path;

  tmp_copy_path = g_build_filename (g_get_tmp_dir (), "glib-test-4096-random-bytes", NULL);

  g_file_get_contents (g_test_get_filename (G_TEST_DIST, "4096-random-bytes", NULL), &contents, &len, &error);
  g_assert_no_error (error);
  g_file_set_contents (tmp_copy_path, contents, len, &error);
  g_assert_no_error (error);
  
  g_free (contents);

  fd = g_open (tmp_copy_path, O_RDWR, 0);
  g_assert (fd != -1);
  file = g_mapped_file_new_from_fd (fd, TRUE, &error);
  g_assert_no_error (error);

  contents = g_mapped_file_get_contents (file);
  g_assert (strncmp (contents, old, strlen (old)) == 0);

  memcpy (contents, new, strlen (new));
  g_assert (strncmp (contents, new, strlen (new)) == 0);

  g_mapped_file_free (file);
  close (fd);

  error = NULL;
  fd = g_open (tmp_copy_path, O_RDWR, 0);
  g_assert (fd != -1);
  file = g_mapped_file_new_from_fd (fd, TRUE, &error);
  g_assert_no_error (error);

  contents = g_mapped_file_get_contents (file);
  g_assert (strncmp (contents, old, strlen (old)) == 0);

  g_mapped_file_free (file);

  g_free (tmp_copy_path);
}

static void
test_gbytes (void)
{
  GMappedFile *file;
  GBytes *bytes;
  GError *error;

  error = NULL;
  file = g_mapped_file_new (g_test_get_filename (G_TEST_DIST, "empty", NULL), FALSE, &error);
  g_assert_no_error (error);

  bytes = g_mapped_file_get_bytes (file);
  g_mapped_file_unref (file);

  g_assert_cmpint (g_bytes_get_size (bytes), ==, 0);
  g_bytes_unref (bytes);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/mappedfile/basic", test_basic);
  g_test_add_func ("/mappedfile/empty", test_empty);
#ifdef G_OS_UNIX
  g_test_add_func ("/mappedfile/device", test_device);
#endif
  g_test_add_func ("/mappedfile/nonexisting", test_nonexisting);
  g_test_add_func ("/mappedfile/writable", test_writable);
  g_test_add_func ("/mappedfile/writable_fd", test_writable_fd);
  g_test_add_func ("/mappedfile/gbytes", test_gbytes);

  return g_test_run ();
}
