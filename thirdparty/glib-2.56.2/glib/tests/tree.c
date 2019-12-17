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

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

/* We are testing some deprecated APIs here */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <stdio.h>
#include <string.h>
#include "glib.h"


static gint
my_compare (gconstpointer a,
            gconstpointer b)
{
  const char *cha = a;
  const char *chb = b;

  return *cha - *chb;
}

static gint
my_compare_with_data (gconstpointer a,
                      gconstpointer b,
                      gpointer      user_data)
{
  const char *cha = a;
  const char *chb = b;

  /* just check that we got the right data */
  g_assert (GPOINTER_TO_INT(user_data) == 123);

  return *cha - *chb;
}

static gint
my_search (gconstpointer a,
           gconstpointer b)
{
  return my_compare (b, a);
}

static gpointer destroyed_key = NULL;
static gpointer destroyed_value = NULL;

static void
my_key_destroy (gpointer key)
{
  destroyed_key = key;
}

static void
my_value_destroy (gpointer value)
{
  destroyed_value = value;
}

static gint
my_traverse (gpointer key,
             gpointer value,
             gpointer data)
{
  char *ch = key;

  g_assert ((*ch) > 0);

  if (*ch == 'd')
    return TRUE;

  return FALSE;
}

char chars[] =
  "0123456789"
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz";

char chars2[] =
  "0123456789"
  "abcdefghijklmnopqrstuvwxyz";

static gint
check_order (gpointer key,
             gpointer value,
             gpointer data)
{
  char **p = data;
  char *ch = key;
 
  g_assert (**p == *ch);

  (*p)++;

  return FALSE;
}

static void
test_tree_search (void)
{
  gint i;
  GTree *tree;
  gboolean removed;
  gchar c;
  gchar *p, *d;

  tree = g_tree_new_with_data (my_compare_with_data, GINT_TO_POINTER(123));

  for (i = 0; chars[i]; i++)
    g_tree_insert (tree, &chars[i], &chars[i]);

  g_tree_foreach (tree, my_traverse, NULL);

  g_assert_cmpint (g_tree_nnodes (tree), ==, strlen (chars));
  g_assert_cmpint (g_tree_height (tree), ==, 6);
 
  p = chars;
  g_tree_foreach (tree, check_order, &p);

  for (i = 0; i < 26; i++)
    {
      removed = g_tree_remove (tree, &chars[i + 10]);
      g_assert (removed);
    }

  c = '\0';
  removed = g_tree_remove (tree, &c);
  g_assert (!removed);

  g_tree_foreach (tree, my_traverse, NULL);

  g_assert_cmpint (g_tree_nnodes (tree), ==, strlen (chars2));
  g_assert_cmpint (g_tree_height (tree), ==, 6);

  p = chars2;
  g_tree_foreach (tree, check_order, &p);

  for (i = 25; i >= 0; i--)
    g_tree_insert (tree, &chars[i + 10], &chars[i + 10]);

  p = chars;
  g_tree_foreach (tree, check_order, &p);

  c = '0';
  p = g_tree_lookup (tree, &c);
  g_assert (p && *p == c);
  g_assert (g_tree_lookup_extended (tree, &c, (gpointer *)&d, (gpointer *)&p));
  g_assert (c == *d && c == *p);

  c = 'A';
  p = g_tree_lookup (tree, &c);
  g_assert (p && *p == c);

  c = 'a';
  p = g_tree_lookup (tree, &c);
  g_assert (p && *p == c);

  c = 'z';
  p = g_tree_lookup (tree, &c);
  g_assert (p && *p == c);

  c = '!';
  p = g_tree_lookup (tree, &c);
  g_assert (p == NULL);

  c = '=';
  p = g_tree_lookup (tree, &c);
  g_assert (p == NULL);

  c = '|';
  p = g_tree_lookup (tree, &c);
  g_assert (p == NULL);

  c = '0';
  p = g_tree_search (tree, my_search, &c);
  g_assert (p && *p == c);

  c = 'A';
  p = g_tree_search (tree, my_search, &c);
  g_assert (p && *p == c);

  c = 'a';
  p = g_tree_search (tree, my_search, &c);
  g_assert (p &&*p == c);

  c = 'z';
  p = g_tree_search (tree, my_search, &c);
  g_assert (p && *p == c);

  c = '!';
  p = g_tree_search (tree, my_search, &c);
  g_assert (p == NULL);

  c = '=';
  p = g_tree_search (tree, my_search, &c);
  g_assert (p == NULL);

  c = '|';
  p = g_tree_search (tree, my_search, &c);
  g_assert (p == NULL);

  g_tree_destroy (tree);
}

static void
test_tree_remove (void)
{
  GTree *tree;
  char c, d;
  gint i;
  gboolean removed;
  gchar *remove;

  tree = g_tree_new_full ((GCompareDataFunc)my_compare, NULL,
                          my_key_destroy,
                          my_value_destroy);

  for (i = 0; chars[i]; i++)
    g_tree_insert (tree, &chars[i], &chars[i]);

  c = '0';
  g_tree_insert (tree, &c, &c);
  g_assert (destroyed_key == &c);
  g_assert (destroyed_value == &chars[0]);
  destroyed_key = NULL;
  destroyed_value = NULL;

  d = '1';
  g_tree_replace (tree, &d, &d);
  g_assert (destroyed_key == &chars[1]);
  g_assert (destroyed_value == &chars[1]);
  destroyed_key = NULL;
  destroyed_value = NULL;

  c = '2';
  removed = g_tree_remove (tree, &c);
  g_assert (removed);
  g_assert (destroyed_key == &chars[2]);
  g_assert (destroyed_value == &chars[2]);
  destroyed_key = NULL;
  destroyed_value = NULL;

  c = '3';
  removed = g_tree_steal (tree, &c);
  g_assert (removed);
  g_assert (destroyed_key == NULL);
  g_assert (destroyed_value == NULL);

  remove = "omkjigfedba";
  for (i = 0; remove[i]; i++)
    {
      removed = g_tree_remove (tree, &remove[i]);
      g_assert (removed);
    }

  g_tree_destroy (tree);
}

static void
test_tree_destroy (void)
{
  GTree *tree;
  gint i;

  tree = g_tree_new (my_compare);

  for (i = 0; chars[i]; i++)
    g_tree_insert (tree, &chars[i], &chars[i]);

  g_assert_cmpint (g_tree_nnodes (tree), ==, strlen (chars));

  g_tree_ref (tree);
  g_tree_destroy (tree);

  g_assert_cmpint (g_tree_nnodes (tree), ==, 0);

  g_tree_unref (tree);
}

typedef struct {
  GString *s;
  gint count;
} CallbackData;

static gboolean
traverse_func (gpointer key, gpointer value, gpointer data)
{
  CallbackData *d = data;

  gchar *c = value;
  g_string_append_c (d->s, *c);

  d->count--;

  if (d->count == 0)
    return TRUE;

  return FALSE;
}

typedef struct {
  GTraverseType  traverse;
  gint           limit;
  const gchar   *expected;
} TraverseData;

static void
test_tree_traverse (void)
{
  GTree *tree;
  gint i;
  TraverseData orders[] = {
    { G_IN_ORDER,   -1, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" },
    { G_IN_ORDER,    1, "0" },
    { G_IN_ORDER,    2, "01" },
    { G_IN_ORDER,    3, "012" },
    { G_IN_ORDER,    4, "0123" },
    { G_IN_ORDER,    5, "01234" },
    { G_IN_ORDER,    6, "012345" },
    { G_IN_ORDER,    7, "0123456" },
    { G_IN_ORDER,    8, "01234567" },
    { G_IN_ORDER,    9, "012345678" },
    { G_IN_ORDER,   10, "0123456789" },
    { G_IN_ORDER,   11, "0123456789A" },
    { G_IN_ORDER,   12, "0123456789AB" },
    { G_IN_ORDER,   13, "0123456789ABC" },
    { G_IN_ORDER,   14, "0123456789ABCD" },

    { G_PRE_ORDER,  -1, "VF73102546B98ADCENJHGILKMRPOQTSUldZXWYbachfegjiktpnmorqsxvuwyz" },
    { G_PRE_ORDER,   1, "V" },
    { G_PRE_ORDER,   2, "VF" },
    { G_PRE_ORDER,   3, "VF7" },
    { G_PRE_ORDER,   4, "VF73" },
    { G_PRE_ORDER,   5, "VF731" },
    { G_PRE_ORDER,   6, "VF7310" },
    { G_PRE_ORDER,   7, "VF73102" },
    { G_PRE_ORDER,   8, "VF731025" },
    { G_PRE_ORDER,   9, "VF7310254" },
    { G_PRE_ORDER,  10, "VF73102546" },
    { G_PRE_ORDER,  11, "VF73102546B" },
    { G_PRE_ORDER,  12, "VF73102546B9" },
    { G_PRE_ORDER,  13, "VF73102546B98" },
    { G_PRE_ORDER,  14, "VF73102546B98A" },

    { G_POST_ORDER, -1, "02146538A9CEDB7GIHKMLJOQPSUTRNFWYXacbZegfikjhdmonqsrpuwvzyxtlV" },
    { G_POST_ORDER,  1, "0" },
    { G_POST_ORDER,  2, "02" },
    { G_POST_ORDER,  3, "021" },
    { G_POST_ORDER,  4, "0214" },
    { G_POST_ORDER,  5, "02146" },
    { G_POST_ORDER,  6, "021465" },
    { G_POST_ORDER,  7, "0214653" },
    { G_POST_ORDER,  8, "02146538" },
    { G_POST_ORDER,  9, "02146538A" },
    { G_POST_ORDER, 10, "02146538A9" },
    { G_POST_ORDER, 11, "02146538A9C" },
    { G_POST_ORDER, 12, "02146538A9CE" },
    { G_POST_ORDER, 13, "02146538A9CED" },
    { G_POST_ORDER, 14, "02146538A9CEDB" }
  };
  CallbackData data;

  tree = g_tree_new (my_compare);

  for (i = 0; chars[i]; i++)
    g_tree_insert (tree, &chars[i], &chars[i]);

  data.s = g_string_new ("");
  for (i = 0; i < G_N_ELEMENTS (orders); i++)
    {
      g_string_set_size (data.s, 0);
      data.count = orders[i].limit;
      g_tree_traverse (tree, traverse_func, orders[i].traverse, &data);
      g_assert_cmpstr (data.s->str, ==, orders[i].expected);
    }

  g_tree_unref (tree);
  g_string_free (data.s, TRUE); 
}

static void
test_tree_insert (void)
{
  GTree *tree;
  gchar *p;
  gint i;
  gchar *scrambled;

  tree = g_tree_new (my_compare);

  for (i = 0; chars[i]; i++)
    g_tree_insert (tree, &chars[i], &chars[i]);
  p = chars;
  g_tree_foreach (tree, check_order, &p);

  g_tree_unref (tree);
  tree = g_tree_new (my_compare);

  for (i = strlen (chars) - 1; i >= 0; i--)
    g_tree_insert (tree, &chars[i], &chars[i]);
  p = chars;
  g_tree_foreach (tree, check_order, &p);

  g_tree_unref (tree);
  tree = g_tree_new (my_compare);

  scrambled = g_strdup (chars);

  for (i = 0; i < 30; i++)
    {
      gchar tmp;
      gint a, b;

      a = g_random_int_range (0, strlen (scrambled));
      b = g_random_int_range (0, strlen (scrambled));
      tmp = scrambled[a];
      scrambled[a] = scrambled[b];
      scrambled[b] = tmp;
    }

  for (i = 0; scrambled[i]; i++)
    g_tree_insert (tree, &scrambled[i], &scrambled[i]);
  p = chars;
  g_tree_foreach (tree, check_order, &p);

  g_free (scrambled);
  g_tree_unref (tree);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/tree/search", test_tree_search);
  g_test_add_func ("/tree/remove", test_tree_remove);
  g_test_add_func ("/tree/destroy", test_tree_destroy);
  g_test_add_func ("/tree/traverse", test_tree_traverse);
  g_test_add_func ("/tree/insert", test_tree_insert);

  return g_test_run ();
}

