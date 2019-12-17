/* Unit tests for grand
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "glib.h"

/* Outputs tested against the reference implementation mt19937ar.c from
 * http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/emt19937ar.html
 */

/* Tests for a simple seed, first number is the seed */
const guint32 first_numbers[] = 
{
  0x7a7a7a7a,
  0xfdcc2d54,
  0x3a279ceb,
  0xc4d39c33,
  0xf31895cd,
  0x46ca0afc,
  0x3f5484ff,
  0x54bc9557,
  0xed2c24b1,
  0x84062503,
  0x8f6404b3,
  0x599a94b3,
  0xe46d03d5,
  0x310beb78,
  0x7bee5d08,
  0x760d09be,
  0x59b6e163,
  0xbf6d16ec,
  0xcca5fb54,
  0x5de7259b,
  0x1696330c,
};

/* array seed */
const guint32 seed_array[] =
{
  0x6553375f,
  0xd6b8d43b,
  0xa1e7667f,
  0x2b10117c
};

/* tests for the array seed */
const guint32 array_outputs[] =
{
  0xc22b7dc3,
  0xfdecb8ae,
  0xb4af0738,
  0x516bc6e1,
  0x7e372e91,
  0x2d38ff80,
  0x6096494a,
  0xd162d5a8,
  0x3c0aaa0d,
  0x10e736ae
};

static void
test_rand (void)
{
  guint n;
  guint ones;
  double proportion;
  GRand *rand;
  GRand *copy;

  rand = g_rand_new_with_seed (first_numbers[0]);

  for (n = 1; n < G_N_ELEMENTS (first_numbers); n++)
    g_assert (first_numbers[n] == g_rand_int (rand));

  g_rand_set_seed (rand, 2);
  g_rand_set_seed_array (rand, seed_array, G_N_ELEMENTS (seed_array));

  for (n = 0; n < G_N_ELEMENTS (array_outputs); n++)
    g_assert (array_outputs[n] == g_rand_int (rand));

  copy = g_rand_copy (rand);
  for (n = 0; n < 100; n++)
    g_assert (g_rand_int (copy) == g_rand_int (rand));

  for (n = 1; n < 100000; n++)
    {
      gint32 i;
      gdouble d;
      gboolean b;

      i = g_rand_int_range (rand, 8,16);
      g_assert (i >= 8 && i < 16);
      
      i = g_random_int_range (8,16);
      g_assert (i >= 8 && i < 16);

      d = g_rand_double (rand);
      g_assert (d >= 0 && d < 1);

      d = g_random_double ();
      g_assert (d >= 0 && d < 1);

      d = g_rand_double_range (rand, -8, 32);
      g_assert (d >= -8 && d < 32);
 
      d = g_random_double_range (-8, 32);
      g_assert (d >= -8 && d < 32);
 
      b = g_random_boolean ();
      g_assert (b == TRUE || b  == FALSE);
 
      b = g_rand_boolean (rand);
      g_assert (b == TRUE || b  == FALSE);     
    }

  /* Statistical sanity check, count the number of ones
   * when getting random numbers in range [0,3) and see
   * that it must be semi-close to 0.25 with a VERY large
   * probability */
  ones = 0;
  for (n = 1; n < 100000; n++)
    {
      if (g_random_int_range (0, 4) == 1)
        ones ++;
    }

  proportion = (double)ones / (double)100000;
  /* 0.025 is overkill, but should suffice to test for some unreasonability */
  g_assert (ABS (proportion - 0.25) < 0.025);

  g_rand_free (rand);
  g_rand_free (copy);
}

static void
test_double_range (void)
{
  gdouble d;

  g_test_bug ("502560");

  d = g_random_double_range (-G_MAXDOUBLE, G_MAXDOUBLE);

  g_assert (-G_MAXDOUBLE <= d);
  g_assert (d < G_MAXDOUBLE);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/rand/test-rand", test_rand);
  g_test_add_func ("/rand/double-range", test_double_range);

  return g_test_run();
}
