#include <glib.h>
#include <glib/gwakeup.h>
#ifdef G_OS_UNIX
#include <unistd.h>
#endif

#ifdef _WIN32
static void alarm (int sec) { }
#endif

static gboolean
check_signaled (GWakeup *wakeup)
{
  GPollFD fd;

  g_wakeup_get_pollfd (wakeup, &fd);
  return g_poll (&fd, 1, 0);
}

static void
wait_for_signaled (GWakeup *wakeup)
{
  GPollFD fd;

  g_wakeup_get_pollfd (wakeup, &fd);
  g_poll (&fd, 1, -1);
}

static void
test_semantics (void)
{
  GWakeup *wakeup;
  gint i;

  /* prevent the test from deadlocking */
  alarm (60);

  wakeup = g_wakeup_new ();
  g_assert (!check_signaled (wakeup));

  g_wakeup_signal (wakeup);
  g_assert (check_signaled (wakeup));

  g_wakeup_acknowledge (wakeup);
  g_assert (!check_signaled (wakeup));

  g_wakeup_free (wakeup);

  /* free unused */
  wakeup = g_wakeup_new ();
  g_wakeup_free (wakeup);

  /* free while signaled */
  wakeup = g_wakeup_new ();
  g_wakeup_signal (wakeup);
  g_wakeup_free (wakeup);

  /* ensure excessive signalling doesn't deadlock */
  wakeup = g_wakeup_new ();
  for (i = 0; i < 1000000; i++)
    g_wakeup_signal (wakeup);
  g_assert (check_signaled (wakeup));

  /* ensure a single acknowledgement is sufficient */
  g_wakeup_acknowledge (wakeup);
  g_assert (!check_signaled (wakeup));

  g_wakeup_free (wakeup);

  /* cancel the alarm */
  alarm (0);
}

struct token
{
  gpointer owner;
  gint ttl;
};

struct context
{
  GSList *pending_tokens;
  GMutex lock;
  GWakeup *wakeup;
  gboolean quit;
};

#define NUM_THREADS     50
#define NUM_TOKENS       5
#define TOKEN_TTL   100000

static struct context contexts[NUM_THREADS];
static GThread *threads[NUM_THREADS];
static GWakeup *last_token_wakeup;
static volatile gint tokens_alive;

static void
context_init (struct context *ctx)
{
  ctx->pending_tokens = NULL;
  g_mutex_init (&ctx->lock);
  ctx->wakeup = g_wakeup_new ();
  ctx->quit = FALSE;
}

static void
context_clear (struct context *ctx)
{
  g_assert (ctx->pending_tokens == NULL);
  g_assert (ctx->quit);

  g_mutex_clear (&ctx->lock);
  g_wakeup_free (ctx->wakeup);
}

static void
context_quit (struct context *ctx)
{
  ctx->quit = TRUE;
  g_wakeup_signal (ctx->wakeup);
}

static struct token *
context_pop_token (struct context *ctx)
{
  struct token *token;

  g_mutex_lock (&ctx->lock);
  token = ctx->pending_tokens->data;
  ctx->pending_tokens = g_slist_delete_link (ctx->pending_tokens,
                                             ctx->pending_tokens);
  g_mutex_unlock (&ctx->lock);

  return token;
}

static void
context_push_token (struct context *ctx,
                    struct token   *token)
{
  g_assert (token->owner == ctx);

  g_mutex_lock (&ctx->lock);
  ctx->pending_tokens = g_slist_prepend (ctx->pending_tokens, token);
  g_mutex_unlock (&ctx->lock);

  g_wakeup_signal (ctx->wakeup);
}

static void
dispatch_token (struct token *token)
{
  if (token->ttl > 0)
    {
      struct context *ctx;
      gint next_ctx;

      next_ctx = g_test_rand_int_range (0, NUM_THREADS);
      ctx = &contexts[next_ctx];
      token->owner = ctx;
      token->ttl--;

      context_push_token (ctx, token);
    }
  else
    {
      g_slice_free (struct token, token);

      if (g_atomic_int_dec_and_test (&tokens_alive))
        g_wakeup_signal (last_token_wakeup);
    }
}

static struct token *
token_new (int ttl)
{
  struct token *token;

  token = g_slice_new (struct token);
  token->ttl = ttl;

  g_atomic_int_inc (&tokens_alive);

  return token;
}

static gpointer
thread_func (gpointer data)
{
  struct context *ctx = data;

  while (!ctx->quit)
    {
      wait_for_signaled (ctx->wakeup);
      g_wakeup_acknowledge (ctx->wakeup);

      while (ctx->pending_tokens)
        {
          struct token *token;

          token = context_pop_token (ctx);
          g_assert (token->owner == ctx);
          dispatch_token (token);
        }
    }

  return NULL;
}

static void
test_threaded (void)
{
  gint i;

  /* make sure we don't block forever */
  alarm (60);

  /* simple mainloop test based on GWakeup.
   *
   * create a bunch of contexts and a thread to 'run' each one.  create
   * some tokens and randomly pass them between the threads, until the
   * TTL on each token is zero.
   *
   * when no tokens are left, signal that we are done.  the mainthread
   * will then signal each worker thread to exit and join them to make
   * sure that works.
   */

  last_token_wakeup = g_wakeup_new ();

  /* create contexts, assign to threads */
  for (i = 0; i < NUM_THREADS; i++)
    {
      context_init (&contexts[i]);
      threads[i] = g_thread_new ("test", thread_func, &contexts[i]);
    }

  /* dispatch tokens */
  for (i = 0; i < NUM_TOKENS; i++)
    dispatch_token (token_new (TOKEN_TTL));

  /* wait until all tokens are gone */
  wait_for_signaled (last_token_wakeup);

  /* ask threads to quit, join them, cleanup */
  for (i = 0; i < NUM_THREADS; i++)
    {
      context_quit (&contexts[i]);
      g_thread_join (threads[i]);
      context_clear (&contexts[i]);
    }

  g_wakeup_free (last_token_wakeup);

  /* cancel alarm */
  alarm (0);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

#ifdef TEST_EVENTFD_FALLBACK
#define TESTNAME_SUFFIX "-fallback"
#else
#define TESTNAME_SUFFIX
#endif


  g_test_add_func ("/gwakeup/semantics" TESTNAME_SUFFIX, test_semantics);
  g_test_add_func ("/gwakeup/threaded" TESTNAME_SUFFIX, test_threaded);

  return g_test_run ();
}
