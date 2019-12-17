/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#include "config.h"

#include "gmem.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "gslice.h"
#include "gbacktrace.h"
#include "gtestutils.h"
#include "gthread.h"
#include "glib_trace.h"

/* notes on macros:
 * having G_DISABLE_CHECKS defined disables use of glib_mem_profiler_table and
 * g_mem_profile().
 * If g_mem_gc_friendly is TRUE, freed memory should be 0-wiped.
 */

/* --- variables --- */
static GMemVTable glib_mem_vtable = {
  malloc,
  realloc,
  free,
  calloc,
  malloc,
  realloc,
};

/**
 * SECTION:memory
 * @Short_Description: general memory-handling
 * @Title: Memory Allocation
 * 
 * These functions provide support for allocating and freeing memory.
 * 
 * If any call to allocate memory using functions g_new(), g_new0(), g_renew(),
 * g_malloc(), g_malloc0(), g_malloc0_n(), g_realloc(), and g_realloc_n()
 * fails, the application is terminated. This also means that there is no
 * need to check if the call succeeded. On the other hand, g_try_...() family
 * of functions returns %NULL on failure that can be used as a check
 * for unsuccessful memory allocation. The application is not terminated
 * in this case.
 *
 * It's important to match g_malloc() (and wrappers such as g_new()) with
 * g_free(), g_slice_alloc() (and wrappers such as g_slice_new()) with
 * g_slice_free(), plain malloc() with free(), and (if you're using C++)
 * new with delete and new[] with delete[]. Otherwise bad things can happen,
 * since these allocators may use different memory pools (and new/delete call
 * constructors and destructors).
 */

/* --- functions --- */
/**
 * g_malloc:
 * @n_bytes: the number of bytes to allocate
 * 
 * Allocates @n_bytes bytes of memory.
 * If @n_bytes is 0 it returns %NULL.
 * 
 * Returns: a pointer to the allocated memory
 */
gpointer
g_malloc (gsize n_bytes)
{
  if (G_LIKELY (n_bytes))
    {
      gpointer mem;

      mem = malloc (n_bytes);
      TRACE (GLIB_MEM_ALLOC((void*) mem, (unsigned int) n_bytes, 0, 0));
      if (mem)
	return mem;

      g_error ("%s: failed to allocate %"G_GSIZE_FORMAT" bytes",
               G_STRLOC, n_bytes);
    }

  TRACE(GLIB_MEM_ALLOC((void*) NULL, (int) n_bytes, 0, 0));

  return NULL;
}

/**
 * g_malloc0:
 * @n_bytes: the number of bytes to allocate
 * 
 * Allocates @n_bytes bytes of memory, initialized to 0's.
 * If @n_bytes is 0 it returns %NULL.
 * 
 * Returns: a pointer to the allocated memory
 */
gpointer
g_malloc0 (gsize n_bytes)
{
  if (G_LIKELY (n_bytes))
    {
      gpointer mem;

      mem = calloc (1, n_bytes);
      TRACE (GLIB_MEM_ALLOC((void*) mem, (unsigned int) n_bytes, 1, 0));
      if (mem)
	return mem;

      g_error ("%s: failed to allocate %"G_GSIZE_FORMAT" bytes",
               G_STRLOC, n_bytes);
    }

  TRACE(GLIB_MEM_ALLOC((void*) NULL, (int) n_bytes, 1, 0));

  return NULL;
}

/**
 * g_realloc:
 * @mem: (nullable): the memory to reallocate
 * @n_bytes: new size of the memory in bytes
 * 
 * Reallocates the memory pointed to by @mem, so that it now has space for
 * @n_bytes bytes of memory. It returns the new address of the memory, which may
 * have been moved. @mem may be %NULL, in which case it's considered to
 * have zero-length. @n_bytes may be 0, in which case %NULL will be returned
 * and @mem will be freed unless it is %NULL.
 * 
 * Returns: the new address of the allocated memory
 */
gpointer
g_realloc (gpointer mem,
	   gsize    n_bytes)
{
  gpointer newmem;

  if (G_LIKELY (n_bytes))
    {
      newmem = realloc (mem, n_bytes);
      TRACE (GLIB_MEM_REALLOC((void*) newmem, (void*)mem, (unsigned int) n_bytes, 0));
      if (newmem)
	return newmem;

      g_error ("%s: failed to allocate %"G_GSIZE_FORMAT" bytes",
               G_STRLOC, n_bytes);
    }

  if (mem)
    free (mem);

  TRACE (GLIB_MEM_REALLOC((void*) NULL, (void*)mem, 0, 0));

  return NULL;
}

/**
 * g_free:
 * @mem: (nullable): the memory to free
 * 
 * Frees the memory pointed to by @mem.
 *
 * If @mem is %NULL it simply returns, so there is no need to check @mem
 * against %NULL before calling this function.
 */
void
g_free (gpointer mem)
{
  if (G_LIKELY (mem))
    free (mem);
  TRACE(GLIB_MEM_FREE((void*) mem));
}

/**
 * g_clear_pointer: (skip)
 * @pp: (not nullable): a pointer to a variable, struct member etc. holding a
 *    pointer
 * @destroy: a function to which a gpointer can be passed, to destroy *@pp
 *
 * Clears a reference to a variable.
 *
 * @pp must not be %NULL.
 *
 * If the reference is %NULL then this function does nothing.
 * Otherwise, the variable is destroyed using @destroy and the
 * pointer is set to %NULL.
 *
 * A macro is also included that allows this function to be used without
 * pointer casts.
 *
 * Since: 2.34
 **/
#undef g_clear_pointer
void
g_clear_pointer (gpointer      *pp,
                 GDestroyNotify destroy)
{
  gpointer _p;

  _p = *pp;
  if (_p)
    {
      *pp = NULL;
      destroy (_p);
    }
}

/**
 * g_try_malloc:
 * @n_bytes: number of bytes to allocate.
 * 
 * Attempts to allocate @n_bytes, and returns %NULL on failure.
 * Contrast with g_malloc(), which aborts the program on failure.
 * 
 * Returns: the allocated memory, or %NULL.
 */
gpointer
g_try_malloc (gsize n_bytes)
{
  gpointer mem;

  if (G_LIKELY (n_bytes))
    mem = malloc (n_bytes);
  else
    mem = NULL;

  TRACE (GLIB_MEM_ALLOC((void*) mem, (unsigned int) n_bytes, 0, 1));

  return mem;
}

/**
 * g_try_malloc0:
 * @n_bytes: number of bytes to allocate
 * 
 * Attempts to allocate @n_bytes, initialized to 0's, and returns %NULL on
 * failure. Contrast with g_malloc0(), which aborts the program on failure.
 * 
 * Since: 2.8
 * Returns: the allocated memory, or %NULL
 */
gpointer
g_try_malloc0 (gsize n_bytes)
{
  gpointer mem;

  if (G_LIKELY (n_bytes))
    mem = calloc (1, n_bytes);
  else
    mem = NULL;

  return mem;
}

/**
 * g_try_realloc:
 * @mem: (nullable): previously-allocated memory, or %NULL.
 * @n_bytes: number of bytes to allocate.
 * 
 * Attempts to realloc @mem to a new size, @n_bytes, and returns %NULL
 * on failure. Contrast with g_realloc(), which aborts the program
 * on failure.
 *
 * If @mem is %NULL, behaves the same as g_try_malloc().
 * 
 * Returns: the allocated memory, or %NULL.
 */
gpointer
g_try_realloc (gpointer mem,
	       gsize    n_bytes)
{
  gpointer newmem;

  if (G_LIKELY (n_bytes))
    newmem = realloc (mem, n_bytes);
  else
    {
      newmem = NULL;
      if (mem)
	free (mem);
    }

  TRACE (GLIB_MEM_REALLOC((void*) newmem, (void*)mem, (unsigned int) n_bytes, 1));

  return newmem;
}


#define SIZE_OVERFLOWS(a,b) (G_UNLIKELY ((b) > 0 && (a) > G_MAXSIZE / (b)))

/**
 * g_malloc_n:
 * @n_blocks: the number of blocks to allocate
 * @n_block_bytes: the size of each block in bytes
 * 
 * This function is similar to g_malloc(), allocating (@n_blocks * @n_block_bytes) bytes,
 * but care is taken to detect possible overflow during multiplication.
 * 
 * Since: 2.24
 * Returns: a pointer to the allocated memory
 */
gpointer
g_malloc_n (gsize n_blocks,
	    gsize n_block_bytes)
{
  if (SIZE_OVERFLOWS (n_blocks, n_block_bytes))
    {
      g_error ("%s: overflow allocating %"G_GSIZE_FORMAT"*%"G_GSIZE_FORMAT" bytes",
               G_STRLOC, n_blocks, n_block_bytes);
    }

  return g_malloc (n_blocks * n_block_bytes);
}

/**
 * g_malloc0_n:
 * @n_blocks: the number of blocks to allocate
 * @n_block_bytes: the size of each block in bytes
 * 
 * This function is similar to g_malloc0(), allocating (@n_blocks * @n_block_bytes) bytes,
 * but care is taken to detect possible overflow during multiplication.
 * 
 * Since: 2.24
 * Returns: a pointer to the allocated memory
 */
gpointer
g_malloc0_n (gsize n_blocks,
	     gsize n_block_bytes)
{
  if (SIZE_OVERFLOWS (n_blocks, n_block_bytes))
    {
      g_error ("%s: overflow allocating %"G_GSIZE_FORMAT"*%"G_GSIZE_FORMAT" bytes",
               G_STRLOC, n_blocks, n_block_bytes);
    }

  return g_malloc0 (n_blocks * n_block_bytes);
}

/**
 * g_realloc_n:
 * @mem: (nullable): the memory to reallocate
 * @n_blocks: the number of blocks to allocate
 * @n_block_bytes: the size of each block in bytes
 * 
 * This function is similar to g_realloc(), allocating (@n_blocks * @n_block_bytes) bytes,
 * but care is taken to detect possible overflow during multiplication.
 * 
 * Since: 2.24
 * Returns: the new address of the allocated memory
 */
gpointer
g_realloc_n (gpointer mem,
	     gsize    n_blocks,
	     gsize    n_block_bytes)
{
  if (SIZE_OVERFLOWS (n_blocks, n_block_bytes))
    {
      g_error ("%s: overflow allocating %"G_GSIZE_FORMAT"*%"G_GSIZE_FORMAT" bytes",
               G_STRLOC, n_blocks, n_block_bytes);
    }

  return g_realloc (mem, n_blocks * n_block_bytes);
}

/**
 * g_try_malloc_n:
 * @n_blocks: the number of blocks to allocate
 * @n_block_bytes: the size of each block in bytes
 * 
 * This function is similar to g_try_malloc(), allocating (@n_blocks * @n_block_bytes) bytes,
 * but care is taken to detect possible overflow during multiplication.
 * 
 * Since: 2.24
 * Returns: the allocated memory, or %NULL.
 */
gpointer
g_try_malloc_n (gsize n_blocks,
		gsize n_block_bytes)
{
  if (SIZE_OVERFLOWS (n_blocks, n_block_bytes))
    return NULL;

  return g_try_malloc (n_blocks * n_block_bytes);
}

/**
 * g_try_malloc0_n:
 * @n_blocks: the number of blocks to allocate
 * @n_block_bytes: the size of each block in bytes
 * 
 * This function is similar to g_try_malloc0(), allocating (@n_blocks * @n_block_bytes) bytes,
 * but care is taken to detect possible overflow during multiplication.
 * 
 * Since: 2.24
 * Returns: the allocated memory, or %NULL
 */
gpointer
g_try_malloc0_n (gsize n_blocks,
		 gsize n_block_bytes)
{
  if (SIZE_OVERFLOWS (n_blocks, n_block_bytes))
    return NULL;

  return g_try_malloc0 (n_blocks * n_block_bytes);
}

/**
 * g_try_realloc_n:
 * @mem: (nullable): previously-allocated memory, or %NULL.
 * @n_blocks: the number of blocks to allocate
 * @n_block_bytes: the size of each block in bytes
 * 
 * This function is similar to g_try_realloc(), allocating (@n_blocks * @n_block_bytes) bytes,
 * but care is taken to detect possible overflow during multiplication.
 * 
 * Since: 2.24
 * Returns: the allocated memory, or %NULL.
 */
gpointer
g_try_realloc_n (gpointer mem,
		 gsize    n_blocks,
		 gsize    n_block_bytes)
{
  if (SIZE_OVERFLOWS (n_blocks, n_block_bytes))
    return NULL;

  return g_try_realloc (mem, n_blocks * n_block_bytes);
}

/**
 * g_mem_is_system_malloc:
 * 
 * Checks whether the allocator used by g_malloc() is the system's
 * malloc implementation. If it returns %TRUE memory allocated with
 * malloc() can be used interchangeable with memory allocated using g_malloc().
 * This function is useful for avoiding an extra copy of allocated memory returned
 * by a non-GLib-based API.
 *
 * Returns: if %TRUE, malloc() and g_malloc() can be mixed.
 *
 * Deprecated: 2.46: GLib always uses the system malloc, so this function always
 * returns %TRUE.
 **/
gboolean
g_mem_is_system_malloc (void)
{
  return TRUE;
}

/**
 * g_mem_set_vtable:
 * @vtable: table of memory allocation routines.
 * 
 * This function used to let you override the memory allocation function.
 * However, its use was incompatible with the use of global constructors
 * in GLib and GIO, because those use the GLib allocators before main is
 * reached. Therefore this function is now deprecated and is just a stub.
 *
 * Deprecated: 2.46: This function now does nothing. Use other memory
 * profiling tools instead
 */
void
g_mem_set_vtable (GMemVTable *vtable)
{
  g_warning (G_STRLOC ": custom memory allocation vtable not supported");
}


/**
 * glib_mem_profiler_table:
 *
 * Used to be a #GMemVTable containing profiling variants of the memory
 * allocation functions, but this variable shouldn't be modified anymore.
 *
 * Deprecated: 2.46: Use other memory profiling tools instead
 */
GMemVTable *glib_mem_profiler_table = &glib_mem_vtable;

/**
 * g_mem_profile:
 *
 * GLib used to support some tools for memory profiling, but this
 * no longer works. There are many other useful tools for memory
 * profiling these days which can be used instead.
 *
 * Deprecated: 2.46: Use other memory profiling tools instead
 */
void
g_mem_profile (void)
{
  g_warning (G_STRLOC ": memory profiling not supported");
}
