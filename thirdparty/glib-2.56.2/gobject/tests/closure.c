#include <glib-object.h>

#ifdef G_OS_UNIX
#include <glib-unix.h>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif

static void
test_source (GSource *one, GCallback quit_callback)
{
  GClosure *closure;
  GMainLoop *loop;

  /* Callback with GMainLoop user_data */
  loop = g_main_loop_new (NULL, FALSE);

  closure = g_cclosure_new (quit_callback, loop, NULL);
  g_source_set_closure (one, closure);

  g_source_attach (one, NULL);
  g_main_loop_run (loop);

  g_source_destroy (one);
  g_main_loop_unref (loop);
}

static gboolean
simple_quit_callback (gpointer user_data)
{
  GMainLoop *loop = user_data;

  g_main_loop_quit (loop);

  return TRUE;
}

static void
test_closure_idle (void)
{
  GSource *source;

  source = g_idle_source_new ();
  test_source (source, G_CALLBACK (simple_quit_callback));
  g_source_unref (source);
}

static void
test_closure_timeout (void)
{
  GSource *source;

  source = g_timeout_source_new (10);
  test_source (source, G_CALLBACK (simple_quit_callback));
  g_source_unref (source);
}

static gboolean
iochannel_quit_callback (GIOChannel   *channel,
                         GIOCondition  cond,
                         gpointer      user_data)
{
  GMainLoop *loop = user_data;

  g_main_loop_quit (loop);

  return TRUE;
}

static void
test_closure_iochannel (void)
{
  GIOChannel *chan;
  GSource *source;
  char *path;
  GError *error = NULL;

  if (g_path_is_absolute (g_get_prgname ()))
    path = g_strdup (g_get_prgname ());
  else
    {
      path = g_test_build_filename (G_TEST_BUILT,
                                    g_get_prgname (),
                                    NULL);
    }
  chan = g_io_channel_new_file (path, "r", &error);
  g_assert_no_error (error);
  g_free (path);

  source = g_io_create_watch (chan, G_IO_IN);
  test_source (source, G_CALLBACK (iochannel_quit_callback));
  g_source_unref (source);

  g_io_channel_unref (chan);
}

static void
test_closure_child (void)
{
  GSource *source;
  GPid pid;
  GError *error = NULL;
  gchar *argv[3];

  g_assert (g_getenv ("DO_NOT_ACCIDENTALLY_RECURSE") == NULL);
  g_setenv ("DO_NOT_ACCIDENTALLY_RECURSE", "1", TRUE);

  if (g_path_is_absolute (g_get_prgname ()))
    argv[0] = g_strdup (g_get_prgname ());
  else
    {
      argv[0] = g_test_build_filename (G_TEST_BUILT,
                                       g_get_prgname (),
                                       NULL);
    }
  argv[1] = "-l";
  argv[2] = NULL;

  g_spawn_async (NULL, argv, NULL,
                 G_SPAWN_STDOUT_TO_DEV_NULL |
                 G_SPAWN_STDERR_TO_DEV_NULL |
                 G_SPAWN_DO_NOT_REAP_CHILD,
                 NULL, NULL,
                 &pid, &error);
  g_assert_no_error (error);

  g_free (argv[0]);

  source = g_child_watch_source_new (pid);
  test_source (source, G_CALLBACK (iochannel_quit_callback));
  g_source_unref (source);
}

#ifdef G_OS_UNIX
static gboolean
fd_quit_callback (gint         fd,
                  GIOCondition condition,
                  gpointer     user_data)
{
  GMainLoop *loop = user_data;

  g_main_loop_quit (loop);

  return TRUE;
}

static void
test_closure_fd (void)
{
  gint fd;
  GSource *source;

  fd = open ("/dev/null", O_RDONLY);
  g_assert (fd != -1);

  source = g_unix_fd_source_new (fd, G_IO_IN);
  test_source (source, G_CALLBACK (fd_quit_callback));
  g_source_unref (source);

  close (fd);
}

static gboolean
send_usr1 (gpointer user_data)
{
  kill (getpid (), SIGUSR1);
  return FALSE;
}

static gboolean
closure_quit_callback (gpointer     user_data)
{
  GMainLoop *loop = user_data;

  g_main_loop_quit (loop);

  return TRUE;
}

static void
test_closure_signal (void)
{
  GSource *source;

  g_idle_add_full (G_PRIORITY_LOW, send_usr1, NULL, NULL);

  source = g_unix_signal_source_new (SIGUSR1);
  test_source (source, G_CALLBACK (closure_quit_callback));
  g_source_unref (source);
}
#endif

int
main (int argc,
      char *argv[])
{
#ifndef G_OS_WIN32
  sigset_t sig_mask, old_mask;

  sigemptyset (&sig_mask);
  sigaddset (&sig_mask, SIGUSR1);
  if (sigprocmask (SIG_UNBLOCK, &sig_mask, &old_mask) == 0)
    {
      if (sigismember (&old_mask, SIGUSR1))
        g_message ("SIGUSR1 was blocked, unblocking it");
    }
#endif

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/closure/idle", test_closure_idle);
  g_test_add_func ("/closure/timeout", test_closure_timeout);
  g_test_add_func ("/closure/iochannel", test_closure_iochannel);
  g_test_add_func ("/closure/child", test_closure_child);
#ifdef G_OS_UNIX
  g_test_add_func ("/closure/fd", test_closure_fd);
  g_test_add_func ("/closure/signal", test_closure_signal);
#endif

  return g_test_run ();
}
