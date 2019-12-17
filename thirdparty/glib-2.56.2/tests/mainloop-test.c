#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <errno.h>
#include <glib.h>
#ifdef G_OS_UNIX
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#ifdef G_OS_WIN32
#include <fcntl.h>		/* For _O_BINARY used by pipe() macro */
#include <io.h>			/* for _pipe() */
#define pipe(fds) _pipe(fds, 4096, _O_BINARY)
#endif

#define ITERS 10000
#define INCREMENT 10
#define NTHREADS 4
#define NCRAWLERS 4
#define CRAWLER_TIMEOUT_RANGE 40
#define RECURSER_TIMEOUT 50

/* The partial ordering between the context array mutex and
 * crawler array mutex is that the crawler array mutex cannot
 * be locked while the context array mutex is locked
 */
GPtrArray *context_array;
GMutex context_array_mutex;
GCond context_array_cond;

GMainLoop *main_loop;

G_LOCK_DEFINE_STATIC (crawler_array_lock);
GPtrArray *crawler_array;

typedef struct _AddrData AddrData;
typedef struct _TestData TestData;

struct _AddrData
{
  GMainLoop *loop;
  GIOChannel *dest;
  gint count;
};

struct _TestData
{
  gint current_val;
  gint iters;
  GIOChannel *in;
};

static void cleanup_crawlers (GMainContext *context);

static gboolean
read_all (GIOChannel *channel, char *buf, gsize len)
{
  gsize bytes_read = 0;
  gsize count;
  GIOError err;

  while (bytes_read < len)
    {
      err = g_io_channel_read (channel, buf + bytes_read, len - bytes_read, &count);
      if (err)
	{
	  if (err != G_IO_ERROR_AGAIN)
	    return FALSE;
	}
      else if (count == 0)
	return FALSE;

      bytes_read += count;
    }

  return TRUE;
}

static gboolean
write_all (GIOChannel *channel, char *buf, gsize len)
{
  gsize bytes_written = 0;
  gsize count;
  GIOError err;

  while (bytes_written < len)
    {
      err = g_io_channel_write (channel, buf + bytes_written, len - bytes_written, &count);
      if (err && err != G_IO_ERROR_AGAIN)
	return FALSE;

      bytes_written += count;
    }

  return TRUE;
}

static gboolean
adder_callback (GIOChannel   *source,
		GIOCondition  condition,
		gpointer      data)
{
  char buf1[32];
  char buf2[32];

  char result[32] = { 0, };

  AddrData *addr_data = data;

  if (!read_all (source, buf1, 32) ||
      !read_all (source, buf2, 32))
    {
      g_main_loop_quit (addr_data->loop);
      return FALSE;
    }

  sprintf (result, "%d", atoi(buf1) + atoi(buf2));
  write_all (addr_data->dest, result, 32);
  
  return TRUE;
}

static gboolean
timeout_callback (gpointer data)
{
  AddrData *addr_data = data;

  addr_data->count++;
  
  return TRUE;
}

static gpointer
adder_thread (gpointer data)
{
  GMainContext *context;
  GSource *adder_source;
  GSource *timeout_source;

  GIOChannel **channels = data;
  AddrData addr_data;

  context = g_main_context_new ();

  g_mutex_lock (&context_array_mutex);
  
  g_ptr_array_add (context_array, context);

  if (context_array->len == NTHREADS)
    g_cond_broadcast (&context_array_cond);
  
  g_mutex_unlock (&context_array_mutex);

  addr_data.dest = channels[1];
  addr_data.loop = g_main_loop_new (context, FALSE);
  addr_data.count = 0;

  adder_source = g_io_create_watch (channels[0], G_IO_IN | G_IO_HUP);
  g_source_set_name (adder_source, "Adder I/O");
  g_source_set_callback (adder_source, (GSourceFunc)adder_callback, &addr_data, NULL);
  g_source_attach (adder_source, context);
  g_source_unref (adder_source);

  timeout_source = g_timeout_source_new (10);
  g_source_set_name (timeout_source, "Adder timeout");
  g_source_set_callback (timeout_source, (GSourceFunc)timeout_callback, &addr_data, NULL);
  g_source_set_priority (timeout_source, G_PRIORITY_HIGH);
  g_source_attach (timeout_source, context);
  g_source_unref (timeout_source);

  g_main_loop_run (addr_data.loop);

  g_io_channel_unref (channels[0]);
  g_io_channel_unref (channels[1]);

  g_free (channels);
  
  g_main_loop_unref (addr_data.loop);

#ifdef VERBOSE
  g_print ("Timeout run %d times\n", addr_data.count);
#endif

  g_mutex_lock (&context_array_mutex);
  g_ptr_array_remove (context_array, context);
  if (context_array->len == 0)
    g_main_loop_quit (main_loop);
  g_mutex_unlock (&context_array_mutex);

  cleanup_crawlers (context);
  g_main_context_unref (context);

  return NULL;
}

static void
io_pipe (GIOChannel **channels)
{
  gint fds[2];

  if (pipe(fds) < 0)
    {
      int errsv = errno;
      g_warning ("Cannot create pipe %s\n", g_strerror (errsv));
      exit (1);
    }

  channels[0] = g_io_channel_unix_new (fds[0]);
  channels[1] = g_io_channel_unix_new (fds[1]);

  g_io_channel_set_close_on_unref (channels[0], TRUE);
  g_io_channel_set_close_on_unref (channels[1], TRUE);
}

static void
do_add (GIOChannel *in, gint a, gint b)
{
  char buf1[32] = { 0, };
  char buf2[32] = { 0, };

  sprintf (buf1, "%d", a);
  sprintf (buf2, "%d", b);

  write_all (in, buf1, 32);
  write_all (in, buf2, 32);
}

static gboolean
adder_response (GIOChannel   *source,
		GIOCondition  condition,
		gpointer      data)
{
  char result[32];
  TestData *test_data = data;
  
  if (!read_all (source, result, 32))
    return FALSE;

  test_data->current_val = atoi (result);
  test_data->iters--;

  if (test_data->iters == 0)
    {
      if (test_data->current_val != ITERS * INCREMENT)
	{
	  g_print ("Addition failed: %d != %d\n",
		   test_data->current_val, ITERS * INCREMENT);
	  exit (1);
	}

      g_io_channel_unref (source);
      g_io_channel_unref (test_data->in);

      g_free (test_data);
      
      return FALSE;
    }
  
  do_add (test_data->in, test_data->current_val, INCREMENT);

  return TRUE;
}

static void
create_adder_thread (void)
{
  GError *err = NULL;
  TestData *test_data;
  
  GIOChannel *in_channels[2];
  GIOChannel *out_channels[2];

  GIOChannel **sub_channels;

  sub_channels = g_new (GIOChannel *, 2);

  io_pipe (in_channels);
  io_pipe (out_channels);

  sub_channels[0] = in_channels[0];
  sub_channels[1] = out_channels[1];

  g_thread_create (adder_thread, sub_channels, FALSE, &err);

  if (err)
    {
      g_warning ("Cannot create thread: %s", err->message);
      exit (1);
    }

  test_data = g_new (TestData, 1);
  test_data->in = in_channels[1];
  test_data->current_val = 0;
  test_data->iters = ITERS;

  g_io_add_watch (out_channels[0], G_IO_IN | G_IO_HUP,
		  adder_response, test_data);
  
  do_add (test_data->in, test_data->current_val, INCREMENT);
}

static void create_crawler (void);

static void
remove_crawler (void)
{
  GSource *other_source;

  if (crawler_array->len > 0)
    {
      other_source = crawler_array->pdata[g_random_int_range (0, crawler_array->len)];
      g_source_destroy (other_source);
      g_assert (g_ptr_array_remove_fast (crawler_array, other_source));
    }
}

static gint
crawler_callback (gpointer data)
{
  GSource *source = data;

  G_LOCK (crawler_array_lock);
  
  if (!g_ptr_array_remove_fast (crawler_array, source))
    remove_crawler();

  remove_crawler();
  G_UNLOCK (crawler_array_lock);
	    
  create_crawler();
  create_crawler();

  return FALSE;
}

static void
create_crawler (void)
{
  GSource *source = g_timeout_source_new (g_random_int_range (0, CRAWLER_TIMEOUT_RANGE));
  g_source_set_name (source, "Crawler timeout");
  g_source_set_callback (source, (GSourceFunc)crawler_callback, source, NULL);

  G_LOCK (crawler_array_lock);
  g_ptr_array_add (crawler_array, source);
  
  g_mutex_lock (&context_array_mutex);
  g_source_attach (source, context_array->pdata[g_random_int_range (0, context_array->len)]);
  g_source_unref (source);
  g_mutex_unlock (&context_array_mutex);

  G_UNLOCK (crawler_array_lock);
}

static void
cleanup_crawlers (GMainContext *context)
{
  gint i;
  
  G_LOCK (crawler_array_lock);
  for (i=0; i < crawler_array->len; i++)
    {
      if (g_source_get_context (crawler_array->pdata[i]) == context)
	{
	  g_source_destroy (g_ptr_array_remove_index (crawler_array, i));
	  i--;
	}
    }
  G_UNLOCK (crawler_array_lock);
}

static gboolean
recurser_idle (gpointer data)
{
  GMainContext *context = data;
  gint i;

  for (i = 0; i < 10; i++)
    g_main_context_iteration (context, FALSE);

  return FALSE;
}

static gboolean
recurser_start (gpointer data)
{
  GMainContext *context;
  GSource *source;
  
  g_mutex_lock (&context_array_mutex);
  context = context_array->pdata[g_random_int_range (0, context_array->len)];
  source = g_idle_source_new ();
  g_source_set_name (source, "Recursing idle source");
  g_source_set_callback (source, recurser_idle, context, NULL);
  g_source_attach (source, context);
  g_source_unref (source);
  g_mutex_unlock (&context_array_mutex);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  gint i;

  context_array = g_ptr_array_new ();

  crawler_array = g_ptr_array_new ();

  main_loop = g_main_loop_new (NULL, FALSE);

  for (i = 0; i < NTHREADS; i++)
    create_adder_thread ();

  /* Wait for all threads to start
   */
  g_mutex_lock (&context_array_mutex);
  
  if (context_array->len < NTHREADS)
    g_cond_wait (&context_array_cond, &context_array_mutex);
  
  g_mutex_unlock (&context_array_mutex);
  
  for (i = 0; i < NCRAWLERS; i++)
    create_crawler ();

  g_timeout_add (RECURSER_TIMEOUT, recurser_start, NULL);

  g_main_loop_run (main_loop);
  g_main_loop_unref (main_loop);

  g_ptr_array_unref (crawler_array);
  g_ptr_array_unref (context_array);

  return 0;
}
