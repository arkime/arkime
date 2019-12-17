/*
 * Copyright © 2011 Ryan Lortie
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gatomic.h"

/**
 * SECTION:atomic_operations
 * @title: Atomic Operations
 * @short_description: basic atomic integer and pointer operations
 * @see_also: #GMutex
 *
 * The following is a collection of compiler macros to provide atomic
 * access to integer and pointer-sized values.
 *
 * The macros that have 'int' in the name will operate on pointers to
 * #gint and #guint.  The macros with 'pointer' in the name will operate
 * on pointers to any pointer-sized value, including #gsize.  There is
 * no support for 64bit operations on platforms with 32bit pointers
 * because it is not generally possible to perform these operations
 * atomically.
 *
 * The get, set and exchange operations for integers and pointers
 * nominally operate on #gint and #gpointer, respectively.  Of the
 * arithmetic operations, the 'add' operation operates on (and returns)
 * signed integer values (#gint and #gssize) and the 'and', 'or', and
 * 'xor' operations operate on (and return) unsigned integer values
 * (#guint and #gsize).
 *
 * All of the operations act as a full compiler and (where appropriate)
 * hardware memory barrier.  Acquire and release or producer and
 * consumer barrier semantics are not available through this API.
 *
 * It is very important that all accesses to a particular integer or
 * pointer be performed using only this API and that different sizes of
 * operation are not mixed or used on overlapping memory regions.  Never
 * read or assign directly from or to a value -- always use this API.
 *
 * For simple reference counting purposes you should use
 * g_atomic_int_inc() and g_atomic_int_dec_and_test().  Other uses that
 * fall outside of simple reference counting patterns are prone to
 * subtle bugs and occasionally undefined behaviour.  It is also worth
 * noting that since all of these operations require global
 * synchronisation of the entire machine, they can be quite slow.  In
 * the case of performing multiple atomic operations it can often be
 * faster to simply acquire a mutex lock around the critical area,
 * perform the operations normally and then release the lock.
 **/

/**
 * G_ATOMIC_LOCK_FREE:
 *
 * This macro is defined if the atomic operations of GLib are
 * implemented using real hardware atomic operations.  This means that
 * the GLib atomic API can be used between processes and safely mixed
 * with other (hardware) atomic APIs.
 *
 * If this macro is not defined, the atomic operations may be
 * emulated using a mutex.  In that case, the GLib atomic operations are
 * only atomic relative to themselves and within a single process.
 **/

/* NOTE CAREFULLY:
 *
 * This file is the lowest-level part of GLib.
 *
 * Other lowlevel parts of GLib (threads, slice allocator, g_malloc,
 * messages, etc) call into these functions and macros to get work done.
 *
 * As such, these functions can not call back into any part of GLib
 * without risking recursion.
 */

#ifdef G_ATOMIC_LOCK_FREE

/* if G_ATOMIC_LOCK_FREE was defined by ./configure then we MUST
 * implement the atomic operations in a lock-free manner.
 */

#if defined (__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)

#if defined(__ATOMIC_SEQ_CST) && !defined(__clang__)
/* The implementation used in this code path in gatomic.h assumes
 * 4-byte int */
G_STATIC_ASSERT (sizeof (gint) == 4);

/* The implementations in gatomic.h assume 4- or 8-byte pointers */
G_STATIC_ASSERT (sizeof (void *) == 4 || sizeof (void *) == 8);
#endif

/**
 * g_atomic_int_get:
 * @atomic: a pointer to a #gint or #guint
 *
 * Gets the current value of @atomic.
 *
 * This call acts as a full compiler and hardware
 * memory barrier (before the get).
 *
 * Returns: the value of the integer
 *
 * Since: 2.4
 **/
gint
(g_atomic_int_get) (const volatile gint *atomic)
{
  return g_atomic_int_get (atomic);
}

/**
 * g_atomic_int_set:
 * @atomic: a pointer to a #gint or #guint
 * @newval: a new value to store
 *
 * Sets the value of @atomic to @newval.
 *
 * This call acts as a full compiler and hardware
 * memory barrier (after the set).
 *
 * Since: 2.4
 */
void
(g_atomic_int_set) (volatile gint *atomic,
                    gint           newval)
{
  g_atomic_int_set (atomic, newval);
}

/**
 * g_atomic_int_inc:
 * @atomic: a pointer to a #gint or #guint
 *
 * Increments the value of @atomic by 1.
 *
 * Think of this operation as an atomic version of `{ *atomic += 1; }`.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Since: 2.4
 **/
void
(g_atomic_int_inc) (volatile gint *atomic)
{
  g_atomic_int_inc (atomic);
}

/**
 * g_atomic_int_dec_and_test:
 * @atomic: a pointer to a #gint or #guint
 *
 * Decrements the value of @atomic by 1.
 *
 * Think of this operation as an atomic version of
 * `{ *atomic -= 1; return (*atomic == 0); }`.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: %TRUE if the resultant value is zero
 *
 * Since: 2.4
 **/
gboolean
(g_atomic_int_dec_and_test) (volatile gint *atomic)
{
  return g_atomic_int_dec_and_test (atomic);
}

/**
 * g_atomic_int_compare_and_exchange:
 * @atomic: a pointer to a #gint or #guint
 * @oldval: the value to compare with
 * @newval: the value to conditionally replace with
 *
 * Compares @atomic to @oldval and, if equal, sets it to @newval.
 * If @atomic was not equal to @oldval then no change occurs.
 *
 * This compare and exchange is done atomically.
 *
 * Think of this operation as an atomic version of
 * `{ if (*atomic == oldval) { *atomic = newval; return TRUE; } else return FALSE; }`.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: %TRUE if the exchange took place
 *
 * Since: 2.4
 **/
gboolean
(g_atomic_int_compare_and_exchange) (volatile gint *atomic,
                                     gint           oldval,
                                     gint           newval)
{
  return g_atomic_int_compare_and_exchange (atomic, oldval, newval);
}

/**
 * g_atomic_int_add:
 * @atomic: a pointer to a #gint or #guint
 * @val: the value to add
 *
 * Atomically adds @val to the value of @atomic.
 *
 * Think of this operation as an atomic version of
 * `{ tmp = *atomic; *atomic += val; return tmp; }`.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Before version 2.30, this function did not return a value
 * (but g_atomic_int_exchange_and_add() did, and had the same meaning).
 *
 * Returns: the value of @atomic before the add, signed
 *
 * Since: 2.4
 **/
gint
(g_atomic_int_add) (volatile gint *atomic,
                    gint           val)
{
  return g_atomic_int_add (atomic, val);
}

/**
 * g_atomic_int_and:
 * @atomic: a pointer to a #gint or #guint
 * @val: the value to 'and'
 *
 * Performs an atomic bitwise 'and' of the value of @atomic and @val,
 * storing the result back in @atomic.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Think of this operation as an atomic version of
 * `{ tmp = *atomic; *atomic &= val; return tmp; }`.
 *
 * Returns: the value of @atomic before the operation, unsigned
 *
 * Since: 2.30
 **/
guint
(g_atomic_int_and) (volatile guint *atomic,
                    guint           val)
{
  return g_atomic_int_and (atomic, val);
}

/**
 * g_atomic_int_or:
 * @atomic: a pointer to a #gint or #guint
 * @val: the value to 'or'
 *
 * Performs an atomic bitwise 'or' of the value of @atomic and @val,
 * storing the result back in @atomic.
 *
 * Think of this operation as an atomic version of
 * `{ tmp = *atomic; *atomic |= val; return tmp; }`.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: the value of @atomic before the operation, unsigned
 *
 * Since: 2.30
 **/
guint
(g_atomic_int_or) (volatile guint *atomic,
                   guint           val)
{
  return g_atomic_int_or (atomic, val);
}

/**
 * g_atomic_int_xor:
 * @atomic: a pointer to a #gint or #guint
 * @val: the value to 'xor'
 *
 * Performs an atomic bitwise 'xor' of the value of @atomic and @val,
 * storing the result back in @atomic.
 *
 * Think of this operation as an atomic version of
 * `{ tmp = *atomic; *atomic ^= val; return tmp; }`.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: the value of @atomic before the operation, unsigned
 *
 * Since: 2.30
 **/
guint
(g_atomic_int_xor) (volatile guint *atomic,
                    guint           val)
{
  return g_atomic_int_xor (atomic, val);
}


/**
 * g_atomic_pointer_get:
 * @atomic: (not nullable): a pointer to a #gpointer-sized value
 *
 * Gets the current value of @atomic.
 *
 * This call acts as a full compiler and hardware
 * memory barrier (before the get).
 *
 * Returns: the value of the pointer
 *
 * Since: 2.4
 **/
gpointer
(g_atomic_pointer_get) (const volatile void *atomic)
{
  return g_atomic_pointer_get ((const volatile gpointer *) atomic);
}

/**
 * g_atomic_pointer_set:
 * @atomic: (not nullable): a pointer to a #gpointer-sized value
 * @newval: a new value to store
 *
 * Sets the value of @atomic to @newval.
 *
 * This call acts as a full compiler and hardware
 * memory barrier (after the set).
 *
 * Since: 2.4
 **/
void
(g_atomic_pointer_set) (volatile void *atomic,
                        gpointer       newval)
{
  g_atomic_pointer_set ((volatile gpointer *) atomic, newval);
}

/**
 * g_atomic_pointer_compare_and_exchange:
 * @atomic: (not nullable): a pointer to a #gpointer-sized value
 * @oldval: the value to compare with
 * @newval: the value to conditionally replace with
 *
 * Compares @atomic to @oldval and, if equal, sets it to @newval.
 * If @atomic was not equal to @oldval then no change occurs.
 *
 * This compare and exchange is done atomically.
 *
 * Think of this operation as an atomic version of
 * `{ if (*atomic == oldval) { *atomic = newval; return TRUE; } else return FALSE; }`.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: %TRUE if the exchange took place
 *
 * Since: 2.4
 **/
gboolean
(g_atomic_pointer_compare_and_exchange) (volatile void *atomic,
                                         gpointer       oldval,
                                         gpointer       newval)
{
  return g_atomic_pointer_compare_and_exchange ((volatile gpointer *) atomic,
                                                oldval, newval);
}

/**
 * g_atomic_pointer_add:
 * @atomic: (not nullable): a pointer to a #gpointer-sized value
 * @val: the value to add
 *
 * Atomically adds @val to the value of @atomic.
 *
 * Think of this operation as an atomic version of
 * `{ tmp = *atomic; *atomic += val; return tmp; }`.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: the value of @atomic before the add, signed
 *
 * Since: 2.30
 **/
gssize
(g_atomic_pointer_add) (volatile void *atomic,
                        gssize         val)
{
  return g_atomic_pointer_add ((volatile gpointer *) atomic, val);
}

/**
 * g_atomic_pointer_and:
 * @atomic: (not nullable): a pointer to a #gpointer-sized value
 * @val: the value to 'and'
 *
 * Performs an atomic bitwise 'and' of the value of @atomic and @val,
 * storing the result back in @atomic.
 *
 * Think of this operation as an atomic version of
 * `{ tmp = *atomic; *atomic &= val; return tmp; }`.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: the value of @atomic before the operation, unsigned
 *
 * Since: 2.30
 **/
gsize
(g_atomic_pointer_and) (volatile void *atomic,
                        gsize          val)
{
  return g_atomic_pointer_and ((volatile gpointer *) atomic, val);
}

/**
 * g_atomic_pointer_or:
 * @atomic: (not nullable): a pointer to a #gpointer-sized value
 * @val: the value to 'or'
 *
 * Performs an atomic bitwise 'or' of the value of @atomic and @val,
 * storing the result back in @atomic.
 *
 * Think of this operation as an atomic version of
 * `{ tmp = *atomic; *atomic |= val; return tmp; }`.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: the value of @atomic before the operation, unsigned
 *
 * Since: 2.30
 **/
gsize
(g_atomic_pointer_or) (volatile void *atomic,
                       gsize          val)
{
  return g_atomic_pointer_or ((volatile gpointer *) atomic, val);
}

/**
 * g_atomic_pointer_xor:
 * @atomic: (not nullable): a pointer to a #gpointer-sized value
 * @val: the value to 'xor'
 *
 * Performs an atomic bitwise 'xor' of the value of @atomic and @val,
 * storing the result back in @atomic.
 *
 * Think of this operation as an atomic version of
 * `{ tmp = *atomic; *atomic ^= val; return tmp; }`.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: the value of @atomic before the operation, unsigned
 *
 * Since: 2.30
 **/
gsize
(g_atomic_pointer_xor) (volatile void *atomic,
                        gsize          val)
{
  return g_atomic_pointer_xor ((volatile gpointer *) atomic, val);
}

#elif defined (G_PLATFORM_WIN32)

#include <windows.h>
#if !defined(_M_AMD64) && !defined (_M_IA64) && !defined(_M_X64) && !(defined _MSC_VER && _MSC_VER <= 1200)
#define InterlockedAnd _InterlockedAnd
#define InterlockedOr _InterlockedOr
#define InterlockedXor _InterlockedXor
#endif

#if !defined (_MSC_VER) || _MSC_VER <= 1200
#include "gmessages.h"
/* Inlined versions for older compiler */
static LONG
_gInterlockedAnd (volatile guint *atomic,
                  guint           val)
{
  LONG i, j;

  j = *atomic;
  do {
    i = j;
    j = InterlockedCompareExchange(atomic, i & val, i);
  } while (i != j);

  return j;
}
#define InterlockedAnd(a,b) _gInterlockedAnd(a,b)
static LONG
_gInterlockedOr (volatile guint *atomic,
                 guint           val)
{
  LONG i, j;

  j = *atomic;
  do {
    i = j;
    j = InterlockedCompareExchange(atomic, i | val, i);
  } while (i != j);

  return j;
}
#define InterlockedOr(a,b) _gInterlockedOr(a,b)
static LONG
_gInterlockedXor (volatile guint *atomic,
                  guint           val)
{
  LONG i, j;

  j = *atomic;
  do {
    i = j;
    j = InterlockedCompareExchange(atomic, i ^ val, i);
  } while (i != j);

  return j;
}
#define InterlockedXor(a,b) _gInterlockedXor(a,b)
#endif

/*
 * http://msdn.microsoft.com/en-us/library/ms684122(v=vs.85).aspx
 */
gint
(g_atomic_int_get) (const volatile gint *atomic)
{
  MemoryBarrier ();
  return *atomic;
}

void
(g_atomic_int_set) (volatile gint *atomic,
                    gint           newval)
{
  *atomic = newval;
  MemoryBarrier ();
}

void
(g_atomic_int_inc) (volatile gint *atomic)
{
  InterlockedIncrement (atomic);
}

gboolean
(g_atomic_int_dec_and_test) (volatile gint *atomic)
{
  return InterlockedDecrement (atomic) == 0;
}

gboolean
(g_atomic_int_compare_and_exchange) (volatile gint *atomic,
                                     gint           oldval,
                                     gint           newval)
{
  return InterlockedCompareExchange (atomic, newval, oldval) == oldval;
}

gint
(g_atomic_int_add) (volatile gint *atomic,
                    gint           val)
{
  return InterlockedExchangeAdd (atomic, val);
}

guint
(g_atomic_int_and) (volatile guint *atomic,
                    guint           val)
{
  return InterlockedAnd (atomic, val);
}

guint
(g_atomic_int_or) (volatile guint *atomic,
                   guint           val)
{
  return InterlockedOr (atomic, val);
}

guint
(g_atomic_int_xor) (volatile guint *atomic,
                    guint           val)
{
  return InterlockedXor (atomic, val);
}


gpointer
(g_atomic_pointer_get) (const volatile void *atomic)
{
  const volatile gpointer *ptr = atomic;

  MemoryBarrier ();
  return *ptr;
}

void
(g_atomic_pointer_set) (volatile void *atomic,
                        gpointer       newval)
{
  volatile gpointer *ptr = atomic;

  *ptr = newval;
  MemoryBarrier ();
}

gboolean
(g_atomic_pointer_compare_and_exchange) (volatile void *atomic,
                                         gpointer       oldval,
                                         gpointer       newval)
{
  return InterlockedCompareExchangePointer (atomic, newval, oldval) == oldval;
}

gssize
(g_atomic_pointer_add) (volatile void *atomic,
                        gssize         val)
{
#if GLIB_SIZEOF_VOID_P == 8
  return InterlockedExchangeAdd64 (atomic, val);
#else
  return InterlockedExchangeAdd (atomic, val);
#endif
}

gsize
(g_atomic_pointer_and) (volatile void *atomic,
                        gsize          val)
{
#if GLIB_SIZEOF_VOID_P == 8
  return InterlockedAnd64 (atomic, val);
#else
  return InterlockedAnd (atomic, val);
#endif
}

gsize
(g_atomic_pointer_or) (volatile void *atomic,
                       gsize          val)
{
#if GLIB_SIZEOF_VOID_P == 8
  return InterlockedOr64 (atomic, val);
#else
  return InterlockedOr (atomic, val);
#endif
}

gsize
(g_atomic_pointer_xor) (volatile void *atomic,
                        gsize          val)
{
#if GLIB_SIZEOF_VOID_P == 8
  return InterlockedXor64 (atomic, val);
#else
  return InterlockedXor (atomic, val);
#endif
}
#else

/* This error occurs when ./configure decided that we should be capable
 * of lock-free atomics but we find at compile-time that we are not.
 */
#error G_ATOMIC_LOCK_FREE defined, but incapable of lock-free atomics.

#endif /* defined (__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) */

#else /* G_ATOMIC_LOCK_FREE */

/* We are not permitted to call into any GLib functions from here, so we
 * can not use GMutex.
 *
 * Fortunately, we already take care of the Windows case above, and all
 * non-Windows platforms on which glib runs have pthreads.  Use those.
 */
#include <pthread.h>

static pthread_mutex_t g_atomic_lock = PTHREAD_MUTEX_INITIALIZER;

gint
(g_atomic_int_get) (const volatile gint *atomic)
{
  gint value;

  pthread_mutex_lock (&g_atomic_lock);
  value = *atomic;
  pthread_mutex_unlock (&g_atomic_lock);

  return value;
}

void
(g_atomic_int_set) (volatile gint *atomic,
                    gint           value)
{
  pthread_mutex_lock (&g_atomic_lock);
  *atomic = value;
  pthread_mutex_unlock (&g_atomic_lock);
}

void
(g_atomic_int_inc) (volatile gint *atomic)
{
  pthread_mutex_lock (&g_atomic_lock);
  (*atomic)++;
  pthread_mutex_unlock (&g_atomic_lock);
}

gboolean
(g_atomic_int_dec_and_test) (volatile gint *atomic)
{
  gboolean is_zero;

  pthread_mutex_lock (&g_atomic_lock);
  is_zero = --(*atomic) == 0;
  pthread_mutex_unlock (&g_atomic_lock);

  return is_zero;
}

gboolean
(g_atomic_int_compare_and_exchange) (volatile gint *atomic,
                                     gint           oldval,
                                     gint           newval)
{
  gboolean success;

  pthread_mutex_lock (&g_atomic_lock);

  if ((success = (*atomic == oldval)))
    *atomic = newval;

  pthread_mutex_unlock (&g_atomic_lock);

  return success;
}

gint
(g_atomic_int_add) (volatile gint *atomic,
                    gint           val)
{
  gint oldval;

  pthread_mutex_lock (&g_atomic_lock);
  oldval = *atomic;
  *atomic = oldval + val;
  pthread_mutex_unlock (&g_atomic_lock);

  return oldval;
}

guint
(g_atomic_int_and) (volatile guint *atomic,
                    guint           val)
{
  guint oldval;

  pthread_mutex_lock (&g_atomic_lock);
  oldval = *atomic;
  *atomic = oldval & val;
  pthread_mutex_unlock (&g_atomic_lock);

  return oldval;
}

guint
(g_atomic_int_or) (volatile guint *atomic,
                   guint           val)
{
  guint oldval;

  pthread_mutex_lock (&g_atomic_lock);
  oldval = *atomic;
  *atomic = oldval | val;
  pthread_mutex_unlock (&g_atomic_lock);

  return oldval;
}

guint
(g_atomic_int_xor) (volatile guint *atomic,
                    guint           val)
{
  guint oldval;

  pthread_mutex_lock (&g_atomic_lock);
  oldval = *atomic;
  *atomic = oldval ^ val;
  pthread_mutex_unlock (&g_atomic_lock);

  return oldval;
}


gpointer
(g_atomic_pointer_get) (const volatile void *atomic)
{
  const volatile gpointer *ptr = atomic;
  gpointer value;

  pthread_mutex_lock (&g_atomic_lock);
  value = *ptr;
  pthread_mutex_unlock (&g_atomic_lock);

  return value;
}

void
(g_atomic_pointer_set) (volatile void *atomic,
                        gpointer       newval)
{
  volatile gpointer *ptr = atomic;

  pthread_mutex_lock (&g_atomic_lock);
  *ptr = newval;
  pthread_mutex_unlock (&g_atomic_lock);
}

gboolean
(g_atomic_pointer_compare_and_exchange) (volatile void *atomic,
                                         gpointer       oldval,
                                         gpointer       newval)
{
  volatile gpointer *ptr = atomic;
  gboolean success;

  pthread_mutex_lock (&g_atomic_lock);

  if ((success = (*ptr == oldval)))
    *ptr = newval;

  pthread_mutex_unlock (&g_atomic_lock);

  return success;
}

gssize
(g_atomic_pointer_add) (volatile void *atomic,
                        gssize         val)
{
  volatile gssize *ptr = atomic;
  gssize oldval;

  pthread_mutex_lock (&g_atomic_lock);
  oldval = *ptr;
  *ptr = oldval + val;
  pthread_mutex_unlock (&g_atomic_lock);

  return oldval;
}

gsize
(g_atomic_pointer_and) (volatile void *atomic,
                        gsize          val)
{
  volatile gsize *ptr = atomic;
  gsize oldval;

  pthread_mutex_lock (&g_atomic_lock);
  oldval = *ptr;
  *ptr = oldval & val;
  pthread_mutex_unlock (&g_atomic_lock);

  return oldval;
}

gsize
(g_atomic_pointer_or) (volatile void *atomic,
                       gsize          val)
{
  volatile gsize *ptr = atomic;
  gsize oldval;

  pthread_mutex_lock (&g_atomic_lock);
  oldval = *ptr;
  *ptr = oldval | val;
  pthread_mutex_unlock (&g_atomic_lock);

  return oldval;
}

gsize
(g_atomic_pointer_xor) (volatile void *atomic,
                        gsize          val)
{
  volatile gsize *ptr = atomic;
  gsize oldval;

  pthread_mutex_lock (&g_atomic_lock);
  oldval = *ptr;
  *ptr = oldval ^ val;
  pthread_mutex_unlock (&g_atomic_lock);

  return oldval;
}

#endif

/**
 * g_atomic_int_exchange_and_add:
 * @atomic: a pointer to a #gint
 * @val: the value to add
 *
 * This function existed before g_atomic_int_add() returned the prior
 * value of the integer (which it now does).  It is retained only for
 * compatibility reasons.  Don't use this function in new code.
 *
 * Returns: the value of @atomic before the add, signed
 * Since: 2.4
 * Deprecated: 2.30: Use g_atomic_int_add() instead.
 **/
gint
g_atomic_int_exchange_and_add (volatile gint *atomic,
                               gint           val)
{
  return (g_atomic_int_add) (atomic, val);
}
