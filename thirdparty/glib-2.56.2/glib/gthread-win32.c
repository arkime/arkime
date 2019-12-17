/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: solaris thread system implementation
 * Copyright 1998-2001 Sebastian Wilhelmi; University of Karlsruhe
 * Copyright 2001 Hans Breuer
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

/* The GMutex and GCond implementations in this file are some of the
 * lowest-level code in GLib.  All other parts of GLib (messages,
 * memory, slices, etc) assume that they can freely use these facilities
 * without risking recursion.
 *
 * As such, these functions are NOT permitted to call any other part of
 * GLib.
 *
 * The thread manipulation functions (create, exit, join, etc.) have
 * more freedom -- they can do as they please.
 */

#include "config.h"

#include "glib.h"
#include "glib-init.h"
#include "gthread.h"
#include "gthreadprivate.h"
#include "gslice.h"

#include <windows.h>

#include <process.h>
#include <stdlib.h>
#include <stdio.h>

static void
g_thread_abort (gint         status,
                const gchar *function)
{
  fprintf (stderr, "GLib (gthread-win32.c): Unexpected error from C library during '%s': %s.  Aborting.\n",
           strerror (status), function);
  g_abort ();
}

/* Starting with Vista and Windows 2008, we have access to the
 * CONDITION_VARIABLE and SRWLock primatives on Windows, which are
 * pretty reasonable approximations of the primatives specified in
 * POSIX 2001 (pthread_cond_t and pthread_mutex_t respectively).
 *
 * Both of these types are structs containing a single pointer.  That
 * pointer is used as an atomic bitfield to support user-space mutexes
 * that only get the kernel involved in cases of contention (similar
 * to how futex()-based mutexes work on Linux).  The biggest advantage
 * of these new types is that they can be statically initialised to
 * zero.  That means that they are completely ABI compatible with our
 * GMutex and GCond APIs.
 *
 * Unfortunately, Windows XP lacks these facilities and GLib still
 * needs to support Windows XP.  Our approach here is as follows:
 *
 *   - avoid depending on structure declarations at compile-time by
 *     declaring our own GMutex and GCond strutures to be
 *     ABI-compatible with SRWLock and CONDITION_VARIABLE and using
 *     those instead
 *
 *   - avoid a hard dependency on the symbols used to manipulate these
 *     structures by doing a dynamic lookup of those symbols at
 *     runtime
 *
 *   - if the symbols are not available, emulate them using other
 *     primatives
 *
 * Using this approach also allows us to easily build a GLib that lacks
 * support for Windows XP or to remove this code entirely when XP is no
 * longer supported (end of line is currently April 8, 2014).
 */
typedef struct
{
  void     (__stdcall * CallThisOnThreadExit)        (void);              /* fake */

  void     (__stdcall * InitializeSRWLock)           (gpointer lock);
  void     (__stdcall * DeleteSRWLock)               (gpointer lock);     /* fake */
  void     (__stdcall * AcquireSRWLockExclusive)     (gpointer lock);
  BOOLEAN  (__stdcall * TryAcquireSRWLockExclusive)  (gpointer lock);
  void     (__stdcall * ReleaseSRWLockExclusive)     (gpointer lock);
  void     (__stdcall * AcquireSRWLockShared)        (gpointer lock);
  BOOLEAN  (__stdcall * TryAcquireSRWLockShared)     (gpointer lock);
  void     (__stdcall * ReleaseSRWLockShared)        (gpointer lock);

  void     (__stdcall * InitializeConditionVariable) (gpointer cond);
  void     (__stdcall * DeleteConditionVariable)     (gpointer cond);     /* fake */
  BOOL     (__stdcall * SleepConditionVariableSRW)   (gpointer cond,
                                                      gpointer lock,
                                                      DWORD    timeout,
                                                      ULONG    flags);
  void     (__stdcall * WakeAllConditionVariable)    (gpointer cond);
  void     (__stdcall * WakeConditionVariable)       (gpointer cond);
} GThreadImplVtable;

static GThreadImplVtable g_thread_impl_vtable;

/* {{{1 GMutex */
void
g_mutex_init (GMutex *mutex)
{
  g_thread_impl_vtable.InitializeSRWLock (mutex);
}

void
g_mutex_clear (GMutex *mutex)
{
  if (g_thread_impl_vtable.DeleteSRWLock != NULL)
    g_thread_impl_vtable.DeleteSRWLock (mutex);
}

void
g_mutex_lock (GMutex *mutex)
{
  g_thread_impl_vtable.AcquireSRWLockExclusive (mutex);
}

gboolean
g_mutex_trylock (GMutex *mutex)
{
  return g_thread_impl_vtable.TryAcquireSRWLockExclusive (mutex);
}

void
g_mutex_unlock (GMutex *mutex)
{
  g_thread_impl_vtable.ReleaseSRWLockExclusive (mutex);
}

/* {{{1 GRecMutex */

static CRITICAL_SECTION *
g_rec_mutex_impl_new (void)
{
  CRITICAL_SECTION *cs;

  cs = g_slice_new (CRITICAL_SECTION);
  InitializeCriticalSection (cs);

  return cs;
}

static void
g_rec_mutex_impl_free (CRITICAL_SECTION *cs)
{
  DeleteCriticalSection (cs);
  g_slice_free (CRITICAL_SECTION, cs);
}

static CRITICAL_SECTION *
g_rec_mutex_get_impl (GRecMutex *mutex)
{
  CRITICAL_SECTION *impl = mutex->p;

  if G_UNLIKELY (mutex->p == NULL)
    {
      impl = g_rec_mutex_impl_new ();
      if (InterlockedCompareExchangePointer (&mutex->p, impl, NULL) != NULL)
        g_rec_mutex_impl_free (impl);
      impl = mutex->p;
    }

  return impl;
}

void
g_rec_mutex_init (GRecMutex *mutex)
{
  mutex->p = g_rec_mutex_impl_new ();
}

void
g_rec_mutex_clear (GRecMutex *mutex)
{
  g_rec_mutex_impl_free (mutex->p);
}

void
g_rec_mutex_lock (GRecMutex *mutex)
{
  EnterCriticalSection (g_rec_mutex_get_impl (mutex));
}

void
g_rec_mutex_unlock (GRecMutex *mutex)
{
  LeaveCriticalSection (mutex->p);
}

gboolean
g_rec_mutex_trylock (GRecMutex *mutex)
{
  return TryEnterCriticalSection (g_rec_mutex_get_impl (mutex));
}

/* {{{1 GRWLock */

void
g_rw_lock_init (GRWLock *lock)
{
  g_thread_impl_vtable.InitializeSRWLock (lock);
}

void
g_rw_lock_clear (GRWLock *lock)
{
  if (g_thread_impl_vtable.DeleteSRWLock != NULL)
    g_thread_impl_vtable.DeleteSRWLock (lock);
}

void
g_rw_lock_writer_lock (GRWLock *lock)
{
  g_thread_impl_vtable.AcquireSRWLockExclusive (lock);
}

gboolean
g_rw_lock_writer_trylock (GRWLock *lock)
{
  return g_thread_impl_vtable.TryAcquireSRWLockExclusive (lock);
}

void
g_rw_lock_writer_unlock (GRWLock *lock)
{
  g_thread_impl_vtable.ReleaseSRWLockExclusive (lock);
}

void
g_rw_lock_reader_lock (GRWLock *lock)
{
  g_thread_impl_vtable.AcquireSRWLockShared (lock);
}

gboolean
g_rw_lock_reader_trylock (GRWLock *lock)
{
  return g_thread_impl_vtable.TryAcquireSRWLockShared (lock);
}

void
g_rw_lock_reader_unlock (GRWLock *lock)
{
  g_thread_impl_vtable.ReleaseSRWLockShared (lock);
}

/* {{{1 GCond */
void
g_cond_init (GCond *cond)
{
  g_thread_impl_vtable.InitializeConditionVariable (cond);
}

void
g_cond_clear (GCond *cond)
{
  if (g_thread_impl_vtable.DeleteConditionVariable)
    g_thread_impl_vtable.DeleteConditionVariable (cond);
}

void
g_cond_signal (GCond *cond)
{
  g_thread_impl_vtable.WakeConditionVariable (cond);
}

void
g_cond_broadcast (GCond *cond)
{
  g_thread_impl_vtable.WakeAllConditionVariable (cond);
}

void
g_cond_wait (GCond  *cond,
             GMutex *entered_mutex)
{
  g_thread_impl_vtable.SleepConditionVariableSRW (cond, entered_mutex, INFINITE, 0);
}

gboolean
g_cond_wait_until (GCond  *cond,
                   GMutex *entered_mutex,
                   gint64  end_time)
{
  gint64 span;

  span = end_time - g_get_monotonic_time ();

  if G_UNLIKELY (span < 0)
    span = 0;

  if G_UNLIKELY (span > G_GINT64_CONSTANT (1000) * G_MAXINT32)
    span = INFINITE;

  return g_thread_impl_vtable.SleepConditionVariableSRW (cond, entered_mutex, span / 1000, 0);
}

/* {{{1 GPrivate */

typedef struct _GPrivateDestructor GPrivateDestructor;

struct _GPrivateDestructor
{
  DWORD               index;
  GDestroyNotify      notify;
  GPrivateDestructor *next;
};

static GPrivateDestructor * volatile g_private_destructors;
static CRITICAL_SECTION g_private_lock;

static DWORD
g_private_get_impl (GPrivate *key)
{
  DWORD impl = (DWORD) key->p;

  if G_UNLIKELY (impl == 0)
    {
      EnterCriticalSection (&g_private_lock);
      impl = (DWORD) key->p;
      if (impl == 0)
        {
          GPrivateDestructor *destructor;

          impl = TlsAlloc ();

          if (impl == TLS_OUT_OF_INDEXES)
            g_thread_abort (0, "TlsAlloc");

          if (key->notify != NULL)
            {
              destructor = malloc (sizeof (GPrivateDestructor));
              if G_UNLIKELY (destructor == NULL)
                g_thread_abort (errno, "malloc");
              destructor->index = impl;
              destructor->notify = key->notify;
              destructor->next = g_private_destructors;

              /* We need to do an atomic store due to the unlocked
               * access to the destructor list from the thread exit
               * function.
               *
               * It can double as a sanity check...
               */
              if (InterlockedCompareExchangePointer (&g_private_destructors, destructor,
                                                     destructor->next) != destructor->next)
                g_thread_abort (0, "g_private_get_impl(1)");
            }

          /* Ditto, due to the unlocked access on the fast path */
          if (InterlockedCompareExchangePointer (&key->p, impl, NULL) != NULL)
            g_thread_abort (0, "g_private_get_impl(2)");
        }
      LeaveCriticalSection (&g_private_lock);
    }

  return impl;
}

gpointer
g_private_get (GPrivate *key)
{
  return TlsGetValue (g_private_get_impl (key));
}

void
g_private_set (GPrivate *key,
               gpointer  value)
{
  TlsSetValue (g_private_get_impl (key), value);
}

void
g_private_replace (GPrivate *key,
                   gpointer  value)
{
  DWORD impl = g_private_get_impl (key);
  gpointer old;

  old = TlsGetValue (impl);
  if (old && key->notify)
    key->notify (old);
  TlsSetValue (impl, value);
}

/* {{{1 GThread */

#define win32_check_for_error(what) G_STMT_START{			\
  if (!(what))								\
    g_error ("file %s: line %d (%s): error %s during %s",		\
	     __FILE__, __LINE__, G_STRFUNC,				\
	     g_win32_error_message (GetLastError ()), #what);		\
  }G_STMT_END

#define G_MUTEX_SIZE (sizeof (gpointer))

typedef BOOL (__stdcall *GTryEnterCriticalSectionFunc) (CRITICAL_SECTION *);

typedef struct
{
  GRealThread thread;

  GThreadFunc proxy;
  HANDLE      handle;
} GThreadWin32;

void
g_system_thread_free (GRealThread *thread)
{
  GThreadWin32 *wt = (GThreadWin32 *) thread;

  win32_check_for_error (CloseHandle (wt->handle));
  g_slice_free (GThreadWin32, wt);
}

void
g_system_thread_exit (void)
{
  _endthreadex (0);
}

static guint __stdcall
g_thread_win32_proxy (gpointer data)
{
  GThreadWin32 *self = data;

  self->proxy (self);

  g_system_thread_exit ();

  g_assert_not_reached ();

  return 0;
}

GRealThread *
g_system_thread_new (GThreadFunc   func,
                     gulong        stack_size,
                     GError      **error)
{
  GThreadWin32 *thread;
  guint ignore;

  thread = g_slice_new0 (GThreadWin32);
  thread->proxy = func;

  thread->handle = (HANDLE) _beginthreadex (NULL, stack_size, g_thread_win32_proxy, thread, 0, &ignore);

  if (thread->handle == NULL)
    {
      gchar *win_error = g_win32_error_message (GetLastError ());
      g_set_error (error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN,
                   "Error creating thread: %s", win_error);
      g_free (win_error);
      g_slice_free (GThreadWin32, thread);
      return NULL;
    }

  return (GRealThread *) thread;
}

void
g_thread_yield (void)
{
  Sleep(0);
}

void
g_system_thread_wait (GRealThread *thread)
{
  GThreadWin32 *wt = (GThreadWin32 *) thread;

  win32_check_for_error (WAIT_FAILED != WaitForSingleObject (wt->handle, INFINITE));
}

#define EXCEPTION_SET_THREAD_NAME ((DWORD) 0x406D1388)

#ifndef _MSC_VER
static void *SetThreadName_VEH_handle = NULL;

static LONG __stdcall
SetThreadName_VEH (PEXCEPTION_POINTERS ExceptionInfo)
{
  if (ExceptionInfo->ExceptionRecord != NULL &&
      ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SET_THREAD_NAME)
    return EXCEPTION_CONTINUE_EXECUTION;

  return EXCEPTION_CONTINUE_SEARCH;
}
#endif

typedef struct _THREADNAME_INFO
{
  DWORD  dwType;	/* must be 0x1000 */
  LPCSTR szName;	/* pointer to name (in user addr space) */
  DWORD  dwThreadID;	/* thread ID (-1=caller thread) */
  DWORD  dwFlags;	/* reserved for future use, must be zero */
} THREADNAME_INFO;

static void
SetThreadName (DWORD  dwThreadID,
               LPCSTR szThreadName)
{
   THREADNAME_INFO info;
   DWORD infosize;

   info.dwType = 0x1000;
   info.szName = szThreadName;
   info.dwThreadID = dwThreadID;
   info.dwFlags = 0;

   infosize = sizeof (info) / sizeof (DWORD);

#ifdef _MSC_VER
   __try
     {
       RaiseException (EXCEPTION_SET_THREAD_NAME, 0, infosize, (DWORD *) &info);
     }
   __except (EXCEPTION_EXECUTE_HANDLER)
     {
     }
#else
   /* Without a debugger we *must* have an exception handler,
    * otherwise raising an exception will crash the process.
    */
   if ((!IsDebuggerPresent ()) && (SetThreadName_VEH_handle == NULL))
     return;

   RaiseException (EXCEPTION_SET_THREAD_NAME, 0, infosize, (DWORD *) &info);
#endif
}

void
g_system_thread_set_name (const gchar *name)
{
  SetThreadName ((DWORD) -1, name);
}

/* {{{1 SRWLock and CONDITION_VARIABLE emulation (for Windows XP) */

static CRITICAL_SECTION g_thread_xp_lock;
static DWORD            g_thread_xp_waiter_tls;

/* {{{2 GThreadWaiter utility class for CONDITION_VARIABLE emulation */
typedef struct _GThreadXpWaiter GThreadXpWaiter;
struct _GThreadXpWaiter
{
  HANDLE                     event;
  volatile GThreadXpWaiter  *next;
  volatile GThreadXpWaiter **my_owner;
};

static GThreadXpWaiter *
g_thread_xp_waiter_get (void)
{
  GThreadXpWaiter *waiter;

  waiter = TlsGetValue (g_thread_xp_waiter_tls);

  if G_UNLIKELY (waiter == NULL)
    {
      waiter = malloc (sizeof (GThreadXpWaiter));
      if (waiter == NULL)
        g_thread_abort (GetLastError (), "malloc");
      waiter->event = CreateEvent (0, FALSE, FALSE, NULL);
      if (waiter->event == NULL)
        g_thread_abort (GetLastError (), "CreateEvent");
      waiter->my_owner = NULL;

      TlsSetValue (g_thread_xp_waiter_tls, waiter);
    }

  return waiter;
}

static void __stdcall
g_thread_xp_CallThisOnThreadExit (void)
{
  GThreadXpWaiter *waiter;

  waiter = TlsGetValue (g_thread_xp_waiter_tls);

  if (waiter != NULL)
    {
      TlsSetValue (g_thread_xp_waiter_tls, NULL);
      CloseHandle (waiter->event);
      free (waiter);
    }
}

/* {{{2 SRWLock emulation */
typedef struct
{
  CRITICAL_SECTION  writer_lock;
  gboolean          ever_shared;    /* protected by writer_lock */
  gboolean          writer_locked;  /* protected by writer_lock */

  /* below is only ever touched if ever_shared becomes true */
  CRITICAL_SECTION  atomicity;
  GThreadXpWaiter  *queued_writer; /* protected by atomicity lock */
  gint              num_readers;   /* protected by atomicity lock */
} GThreadSRWLock;

static void __stdcall
g_thread_xp_InitializeSRWLock (gpointer mutex)
{
  *(GThreadSRWLock * volatile *) mutex = NULL;
}

static void __stdcall
g_thread_xp_DeleteSRWLock (gpointer mutex)
{
  GThreadSRWLock *lock = *(GThreadSRWLock * volatile *) mutex;

  if (lock)
    {
      if (lock->ever_shared)
        DeleteCriticalSection (&lock->atomicity);

      DeleteCriticalSection (&lock->writer_lock);
      free (lock);
    }
}

static GThreadSRWLock * __stdcall
g_thread_xp_get_srwlock (GThreadSRWLock * volatile *lock)
{
  GThreadSRWLock *result;

  /* It looks like we're missing some barriers here, but this code only
   * ever runs on Windows XP, which in turn only ever runs on hardware
   * with a relatively rigid memory model.  The 'volatile' will take
   * care of the compiler.
   */
  result = *lock;

  if G_UNLIKELY (result == NULL)
    {
      EnterCriticalSection (&g_thread_xp_lock);

      /* Check again */
      result = *lock;
      if (result == NULL)
        {
          result = malloc (sizeof (GThreadSRWLock));

          if (result == NULL)
            g_thread_abort (errno, "malloc");

          InitializeCriticalSection (&result->writer_lock);
          result->writer_locked = FALSE;
          result->ever_shared = FALSE;
          *lock = result;
        }

      LeaveCriticalSection (&g_thread_xp_lock);
    }

  return result;
}

static void __stdcall
g_thread_xp_AcquireSRWLockExclusive (gpointer mutex)
{
  GThreadSRWLock *lock = g_thread_xp_get_srwlock (mutex);

  EnterCriticalSection (&lock->writer_lock);

  /* CRITICAL_SECTION is reentrant, but SRWLock is not.
   * Detect the deadlock that would occur on later Windows version.
   */
  g_assert (!lock->writer_locked);
  lock->writer_locked = TRUE;

  if (lock->ever_shared)
    {
      GThreadXpWaiter *waiter = NULL;

      EnterCriticalSection (&lock->atomicity);
      if (lock->num_readers > 0)
        lock->queued_writer = waiter = g_thread_xp_waiter_get ();
      LeaveCriticalSection (&lock->atomicity);

      if (waiter != NULL)
        WaitForSingleObject (waiter->event, INFINITE);

      lock->queued_writer = NULL;
    }
}

static BOOLEAN __stdcall
g_thread_xp_TryAcquireSRWLockExclusive (gpointer mutex)
{
  GThreadSRWLock *lock = g_thread_xp_get_srwlock (mutex);

  if (!TryEnterCriticalSection (&lock->writer_lock))
    return FALSE;

  /* CRITICAL_SECTION is reentrant, but SRWLock is not.
   * Ensure that this properly returns FALSE (as SRWLock would).
   */
  if G_UNLIKELY (lock->writer_locked)
    {
      LeaveCriticalSection (&lock->writer_lock);
      return FALSE;
    }

  lock->writer_locked = TRUE;

  if (lock->ever_shared)
    {
      gboolean available;

      EnterCriticalSection (&lock->atomicity);
      available = lock->num_readers == 0;
      LeaveCriticalSection (&lock->atomicity);

      if (!available)
        {
          LeaveCriticalSection (&lock->writer_lock);
          return FALSE;
        }
    }

  return TRUE;
}

static void __stdcall
g_thread_xp_ReleaseSRWLockExclusive (gpointer mutex)
{
  GThreadSRWLock *lock = *(GThreadSRWLock * volatile *) mutex;

  lock->writer_locked = FALSE;

  /* We need this until we fix some weird parts of GLib that try to
   * unlock freshly-allocated mutexes.
   */
  if (lock != NULL)
    LeaveCriticalSection (&lock->writer_lock);
}

static void
g_thread_xp_srwlock_become_reader (GThreadSRWLock *lock)
{
  if G_UNLIKELY (!lock->ever_shared)
    {
      InitializeCriticalSection (&lock->atomicity);
      lock->queued_writer = NULL;
      lock->num_readers = 0;

      lock->ever_shared = TRUE;
    }

  EnterCriticalSection (&lock->atomicity);
  lock->num_readers++;
  LeaveCriticalSection (&lock->atomicity);
}

static void __stdcall
g_thread_xp_AcquireSRWLockShared (gpointer mutex)
{
  GThreadSRWLock *lock = g_thread_xp_get_srwlock (mutex);

  EnterCriticalSection (&lock->writer_lock);

  /* See g_thread_xp_AcquireSRWLockExclusive */
  g_assert (!lock->writer_locked);

  g_thread_xp_srwlock_become_reader (lock);

  LeaveCriticalSection (&lock->writer_lock);
}

static BOOLEAN __stdcall
g_thread_xp_TryAcquireSRWLockShared (gpointer mutex)
{
  GThreadSRWLock *lock = g_thread_xp_get_srwlock (mutex);

  if (!TryEnterCriticalSection (&lock->writer_lock))
    return FALSE;

  /* See g_thread_xp_AcquireSRWLockExclusive */
  if G_UNLIKELY (lock->writer_locked)
    {
      LeaveCriticalSection (&lock->writer_lock);
      return FALSE;
    }

  g_thread_xp_srwlock_become_reader (lock);

  LeaveCriticalSection (&lock->writer_lock);

  return TRUE;
}

static void __stdcall
g_thread_xp_ReleaseSRWLockShared (gpointer mutex)
{
  GThreadSRWLock *lock = g_thread_xp_get_srwlock (mutex);

  EnterCriticalSection (&lock->atomicity);

  lock->num_readers--;

  if (lock->num_readers == 0 && lock->queued_writer)
    SetEvent (lock->queued_writer->event);

  LeaveCriticalSection (&lock->atomicity);
}

/* {{{2 CONDITION_VARIABLE emulation */
typedef struct
{
  volatile GThreadXpWaiter  *first;
  volatile GThreadXpWaiter **last_ptr;
} GThreadXpCONDITION_VARIABLE;

static void __stdcall
g_thread_xp_InitializeConditionVariable (gpointer cond)
{
  *(GThreadXpCONDITION_VARIABLE * volatile *) cond = NULL;
}

static void __stdcall
g_thread_xp_DeleteConditionVariable (gpointer cond)
{
  GThreadXpCONDITION_VARIABLE *cv = *(GThreadXpCONDITION_VARIABLE * volatile *) cond;

  if (cv)
    free (cv);
}

static GThreadXpCONDITION_VARIABLE * __stdcall
g_thread_xp_get_condition_variable (GThreadXpCONDITION_VARIABLE * volatile *cond)
{
  GThreadXpCONDITION_VARIABLE *result;

  /* It looks like we're missing some barriers here, but this code only
   * ever runs on Windows XP, which in turn only ever runs on hardware
   * with a relatively rigid memory model.  The 'volatile' will take
   * care of the compiler.
   */
  result = *cond;

  if G_UNLIKELY (result == NULL)
    {
      result = malloc (sizeof (GThreadXpCONDITION_VARIABLE));

      if (result == NULL)
        g_thread_abort (errno, "malloc");

      result->first = NULL;
      result->last_ptr = &result->first;

      if (InterlockedCompareExchangePointer (cond, result, NULL) != NULL)
        {
          free (result);
          result = *cond;
        }
    }

  return result;
}

static BOOL __stdcall
g_thread_xp_SleepConditionVariableSRW (gpointer cond,
                                       gpointer mutex,
                                       DWORD    timeout,
                                       ULONG    flags)
{
  GThreadXpCONDITION_VARIABLE *cv = g_thread_xp_get_condition_variable (cond);
  GThreadXpWaiter *waiter = g_thread_xp_waiter_get ();
  DWORD status;

  waiter->next = NULL;

  EnterCriticalSection (&g_thread_xp_lock);
  waiter->my_owner = cv->last_ptr;
  *cv->last_ptr = waiter;
  cv->last_ptr = &waiter->next;
  LeaveCriticalSection (&g_thread_xp_lock);

  g_mutex_unlock (mutex);
  status = WaitForSingleObject (waiter->event, timeout);

  if (status != WAIT_TIMEOUT && status != WAIT_OBJECT_0)
    g_thread_abort (GetLastError (), "WaitForSingleObject");
  g_mutex_lock (mutex);

  if (status == WAIT_TIMEOUT)
    {
      EnterCriticalSection (&g_thread_xp_lock);
      if (waiter->my_owner)
        {
          if (waiter->next)
            waiter->next->my_owner = waiter->my_owner;
          else
            cv->last_ptr = waiter->my_owner;
          *waiter->my_owner = waiter->next;
          waiter->my_owner = NULL;
        }
      LeaveCriticalSection (&g_thread_xp_lock);
    }

  return status == WAIT_OBJECT_0;
}

static void __stdcall
g_thread_xp_WakeConditionVariable (gpointer cond)
{
  GThreadXpCONDITION_VARIABLE *cv = g_thread_xp_get_condition_variable (cond);
  volatile GThreadXpWaiter *waiter;

  EnterCriticalSection (&g_thread_xp_lock);

  waiter = cv->first;
  if (waiter != NULL)
    {
      waiter->my_owner = NULL;
      cv->first = waiter->next;
      if (cv->first != NULL)
        cv->first->my_owner = &cv->first;
      else
        cv->last_ptr = &cv->first;
    }

  if (waiter != NULL)
    SetEvent (waiter->event);

  LeaveCriticalSection (&g_thread_xp_lock);
}

static void __stdcall
g_thread_xp_WakeAllConditionVariable (gpointer cond)
{
  GThreadXpCONDITION_VARIABLE *cv = g_thread_xp_get_condition_variable (cond);
  volatile GThreadXpWaiter *waiter;

  EnterCriticalSection (&g_thread_xp_lock);

  waiter = cv->first;
  cv->first = NULL;
  cv->last_ptr = &cv->first;

  while (waiter != NULL)
    {
      volatile GThreadXpWaiter *next;

      next = waiter->next;
      SetEvent (waiter->event);
      waiter->my_owner = NULL;
      waiter = next;
    }

  LeaveCriticalSection (&g_thread_xp_lock);
}

/* {{{2 XP Setup */
static void
g_thread_xp_init (void)
{
  static const GThreadImplVtable g_thread_xp_impl_vtable = {
    g_thread_xp_CallThisOnThreadExit,
    g_thread_xp_InitializeSRWLock,
    g_thread_xp_DeleteSRWLock,
    g_thread_xp_AcquireSRWLockExclusive,
    g_thread_xp_TryAcquireSRWLockExclusive,
    g_thread_xp_ReleaseSRWLockExclusive,
    g_thread_xp_AcquireSRWLockShared,
    g_thread_xp_TryAcquireSRWLockShared,
    g_thread_xp_ReleaseSRWLockShared,
    g_thread_xp_InitializeConditionVariable,
    g_thread_xp_DeleteConditionVariable,
    g_thread_xp_SleepConditionVariableSRW,
    g_thread_xp_WakeAllConditionVariable,
    g_thread_xp_WakeConditionVariable
  };

  InitializeCriticalSection (&g_thread_xp_lock);
  g_thread_xp_waiter_tls = TlsAlloc ();

  g_thread_impl_vtable = g_thread_xp_impl_vtable;
}

/* {{{1 Epilogue */

static gboolean
g_thread_lookup_native_funcs (void)
{
  GThreadImplVtable native_vtable = { 0, };
  HMODULE kernel32;

  kernel32 = GetModuleHandle ("KERNEL32.DLL");

  if (kernel32 == NULL)
    return FALSE;

#define GET_FUNC(name) if ((native_vtable.name = (void *) GetProcAddress (kernel32, #name)) == NULL) return FALSE
  GET_FUNC(InitializeSRWLock);
  GET_FUNC(AcquireSRWLockExclusive);
  GET_FUNC(TryAcquireSRWLockExclusive);
  GET_FUNC(ReleaseSRWLockExclusive);
  GET_FUNC(AcquireSRWLockShared);
  GET_FUNC(TryAcquireSRWLockShared);
  GET_FUNC(ReleaseSRWLockShared);

  GET_FUNC(InitializeConditionVariable);
  GET_FUNC(SleepConditionVariableSRW);
  GET_FUNC(WakeAllConditionVariable);
  GET_FUNC(WakeConditionVariable);
#undef GET_FUNC

  g_thread_impl_vtable = native_vtable;

  return TRUE;
}

void
g_thread_win32_init (void)
{
  if (!g_thread_lookup_native_funcs ())
    g_thread_xp_init ();

  InitializeCriticalSection (&g_private_lock);

#ifndef _MSC_VER
  SetThreadName_VEH_handle = AddVectoredExceptionHandler (1, &SetThreadName_VEH);
  if (SetThreadName_VEH_handle == NULL)
    {
      /* This is bad, but what can we do? */
    }
#endif
}

void
g_thread_win32_thread_detach (void)
{
  gboolean dtors_called;

  do
    {
      GPrivateDestructor *dtor;

      /* We go by the POSIX book on this one.
       *
       * If we call a destructor then there is a chance that some new
       * TLS variables got set by code called in that destructor.
       *
       * Loop until nothing is left.
       */
      dtors_called = FALSE;

      for (dtor = g_private_destructors; dtor; dtor = dtor->next)
        {
          gpointer value;

          value = TlsGetValue (dtor->index);
          if (value != NULL && dtor->notify != NULL)
            {
              /* POSIX says to clear this before the call */
              TlsSetValue (dtor->index, NULL);
              dtor->notify (value);
              dtors_called = TRUE;
            }
        }
    }
  while (dtors_called);

  if (g_thread_impl_vtable.CallThisOnThreadExit)
    g_thread_impl_vtable.CallThisOnThreadExit ();
}

void
g_thread_win32_process_detach (void)
{
#ifndef _MSC_VER
  if (SetThreadName_VEH_handle != NULL)
    {
      RemoveVectoredExceptionHandler (SetThreadName_VEH_handle);
      SetThreadName_VEH_handle = NULL;
    }
#endif
}

/* vim:set foldmethod=marker: */
