/* 
 * Copyright (C) 2011 Red Hat, Inc.
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
 *
 * Author: Colin Walters <walters@verbum.org> 
 */

#include "config.h"

#include <glib.h>
#include <string.h>

static char *echo_prog_path;

static void
multithreaded_test_run (GThreadFunc function)
{
  int i;
  GPtrArray *threads = g_ptr_array_new ();
  guint n_threads;

  /* Limit to 64, otherwise we may hit file descriptor limits and such */
  n_threads = MIN (g_get_num_processors () * 2, 64);

  for (i = 0; i < n_threads; i++)
    {
      GThread *thread;

      thread = g_thread_new ("test", function, GINT_TO_POINTER (i));
      g_ptr_array_add (threads, thread);
    }

  for (i = 0; i < n_threads; i++)
    {
      gpointer ret;
      ret = g_thread_join (g_ptr_array_index (threads, i));
      g_assert_cmpint (GPOINTER_TO_INT (ret), ==, i);
    }
  g_ptr_array_free (threads, TRUE);
}

static gpointer
test_spawn_sync_multithreaded_instance (gpointer data)
{
  int tnum = GPOINTER_TO_INT (data);
  GError *error = NULL;
  GPtrArray *argv;
  char *arg;
  char *stdout_str;
  int estatus;

  arg = g_strdup_printf ("thread %d", tnum);

  argv = g_ptr_array_new ();
  g_ptr_array_add (argv, echo_prog_path);
  g_ptr_array_add (argv, arg);
  g_ptr_array_add (argv, NULL);

  g_spawn_sync (NULL, (char**)argv->pdata, NULL, G_SPAWN_DEFAULT, NULL, NULL, &stdout_str, NULL, &estatus, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (arg, ==, stdout_str);
  g_free (arg);
  g_free (stdout_str);
  g_ptr_array_free (argv, TRUE);

  return GINT_TO_POINTER (tnum);
}

static void
test_spawn_sync_multithreaded (void)
{
  multithreaded_test_run (test_spawn_sync_multithreaded_instance);
}

typedef struct {
  GMainLoop *loop;
  gboolean child_exited;
  gboolean stdout_done;
  GString *stdout_buf;
} SpawnAsyncMultithreadedData;

static gboolean
on_child_exited (GPid     pid,
		 gint     status,
		 gpointer datap)
{
  SpawnAsyncMultithreadedData *data = datap;

  data->child_exited = TRUE;
  if (data->child_exited && data->stdout_done)
    g_main_loop_quit (data->loop);
  
  return G_SOURCE_REMOVE;
}

static gboolean
on_child_stdout (GIOChannel   *channel,
		 GIOCondition  condition,
		 gpointer      datap)
{
  char buf[1024];
  GError *error = NULL;
  gsize bytes_read;
  GIOStatus status;
  SpawnAsyncMultithreadedData *data = datap;

 read:
  status = g_io_channel_read_chars (channel, buf, sizeof (buf), &bytes_read, &error);
  if (status == G_IO_STATUS_NORMAL)
    {
      g_string_append_len (data->stdout_buf, buf, (gssize) bytes_read);
      if (bytes_read == sizeof (buf))
	goto read;
    }
  else if (status == G_IO_STATUS_EOF)
    {
      g_string_append_len (data->stdout_buf, buf, (gssize) bytes_read);
      data->stdout_done = TRUE;
    }
  else if (status == G_IO_STATUS_ERROR)
    {
      g_error ("Error reading from child stdin: %s", error->message);
    }

  if (data->child_exited && data->stdout_done)
    g_main_loop_quit (data->loop);

  return !data->stdout_done;
}

static gpointer
test_spawn_async_multithreaded_instance (gpointer thread_data)
{
  int tnum = GPOINTER_TO_INT (thread_data);
  GError *error = NULL;
  GPtrArray *argv;
  char *arg;
  GPid pid;
  GMainContext *context;
  GMainLoop *loop;
  GIOChannel *channel;
  GSource *source;
  int child_stdout_fd;
  SpawnAsyncMultithreadedData data;

  context = g_main_context_new ();
  loop = g_main_loop_new (context, TRUE);

  arg = g_strdup_printf ("thread %d", tnum);

  argv = g_ptr_array_new ();
  g_ptr_array_add (argv, echo_prog_path);
  g_ptr_array_add (argv, arg);
  g_ptr_array_add (argv, NULL);

  g_spawn_async_with_pipes (NULL, (char**)argv->pdata, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL,
			    &child_stdout_fd, NULL, &error);
  g_assert_no_error (error);
  g_ptr_array_free (argv, TRUE);

  data.loop = loop;
  data.stdout_done = FALSE;
  data.child_exited = FALSE;
  data.stdout_buf = g_string_new (0);

  source = g_child_watch_source_new (pid);
  g_source_set_callback (source, (GSourceFunc)on_child_exited, &data, NULL);
  g_source_attach (source, context);
  g_source_unref (source);

  channel = g_io_channel_unix_new (child_stdout_fd);
  source = g_io_create_watch (channel, G_IO_IN | G_IO_HUP);
  g_source_set_callback (source, (GSourceFunc)on_child_stdout, &data, NULL);
  g_source_attach (source, context);
  g_source_unref (source);

  g_main_loop_run (loop);

  g_assert (data.child_exited);
  g_assert (data.stdout_done);
  g_assert_cmpstr (data.stdout_buf->str, ==, arg);
  g_string_free (data.stdout_buf, TRUE);

  g_io_channel_unref (channel);
  g_main_context_unref (context);
  g_main_loop_unref (loop);

  g_free (arg);

  return GINT_TO_POINTER (tnum);
}

static void
test_spawn_async_multithreaded (void)
{
  multithreaded_test_run (test_spawn_async_multithreaded_instance);
}

int
main (int   argc,
      char *argv[])
{
  char *dirname;
  int ret;

  g_test_init (&argc, &argv, NULL);

  dirname = g_path_get_dirname (argv[0]);
  echo_prog_path = g_build_filename (dirname, "test-spawn-echo" EXEEXT, NULL);
  if (!g_file_test (echo_prog_path, G_FILE_TEST_EXISTS))
    {
      g_free (echo_prog_path);
      echo_prog_path = g_build_filename (dirname, "lt-test-spawn-echo" EXEEXT, NULL);
    }
  g_free (dirname);

  g_assert (g_file_test (echo_prog_path, G_FILE_TEST_EXISTS));

  g_test_add_func ("/gthread/spawn-sync", test_spawn_sync_multithreaded);
  g_test_add_func ("/gthread/spawn-async", test_spawn_async_multithreaded);

  ret = g_test_run();

  g_free (echo_prog_path);

  return ret;
}
