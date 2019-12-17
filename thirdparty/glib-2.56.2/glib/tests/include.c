/* Test case for bug 659866 */

#define _POSIX_C_SOURCE 199309L
#undef _GNU_SOURCE
#undef _XOPEN_SOURCE
#include <pthread.h>
#include <glib.h>

static void
test_rwlock (void)
{
  GRWLock lock;

  g_rw_lock_init (&lock);
  g_rw_lock_clear (&lock);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/include/rwlock", test_rwlock);

  return g_test_run ();
}
