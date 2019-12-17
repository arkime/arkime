#include <glib/glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <string.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#endif

static const char *original_data = "This is some test data that we can put in a file...";
static const char *new_data = "new data..";

static void
verify_pos (GIOStream *iostream, goffset expected_pos)
{
  goffset pos;

  pos = g_seekable_tell (G_SEEKABLE (iostream));
  g_assert_cmpint (pos, ==, expected_pos);

  pos = g_seekable_tell (G_SEEKABLE (g_io_stream_get_input_stream (iostream)));
  g_assert_cmpint (pos, ==, expected_pos);

  pos = g_seekable_tell (G_SEEKABLE (g_io_stream_get_output_stream (iostream)));
  g_assert_cmpint (pos, ==, expected_pos);
}

static void
verify_iostream (GFileIOStream *file_iostream)
{
  gboolean res;
  GIOStream *iostream;
  GError *error;
  GInputStream *in;
  GOutputStream *out;
  char buffer[1024];
  gsize n_bytes;
  char *modified_data;

  iostream = G_IO_STREAM (file_iostream);

  verify_pos (iostream, 0);

  in = g_io_stream_get_input_stream (iostream);
  out = g_io_stream_get_output_stream (iostream);

  res = g_input_stream_read_all (in, buffer, 20, &n_bytes, NULL, NULL);
  g_assert (res);
  g_assert_cmpmem (buffer, n_bytes, original_data, 20);

  verify_pos (iostream, 20);

  res = g_seekable_seek (G_SEEKABLE (iostream),
			 -10, G_SEEK_END,
			 NULL, NULL);
  g_assert (res);
  verify_pos (iostream, strlen (original_data) - 10);

  res = g_input_stream_read_all (in, buffer, 20, &n_bytes, NULL, NULL);
  g_assert (res);
  g_assert_cmpmem (buffer, n_bytes, original_data + strlen (original_data) - 10, 10);

  verify_pos (iostream, strlen (original_data));

  res = g_seekable_seek (G_SEEKABLE (iostream),
			 10, G_SEEK_SET,
			 NULL, NULL);

  res = g_input_stream_skip (in, 5, NULL, NULL);
  g_assert (res == 5);
  verify_pos (iostream, 15);

  res = g_input_stream_skip (in, 10000, NULL, NULL);
  g_assert (res == strlen (original_data) - 15);
  verify_pos (iostream, strlen (original_data));

  res = g_seekable_seek (G_SEEKABLE (iostream),
			 10, G_SEEK_SET,
			 NULL, NULL);

  verify_pos (iostream, 10);

  res = g_output_stream_write_all (out, new_data, strlen (new_data),
				   &n_bytes, NULL, NULL);
  g_assert (res);
  g_assert_cmpint (n_bytes, ==, strlen (new_data));

  verify_pos (iostream, 10 + strlen (new_data));

  res = g_seekable_seek (G_SEEKABLE (iostream),
			 0, G_SEEK_SET,
			 NULL, NULL);
  g_assert (res);
  verify_pos (iostream, 0);

  res = g_input_stream_read_all (in, buffer, strlen (original_data), &n_bytes, NULL, NULL);
  g_assert (res);
  g_assert_cmpint ((int)n_bytes, ==, strlen (original_data));
  buffer[n_bytes] = 0;

  modified_data = g_strdup (original_data);
  memcpy (modified_data + 10, new_data, strlen (new_data));
  g_assert_cmpstr (buffer, ==, modified_data);

  verify_pos (iostream, strlen (original_data));

  res = g_seekable_seek (G_SEEKABLE (iostream),
			 0, G_SEEK_SET,
			 NULL, NULL);
  g_assert (res);
  verify_pos (iostream, 0);

  res = g_output_stream_close (out, NULL, NULL);
  g_assert (res);

  res = g_input_stream_read_all (in, buffer, 15, &n_bytes, NULL, NULL);
  g_assert (res);
  g_assert_cmpmem (buffer, n_bytes, modified_data, 15);

  error = NULL;
  res = g_output_stream_write_all (out, new_data, strlen (new_data),
				   &n_bytes, NULL, &error);
  g_assert (!res);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CLOSED);
  g_error_free (error);

  error = NULL;
  res = g_io_stream_close (iostream, NULL, &error);
  g_assert (res);
  g_assert_no_error (error);

  g_free (modified_data);
}

static void
test_g_file_open_readwrite (void)
{
  char *tmp_file;
  int fd;
  gboolean res;
  GFileIOStream *file_iostream;
  char *path;
  GFile *file;
  GError *error;

  fd = g_file_open_tmp ("readwrite_XXXXXX",
			&tmp_file, NULL);
  g_assert (fd != -1);
  close (fd);

  res = g_file_set_contents (tmp_file,
			     original_data, -1, NULL);
  g_assert (res);

  path = g_build_filename (g_get_tmp_dir (), "g-a-nonexisting-file", NULL);
  file = g_file_new_for_path (path);
  g_free (path);
  error = NULL;
  file_iostream = g_file_open_readwrite (file, NULL, &error);
  g_assert (file_iostream == NULL);
  g_assert (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND));
  g_error_free (error);
  g_object_unref (file);

  file = g_file_new_for_path (tmp_file);
  error = NULL;
  file_iostream = g_file_open_readwrite (file, NULL, &error);
  g_assert (file_iostream != NULL);
  g_object_unref (file);

  verify_iostream (file_iostream);

  g_object_unref (file_iostream);

  g_unlink (tmp_file);
  g_free (tmp_file);
}

static void
test_g_file_create_readwrite (void)
{
  char *tmp_file;
  int fd;
  gboolean res;
  GFileIOStream *file_iostream;
  GOutputStream *out;
  GFile *file;
  GError *error;
  gsize n_bytes;

  fd = g_file_open_tmp ("readwrite_XXXXXX",
			&tmp_file, NULL);
  g_assert (fd != -1);
  close (fd);

  file = g_file_new_for_path (tmp_file);
  error = NULL;
  file_iostream = g_file_create_readwrite (file, 0, NULL, &error);
  g_assert (file_iostream == NULL);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS);
  g_error_free (error);

  g_unlink (tmp_file);
  file_iostream = g_file_create_readwrite (file, 0, NULL, &error);
  g_assert (file_iostream != NULL);

  out = g_io_stream_get_output_stream (G_IO_STREAM (file_iostream));
  res = g_output_stream_write_all (out, original_data, strlen (original_data),
				   &n_bytes, NULL, NULL);
  g_assert (res);
  g_assert_cmpint (n_bytes, ==, strlen (original_data));

  res = g_seekable_seek (G_SEEKABLE (file_iostream),
			 0, G_SEEK_SET,
			 NULL, NULL);
  g_assert (res);

  verify_iostream (file_iostream);

  g_object_unref (file_iostream);
  g_object_unref (file);

  g_unlink (tmp_file);
  g_free (tmp_file);
}

static void
test_g_file_replace_readwrite (void)
{
  char *tmp_file, *backup, *data;
  int fd;
  gboolean res;
  GFileIOStream *file_iostream;
  GInputStream *in;
  GOutputStream *out;
  GFile *file;
  GError *error;
  char buffer[1024];
  gsize n_bytes;

  fd = g_file_open_tmp ("readwrite_XXXXXX",
			&tmp_file, NULL);
  g_assert (fd != -1);
  close (fd);

  res = g_file_set_contents (tmp_file,
			     new_data, -1, NULL);
  g_assert (res);

  file = g_file_new_for_path (tmp_file);
  error = NULL;
  file_iostream = g_file_replace_readwrite (file, NULL,
					    TRUE, 0, NULL, &error);
  g_assert (file_iostream != NULL);

  in = g_io_stream_get_input_stream (G_IO_STREAM (file_iostream));

  /* Ensure its empty */
  res = g_input_stream_read_all (in, buffer, sizeof buffer, &n_bytes, NULL, NULL);
  g_assert (res);
  g_assert_cmpint ((int)n_bytes, ==, 0);

  out = g_io_stream_get_output_stream (G_IO_STREAM (file_iostream));
  res = g_output_stream_write_all (out, original_data, strlen (original_data),
				   &n_bytes, NULL, NULL);
  g_assert (res);
  g_assert_cmpint (n_bytes, ==, strlen (original_data));

  res = g_seekable_seek (G_SEEKABLE (file_iostream),
			 0, G_SEEK_SET,
			 NULL, NULL);
  g_assert (res);

  verify_iostream (file_iostream);

  g_object_unref (file_iostream);
  g_object_unref (file);

  backup = g_strconcat (tmp_file, "~", NULL);
  res = g_file_get_contents (backup,
			     &data,
			     NULL, NULL);
  g_assert (res);
  g_assert_cmpstr (data, ==, new_data);
  g_free (data);
  g_unlink (backup);
  g_free (backup);

  g_unlink (tmp_file);
  g_free (tmp_file);
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/readwrite/test_g_file_open_readwrite",
		   test_g_file_open_readwrite);
  g_test_add_func ("/readwrite/test_g_file_create_readwrite",
		   test_g_file_create_readwrite);
  g_test_add_func ("/readwrite/test_g_file_replace_readwrite",
		   test_g_file_replace_readwrite);

  return g_test_run();
}
