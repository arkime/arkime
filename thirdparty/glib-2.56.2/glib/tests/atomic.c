/*
 * Copyright 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#include <glib.h>

static void
test_types (void)
{
  const gint *csp;
  const gint * const *cspp;
  guint u, u2;
  gint s, s2;
  gpointer vp, vp2;
  int *ip, *ip2;
  gsize gs, gs2;
  gboolean res;

  csp = &s;
  cspp = &csp;

  g_atomic_int_set (&u, 5);
  u2 = g_atomic_int_get (&u);
  g_assert_cmpint (u2, ==, 5);
  res = g_atomic_int_compare_and_exchange (&u, 6, 7);
  g_assert (!res);
  g_assert_cmpint (u, ==, 5);
  g_atomic_int_add (&u, 1);
  g_assert_cmpint (u, ==, 6);
  g_atomic_int_inc (&u);
  g_assert_cmpint (u, ==, 7);
  res = g_atomic_int_dec_and_test (&u);
  g_assert (!res);
  g_assert_cmpint (u, ==, 6);
  u2 = g_atomic_int_and (&u, 5);
  g_assert_cmpint (u2, ==, 6);
  g_assert_cmpint (u, ==, 4);
  u2 = g_atomic_int_or (&u, 8);
  g_assert_cmpint (u2, ==, 4);
  g_assert_cmpint (u, ==, 12);
  u2 = g_atomic_int_xor (&u, 4);
  g_assert_cmpint (u2, ==, 12);
  g_assert_cmpint (u, ==, 8);

  g_atomic_int_set (&s, 5);
  s2 = g_atomic_int_get (&s);
  g_assert_cmpint (s2, ==, 5);
  res = g_atomic_int_compare_and_exchange (&s, 6, 7);
  g_assert (!res);
  g_assert_cmpint (s, ==, 5);
  g_atomic_int_add (&s, 1);
  g_assert_cmpint (s, ==, 6);
  g_atomic_int_inc (&s);
  g_assert_cmpint (s, ==, 7);
  res = g_atomic_int_dec_and_test (&s);
  g_assert (!res);
  g_assert_cmpint (s, ==, 6);
  s2 = g_atomic_int_and (&s, 5);
  g_assert_cmpint (s2, ==, 6);
  g_assert_cmpint (s, ==, 4);
  s2 = g_atomic_int_or (&s, 8);
  g_assert_cmpint (s2, ==, 4);
  g_assert_cmpint (s, ==, 12);
  s2 = g_atomic_int_xor (&s, 4);
  g_assert_cmpint (s2, ==, 12);
  g_assert_cmpint (s, ==, 8);

  g_atomic_pointer_set (&vp, 0);
  vp2 = g_atomic_pointer_get (&vp);
  g_assert (vp2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&vp, 0, 0);
  g_assert (res);
  g_assert (vp == 0);

  g_atomic_pointer_set (&ip, 0);
  ip2 = g_atomic_pointer_get (&ip);
  g_assert (ip2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&ip, 0, 0);
  g_assert (res);
  g_assert (ip == 0);

  g_atomic_pointer_set (&gs, 0);
  gs2 = (gsize) g_atomic_pointer_get (&gs);
  g_assert (gs2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&gs, 0, 0);
  g_assert (res);
  g_assert (gs == 0);
  gs2 = g_atomic_pointer_add (&gs, 5);
  g_assert (gs2 == 0);
  g_assert (gs == 5);
  gs2 = g_atomic_pointer_and (&gs, 6);
  g_assert (gs2 == 5);
  g_assert (gs == 4);
  gs2 = g_atomic_pointer_or (&gs, 8);
  g_assert (gs2 == 4);
  g_assert (gs == 12);
  gs2 = g_atomic_pointer_xor (&gs, 4);
  g_assert (gs2 == 12);
  g_assert (gs == 8);

  g_assert (g_atomic_int_get (csp) == s);
  g_assert (g_atomic_pointer_get (cspp) == csp);

  /* repeat, without the macros */
#undef g_atomic_int_set
#undef g_atomic_int_get
#undef g_atomic_int_compare_and_exchange
#undef g_atomic_int_add
#undef g_atomic_int_inc
#undef g_atomic_int_and
#undef g_atomic_int_or
#undef g_atomic_int_xor
#undef g_atomic_int_dec_and_test
#undef g_atomic_pointer_set
#undef g_atomic_pointer_get
#undef g_atomic_pointer_compare_and_exchange
#undef g_atomic_pointer_add
#undef g_atomic_pointer_and
#undef g_atomic_pointer_or
#undef g_atomic_pointer_xor

  g_atomic_int_set ((gint*)&u, 5);
  u2 = g_atomic_int_get ((gint*)&u);
  g_assert_cmpint (u2, ==, 5);
  res = g_atomic_int_compare_and_exchange ((gint*)&u, 6, 7);
  g_assert (!res);
  g_assert_cmpint (u, ==, 5);
  g_atomic_int_add ((gint*)&u, 1);
  g_assert_cmpint (u, ==, 6);
  g_atomic_int_inc ((gint*)&u);
  g_assert_cmpint (u, ==, 7);
  res = g_atomic_int_dec_and_test ((gint*)&u);
  g_assert (!res);
  g_assert_cmpint (u, ==, 6);
  u2 = g_atomic_int_and (&u, 5);
  g_assert_cmpint (u2, ==, 6);
  g_assert_cmpint (u, ==, 4);
  u2 = g_atomic_int_or (&u, 8);
  g_assert_cmpint (u2, ==, 4);
  g_assert_cmpint (u, ==, 12);
  u2 = g_atomic_int_xor (&u, 4);
  g_assert_cmpint (u2, ==, 12);

  g_atomic_int_set (&s, 5);
  s2 = g_atomic_int_get (&s);
  g_assert_cmpint (s2, ==, 5);
  res = g_atomic_int_compare_and_exchange (&s, 6, 7);
  g_assert (!res);
  g_assert_cmpint (s, ==, 5);
  g_atomic_int_add (&s, 1);
  g_assert_cmpint (s, ==, 6);
  g_atomic_int_inc (&s);
  g_assert_cmpint (s, ==, 7);
  res = g_atomic_int_dec_and_test (&s);
  g_assert (!res);
  g_assert_cmpint (s, ==, 6);
  s2 = g_atomic_int_and ((guint*)&s, 5);
  g_assert_cmpint (s2, ==, 6);
  g_assert_cmpint (s, ==, 4);
  s2 = g_atomic_int_or ((guint*)&s, 8);
  g_assert_cmpint (s2, ==, 4);
  g_assert_cmpint (s, ==, 12);
  s2 = g_atomic_int_xor ((guint*)&s, 4);
  g_assert_cmpint (s2, ==, 12);
  g_assert_cmpint (s, ==, 8);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  s2 = g_atomic_int_exchange_and_add ((gint*)&s, 1);
G_GNUC_END_IGNORE_DEPRECATIONS
  g_assert_cmpint (s2, ==, 8);
  g_assert_cmpint (s, ==, 9);

  g_atomic_pointer_set (&vp, 0);
  vp2 = g_atomic_pointer_get (&vp);
  g_assert (vp2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&vp, 0, 0);
  g_assert (res);
  g_assert (vp == 0);

  g_atomic_pointer_set (&ip, 0);
  ip2 = g_atomic_pointer_get (&ip);
  g_assert (ip2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&ip, 0, 0);
  g_assert (res);
  g_assert (ip == 0);

  g_atomic_pointer_set (&gs, 0);
  gs2 = (gsize) g_atomic_pointer_get (&gs);
  g_assert (gs2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&gs, 0, 0);
  g_assert (res);
  g_assert (gs == 0);
  gs2 = g_atomic_pointer_add (&gs, 5);
  g_assert (gs2 == 0);
  g_assert (gs == 5);
  gs2 = g_atomic_pointer_and (&gs, 6);
  g_assert (gs2 == 5);
  g_assert (gs == 4);
  gs2 = g_atomic_pointer_or (&gs, 8);
  g_assert (gs2 == 4);
  g_assert (gs == 12);
  gs2 = g_atomic_pointer_xor (&gs, 4);
  g_assert (gs2 == 12);
  g_assert (gs == 8);

  g_assert (g_atomic_int_get (csp) == s);
  g_assert (g_atomic_pointer_get (cspp) == csp);
}

#define THREADS 10
#define ROUNDS 10000

volatile gint bucket[THREADS];
volatile gint atomic;

static gpointer
thread_func (gpointer data)
{
  gint idx = GPOINTER_TO_INT (data);
  gint i;
  gint d;

  for (i = 0; i < ROUNDS; i++)
    {
      d = g_random_int_range (-10, 100);
      bucket[idx] += d;
      g_atomic_int_add (&atomic, d);
      g_thread_yield ();
    }

  return NULL;
}

static void
test_threaded (void)
{
  gint sum;
  gint i;
  GThread *threads[THREADS];

  atomic = 0;
  for (i = 0; i < THREADS; i++)
    bucket[i] = 0;

  for (i = 0; i < THREADS; i++)
    threads[i] = g_thread_new ("atomic", thread_func, GINT_TO_POINTER (i));

  for (i = 0; i < THREADS; i++)
    g_thread_join (threads[i]);

  sum = 0;
  for (i = 0; i < THREADS; i++)
    sum += bucket[i];

  g_assert_cmpint (sum, ==, atomic);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/atomic/types", test_types);
  g_test_add_func ("/atomic/threaded", test_threaded);

  return g_test_run ();
}
