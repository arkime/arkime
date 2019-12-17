#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <glib.h>

/* GMutex */

static GMutex test_g_mutex_mutex;
static guint test_g_mutex_int = 0;
static gboolean test_g_mutex_thread_ready;
G_LOCK_DEFINE_STATIC (test_g_mutex);

static gpointer
test_g_mutex_thread (gpointer data)
{
  g_assert (GPOINTER_TO_INT (data) == 42);
  g_assert (g_mutex_trylock (&test_g_mutex_mutex) == FALSE);
  g_assert (G_TRYLOCK (test_g_mutex) == FALSE);
  test_g_mutex_thread_ready = TRUE;
  g_mutex_lock (&test_g_mutex_mutex);
  g_assert (test_g_mutex_int == 42);
  g_mutex_unlock (&test_g_mutex_mutex);

  return GINT_TO_POINTER (41);
}

static void
test_g_mutex (void)
{
  GThread *thread;

  g_assert (g_mutex_trylock (&test_g_mutex_mutex));
  g_assert (G_TRYLOCK (test_g_mutex));
  test_g_mutex_thread_ready = FALSE;
  thread = g_thread_create (test_g_mutex_thread, GINT_TO_POINTER (42),
			    TRUE, NULL);
  /* This busy wait is only for testing purposes and not an example of
   * good code!*/
  while (!test_g_mutex_thread_ready)
    g_usleep (G_USEC_PER_SEC / 5);
  test_g_mutex_int = 42;
  G_UNLOCK (test_g_mutex);
  g_mutex_unlock (&test_g_mutex_mutex);
  g_assert (GPOINTER_TO_INT (g_thread_join (thread)) == 41);
}

/* GStaticRecMutex */

static GStaticRecMutex test_g_static_rec_mutex_mutex = G_STATIC_REC_MUTEX_INIT;
static guint test_g_static_rec_mutex_int = 0;
static gboolean test_g_static_rec_mutex_thread_ready;

static gpointer
test_g_static_rec_mutex_thread (gpointer data)
{
  g_assert (GPOINTER_TO_INT (data) == 42);
  g_assert (g_static_rec_mutex_trylock (&test_g_static_rec_mutex_mutex) 
	    == FALSE);
  test_g_static_rec_mutex_thread_ready = TRUE;
  g_static_rec_mutex_lock (&test_g_static_rec_mutex_mutex);
  g_static_rec_mutex_lock (&test_g_static_rec_mutex_mutex);
  g_assert (test_g_static_rec_mutex_int == 42);
  test_g_static_rec_mutex_thread_ready = FALSE;
  g_static_rec_mutex_unlock (&test_g_static_rec_mutex_mutex);
  g_static_rec_mutex_unlock (&test_g_static_rec_mutex_mutex);

  g_thread_exit (GINT_TO_POINTER (43));
  
  g_assert_not_reached ();
  return NULL;
}

static void
test_g_static_rec_mutex (void)
{
  GThread *thread;

  g_assert (g_static_rec_mutex_trylock (&test_g_static_rec_mutex_mutex));
  test_g_static_rec_mutex_thread_ready = FALSE;
  thread = g_thread_create (test_g_static_rec_mutex_thread, 
			    GINT_TO_POINTER (42), TRUE, NULL);
  /* This busy wait is only for testing purposes and not an example of
   * good code!*/
  while (!test_g_static_rec_mutex_thread_ready)
    g_usleep (G_USEC_PER_SEC / 5);

  g_assert (g_static_rec_mutex_trylock (&test_g_static_rec_mutex_mutex));
  test_g_static_rec_mutex_int = 41;
  g_static_rec_mutex_unlock (&test_g_static_rec_mutex_mutex);
  test_g_static_rec_mutex_int = 42;  
  g_static_rec_mutex_unlock (&test_g_static_rec_mutex_mutex);

  /* This busy wait is only for testing purposes and not an example of
   * good code!*/
  while (test_g_static_rec_mutex_thread_ready)
    g_usleep (G_USEC_PER_SEC / 5);

  g_static_rec_mutex_lock (&test_g_static_rec_mutex_mutex);
  test_g_static_rec_mutex_int = 0;  
  g_static_rec_mutex_unlock (&test_g_static_rec_mutex_mutex);

  g_assert (GPOINTER_TO_INT (g_thread_join (thread)) == 43);
}

/* GStaticPrivate */

#define THREADS 10

static GStaticPrivate test_g_static_private_private1 = G_STATIC_PRIVATE_INIT;
static GStaticPrivate test_g_static_private_private2 = G_STATIC_PRIVATE_INIT;
static GMutex test_g_static_private_mutex;
static guint test_g_static_private_counter = 0;
static guint test_g_static_private_ready = 0;

static gpointer
test_g_static_private_constructor (void)
{
  g_mutex_lock (&test_g_static_private_mutex);
  test_g_static_private_counter++;
  g_mutex_unlock (&test_g_static_private_mutex);  
  return g_new (guint,1);
}

static void
test_g_static_private_destructor (gpointer data)
{
  g_mutex_lock (&test_g_static_private_mutex);
  test_g_static_private_counter--;
  g_mutex_unlock (&test_g_static_private_mutex);  
  g_free (data);
}


static gpointer
test_g_static_private_thread (gpointer data)
{
  guint number = GPOINTER_TO_INT (data);
  guint i;
  guint *private1, *private2;
  for (i = 0; i < 10; i++)
    {
      number = number * 11 + 1; /* A very simple and bad RNG ;-) */
      private1 = g_static_private_get (&test_g_static_private_private1);
      if (!private1 || number % 7 > 3)
	{
	  private1 = test_g_static_private_constructor ();
	  g_static_private_set (&test_g_static_private_private1, private1,
				test_g_static_private_destructor);
	}
      *private1 = number;
      private2 = g_static_private_get (&test_g_static_private_private2);
      if (!private2 || number % 13 > 5)
	{
	  private2 = test_g_static_private_constructor ();
	  g_static_private_set (&test_g_static_private_private2, private2,
				test_g_static_private_destructor);
	}
      *private2 = number * 2;
      g_usleep (G_USEC_PER_SEC / 5);
      g_assert (number == *private1);
      g_assert (number * 2 == *private2);      
    }
  g_mutex_lock (&test_g_static_private_mutex);
  test_g_static_private_ready++;
  g_mutex_unlock (&test_g_static_private_mutex);  

  /* Busy wait is not nice but that's just a test */
  while (test_g_static_private_ready != 0)
    g_usleep (G_USEC_PER_SEC / 5);  

  for (i = 0; i < 10; i++)
    {
      private2 = g_static_private_get (&test_g_static_private_private2);
      number = number * 11 + 1; /* A very simple and bad RNG ;-) */
      if (!private2 || number % 13 > 5)
	{
	  private2 = test_g_static_private_constructor ();
	  g_static_private_set (&test_g_static_private_private2, private2,
				test_g_static_private_destructor);
	}      
      *private2 = number * 2;
      g_usleep (G_USEC_PER_SEC / 5);
      g_assert (number * 2 == *private2);      
    }

  return GINT_TO_POINTER (GPOINTER_TO_INT (data) * 3);
}

static void
test_g_static_private (void)
{
  GThread *threads[THREADS];
  guint i;

  test_g_static_private_ready = 0;

  for (i = 0; i < THREADS; i++)
    {
      threads[i] = g_thread_create (test_g_static_private_thread, 
				    GINT_TO_POINTER (i), TRUE, NULL);      
    }

  /* Busy wait is not nice but that's just a test */
  while (test_g_static_private_ready != THREADS)
    g_usleep (G_USEC_PER_SEC / 5);

  /* Reuse the static private */
  g_static_private_free (&test_g_static_private_private2);
  g_static_private_init (&test_g_static_private_private2);
  
  test_g_static_private_ready = 0;

  for (i = 0; i < THREADS; i++)
    g_assert (GPOINTER_TO_INT (g_thread_join (threads[i])) == i * 3);
    
  g_assert (test_g_static_private_counter == 0); 
}

/* GStaticRWLock */

/* -1 = writing; >0 = # of readers */
static gint test_g_static_rw_lock_state = 0; 
G_LOCK_DEFINE (test_g_static_rw_lock_state);

static gboolean test_g_static_rw_lock_run = TRUE; 
static GStaticRWLock test_g_static_rw_lock_lock = G_STATIC_RW_LOCK_INIT;

static gpointer
test_g_static_rw_lock_thread (gpointer data)
{
  while (test_g_static_rw_lock_run)
    {
      if (g_random_double() > .2) /* I'm a reader */
	{
	  
	  if (g_random_double() > .2) /* I'll block */
	    g_static_rw_lock_reader_lock (&test_g_static_rw_lock_lock);
	  else /* I'll only try */
	    if (!g_static_rw_lock_reader_trylock (&test_g_static_rw_lock_lock))
	      continue;
	  G_LOCK (test_g_static_rw_lock_state);
	  g_assert (test_g_static_rw_lock_state >= 0);
	  test_g_static_rw_lock_state++;
	  G_UNLOCK (test_g_static_rw_lock_state);

	  g_usleep (g_random_int_range (20,1000));

	  G_LOCK (test_g_static_rw_lock_state);
	  test_g_static_rw_lock_state--;
	  G_UNLOCK (test_g_static_rw_lock_state);

	  g_static_rw_lock_reader_unlock (&test_g_static_rw_lock_lock);
	}
      else /* I'm a writer */
	{
	  
	  if (g_random_double() > .2) /* I'll block */ 
	    g_static_rw_lock_writer_lock (&test_g_static_rw_lock_lock);
	  else /* I'll only try */
	    if (!g_static_rw_lock_writer_trylock (&test_g_static_rw_lock_lock))
	      continue;
	  G_LOCK (test_g_static_rw_lock_state);
	  g_assert (test_g_static_rw_lock_state == 0);
	  test_g_static_rw_lock_state = -1;
	  G_UNLOCK (test_g_static_rw_lock_state);

	  g_usleep (g_random_int_range (20,1000));

	  G_LOCK (test_g_static_rw_lock_state);
	  test_g_static_rw_lock_state = 0;
	  G_UNLOCK (test_g_static_rw_lock_state);

	  g_static_rw_lock_writer_unlock (&test_g_static_rw_lock_lock);
	}
    }
  return NULL;
}

static void
test_g_static_rw_lock (void)
{
  GThread *threads[THREADS];
  guint i;
  for (i = 0; i < THREADS; i++)
    {
      threads[i] = g_thread_create (test_g_static_rw_lock_thread, 
				    NULL, TRUE, NULL);      
    }
  g_usleep (G_USEC_PER_SEC * 5);
  test_g_static_rw_lock_run = FALSE;
  for (i = 0; i < THREADS; i++)
    {
      g_thread_join (threads[i]);
    }
  g_assert (test_g_static_rw_lock_state == 0);
}

#define G_ONCE_SIZE 100
#define G_ONCE_THREADS 10

G_LOCK_DEFINE (test_g_once);
static guint test_g_once_guint_array[G_ONCE_SIZE];
static GOnce test_g_once_array[G_ONCE_SIZE];

static gpointer
test_g_once_init_func(gpointer arg)
{
  guint *count = arg;
  g_usleep (g_random_int_range (20,1000));
  (*count)++;
  g_usleep (g_random_int_range (20,1000));
  return arg;
}

static gpointer
test_g_once_thread (gpointer ignore)
{
  guint i;
  G_LOCK (test_g_once);
  /* Don't start before all threads are created */
  G_UNLOCK (test_g_once);
  for (i = 0; i < 1000; i++)
    {
      guint pos = g_random_int_range (0, G_ONCE_SIZE);
      gpointer ret = g_once (test_g_once_array + pos, test_g_once_init_func, 
			     test_g_once_guint_array + pos);
      g_assert (ret == test_g_once_guint_array + pos);
    }
  
  /* Make sure, that all counters are touched at least once */
  for (i = 0; i < G_ONCE_SIZE; i++)
    {
      gpointer ret = g_once (test_g_once_array + i, test_g_once_init_func, 
			     test_g_once_guint_array + i);
      g_assert (ret == test_g_once_guint_array + i);
    }

  return NULL;
}

static void
test_g_thread_once (void)
{
  static GOnce once_init = G_ONCE_INIT;
  GThread *threads[G_ONCE_THREADS];
  guint i;
  for (i = 0; i < G_ONCE_SIZE; i++) 
    {
      test_g_once_array[i] = once_init;
      test_g_once_guint_array[i] = i;
    }
  G_LOCK (test_g_once);
  for (i = 0; i < G_ONCE_THREADS; i++)
    {
      threads[i] = g_thread_create (test_g_once_thread, GUINT_TO_POINTER(i%2), 
				    TRUE, NULL);
    }
  G_UNLOCK (test_g_once);
  for (i = 0; i < G_ONCE_THREADS; i++)
    {
      g_thread_join (threads[i]);
    }
  
  for (i = 0; i < G_ONCE_SIZE; i++) 
    {
      g_assert (test_g_once_guint_array[i] == i + 1);
    }
}

/* run all the tests */
static void
run_all_tests (void)
{
  test_g_mutex ();
  test_g_static_rec_mutex ();
  test_g_static_private ();
  test_g_static_rw_lock ();
  test_g_thread_once ();
}

int 
main (int   argc,
      char *argv[])
{
  run_all_tests ();

  /* Now we rerun all tests, but this time we fool the system into
   * thinking, that the available thread system is not native, but
   * userprovided. */

  g_thread_use_default_impl = FALSE;
  run_all_tests ();

  /* XXX: And this shows how silly the above non-native tests are */
  g_static_rw_lock_free (&test_g_static_rw_lock_lock);
  g_static_rec_mutex_free (&test_g_static_rec_mutex_mutex);
  g_static_private_free (&test_g_static_private_private2);

  return 0;
}
