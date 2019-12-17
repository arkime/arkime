#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <gio/gfiledescriptorbased.h>
#ifdef G_OS_UNIX
#include <sys/stat.h>
#endif

static void
test_basic_for_file (GFile       *file,
                     const gchar *suffix)
{
  gchar *s;

  s = g_file_get_basename (file);
  g_assert_cmpstr (s, ==, "testfile");
  g_free (s);

  s = g_file_get_uri (file);
  g_assert (g_str_has_prefix (s, "file://"));
  g_assert (g_str_has_suffix (s, suffix));
  g_free (s);

  g_assert (g_file_has_uri_scheme (file, "file"));
  s = g_file_get_uri_scheme (file);
  g_assert_cmpstr (s, ==, "file");
  g_free (s);
}

static void
test_basic (void)
{
  GFile *file;

  file = g_file_new_for_path ("./some/directory/testfile");
  test_basic_for_file (file, "/some/directory/testfile");
  g_object_unref (file);
}

static void
test_build_filename (void)
{
  GFile *file;

  file = g_file_new_build_filename (".", "some", "directory", "testfile", NULL);
  test_basic_for_file (file, "/some/directory/testfile");
  g_object_unref (file);

  file = g_file_new_build_filename ("testfile", NULL);
  test_basic_for_file (file, "/testfile");
  g_object_unref (file);
}

static void
test_parent (void)
{
  GFile *file;
  GFile *file2;
  GFile *parent;
  GFile *root;

  file = g_file_new_for_path ("./some/directory/testfile");
  file2 = g_file_new_for_path ("./some/directory");
  root = g_file_new_for_path ("/");

  g_assert (g_file_has_parent (file, file2));

  parent = g_file_get_parent (file);
  g_assert (g_file_equal (parent, file2));
  g_object_unref (parent);

  g_assert (g_file_get_parent (root) == NULL);

  g_object_unref (file);
  g_object_unref (file2);
  g_object_unref (root);
}

static void
test_child (void)
{
  GFile *file;
  GFile *child;
  GFile *child2;

  file = g_file_new_for_path ("./some/directory");
  child = g_file_get_child (file, "child");
  g_assert (g_file_has_parent (child, file));

  child2 = g_file_get_child_for_display_name (file, "child2", NULL);
  g_assert (g_file_has_parent (child2, file));

  g_object_unref (child);
  g_object_unref (child2);
  g_object_unref (file);
}

static void
test_type (void)
{
  GFile *datapath_f;
  GFile *file;
  GFileType type;
  GError *error = NULL;

  datapath_f = g_file_new_for_path (g_test_get_dir (G_TEST_DIST));

  file = g_file_get_child (datapath_f, "g-icon.c");
  type = g_file_query_file_type (file, 0, NULL);
  g_assert_cmpint (type, ==, G_FILE_TYPE_REGULAR);
  g_object_unref (file);

  file = g_file_get_child (datapath_f, "cert-tests");
  type = g_file_query_file_type (file, 0, NULL);
  g_assert_cmpint (type, ==, G_FILE_TYPE_DIRECTORY);

  g_file_read (file, NULL, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_IS_DIRECTORY);
  g_error_free (error);
  g_object_unref (file);

  g_object_unref (datapath_f);
}

static void
test_parse_name (void)
{
  GFile *file;
  gchar *name;

  file = g_file_new_for_uri ("file://somewhere");
  name = g_file_get_parse_name (file);
  g_assert_cmpstr (name, ==, "file://somewhere");
  g_object_unref (file);
  g_free (name);

  file = g_file_parse_name ("~foo");
  name = g_file_get_parse_name (file);
  g_assert (name != NULL);
  g_object_unref (file);
  g_free (name);
}

typedef struct
{
  GFile *file;
  GFileMonitor *monitor;
  GOutputStream *ostream;
  GInputStream *istream;
  GMainLoop *loop;
  gint buffersize;
  gint monitor_created;
  gint monitor_deleted;
  gint monitor_changed;
  gchar *monitor_path;
  gint pos;
  gchar *data;
  gchar *buffer;
  guint timeout;
} CreateDeleteData;

static void
monitor_changed (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other_file,
                 GFileMonitorEvent  event_type,
                 gpointer           user_data)
{
  CreateDeleteData *data = user_data;
  gchar *path;
  const gchar *peeked_path;

  path = g_file_get_path (file);
  peeked_path = g_file_peek_path (file);
  g_assert_cmpstr (data->monitor_path, ==, path);
  g_assert_cmpstr (path, ==, peeked_path);
  g_free (path);

  if (event_type == G_FILE_MONITOR_EVENT_CREATED)
    data->monitor_created++;
  if (event_type == G_FILE_MONITOR_EVENT_DELETED)
    data->monitor_deleted++;
  if (event_type == G_FILE_MONITOR_EVENT_CHANGED)
    data->monitor_changed++;
}


static gboolean
quit_idle (gpointer user_data)
{
  CreateDeleteData *data = user_data;

  g_source_remove (data->timeout); 
  g_main_loop_quit (data->loop);

  return FALSE;
}

static void
iclosed_cb (GObject      *source,
            GAsyncResult *res,
            gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  GError *error;
  gboolean ret;

  error = NULL;
  ret = g_input_stream_close_finish (data->istream, res, &error);
  g_assert_no_error (error);
  g_assert (ret);

  g_assert (g_input_stream_is_closed (data->istream));

  ret = g_file_delete (data->file, NULL, &error);
  g_assert (ret);
  g_assert_no_error (error);

  /* work around file monitor bug:
   * inotify events are only processed every 1000 ms, regardless
   * of the rate limit set on the file monitor
   */
  g_timeout_add (2000, quit_idle, data);
}

static void
read_cb (GObject      *source,
         GAsyncResult *res,
         gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  GError *error;
  gssize size;

  error = NULL;
  size = g_input_stream_read_finish (data->istream, res, &error);
  g_assert_no_error (error);

  data->pos += size;
  if (data->pos < strlen (data->data))
    {
      g_input_stream_read_async (data->istream,
                                 data->buffer + data->pos,
                                 strlen (data->data) - data->pos,
                                 0,
                                 NULL,
                                 read_cb,
                                 data);
    }
  else
    {
      g_assert_cmpstr (data->buffer, ==, data->data);
      g_assert (!g_input_stream_is_closed (data->istream));
      g_input_stream_close_async (data->istream, 0, NULL, iclosed_cb, data);
    }
}

static void
ipending_cb (GObject      *source,
             GAsyncResult *res,
             gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  GError *error;

  error = NULL;
  g_input_stream_read_finish (data->istream, res, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PENDING);
  g_error_free (error);
}

static void
skipped_cb (GObject      *source,
            GAsyncResult *res,
            gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  GError *error;
  gssize size;

  error = NULL;
  size = g_input_stream_skip_finish (data->istream, res, &error);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, data->pos);

  g_input_stream_read_async (data->istream,
                             data->buffer + data->pos,
                             strlen (data->data) - data->pos,
                             0,
                             NULL,
                             read_cb,
                             data);
  /* check that we get a pending error */
  g_input_stream_read_async (data->istream,
                             data->buffer + data->pos,
                             strlen (data->data) - data->pos,
                             0,
                             NULL,
                             ipending_cb,
                             data);
}

static void
opened_cb (GObject      *source,
           GAsyncResult *res,
           gpointer      user_data)
{
  GFileInputStream *base;
  CreateDeleteData *data = user_data;
  GError *error;

  error = NULL;
  base = g_file_read_finish (data->file, res, &error);
  g_assert_no_error (error);

  if (data->buffersize == 0)
    data->istream = G_INPUT_STREAM (g_object_ref (base));
  else
    data->istream = g_buffered_input_stream_new_sized (G_INPUT_STREAM (base), data->buffersize);
  g_object_unref (base);

  data->buffer = g_new0 (gchar, strlen (data->data) + 1);

  /* copy initial segment directly, then skip */
  memcpy (data->buffer, data->data, 10);
  data->pos = 10;

  g_input_stream_skip_async (data->istream,
                             10,
                             0,
                             NULL,
                             skipped_cb,
                             data);
}

static void
oclosed_cb (GObject      *source,
            GAsyncResult *res,
            gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  GError *error;
  gboolean ret;

  error = NULL;
  ret = g_output_stream_close_finish (data->ostream, res, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert (g_output_stream_is_closed (data->ostream));

  g_file_read_async (data->file, 0, NULL, opened_cb, data);
}

static void
written_cb (GObject      *source,
            GAsyncResult *res,
            gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  gssize size;
  GError *error;

  error = NULL;
  size = g_output_stream_write_finish (data->ostream, res, &error);
  g_assert_no_error (error);

  data->pos += size;
  if (data->pos < strlen (data->data))
    {
      g_output_stream_write_async (data->ostream,
                                   data->data + data->pos,
                                   strlen (data->data) - data->pos,
                                   0,
                                   NULL,
                                   written_cb,
                                   data);
    }
  else
    {
      g_assert (!g_output_stream_is_closed (data->ostream));
      g_output_stream_close_async (data->ostream, 0, NULL, oclosed_cb, data);
    }
}

static void
opending_cb (GObject      *source,
             GAsyncResult *res,
             gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  GError *error;

  error = NULL;
  g_output_stream_write_finish (data->ostream, res, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PENDING);
  g_error_free (error);
}

static void
created_cb (GObject      *source,
            GAsyncResult *res,
            gpointer      user_data)
{
  GFileOutputStream *base;
  CreateDeleteData *data = user_data;
  GError *error;

  error = NULL;
  base = g_file_create_finish (G_FILE (source), res, &error);
  g_assert_no_error (error);
  g_assert (g_file_query_exists  (data->file, NULL));

  if (data->buffersize == 0)
    data->ostream = G_OUTPUT_STREAM (g_object_ref (base));
  else
    data->ostream = g_buffered_output_stream_new_sized (G_OUTPUT_STREAM (base), data->buffersize);
  g_object_unref (base);

  g_output_stream_write_async (data->ostream,
                               data->data,
                               strlen (data->data),
                               0,
                               NULL,
                               written_cb,
                               data);
  /* check that we get a pending error */
  g_output_stream_write_async (data->ostream,
                               data->data,
                               strlen (data->data),
                               0,
                               NULL,
                               opending_cb,
                               data);
}

static gboolean
stop_timeout (gpointer data)
{
  g_assert_not_reached ();

  return FALSE;
}

/*
 * This test does a fully async create-write-read-delete.
 * Callbackistan.
 */
static void
test_create_delete (gconstpointer d)
{
  GError *error;
  CreateDeleteData *data;
  GFileIOStream *iostream;

  data = g_new0 (CreateDeleteData, 1);

  data->buffersize = GPOINTER_TO_INT (d);
  data->data = "abcdefghijklmnopqrstuvxyzABCDEFGHIJKLMNOPQRSTUVXYZ0123456789";
  data->pos = 0;

  data->file = g_file_new_tmp ("g_file_create_delete_XXXXXX",
			       &iostream, NULL);
  g_assert (data->file != NULL);
  g_object_unref (iostream);

  data->monitor_path = g_file_get_path (data->file);
  remove (data->monitor_path);

  g_assert (!g_file_query_exists  (data->file, NULL));

  error = NULL;
  data->monitor = g_file_monitor_file (data->file, 0, NULL, &error);
  g_assert_no_error (error);

  /* This test doesn't work with GPollFileMonitor, because it assumes
   * that the monitor will notice a create immediately followed by a
   * delete, rather than coalescing them into nothing.
   */
  if (!strcmp (G_OBJECT_TYPE_NAME (data->monitor), "GPollFileMonitor"))
    {
      g_test_skip ("skipping test for this GFileMonitor implementation");
      goto skip;
    }

  g_file_monitor_set_rate_limit (data->monitor, 100);

  g_signal_connect (data->monitor, "changed", G_CALLBACK (monitor_changed), data);

  data->loop = g_main_loop_new (NULL, FALSE);

  data->timeout = g_timeout_add (10000, stop_timeout, NULL);

  g_file_create_async (data->file, 0, 0, NULL, created_cb, data);

  g_main_loop_run (data->loop);

  g_assert_cmpint (data->monitor_created, ==, 1);
  g_assert_cmpint (data->monitor_deleted, ==, 1);
  g_assert_cmpint (data->monitor_changed, >, 0);

  g_assert (!g_file_monitor_is_cancelled (data->monitor));
  g_file_monitor_cancel (data->monitor);
  g_assert (g_file_monitor_is_cancelled (data->monitor));

  g_main_loop_unref (data->loop);
  g_object_unref (data->ostream);
  g_object_unref (data->istream);

 skip:
  g_object_unref (data->monitor);
  g_object_unref (data->file);
  free (data->monitor_path);
  g_free (data->buffer);
  g_free (data);
}

static const gchar *replace_data =
    "/**\n"
    " * g_file_replace_contents_async:\n"
    " * @file: input #GFile.\n"
    " * @contents: string of contents to replace the file with.\n"
    " * @length: the length of @contents in bytes.\n"
    " * @etag: (nullable): a new <link linkend=\"gfile-etag\">entity tag</link> for the @file, or %NULL\n"
    " * @make_backup: %TRUE if a backup should be created.\n"
    " * @flags: a set of #GFileCreateFlags.\n"
    " * @cancellable: optional #GCancellable object, %NULL to ignore.\n"
    " * @callback: a #GAsyncReadyCallback to call when the request is satisfied\n"
    " * @user_data: the data to pass to callback function\n"
    " * \n"
    " * Starts an asynchronous replacement of @file with the given \n"
    " * @contents of @length bytes. @etag will replace the document's\n"
    " * current entity tag.\n"
    " * \n"
    " * When this operation has completed, @callback will be called with\n"
    " * @user_user data, and the operation can be finalized with \n"
    " * g_file_replace_contents_finish().\n"
    " * \n"
    " * If @cancellable is not %NULL, then the operation can be cancelled by\n"
    " * triggering the cancellable object from another thread. If the operation\n"
    " * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. \n"
    " * \n"
    " * If @make_backup is %TRUE, this function will attempt to \n"
    " * make a backup of @file.\n"
    " **/\n";

typedef struct
{
  GFile *file;
  const gchar *data;
  GMainLoop *loop;
  gboolean again;
} ReplaceLoadData;

static void replaced_cb (GObject      *source,
                         GAsyncResult *res,
                         gpointer      user_data);

static void
loaded_cb (GObject      *source,
           GAsyncResult *res,
           gpointer      user_data)
{
  ReplaceLoadData *data = user_data;
  gboolean ret;
  GError *error;
  gchar *contents;
  gsize length;

  error = NULL;
  ret = g_file_load_contents_finish (data->file, res, &contents, &length, NULL, &error);
  g_assert (ret);
  g_assert_no_error (error);
  g_assert_cmpint (length, ==, strlen (data->data));
  g_assert_cmpstr (contents, ==, data->data);

  g_free (contents);

  if (data->again)
    {
      data->again = FALSE;
      data->data = "pi pa po";

      g_file_replace_contents_async (data->file,
                                     data->data,
                                     strlen (data->data),
                                     NULL,
                                     FALSE,
                                     0,
                                     NULL,
                                     replaced_cb,
                                     data);
    }
  else
    {
       error = NULL;
       ret = g_file_delete (data->file, NULL, &error);
       g_assert_no_error (error);
       g_assert (ret);
       g_assert (!g_file_query_exists (data->file, NULL));

       g_main_loop_quit (data->loop);
    }
}

static void
replaced_cb (GObject      *source,
             GAsyncResult *res,
             gpointer      user_data)
{
  ReplaceLoadData *data = user_data;
  GError *error;

  error = NULL;
  g_file_replace_contents_finish (data->file, res, NULL, &error);
  g_assert_no_error (error);

  g_file_load_contents_async (data->file, NULL, loaded_cb, data);
}

static void
test_replace_load (void)
{
  ReplaceLoadData *data;
  const gchar *path;
  GFileIOStream *iostream;

  data = g_new0 (ReplaceLoadData, 1);
  data->again = TRUE;
  data->data = replace_data;

  data->file = g_file_new_tmp ("g_file_replace_load_XXXXXX",
			       &iostream, NULL);
  g_assert (data->file != NULL);
  g_object_unref (iostream);

  path = g_file_peek_path (data->file);
  remove (path);

  g_assert (!g_file_query_exists (data->file, NULL));

  data->loop = g_main_loop_new (NULL, FALSE);

  g_file_replace_contents_async (data->file,
                                 data->data,
                                 strlen (data->data),
                                 NULL,
                                 FALSE,
                                 0,
                                 NULL,
                                 replaced_cb,
                                 data);

  g_main_loop_run (data->loop);

  g_main_loop_unref (data->loop);
  g_object_unref (data->file);
  g_free (data);
}

static void
test_replace_cancel (void)
{
  GFile *tmpdir, *file;
  GFileOutputStream *ostream;
  GFileEnumerator *fenum;
  GFileInfo *info;
  GCancellable *cancellable;
  gchar *path;
  gsize nwrote;
  guint count;
  GError *error = NULL;

  g_test_bug ("629301");

  path = g_dir_make_tmp ("g_file_replace_cancel_XXXXXX", &error);
  g_assert_no_error (error);
  tmpdir = g_file_new_for_path (path);
  g_free (path);

  file = g_file_get_child (tmpdir, "file");
  g_file_replace_contents (file,
                           replace_data,
                           strlen (replace_data),
                           NULL, FALSE, 0, NULL,
                           NULL, &error);
  g_assert_no_error (error);

  ostream = g_file_replace (file, NULL, TRUE, 0, NULL, &error);
  g_assert_no_error (error);

  g_output_stream_write_all (G_OUTPUT_STREAM (ostream),
                             replace_data, strlen (replace_data),
                             &nwrote, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (nwrote, ==, strlen (replace_data));

  /* At this point there should be two files; the original and the
   * temporary.
   */
  fenum = g_file_enumerate_children (tmpdir, NULL, 0, NULL, &error);
  g_assert_no_error (error);

  info = g_file_enumerator_next_file (fenum, NULL, &error);
  g_assert_no_error (error);
  g_assert (info != NULL);
  g_object_unref (info);
  info = g_file_enumerator_next_file (fenum, NULL, &error);
  g_assert_no_error (error);
  g_assert (info != NULL);
  g_object_unref (info);

  g_file_enumerator_close (fenum, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (fenum);

  /* Also test the g_file_enumerator_iterate() API */
  fenum = g_file_enumerate_children (tmpdir, NULL, 0, NULL, &error);
  g_assert_no_error (error);
  count = 0;

  while (TRUE)
    {
      gboolean ret = g_file_enumerator_iterate (fenum, &info, NULL, NULL, &error);
      g_assert (ret);
      g_assert_no_error (error);
      if (!info)
        break;
      count++;
    }
  g_assert_cmpint (count, ==, 2);

  g_file_enumerator_close (fenum, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (fenum);

  /* Now test just getting child from the g_file_enumerator_iterate() API */
  fenum = g_file_enumerate_children (tmpdir, "standard::name", 0, NULL, &error);
  g_assert_no_error (error);
  count = 0;

  while (TRUE)
    {
      GFile *child;
      gboolean ret = g_file_enumerator_iterate (fenum, NULL, &child, NULL, &error);

      g_assert (ret);
      g_assert_no_error (error);

      if (!child)
        break;

      g_assert (G_IS_FILE (child));
      count++;
    }
  g_assert_cmpint (count, ==, 2);

  g_file_enumerator_close (fenum, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (fenum);

  /* Make sure the temporary gets deleted even if we cancel. */
  cancellable = g_cancellable_new ();
  g_cancellable_cancel (cancellable);
  g_output_stream_close (G_OUTPUT_STREAM (ostream), cancellable, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);

  g_object_unref (cancellable);
  g_object_unref (ostream);

  g_file_delete (file, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (file);

  /* This will only succeed if the temp file was deleted. */
  g_file_delete (tmpdir, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (tmpdir);
}

static void
on_file_deleted (GObject      *object,
		 GAsyncResult *result,
		 gpointer      user_data)
{
  GError *local_error = NULL;
  GMainLoop *loop = user_data;

  (void) g_file_delete_finish ((GFile*)object, result, &local_error);
  g_assert_no_error (local_error);

  g_main_loop_quit (loop);
}

static void
test_async_delete (void)
{
  GFile *file;
  GFileIOStream *iostream;
  GError *local_error = NULL;
  GError **error = &local_error;
  GMainLoop *loop;

  file = g_file_new_tmp ("g_file_delete_XXXXXX",
			 &iostream, error);
  g_assert_no_error (local_error);
  g_object_unref (iostream);

  g_assert (g_file_query_exists (file, NULL));

  loop = g_main_loop_new (NULL, TRUE);

  g_file_delete_async (file, G_PRIORITY_DEFAULT, NULL, on_file_deleted, loop);

  g_main_loop_run (loop);

  g_assert (!g_file_query_exists (file, NULL));

  g_main_loop_unref (loop);
  g_object_unref (file);
}

#ifdef G_OS_UNIX
static void
test_copy_preserve_mode (void)
{
  GFile *tmpfile;
  GFile *dest_tmpfile;
  GFileInfo *dest_info;
  GFileIOStream *iostream;
  GError *local_error = NULL;
  GError **error = &local_error;
  guint32 romode = S_IFREG | 0600;
  guint32 dest_mode;

  tmpfile = g_file_new_tmp ("tmp-copy-preserve-modeXXXXXX",
                            &iostream, error);
  g_assert_no_error (local_error);
  g_io_stream_close ((GIOStream*)iostream, NULL, error);
  g_assert_no_error (local_error);
  g_clear_object (&iostream);

  g_file_set_attribute (tmpfile, G_FILE_ATTRIBUTE_UNIX_MODE, G_FILE_ATTRIBUTE_TYPE_UINT32,
                        &romode, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                        NULL, error);
  g_assert_no_error (local_error);

  dest_tmpfile = g_file_new_tmp ("tmp-copy-preserve-modeXXXXXX",
                                 &iostream, error);
  g_assert_no_error (local_error);
  g_io_stream_close ((GIOStream*)iostream, NULL, error);
  g_assert_no_error (local_error);
  g_clear_object (&iostream);

  g_file_copy (tmpfile, dest_tmpfile, G_FILE_COPY_OVERWRITE | G_FILE_COPY_NOFOLLOW_SYMLINKS | G_FILE_COPY_ALL_METADATA,
               NULL, NULL, NULL, error);
  g_assert_no_error (local_error);

  dest_info = g_file_query_info (dest_tmpfile, G_FILE_ATTRIBUTE_UNIX_MODE, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                 NULL, error);
  g_assert_no_error (local_error);

  dest_mode = g_file_info_get_attribute_uint32 (dest_info, G_FILE_ATTRIBUTE_UNIX_MODE);
  
  g_assert_cmpint (dest_mode, ==, romode);

  (void) g_file_delete (tmpfile, NULL, NULL);
  (void) g_file_delete (dest_tmpfile, NULL, NULL);
  
  g_clear_object (&tmpfile);
  g_clear_object (&dest_tmpfile);
  g_clear_object (&dest_info);
}
#endif

static gchar *
splice_to_string (GInputStream   *stream,
                  GError        **error)
{
  GMemoryOutputStream *buffer = NULL;
  char *ret = NULL;

  buffer = (GMemoryOutputStream*)g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  if (g_output_stream_splice ((GOutputStream*)buffer, stream, 0, NULL, error) < 0)
    goto out;

  if (!g_output_stream_write ((GOutputStream*)buffer, "\0", 1, NULL, error))
    goto out;

  if (!g_output_stream_close ((GOutputStream*)buffer, NULL, error))
    goto out;

  ret = g_memory_output_stream_steal_data (buffer);
 out:
  g_clear_object (&buffer);
  return ret;
}

static guint64
get_size_from_du (const gchar *path)
{
  GSubprocess *du;
  gchar *result;
  gchar *endptr;
  guint64 size;
  GError *error = NULL;

  du = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                         &error,
                         "du", "--bytes", "-s", path, NULL);
  g_assert_no_error (error);

  result = splice_to_string (g_subprocess_get_stdout_pipe (du), &error);
  g_assert_no_error (error);

  size = g_ascii_strtoll (result, &endptr, 10);

  g_object_unref (du);
  g_free (result);

  return size;
}

static void
test_measure (void)
{
  GFile *file;
  guint64 size;
  guint64 num_bytes;
  guint64 num_dirs;
  guint64 num_files;
  GError *error = NULL;
  gboolean ok;
  gchar *path;

  path = g_test_build_filename (G_TEST_DIST, "desktop-files", NULL);
  file = g_file_new_for_path (path);

  if (g_find_program_in_path ("du"))
    {
      size = get_size_from_du (path);
    }
  else
    {
      g_test_message ("du not found, skipping byte measurement");
      size = 0;
    }

  ok = g_file_measure_disk_usage (file,
                                  G_FILE_MEASURE_APPARENT_SIZE,
                                  NULL,
                                  NULL,
                                  NULL,
                                  &num_bytes,
                                  &num_dirs,
                                  &num_files,
                                  &error);
  g_assert (ok);
  g_assert_no_error (error);

  if (size > 0)
    g_assert_cmpuint (num_bytes, ==, size);
  g_assert_cmpuint (num_dirs, ==, 6);
  g_assert_cmpuint (num_files, ==, 30);

  g_object_unref (file);
  g_free (path);
}

typedef struct {
  guint64 expected_bytes;
  guint64 expected_dirs;
  guint64 expected_files;
  gint progress_count;
  guint64 progress_bytes;
  guint64 progress_dirs;
  guint64 progress_files;
} MeasureData;

static void
measure_progress (gboolean reporting,
                  guint64  current_size,
                  guint64  num_dirs,
                  guint64  num_files,
                  gpointer user_data)
{
  MeasureData *data = user_data;

  data->progress_count += 1;

  g_assert_cmpuint (current_size, >=, data->progress_bytes);
  g_assert_cmpuint (num_dirs, >=, data->progress_dirs);
  g_assert_cmpuint (num_files, >=, data->progress_files);

  data->progress_bytes = current_size;
  data->progress_dirs = num_dirs;
  data->progress_files = num_files;
}

static void
measure_done (GObject      *source,
              GAsyncResult *res,
              gpointer      user_data)
{
  MeasureData *data = user_data;
  guint64 num_bytes, num_dirs, num_files;
  GError *error = NULL;
  gboolean ok;

  ok = g_file_measure_disk_usage_finish (G_FILE (source), res, &num_bytes, &num_dirs, &num_files, &error);
  g_assert (ok);
  g_assert_no_error (error);

  if (data->expected_bytes > 0)
    g_assert_cmpuint (data->expected_bytes, ==, num_bytes);
  g_assert_cmpuint (data->expected_dirs, ==, num_dirs);
  g_assert_cmpuint (data->expected_files, ==, num_files);

  g_assert_cmpuint (data->progress_count, >, 0);
  g_assert_cmpuint (num_bytes, >=, data->progress_bytes);
  g_assert_cmpuint (num_dirs, >=, data->progress_dirs);
  g_assert_cmpuint (num_files, >=, data->progress_files);

  g_free (data);
  g_object_unref (source);
}

static void
test_measure_async (void)
{
  gchar *path;
  GFile *file;
  MeasureData *data;

  data = g_new (MeasureData, 1);

  data->progress_count = 0;
  data->progress_bytes = 0;
  data->progress_files = 0;
  data->progress_dirs = 0;

  path = g_test_build_filename (G_TEST_DIST, "desktop-files", NULL);
  file = g_file_new_for_path (path);

  if (g_find_program_in_path ("du"))
    {
      data->expected_bytes = get_size_from_du (path);
    }
  else
    {
      g_test_message ("du not found, skipping byte measurement");
      data->expected_bytes = 0;
    }

  g_free (path);

  data->expected_dirs = 6;
  data->expected_files = 30;

  g_file_measure_disk_usage_async (file,
                                   G_FILE_MEASURE_APPARENT_SIZE,
                                   0, NULL,
                                   measure_progress, data,
                                   measure_done, data);
}

static void
test_load_bytes (void)
{
  gchar filename[] = "g_file_load_bytes_XXXXXX";
  GError *error = NULL;
  GBytes *bytes;
  GFile *file;
  int len;
  int fd;
  int ret;

  fd = g_mkstemp (filename);
  g_assert_cmpint (fd, !=, -1);
  len = strlen ("test_load_bytes");
  ret = write (fd, "test_load_bytes", len);
  g_assert_cmpint (ret, ==, len);
  close (fd);

  file = g_file_new_for_path (filename);
  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert (bytes != NULL);
  g_assert_cmpint (len, ==, g_bytes_get_size (bytes));
  g_assert_cmpstr ("test_load_bytes", ==, (gchar *)g_bytes_get_data (bytes, NULL));

  g_file_delete (file, NULL, NULL);

  g_bytes_unref (bytes);
  g_object_unref (file);
}

typedef struct
{
  GMainLoop *main_loop;
  GFile *file;
  GBytes *bytes;
} LoadBytesAsyncData;

static void
test_load_bytes_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GFile *file = G_FILE (object);
  LoadBytesAsyncData *data = user_data;
  GError *error = NULL;

  data->bytes = g_file_load_bytes_finish (file, result, NULL, &error);
  g_assert_no_error (error);
  g_assert (data->bytes != NULL);

  g_main_loop_quit (data->main_loop);
}

static void
test_load_bytes_async (void)
{
  LoadBytesAsyncData data = { 0 };
  gchar filename[] = "g_file_load_bytes_XXXXXX";
  int len;
  int fd;
  int ret;

  fd = g_mkstemp (filename);
  g_assert_cmpint (fd, !=, -1);
  len = strlen ("test_load_bytes_async");
  ret = write (fd, "test_load_bytes_async", len);
  g_assert_cmpint (ret, ==, len);
  close (fd);

  data.main_loop = g_main_loop_new (NULL, FALSE);
  data.file = g_file_new_for_path (filename);

  g_file_load_bytes_async (data.file, NULL, test_load_bytes_cb, &data);
  g_main_loop_run (data.main_loop);

  g_assert_cmpint (len, ==, g_bytes_get_size (data.bytes));
  g_assert_cmpstr ("test_load_bytes_async", ==, (gchar *)g_bytes_get_data (data.bytes, NULL));

  g_file_delete (data.file, NULL, NULL);
  g_object_unref (data.file);
  g_bytes_unref (data.bytes);
  g_main_loop_unref (data.main_loop);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/file/basic", test_basic);
  g_test_add_func ("/file/build-filename", test_build_filename);
  g_test_add_func ("/file/parent", test_parent);
  g_test_add_func ("/file/child", test_child);
  g_test_add_func ("/file/type", test_type);
  g_test_add_func ("/file/parse-name", test_parse_name);
  g_test_add_data_func ("/file/async-create-delete/0", GINT_TO_POINTER (0), test_create_delete);
  g_test_add_data_func ("/file/async-create-delete/1", GINT_TO_POINTER (1), test_create_delete);
  g_test_add_data_func ("/file/async-create-delete/10", GINT_TO_POINTER (10), test_create_delete);
  g_test_add_data_func ("/file/async-create-delete/25", GINT_TO_POINTER (25), test_create_delete);
  g_test_add_data_func ("/file/async-create-delete/4096", GINT_TO_POINTER (4096), test_create_delete);
  g_test_add_func ("/file/replace-load", test_replace_load);
  g_test_add_func ("/file/replace-cancel", test_replace_cancel);
  g_test_add_func ("/file/async-delete", test_async_delete);
#ifdef G_OS_UNIX
  g_test_add_func ("/file/copy-preserve-mode", test_copy_preserve_mode);
#endif
  g_test_add_func ("/file/measure", test_measure);
  g_test_add_func ("/file/measure-async", test_measure_async);
  g_test_add_func ("/file/load-bytes", test_load_bytes);
  g_test_add_func ("/file/load-bytes-async", test_load_bytes_async);

  return g_test_run ();
}
