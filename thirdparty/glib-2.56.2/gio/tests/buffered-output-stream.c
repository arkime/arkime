#include <gio/gio.h>

static void
test_write (void)
{
  GOutputStream *base;
  GOutputStream *out;
  GError *error;
  const gchar buffer[] = "abcdefghijklmnopqrstuvwxyz";

  base = g_memory_output_stream_new (g_malloc0 (20), 20, NULL, g_free);
  out = g_buffered_output_stream_new (base);

  g_assert_cmpint (g_buffered_output_stream_get_buffer_size (G_BUFFERED_OUTPUT_STREAM (out)), ==, 4096);
  g_assert (!g_buffered_output_stream_get_auto_grow (G_BUFFERED_OUTPUT_STREAM (out)));
  g_object_set (out, "auto-grow", TRUE, NULL);
  g_assert (g_buffered_output_stream_get_auto_grow (G_BUFFERED_OUTPUT_STREAM (out)));
  g_object_set (out, "auto-grow", FALSE, NULL);

  g_buffered_output_stream_set_buffer_size (G_BUFFERED_OUTPUT_STREAM (out), 16);
  g_assert_cmpint (g_buffered_output_stream_get_buffer_size (G_BUFFERED_OUTPUT_STREAM (out)), ==, 16);

  error = NULL;
  g_assert_cmpint (g_output_stream_write (out, buffer, 10, NULL, &error), ==, 10);
  g_assert_no_error (error);

  g_assert_cmpint (g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (base)), ==, 0);

  g_assert_cmpint (g_output_stream_write (out, buffer + 10, 10, NULL, &error), ==, 6);
  g_assert_no_error (error);

  g_assert_cmpint (g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (base)), ==, 0);
  g_assert (g_output_stream_flush (out, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpint (g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (base)), ==, 16);

  g_assert_cmpstr (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (base)), ==, "abcdefghijklmnop");

  g_object_unref (out);
  g_object_unref (base);
}

static void
test_grow (void)
{
  GOutputStream *base;
  GOutputStream *out;
  GError *error;
  const gchar buffer[] = "abcdefghijklmnopqrstuvwxyz";
  gint size;
  gboolean grow;

  base = g_memory_output_stream_new (g_malloc0 (30), 30, g_realloc, g_free);
  out = g_buffered_output_stream_new_sized (base, 16);

  g_buffered_output_stream_set_auto_grow (G_BUFFERED_OUTPUT_STREAM (out), TRUE);

  g_object_get (out, "buffer-size", &size, "auto-grow", &grow, NULL);
  g_assert_cmpint (size, ==, 16);
  g_assert (grow);

  g_assert (g_seekable_can_seek (G_SEEKABLE (out)));

  error = NULL;
  g_assert_cmpint (g_output_stream_write (out, buffer, 10, NULL, &error), ==, 10);
  g_assert_no_error (error);

  g_assert_cmpint (g_buffered_output_stream_get_buffer_size (G_BUFFERED_OUTPUT_STREAM (out)), ==, 16);
  g_assert_cmpint (g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (base)), ==, 0);

  g_assert_cmpint (g_output_stream_write (out, buffer + 10, 10, NULL, &error), ==, 10);
  g_assert_no_error (error);

  g_assert_cmpint (g_buffered_output_stream_get_buffer_size (G_BUFFERED_OUTPUT_STREAM (out)), >=, 20);
  g_assert_cmpint (g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (base)), ==, 0);

  g_assert (g_output_stream_flush (out, NULL, &error));
  g_assert_no_error (error);

  g_assert_cmpstr (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (base)), ==, "abcdefghijklmnopqrst");

  g_object_unref (out);
  g_object_unref (base);
}

static void
test_close (void)
{
  GOutputStream *base;
  GOutputStream *out;
  GError *error;

  base = g_memory_output_stream_new (g_malloc0 (30), 30, g_realloc, g_free);
  out = g_buffered_output_stream_new (base);

  g_assert (g_filter_output_stream_get_close_base_stream (G_FILTER_OUTPUT_STREAM (out)));

  error = NULL;
  g_assert (g_output_stream_close (out, NULL, &error));
  g_assert_no_error (error);
  g_assert (g_output_stream_is_closed (base));

  g_object_unref (out);
  g_object_unref (base);

  base = g_memory_output_stream_new (g_malloc0 (30), 30, g_realloc, g_free);
  out = g_buffered_output_stream_new (base);

  g_filter_output_stream_set_close_base_stream (G_FILTER_OUTPUT_STREAM (out), FALSE);

  error = NULL;
  g_assert (g_output_stream_close (out, NULL, &error));
  g_assert_no_error (error);
  g_assert (!g_output_stream_is_closed (base));

  g_object_unref (out);
  g_object_unref (base);
}

static void
test_seek (void)
{
  GMemoryOutputStream *base;
  GOutputStream *out;
  GSeekable *seekable;
  GError *error;
  gsize bytes_written;
  gboolean ret;
  const gchar buffer[] = "abcdefghijklmnopqrstuvwxyz";

  base = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new (g_malloc0 (30), 30, NULL, g_free));
  out = g_buffered_output_stream_new_sized (G_OUTPUT_STREAM (base), 8);
  seekable = G_SEEKABLE (out);
  error = NULL;

  /* Write data */
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (out)), ==, 0);
  ret = g_output_stream_write_all (out, buffer, 4, &bytes_written, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (bytes_written, ==, 4);
  g_assert (ret);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (out)), ==, 4);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base), ==, 0);

  /* Forward relative seek */
  ret = g_seekable_seek (seekable, 2, G_SEEK_CUR, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (out)), ==, 6);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[0]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[1]);
  g_assert_cmpint ('c', ==, ((gchar *)g_memory_output_stream_get_data (base))[2]);
  g_assert_cmpint ('d', ==, ((gchar *)g_memory_output_stream_get_data (base))[3]);
  ret = g_output_stream_write_all (out, buffer, 2, &bytes_written, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (bytes_written, ==, 2);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (out)), ==, 8);

  /* Backward relative seek */
  ret = g_seekable_seek (seekable, -4, G_SEEK_CUR, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (out)), ==, 4);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[0]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[1]);
  g_assert_cmpint ('c', ==, ((gchar *)g_memory_output_stream_get_data (base))[2]);
  g_assert_cmpint ('d', ==, ((gchar *)g_memory_output_stream_get_data (base))[3]);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[6]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[7]);
  ret = g_output_stream_write_all (out, buffer, 2, &bytes_written, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (bytes_written, ==, 2);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (out)), ==, 6);

  /* From start */
  ret = g_seekable_seek (seekable, 2, G_SEEK_SET, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (out)), ==, 2);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[0]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[1]);
  g_assert_cmpint ('c', ==, ((gchar *)g_memory_output_stream_get_data (base))[2]);
  g_assert_cmpint ('d', ==, ((gchar *)g_memory_output_stream_get_data (base))[3]);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[4]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[5]);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[6]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[7]);
  ret = g_output_stream_write_all (out, buffer, 2, &bytes_written, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (bytes_written, ==, 2);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (out)), ==, 4);

  /* From end */
  ret = g_seekable_seek (seekable, 6 - 30, G_SEEK_END, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (out)), ==, 6);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[0]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[1]);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[2]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[3]);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[4]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[5]);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[6]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[7]);
  ret = g_output_stream_write_all (out, buffer + 2, 2, &bytes_written, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (bytes_written, ==, 2);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (out)), ==, 8);

  /* Check flush */
  ret = g_output_stream_flush (out, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (out)), ==, 8);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[0]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[1]);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[2]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[3]);
  g_assert_cmpint ('a', ==, ((gchar *)g_memory_output_stream_get_data (base))[4]);
  g_assert_cmpint ('b', ==, ((gchar *)g_memory_output_stream_get_data (base))[5]);
  g_assert_cmpint ('c', ==, ((gchar *)g_memory_output_stream_get_data (base))[6]);
  g_assert_cmpint ('d', ==, ((gchar *)g_memory_output_stream_get_data (base))[7]);

  g_object_unref (out);
  g_object_unref (base);
}

static void
test_truncate(void)
{
  GMemoryOutputStream *base_stream;
  GOutputStream *stream;
  GSeekable *seekable;
  GError *error;
  gsize bytes_written;
  guchar *stream_data;
  gsize len;
  gboolean res;

  len = 8;

  /* Create objects */
  stream_data = g_malloc0 (len);
  base_stream = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new (stream_data, len, g_realloc, g_free));
  stream = g_buffered_output_stream_new_sized (G_OUTPUT_STREAM (base_stream), 8);
  seekable = G_SEEKABLE (stream);

  g_assert (g_seekable_can_truncate (seekable));

  /* Write */
  g_assert_cmpint (g_memory_output_stream_get_size (base_stream), ==, len);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 0);

  error = NULL;
  res = g_output_stream_write_all (stream, "ab", 2, &bytes_written, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  res = g_output_stream_write_all (stream, "cd", 2, &bytes_written, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);

  res = g_output_stream_flush (stream, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);

  g_assert_cmpint (g_memory_output_stream_get_size (base_stream), ==, len);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 4);
  stream_data = g_memory_output_stream_get_data (base_stream);
  g_assert_cmpint (stream_data[0], ==, 'a');
  g_assert_cmpint (stream_data[1], ==, 'b');
  g_assert_cmpint (stream_data[2], ==, 'c');
  g_assert_cmpint (stream_data[3], ==, 'd');

  /* Truncate at position */
  res = g_seekable_truncate (seekable, 4, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_memory_output_stream_get_size (base_stream), ==, 4);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 4);
  stream_data = g_memory_output_stream_get_data (base_stream);
  g_assert_cmpint (stream_data[0], ==, 'a');
  g_assert_cmpint (stream_data[1], ==, 'b');
  g_assert_cmpint (stream_data[2], ==, 'c');
  g_assert_cmpint (stream_data[3], ==, 'd');

  /* Truncate beyond position */
  res = g_seekable_truncate (seekable, 6, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_memory_output_stream_get_size (base_stream), ==, 6);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 6);
  stream_data = g_memory_output_stream_get_data (base_stream);
  g_assert_cmpint (stream_data[0], ==, 'a');
  g_assert_cmpint (stream_data[1], ==, 'b');
  g_assert_cmpint (stream_data[2], ==, 'c');
  g_assert_cmpint (stream_data[3], ==, 'd');

  /* Truncate before position */
  res = g_seekable_truncate (seekable, 2, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_memory_output_stream_get_size (base_stream), ==, 2);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 2);
  stream_data = g_memory_output_stream_get_data (base_stream);
  g_assert_cmpint (stream_data[0], ==, 'a');
  g_assert_cmpint (stream_data[1], ==, 'b');

  g_object_unref (stream);
  g_object_unref (base_stream);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/buffered-output-stream/write", test_write);
  g_test_add_func ("/buffered-output-stream/grow", test_grow);
  g_test_add_func ("/buffered-output-stream/seek", test_seek);
  g_test_add_func ("/buffered-output-stream/truncate", test_truncate);
  g_test_add_func ("/filter-output-stream/close", test_close);

  return g_test_run ();
}
