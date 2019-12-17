/*
 * Copyright © 2008 Ryan Lortie
 * Copyright © 2010 Codethink Limited
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
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gbitlock.h"

#include <glib/gmessages.h>
#include <glib/gatomic.h>
#include <glib/gslist.h>
#include <glib/gthread.h>
#include <glib/gslice.h>

#include "gthreadprivate.h"

#ifdef G_BIT_LOCK_FORCE_FUTEX_EMULATION
#undef HAVE_FUTEX
#endif

#ifndef HAVE_FUTEX
static GMutex g_futex_mutex;
static GSList *g_futex_address_list = NULL;
#endif

#ifdef HAVE_FUTEX
/*
 * We have headers for futex(2) on the build machine.  This does not
 * imply that every system that ever runs the resulting glib will have
 * kernel support for futex, but you'd have to have a pretty old
 * kernel in order for that not to be the case.
 *
 * If anyone actually gets bit by this, please file a bug. :)
 */
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef FUTEX_WAIT_PRIVATE
#define FUTEX_WAIT_PRIVATE FUTEX_WAIT
#define FUTEX_WAKE_PRIVATE FUTEX_WAKE
#endif

/* < private >
 * g_futex_wait:
 * @address: a pointer to an integer
 * @value: the value that should be at @address
 *
 * Atomically checks that the value stored at @address is equal to
 * @value and then blocks.  If the value stored at @address is not
 * equal to @value then this function returns immediately.
 *
 * To unblock, call g_futex_wake() on @address.
 *
 * This call may spuriously unblock (for example, in response to the
 * process receiving a signal) but this is not guaranteed.  Unlike the
 * Linux system call of a similar name, there is no guarantee that a
 * waiting process will unblock due to a g_futex_wake() call in a
 * separate process.
 */
static void
g_futex_wait (const volatile gint *address,
              gint                 value)
{
  syscall (__NR_futex, address, (gsize) FUTEX_WAIT_PRIVATE, (gsize) value, NULL);
}

/* < private >
 * g_futex_wake:
 * @address: a pointer to an integer
 *
 * Nominally, wakes one thread that is blocked in g_futex_wait() on
 * @address (if any thread is currently waiting).
 *
 * As mentioned in the documention for g_futex_wait(), spurious
 * wakeups may occur.  As such, this call may result in more than one
 * thread being woken up.
 */
static void
g_futex_wake (const volatile gint *address)
{
  syscall (__NR_futex, address, (gsize) FUTEX_WAKE_PRIVATE, (gsize) 1, NULL);
}

#else

/* emulate futex(2) */
typedef struct
{
  const volatile gint *address;
  gint                 ref_count;
  GCond                wait_queue;
} WaitAddress;

static WaitAddress *
g_futex_find_address (const volatile gint *address)
{
  GSList *node;

  for (node = g_futex_address_list; node; node = node->next)
    {
      WaitAddress *waiter = node->data;

      if (waiter->address == address)
        return waiter;
    }

  return NULL;
}

static void
g_futex_wait (const volatile gint *address,
              gint                 value)
{
  g_mutex_lock (&g_futex_mutex);
  if G_LIKELY (g_atomic_int_get (address) == value)
    {
      WaitAddress *waiter;

      if ((waiter = g_futex_find_address (address)) == NULL)
        {
          waiter = g_slice_new (WaitAddress);
          waiter->address = address;
          g_cond_init (&waiter->wait_queue);
          waiter->ref_count = 0;
          g_futex_address_list =
            g_slist_prepend (g_futex_address_list, waiter);
        }

      waiter->ref_count++;
      g_cond_wait (&waiter->wait_queue, &g_futex_mutex);

      if (!--waiter->ref_count)
        {
          g_futex_address_list =
            g_slist_remove (g_futex_address_list, waiter);
          g_cond_clear (&waiter->wait_queue);
          g_slice_free (WaitAddress, waiter);
        }
    }
  g_mutex_unlock (&g_futex_mutex);
}

static void
g_futex_wake (const volatile gint *address)
{
  WaitAddress *waiter;

  /* need to lock here for two reasons:
   *   1) need to acquire/release lock to ensure waiter is not in
   *      the process of registering a wait
   *   2) need to -stay- locked until the end to ensure a wake()
   *      in another thread doesn't cause 'waiter' to stop existing
   */
  g_mutex_lock (&g_futex_mutex);
  if ((waiter = g_futex_find_address (address)))
    g_cond_signal (&waiter->wait_queue);
  g_mutex_unlock (&g_futex_mutex);
}
#endif

#define CONTENTION_CLASSES 11
static volatile gint g_bit_lock_contended[CONTENTION_CLASSES];

#if (defined (i386) || defined (__amd64__))
  #if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
    #define USE_ASM_GOTO 1
  #endif
#endif

/**
 * g_bit_lock:
 * @address: a pointer to an integer
 * @lock_bit: a bit value between 0 and 31
 *
 * Sets the indicated @lock_bit in @address.  If the bit is already
 * set, this call will block until g_bit_unlock() unsets the
 * corresponding bit.
 *
 * Attempting to lock on two different bits within the same integer is
 * not supported and will very probably cause deadlocks.
 *
 * The value of the bit that is set is (1u << @bit).  If @bit is not
 * between 0 and 31 then the result is undefined.
 *
 * This function accesses @address atomically.  All other accesses to
 * @address must be atomic in order for this function to work
 * reliably.
 *
 * Since: 2.24
 **/
void
g_bit_lock (volatile gint *address,
            gint           lock_bit)
{
#ifdef USE_ASM_GOTO
 retry:
  __asm__ volatile goto ("lock bts %1, (%0)\n"
                         "jc %l[contended]"
                         : /* no output */
                         : "r" (address), "r" (lock_bit)
                         : "cc", "memory"
                         : contended);
  return;

 contended:
  {
    guint mask = 1u << lock_bit;
    guint v;

    v = g_atomic_int_get (address);
    if (v & mask)
      {
        guint class = ((gsize) address) % G_N_ELEMENTS (g_bit_lock_contended);

        g_atomic_int_add (&g_bit_lock_contended[class], +1);
        g_futex_wait (address, v);
        g_atomic_int_add (&g_bit_lock_contended[class], -1);
      }
  }
  goto retry;
#else
  guint mask = 1u << lock_bit;
  guint v;

 retry:
  v = g_atomic_int_or (address, mask);
  if (v & mask)
    /* already locked */
    {
      guint class = ((gsize) address) % G_N_ELEMENTS (g_bit_lock_contended);

      g_atomic_int_add (&g_bit_lock_contended[class], +1);
      g_futex_wait (address, v);
      g_atomic_int_add (&g_bit_lock_contended[class], -1);

      goto retry;
    }
#endif
}

/**
 * g_bit_trylock:
 * @address: a pointer to an integer
 * @lock_bit: a bit value between 0 and 31
 *
 * Sets the indicated @lock_bit in @address, returning %TRUE if
 * successful.  If the bit is already set, returns %FALSE immediately.
 *
 * Attempting to lock on two different bits within the same integer is
 * not supported.
 *
 * The value of the bit that is set is (1u << @bit).  If @bit is not
 * between 0 and 31 then the result is undefined.
 *
 * This function accesses @address atomically.  All other accesses to
 * @address must be atomic in order for this function to work
 * reliably.
 *
 * Returns: %TRUE if the lock was acquired
 *
 * Since: 2.24
 **/
gboolean
g_bit_trylock (volatile gint *address,
               gint           lock_bit)
{
#ifdef USE_ASM_GOTO
  gboolean result;

  __asm__ volatile ("lock bts %2, (%1)\n"
                    "setnc %%al\n"
                    "movzx %%al, %0"
                    : "=r" (result)
                    : "r" (address), "r" (lock_bit)
                    : "cc", "memory");

  return result;
#else
  guint mask = 1u << lock_bit;
  guint v;

  v = g_atomic_int_or (address, mask);

  return ~v & mask;
#endif
}

/**
 * g_bit_unlock:
 * @address: a pointer to an integer
 * @lock_bit: a bit value between 0 and 31
 *
 * Clears the indicated @lock_bit in @address.  If another thread is
 * currently blocked in g_bit_lock() on this same bit then it will be
 * woken up.
 *
 * This function accesses @address atomically.  All other accesses to
 * @address must be atomic in order for this function to work
 * reliably.
 *
 * Since: 2.24
 **/
void
g_bit_unlock (volatile gint *address,
              gint           lock_bit)
{
#ifdef USE_ASM_GOTO
  asm volatile ("lock btr %1, (%0)"
                : /* no output */
                : "r" (address), "r" (lock_bit)
                : "cc", "memory");
#else
  guint mask = 1u << lock_bit;

  g_atomic_int_and (address, ~mask);
#endif

  {
    guint class = ((gsize) address) % G_N_ELEMENTS (g_bit_lock_contended);

    if (g_atomic_int_get (&g_bit_lock_contended[class]))
      g_futex_wake (address);
  }
}


/* We emulate pointer-sized futex(2) because the kernel API only
 * supports integers.
 *
 * We assume that the 'interesting' part is always the lower order bits.
 * This assumption holds because pointer bitlocks are restricted to
 * using the low order bits of the pointer as the lock.
 *
 * On 32 bits, there is nothing to do since the pointer size is equal to
 * the integer size.  On little endian the lower-order bits don't move,
 * so do nothing.  Only on 64bit big endian do we need to do a bit of
 * pointer arithmetic: the low order bits are shifted by 4 bytes.  We
 * have a helper function that always does the right thing here.
 *
 * Since we always consider the low-order bits of the integer value, a
 * simple cast from (gsize) to (guint) always takes care of that.
 *
 * After that, pointer-sized futex becomes as simple as:
 *
 *   g_futex_wait (g_futex_int_address (address), (guint) value);
 *
 * and
 *
 *   g_futex_wake (g_futex_int_address (int_address));
 */
static const volatile gint *
g_futex_int_address (const volatile void *address)
{
  const volatile gint *int_address = address;

  /* this implementation makes these (reasonable) assumptions: */
  G_STATIC_ASSERT (G_BYTE_ORDER == G_LITTLE_ENDIAN ||
      (G_BYTE_ORDER == G_BIG_ENDIAN &&
       sizeof (int) == 4 &&
       (sizeof (gpointer) == 4 || sizeof (gpointer) == 8)));

#if G_BYTE_ORDER == G_BIG_ENDIAN && GLIB_SIZEOF_VOID_P == 8
  int_address++;
#endif

  return int_address;
}

/**
 * g_pointer_bit_lock:
 * @address: (not nullable): a pointer to a #gpointer-sized value
 * @lock_bit: a bit value between 0 and 31
 *
 * This is equivalent to g_bit_lock, but working on pointers (or other
 * pointer-sized values).
 *
 * For portability reasons, you may only lock on the bottom 32 bits of
 * the pointer.
 *
 * Since: 2.30
 **/
void
(g_pointer_bit_lock) (volatile void *address,
                      gint           lock_bit)
{
  g_return_if_fail (lock_bit < 32);

  {
#ifdef USE_ASM_GOTO
 retry:
    asm volatile goto ("lock bts %1, (%0)\n"
                       "jc %l[contended]"
                       : /* no output */
                       : "r" (address), "r" ((gsize) lock_bit)
                       : "cc", "memory"
                       : contended);
    return;

 contended:
    {
      volatile gsize *pointer_address = address;
      gsize mask = 1u << lock_bit;
      gsize v;

      v = (gsize) g_atomic_pointer_get (pointer_address);
      if (v & mask)
        {
          guint class = ((gsize) address) % G_N_ELEMENTS (g_bit_lock_contended);

          g_atomic_int_add (&g_bit_lock_contended[class], +1);
          g_futex_wait (g_futex_int_address (address), v);
          g_atomic_int_add (&g_bit_lock_contended[class], -1);
        }
    }
    goto retry;
#else
  volatile gsize *pointer_address = address;
  gsize mask = 1u << lock_bit;
  gsize v;

 retry:
  v = g_atomic_pointer_or (pointer_address, mask);
  if (v & mask)
    /* already locked */
    {
      guint class = ((gsize) address) % G_N_ELEMENTS (g_bit_lock_contended);

      g_atomic_int_add (&g_bit_lock_contended[class], +1);
      g_futex_wait (g_futex_int_address (address), (guint) v);
      g_atomic_int_add (&g_bit_lock_contended[class], -1);

      goto retry;
    }
#endif
  }
}

/**
 * g_pointer_bit_trylock:
 * @address: (not nullable): a pointer to a #gpointer-sized value
 * @lock_bit: a bit value between 0 and 31
 *
 * This is equivalent to g_bit_trylock, but working on pointers (or
 * other pointer-sized values).
 *
 * For portability reasons, you may only lock on the bottom 32 bits of
 * the pointer.
 *
 * Returns: %TRUE if the lock was acquired
 *
 * Since: 2.30
 **/
gboolean
(g_pointer_bit_trylock) (volatile void *address,
                         gint           lock_bit)
{
  g_return_val_if_fail (lock_bit < 32, FALSE);

  {
#ifdef USE_ASM_GOTO
    gboolean result;

    asm volatile ("lock bts %2, (%1)\n"
                  "setnc %%al\n"
                  "movzx %%al, %0"
                  : "=r" (result)
                  : "r" (address), "r" ((gsize) lock_bit)
                  : "cc", "memory");

    return result;
#else
    volatile gsize *pointer_address = address;
    gsize mask = 1u << lock_bit;
    gsize v;

    g_return_val_if_fail (lock_bit < 32, FALSE);

    v = g_atomic_pointer_or (pointer_address, mask);

    return ~v & mask;
#endif
  }
}

/**
 * g_pointer_bit_unlock:
 * @address: (not nullable): a pointer to a #gpointer-sized value
 * @lock_bit: a bit value between 0 and 31
 *
 * This is equivalent to g_bit_unlock, but working on pointers (or other
 * pointer-sized values).
 *
 * For portability reasons, you may only lock on the bottom 32 bits of
 * the pointer.
 *
 * Since: 2.30
 **/
void
(g_pointer_bit_unlock) (volatile void *address,
                        gint           lock_bit)
{
  g_return_if_fail (lock_bit < 32);

  {
#ifdef USE_ASM_GOTO
    asm volatile ("lock btr %1, (%0)"
                  : /* no output */
                  : "r" (address), "r" ((gsize) lock_bit)
                  : "cc", "memory");
#else
    volatile gsize *pointer_address = address;
    gsize mask = 1u << lock_bit;

    g_atomic_pointer_and (pointer_address, ~mask);
#endif

    {
      guint class = ((gsize) address) % G_N_ELEMENTS (g_bit_lock_contended);
      if (g_atomic_int_get (&g_bit_lock_contended[class]))
        g_futex_wake (g_futex_int_address (address));
    }
  }
}
