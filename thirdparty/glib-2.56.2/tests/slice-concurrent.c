/* test for gslice cross thread allocation/free
 * Copyright (C) 2006 Stefan Westerfeld
 * Copyright (C) 2007 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include <glib.h>
#include <stdlib.h>
#ifdef G_OS_UNIX
#include <unistd.h>
#endif

#define N_THREADS	8
#define N_ALLOCS	50000
#define MAX_BLOCK_SIZE  64

struct ThreadData
{
  int	   thread_id;
  GThread* gthread;

  GMutex   to_free_mutex;
  void*    to_free [N_THREADS * N_ALLOCS];
  int      bytes_to_free [N_THREADS * N_ALLOCS];
  int      n_to_free;
  int      n_freed;
} tdata[N_THREADS];

static void *
thread_func (void *arg)
{
  struct ThreadData *td = arg;
  int i;
/*   g_print ("Thread %d starting\n", td->thread_id); */
  for (i = 0; i < N_ALLOCS; i++)
    {
      int bytes;
      char *mem;
      int f;
      int t;

      if (rand() % (N_ALLOCS / 20) == 0)
	g_print ("%c", 'a' - 1 + td->thread_id);

      /* allocate block of random size and randomly fill */
      bytes = rand() % MAX_BLOCK_SIZE + 1;
      mem = g_slice_alloc (bytes);

      for (f = 0; f < bytes; f++)
	mem[f] = rand();

      /* associate block with random thread */
      t = rand() % N_THREADS;
      g_mutex_lock (&tdata[t].to_free_mutex);
      tdata[t].to_free[tdata[t].n_to_free] = mem;
      tdata[t].bytes_to_free[tdata[t].n_to_free] = bytes;
      tdata[t].n_to_free++;
      g_mutex_unlock (&tdata[t].to_free_mutex);

      /* shuffle thread execution order every once in a while */
      if (rand() % 97 == 0)
        {
          if (rand() % 2)
            g_thread_yield();   /* concurrent shuffling for single core */
          else
            g_usleep (1000);    /* concurrent shuffling for multi core */
        }

      /* free a block associated with this thread */
      g_mutex_lock (&td->to_free_mutex);
      if (td->n_to_free > 0)
	{
	  td->n_to_free--;
	  g_slice_free1 (td->bytes_to_free[td->n_to_free], td->to_free[td->n_to_free]);
	  td->n_freed++;
	}
      g_mutex_unlock (&td->to_free_mutex);
    }

  return NULL;
}

int
main (void)
{
  int t;

  for (t = 0; t < N_THREADS; t++)
    {
      tdata[t].thread_id = t + 1;
      tdata[t].n_to_free = 0;
      tdata[t].n_freed = 0;
    }
  g_print ("Starting %d threads for concurrent GSlice usage...\n", N_THREADS);
  for (t = 0; t < N_THREADS; t++)
    {
      tdata[t].gthread   = g_thread_create (thread_func, &tdata[t], TRUE, NULL);
      g_assert (tdata[t].gthread != NULL);
    }
  for (t = 0; t < N_THREADS; t++)
    {
      g_thread_join (tdata[t].gthread);
    }
  g_print ("\n");
  for (t = 0; t < N_THREADS; t++)
    {
      g_print ("Thread %d: %d blocks freed, %d blocks not freed\n",
		    tdata[t].thread_id, tdata[t].n_freed, tdata[t].n_to_free);
    }
  return 0;
}
