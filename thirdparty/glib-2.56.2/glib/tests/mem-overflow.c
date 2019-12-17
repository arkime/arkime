/* Unit tests for g
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

/* We test for errors in optimize-only definitions in gmem.h */

#ifdef __GNUC__
#pragma GCC optimize (1)
#endif

#include "glib.h"
#include <stdlib.h>

static gsize a = G_MAXSIZE / 10 + 10;
static gsize b = 10;
typedef char X[10];

#define MEM_OVERFLOW_TEST(name, code) \
static void                           \
mem_overflow_ ## name (void)          \
{                                     \
  gpointer p;                         \
  code;                               \
  g_free (p);                         \
  exit (0);                           \
}

MEM_OVERFLOW_TEST (malloc_n_a_a, p = g_malloc_n (a, a))
MEM_OVERFLOW_TEST (malloc_n_a_b, p = g_malloc_n (a, b))
MEM_OVERFLOW_TEST (malloc_n_b_a, p = g_malloc_n (b, a))
MEM_OVERFLOW_TEST (malloc_n_b_b, p = g_malloc_n (b, b))

MEM_OVERFLOW_TEST (malloc0_n_a_a, p = g_malloc0_n (a, a))
MEM_OVERFLOW_TEST (malloc0_n_a_b, p = g_malloc0_n (a, b))
MEM_OVERFLOW_TEST (malloc0_n_b_a, p = g_malloc0_n (b, a))
MEM_OVERFLOW_TEST (malloc0_n_b_b, p = g_malloc0_n (b, b))

MEM_OVERFLOW_TEST (realloc_n_a_a, p = g_malloc (1); p = g_realloc_n (p, a, a))
MEM_OVERFLOW_TEST (realloc_n_a_b, p = g_malloc (1); p = g_realloc_n (p, a, b))
MEM_OVERFLOW_TEST (realloc_n_b_a, p = g_malloc (1); p = g_realloc_n (p, b, a))
MEM_OVERFLOW_TEST (realloc_n_b_b, p = g_malloc (1); p = g_realloc_n (p, b, b))

MEM_OVERFLOW_TEST (new_a, p = g_new (X, a))
MEM_OVERFLOW_TEST (new_b, p = g_new (X, b))

MEM_OVERFLOW_TEST (new0_a, p = g_new0 (X, a))
MEM_OVERFLOW_TEST (new0_b, p = g_new0 (X, b))

MEM_OVERFLOW_TEST (renew_a, p = g_malloc (1); p = g_renew (X, p, a))
MEM_OVERFLOW_TEST (renew_b, p = g_malloc (1); p = g_renew (X, p, b))

static void
mem_overflow_malloc_0 (void)
{
  gpointer p;

  p = g_malloc (0);
  g_assert (p == NULL);
}

static void
mem_overflow_realloc_0 (void)
{
  gpointer p;

  p = g_malloc (10);
  g_assert (p != NULL);
  p = g_realloc (p, 0);
  g_assert (p == NULL);
}

static void
mem_overflow (void)
{
  gpointer p, q;

  /* "FAIL" here apparently means "fail to overflow"... */
#define CHECK_PASS(P)	p = (P); g_assert (p == NULL);
#define CHECK_FAIL(P)	p = (P); g_assert (p != NULL);

  CHECK_PASS (g_try_malloc_n (a, a));
  CHECK_PASS (g_try_malloc_n (a, b));
  CHECK_PASS (g_try_malloc_n (b, a));
  CHECK_FAIL (g_try_malloc_n (b, b));
  g_free (p);

  CHECK_PASS (g_try_malloc0_n (a, a));
  CHECK_PASS (g_try_malloc0_n (a, b));
  CHECK_PASS (g_try_malloc0_n (b, a));
  CHECK_FAIL (g_try_malloc0_n (b, b));
  g_free (p);

  q = g_malloc (1);
  CHECK_PASS (g_try_realloc_n (q, a, a));
  CHECK_PASS (g_try_realloc_n (q, a, b));
  CHECK_PASS (g_try_realloc_n (q, b, a));
  CHECK_FAIL (g_try_realloc_n (q, b, b));
  g_free (p);

  CHECK_PASS (g_try_new (X, a));
  CHECK_FAIL (g_try_new (X, b));
  g_free (p);

  CHECK_PASS (g_try_new0 (X, a));
  CHECK_FAIL (g_try_new0 (X, b));
  g_free (p);

  q = g_try_malloc (1);
  CHECK_PASS (g_try_renew (X, q, a));
  CHECK_FAIL (g_try_renew (X, q, b));
  free (p);

#define CHECK_SUBPROCESS_FAIL(name) do { \
      if (g_test_undefined ()) \
        { \
          g_test_trap_subprocess ("/mem/overflow/subprocess/" #name, 0, 0); \
          g_test_trap_assert_failed(); \
        } \
    } while (0)

#define CHECK_SUBPROCESS_PASS(name) do { \
      if (g_test_undefined ()) \
        { \
          g_test_trap_subprocess ("/mem/overflow/subprocess/" #name, 0, 0); \
          g_test_trap_assert_passed(); \
        } \
    } while (0)

  CHECK_SUBPROCESS_FAIL (malloc_n_a_a);
  CHECK_SUBPROCESS_FAIL (malloc_n_a_b);
  CHECK_SUBPROCESS_FAIL (malloc_n_b_a);
  CHECK_SUBPROCESS_PASS (malloc_n_b_b);

  CHECK_SUBPROCESS_FAIL (malloc0_n_a_a);
  CHECK_SUBPROCESS_FAIL (malloc0_n_a_b);
  CHECK_SUBPROCESS_FAIL (malloc0_n_b_a);
  CHECK_SUBPROCESS_PASS (malloc0_n_b_b);

  CHECK_SUBPROCESS_FAIL (realloc_n_a_a);
  CHECK_SUBPROCESS_FAIL (realloc_n_a_b);
  CHECK_SUBPROCESS_FAIL (realloc_n_b_a);
  CHECK_SUBPROCESS_PASS (realloc_n_b_b);

  CHECK_SUBPROCESS_FAIL (new_a);
  CHECK_SUBPROCESS_PASS (new_b);

  CHECK_SUBPROCESS_FAIL (new0_a);
  CHECK_SUBPROCESS_PASS (new0_b);

  CHECK_SUBPROCESS_FAIL (renew_a);
  CHECK_SUBPROCESS_PASS (renew_b);

  CHECK_SUBPROCESS_PASS (malloc_0);
  CHECK_SUBPROCESS_PASS (realloc_0);
}

#ifdef __GNUC__
typedef struct
{
} Empty;

static void
empty_alloc_subprocess (void)
{
  Empty *empty;

  empty = g_new0 (Empty, 1);
  g_assert (empty == NULL);
  exit (0);
}

static void
empty_alloc (void)
{
  g_test_bug ("615379");

  g_assert_cmpint (sizeof (Empty), ==, 0);

  g_test_trap_subprocess ("/mem/empty-alloc/subprocess", 0, 0);
  g_test_trap_assert_passed ();
}
#endif

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/mem/overflow", mem_overflow);
  g_test_add_func ("/mem/overflow/subprocess/malloc_n_a_a", mem_overflow_malloc_n_a_a);
  g_test_add_func ("/mem/overflow/subprocess/malloc_n_a_b", mem_overflow_malloc_n_a_b);
  g_test_add_func ("/mem/overflow/subprocess/malloc_n_b_a", mem_overflow_malloc_n_b_a);
  g_test_add_func ("/mem/overflow/subprocess/malloc_n_b_b", mem_overflow_malloc_n_b_b);
  g_test_add_func ("/mem/overflow/subprocess/malloc0_n_a_a", mem_overflow_malloc0_n_a_a);
  g_test_add_func ("/mem/overflow/subprocess/malloc0_n_a_b", mem_overflow_malloc0_n_a_b);
  g_test_add_func ("/mem/overflow/subprocess/malloc0_n_b_a", mem_overflow_malloc0_n_b_a);
  g_test_add_func ("/mem/overflow/subprocess/malloc0_n_b_b", mem_overflow_malloc0_n_b_b);
  g_test_add_func ("/mem/overflow/subprocess/realloc_n_a_a", mem_overflow_realloc_n_a_a);
  g_test_add_func ("/mem/overflow/subprocess/realloc_n_a_b", mem_overflow_realloc_n_a_b);
  g_test_add_func ("/mem/overflow/subprocess/realloc_n_b_a", mem_overflow_realloc_n_b_a);
  g_test_add_func ("/mem/overflow/subprocess/realloc_n_b_b", mem_overflow_realloc_n_b_b);
  g_test_add_func ("/mem/overflow/subprocess/new_a", mem_overflow_new_a);
  g_test_add_func ("/mem/overflow/subprocess/new_b", mem_overflow_new_b);
  g_test_add_func ("/mem/overflow/subprocess/new0_a", mem_overflow_new0_a);
  g_test_add_func ("/mem/overflow/subprocess/new0_b", mem_overflow_new0_b);
  g_test_add_func ("/mem/overflow/subprocess/renew_a", mem_overflow_renew_a);
  g_test_add_func ("/mem/overflow/subprocess/renew_b", mem_overflow_renew_b);
  g_test_add_func ("/mem/overflow/subprocess/malloc_0", mem_overflow_malloc_0);
  g_test_add_func ("/mem/overflow/subprocess/realloc_0", mem_overflow_realloc_0);

#ifdef __GNUC__
  g_test_add_func ("/mem/empty-alloc", empty_alloc);
  g_test_add_func ("/mem/empty-alloc/subprocess", empty_alloc_subprocess);
#endif

  return g_test_run();
}
