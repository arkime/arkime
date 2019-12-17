/*
 * Copyright 2015 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Author: Allison Ryan Lortie <desrt@desrt.ca>
 */

#include <glib.h>

typedef struct
{
  gboolean success;
  guint64 c, a, b;
} Case;

static void
test_checked_guint_add (void)
{
  static const Case cases[] = {
    /*  success                c =               a +             b */
      { TRUE,                  0,                0,              0 },
      { TRUE,           G_MAXINT,         G_MAXINT,              0 },
      { TRUE,           G_MAXINT,                0,       G_MAXINT },
      { TRUE,          G_MAXUINT,        G_MAXUINT,              0 },
      { TRUE,          G_MAXUINT,                0,      G_MAXUINT },
      { TRUE,      G_MAXUINT - 1,         G_MAXINT,       G_MAXINT },
      { FALSE,                 0,        G_MAXUINT,              1 },
      { FALSE,                 0,                1,      G_MAXUINT },
      { FALSE,                 0,        G_MAXUINT,      G_MAXUINT }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      guint result;

      g_assert_cmpuint (cases[i].success, ==, g_uint_checked_add (&result, cases[i].a, cases[i].b));
      if (cases[i].success)
        g_assert_cmpuint (cases[i].c, ==, result);
    }
}

static void
test_checked_guint_mul (void)
{
  static const Case cases[] = {
    /*  success                        c =               a *             b */
      { TRUE,                          0,                0,              0 },
      { TRUE,                          0,         G_MAXINT,              0 },
      { TRUE,                   G_MAXINT,         G_MAXINT,              1 },
      { TRUE,                          0,        G_MAXUINT,              0 },
      { TRUE,                  G_MAXUINT,        G_MAXUINT,              1 },
      { TRUE,       2 * (guint) G_MAXINT,                2,       G_MAXINT },
      { TRUE,       2 * (guint) G_MAXINT,         G_MAXINT,              2 },
      { FALSE,                         0,                3,       G_MAXINT },
      { FALSE,                         0,         G_MAXINT,              3 }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      guint result;

      g_assert_cmpuint (cases[i].success, ==, g_uint_checked_mul (&result, cases[i].a, cases[i].b));
      if (cases[i].success)
        g_assert_cmpuint (cases[i].c, ==, result);
    }
}


static void
test_checked_guint64_add (void)
{
  static const Case cases[] = {
    /*  success                c =               a +             b */
      { TRUE,                  0,                0,              0 },
      { TRUE,         G_MAXINT64,       G_MAXINT64,              0 },
      { TRUE,         G_MAXINT64,                0,     G_MAXINT64 },
      { TRUE,        G_MAXUINT64,      G_MAXUINT64,              0 },
      { TRUE,        G_MAXUINT64,                0,    G_MAXUINT64 },
      { TRUE,    G_MAXUINT64 - 1,       G_MAXINT64,     G_MAXINT64 },
      { FALSE,                 0,      G_MAXUINT64,              1 },
      { FALSE,                 0,                1,    G_MAXUINT64 },
      { FALSE,                 0,      G_MAXUINT64,    G_MAXUINT64 }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      guint64 result;

      g_assert_cmpuint (cases[i].success, ==, g_uint64_checked_add (&result, cases[i].a, cases[i].b));
      if (cases[i].success)
        g_assert_cmpuint (cases[i].c, ==, result);
    }
}

static void
test_checked_guint64_mul (void)
{
  static const Case cases[] = {
    /*  success                        c =               a *             b */
      { TRUE,                          0,                0,              0 },
      { TRUE,                          0,       G_MAXINT64,              0 },
      { TRUE,                 G_MAXINT64,       G_MAXINT64,              1 },
      { TRUE,                          0,      G_MAXUINT64,              0 },
      { TRUE,                G_MAXUINT64,      G_MAXUINT64,              1 },
      { TRUE,   2 * (guint64) G_MAXINT64,                2,     G_MAXINT64 },
      { TRUE,   2 * (guint64) G_MAXINT64,       G_MAXINT64,              2 },
      { FALSE,                         0,                3,     G_MAXINT64 },
      { FALSE,                         0,       G_MAXINT64,              3 }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      guint64 result;

      g_assert_cmpuint (cases[i].success, ==, g_uint64_checked_mul (&result, cases[i].a, cases[i].b));
      if (cases[i].success)
        g_assert_cmpuint (cases[i].c, ==, result);
    }
}


static void
test_checked_gsize_add (void)
{
  static const Case cases[] = {
    /*  success                c =               a +             b */
      { TRUE,                  0,                0,              0 },
      { TRUE,         G_MAXSSIZE,       G_MAXSSIZE,              0 },
      { TRUE,         G_MAXSSIZE,                0,     G_MAXSSIZE },
      { TRUE,          G_MAXSIZE,        G_MAXSIZE,              0 },
      { TRUE,          G_MAXSIZE,                0,      G_MAXSIZE },
      { TRUE,      G_MAXSIZE - 1,       G_MAXSSIZE,     G_MAXSSIZE },
      { FALSE,                 0,        G_MAXSIZE,              1 },
      { FALSE,                 0,                1,      G_MAXSIZE },
      { FALSE,                 0,        G_MAXSIZE,      G_MAXSIZE }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      gsize result;

      g_assert_cmpuint (cases[i].success, ==, g_size_checked_add (&result, cases[i].a, cases[i].b));
      if (cases[i].success)
        g_assert_cmpuint (cases[i].c, ==, result);
    }
}

static void
test_checked_gsize_mul (void)
{
  static const Case cases[] = {
    /*  success                        c =               a *             b */
      { TRUE,                          0,                0,              0 },
      { TRUE,                          0,       G_MAXSSIZE,              0 },
      { TRUE,                 G_MAXSSIZE,       G_MAXSSIZE,              1 },
      { TRUE,                          0,        G_MAXSIZE,              0 },
      { TRUE,                  G_MAXSIZE,        G_MAXSIZE,              1 },
      { TRUE,     2 * (gsize) G_MAXSSIZE,                2,     G_MAXSSIZE },
      { TRUE,     2 * (gsize) G_MAXSSIZE,       G_MAXSSIZE,              2 },
      { FALSE,                         0,                3,     G_MAXSSIZE },
      { FALSE,                         0,       G_MAXSSIZE,              3 }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      gsize result;

      g_assert_cmpuint (cases[i].success, ==, g_size_checked_mul (&result, cases[i].a, cases[i].b));
      if (cases[i].success)
        g_assert_cmpuint (cases[i].c, ==, result);
    }
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/glib/checked-math/guint-add", test_checked_guint_add);
  g_test_add_func ("/glib/checked-math/guint-mul", test_checked_guint_mul);
  g_test_add_func ("/glib/checked-math/guint64-add", test_checked_guint64_add);
  g_test_add_func ("/glib/checked-math/guint64-mul", test_checked_guint64_mul);
  g_test_add_func ("/glib/checked-math/gsize-add", test_checked_gsize_add);
  g_test_add_func ("/glib/checked-math/gsize-mul", test_checked_gsize_mul);

  return g_test_run ();
}
