/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1999 The Free Software Foundation
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

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>



int array[10000];

static void
fill_hash_table_and_array (GHashTable *hash_table)
{
  int i;

  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }
}

static void
init_result_array (int result_array[10000])
{
  int i;

  for (i = 0; i < 10000; i++)
    result_array[i] = -1;
}

static void
verify_result_array (int array[10000])
{
  int i;

  for (i = 0; i < 10000; i++)
    g_assert (array[i] == i);
}

static void
handle_pair (gpointer key, gpointer value, int result_array[10000])
{
  int n;

  g_assert (key == value);

  n = *((int *) value);

  g_assert (n >= 0 && n < 10000);
  g_assert (result_array[n] == -1);

  result_array[n] = n;
}

static gboolean
my_hash_callback_remove (gpointer key,
                         gpointer value,
                         gpointer user_data)
{
  int *d = value;

  if ((*d) % 2)
    return TRUE;

  return FALSE;
}

static void
my_hash_callback_remove_test (gpointer key,
                              gpointer value,
                              gpointer user_data)
{
  int *d = value;

  if ((*d) % 2)
    g_assert_not_reached ();
}

static void
my_hash_callback (gpointer key,
                  gpointer value,
                  gpointer user_data)
{
  handle_pair (key, value, user_data);
}

static guint
my_hash (gconstpointer key)
{
  return (guint) *((const gint*) key);
}

static gboolean
my_hash_equal (gconstpointer a,
               gconstpointer b)
{
  return *((const gint*) a) == *((const gint*) b);
}



/*
 * This is a simplified version of the pathalias hashing function.
 * Thanks to Steve Belovin and Peter Honeyman
 *
 * hash a string into a long int.  31 bit crc (from andrew appel).
 * the crc table is computed at run time by crcinit() -- we could
 * precompute, but it takes 1 clock tick on a 750.
 *
 * This fast table calculation works only if POLY is a prime polynomial
 * in the field of integers modulo 2.  Since the coefficients of a
 * 32-bit polynomial won't fit in a 32-bit word, the high-order bit is
 * implicit.  IT MUST ALSO BE THE CASE that the coefficients of orders
 * 31 down to 25 are zero.  Happily, we have candidates, from
 * E. J.  Watson, "Primitive Polynomials (Mod 2)", Math. Comp. 16 (1962):
 *      x^32 + x^7 + x^5 + x^3 + x^2 + x^1 + x^0
 *      x^31 + x^3 + x^0
 *
 * We reverse the bits to get:
 *      111101010000000000000000000000001 but drop the last 1
 *         f   5   0   0   0   0   0   0
 *      010010000000000000000000000000001 ditto, for 31-bit crc
 *         4   8   0   0   0   0   0   0
 */

#define POLY 0x48000000L        /* 31-bit polynomial (avoids sign problems) */

static guint CrcTable[128];

/*
 - crcinit - initialize tables for hash function
 */
static void crcinit(void)
{
  int i, j;
  guint sum;

  for (i = 0; i < 128; ++i)
    {
      sum = 0L;
      for (j = 7 - 1; j >= 0; --j)
        if (i & (1 << j))
          sum ^= POLY >> j;
      CrcTable[i] = sum;
    }
}

/*
 - hash - Honeyman's nice hashing function
 */
static guint
honeyman_hash (gconstpointer key)
{
  const gchar *name = (const gchar *) key;
  gint size;
  guint sum = 0;

  g_assert (name != NULL);
  g_assert (*name != 0);

  size = strlen (name);

  while (size--)
    sum = (sum >> 7) ^ CrcTable[(sum ^ (*name++)) & 0x7f];

  return sum;
}


static gboolean
second_hash_cmp (gconstpointer a, gconstpointer b)
{
  return strcmp (a, b) == 0;
}



static guint
one_hash (gconstpointer key)
{
  return 1;
}


static void
not_even_foreach (gpointer key,
                  gpointer value,
                  gpointer user_data)
{
  const char *_key = (const char *) key;
  const char *_value = (const char *) value;
  int i;
  char val [20];

  g_assert (_key != NULL);
  g_assert (*_key != 0);
  g_assert (_value != NULL);
  g_assert (*_value != 0);

  i = atoi (_key);

  sprintf (val, "%d value", i);
  g_assert (strcmp (_value, val) == 0);

  g_assert ((i % 2) != 0);
  g_assert (i != 3);
}

static gboolean
remove_even_foreach (gpointer key,
                     gpointer value,
                     gpointer user_data)
{
  const char *_key = (const char *) key;
  const char *_value = (const char *) value;
  int i;
  char val [20];

  g_assert (_key != NULL);
  g_assert (*_key != 0);
  g_assert (_value != NULL);
  g_assert (*_value != 0);

  i = atoi (_key);

  sprintf (val, "%d value", i);
  g_assert (strcmp (_value, val) == 0);

  return ((i % 2) == 0) ? TRUE : FALSE;
}




static void
second_hash_test (gconstpointer d)
{
  gboolean simple_hash = GPOINTER_TO_INT (d);

  int       i;
  char      key[20] = "", val[20]="", *v, *orig_key, *orig_val;
  GHashTable     *h;
  gboolean found;

  crcinit ();

  h = g_hash_table_new_full (simple_hash ? one_hash : honeyman_hash,
                             second_hash_cmp,
                             g_free, g_free);
  g_assert (h != NULL);
  for (i = 0; i < 20; i++)
    {
      sprintf (key, "%d", i);
      g_assert (atoi (key) == i);

      sprintf (val, "%d value", i);
      g_assert (atoi (val) == i);

      g_hash_table_insert (h, g_strdup (key), g_strdup (val));
    }

  g_assert (g_hash_table_size (h) == 20);

  for (i = 0; i < 20; i++)
    {
      sprintf (key, "%d", i);
      g_assert (atoi(key) == i);

      v = (char *) g_hash_table_lookup (h, key);

      g_assert (v != NULL);
      g_assert (*v != 0);
      g_assert (atoi (v) == i);
    }

  sprintf (key, "%d", 3);
  g_hash_table_remove (h, key);
  g_assert (g_hash_table_size (h) == 19);
  g_hash_table_foreach_remove (h, remove_even_foreach, NULL);
  g_assert (g_hash_table_size (h) == 9);
  g_hash_table_foreach (h, not_even_foreach, NULL);

  for (i = 0; i < 20; i++)
    {
      sprintf (key, "%d", i);
      g_assert (atoi(key) == i);

      sprintf (val, "%d value", i);
      g_assert (atoi (val) == i);

      orig_key = orig_val = NULL;
      found = g_hash_table_lookup_extended (h, key,
                                            (gpointer)&orig_key,
                                            (gpointer)&orig_val);
      if ((i % 2) == 0 || i == 3)
        {
          g_assert (!found);
          continue;
        }

      g_assert (found);

      g_assert (orig_key != NULL);
      g_assert (strcmp (key, orig_key) == 0);

      g_assert (orig_val != NULL);
      g_assert (strcmp (val, orig_val) == 0);
    }

  g_hash_table_destroy (h);
}

static gboolean
find_first (gpointer key,
            gpointer value,
            gpointer user_data)
{
  gint *v = value;
  gint *test = user_data;
  return (*v == *test);
}

static void
direct_hash_test (void)
{
  gint       i, rc;
  GHashTable     *h;

  h = g_hash_table_new (NULL, NULL);
  g_assert (h != NULL);
  for (i = 1; i <= 20; i++)
    g_hash_table_insert (h, GINT_TO_POINTER (i),
                         GINT_TO_POINTER (i + 42));

  g_assert (g_hash_table_size (h) == 20);

  for (i = 1; i <= 20; i++)
    {
      rc = GPOINTER_TO_INT (g_hash_table_lookup (h, GINT_TO_POINTER (i)));

      g_assert (rc != 0);
      g_assert ((rc - 42) == i);
    }

  g_hash_table_destroy (h);
}

static void
direct_hash_test2 (void)
{
  gint       i, rc;
  GHashTable     *h;

  h = g_hash_table_new (g_direct_hash, g_direct_equal);
  g_assert (h != NULL);
  for (i = 1; i <= 20; i++)
    g_hash_table_insert (h, GINT_TO_POINTER (i),
                         GINT_TO_POINTER (i + 42));

  g_assert (g_hash_table_size (h) == 20);

  for (i = 1; i <= 20; i++)
    {
      rc = GPOINTER_TO_INT (g_hash_table_lookup (h, GINT_TO_POINTER (i)));

      g_assert (rc != 0);
      g_assert ((rc - 42) == i);
    }

  g_hash_table_destroy (h);
}

static void
int_hash_test (void)
{
  gint       i, rc;
  GHashTable     *h;
  gint     values[20];
  gint key;

  h = g_hash_table_new (g_int_hash, g_int_equal);
  g_assert (h != NULL);
  for (i = 0; i < 20; i++)
    {
      values[i] = i + 42;
      g_hash_table_insert (h, &values[i], GINT_TO_POINTER (i + 42));
    }

  g_assert (g_hash_table_size (h) == 20);

  for (i = 0; i < 20; i++)
    {
      key = i + 42;
      rc = GPOINTER_TO_INT (g_hash_table_lookup (h, &key));

      g_assert_cmpint (rc, ==, i + 42);
    }

  g_hash_table_destroy (h);
}

static void
int64_hash_test (void)
{
  gint       i, rc;
  GHashTable     *h;
  gint64     values[20];
  gint64 key;

  h = g_hash_table_new (g_int64_hash, g_int64_equal);
  g_assert (h != NULL);
  for (i = 0; i < 20; i++)
    {
      values[i] = i + 42;
      g_hash_table_insert (h, &values[i], GINT_TO_POINTER (i + 42));
    }

  g_assert (g_hash_table_size (h) == 20);

  for (i = 0; i < 20; i++)
    {
      key = i + 42;
      rc = GPOINTER_TO_INT (g_hash_table_lookup (h, &key));

      g_assert_cmpint (rc, ==, i + 42);
    }

  g_hash_table_destroy (h);
}

static void
double_hash_test (void)
{
  gint       i, rc;
  GHashTable     *h;
  gdouble values[20];
  gdouble key;

  h = g_hash_table_new (g_double_hash, g_double_equal);
  g_assert (h != NULL);
  for (i = 0; i < 20; i++)
    {
      values[i] = i + 42.5;
      g_hash_table_insert (h, &values[i], GINT_TO_POINTER (i + 42));
    }

  g_assert (g_hash_table_size (h) == 20);

  for (i = 0; i < 20; i++)
    {
      key = i + 42.5;
      rc = GPOINTER_TO_INT (g_hash_table_lookup (h, &key));

      g_assert_cmpint (rc, ==, i + 42);
    }

  g_hash_table_destroy (h);
}

static void
string_free (gpointer data)
{
  GString *s = data;

  g_string_free (s, TRUE);
}

static void
string_hash_test (void)
{
  gint       i, rc;
  GHashTable     *h;
  GString *s;

  h = g_hash_table_new_full ((GHashFunc)g_string_hash, (GEqualFunc)g_string_equal, string_free, NULL);
  g_assert (h != NULL);
  for (i = 0; i < 20; i++)
    {
      s = g_string_new ("");
      g_string_append_printf (s, "%d", i + 42);
      g_string_append_c (s, '.');
      g_string_prepend_unichar (s, 0x2301);
      g_hash_table_insert (h, s, GINT_TO_POINTER (i + 42));
    }

  g_assert (g_hash_table_size (h) == 20);

  s = g_string_new ("");
  for (i = 0; i < 20; i++)
    {
      g_string_assign (s, "");
      g_string_append_printf (s, "%d", i + 42);
      g_string_append_c (s, '.');
      g_string_prepend_unichar (s, 0x2301);
      rc = GPOINTER_TO_INT (g_hash_table_lookup (h, s));

      g_assert_cmpint (rc, ==, i + 42);
    }

  g_string_free (s, TRUE);
  g_hash_table_destroy (h);
}

static void
set_check (gpointer key,
           gpointer value,
           gpointer user_data)
{
  int *pi = user_data;
  if (key != value)
    g_assert_not_reached ();

  g_assert_cmpint (atoi (key) % 7, ==, 2);

  (*pi)++;
}

static void
set_hash_test (void)
{
  GHashTable *hash_table =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  int i;

  for (i = 2; i < 5000; i += 7)
    {
      char *s = g_strdup_printf ("%d", i);
      g_assert (g_hash_table_add (hash_table, s));
    }

  g_assert (!g_hash_table_add (hash_table, g_strdup_printf ("%d", 2)));

  i = 0;
  g_hash_table_foreach (hash_table, set_check, &i);
  g_assert_cmpint (i, ==, g_hash_table_size (hash_table));

  g_assert (g_hash_table_contains (hash_table, "2"));
  g_assert (g_hash_table_contains (hash_table, "9"));
  g_assert (!g_hash_table_contains (hash_table, "a"));

  /* this will cause the hash table to loose set nature */
  g_assert (g_hash_table_insert (hash_table, g_strdup ("a"), "b"));
  g_assert (!g_hash_table_insert (hash_table, g_strdup ("a"), "b"));

  g_assert (g_hash_table_replace (hash_table, g_strdup ("c"), "d"));
  g_assert (!g_hash_table_replace (hash_table, g_strdup ("c"), "d"));

  g_assert_cmpstr (g_hash_table_lookup (hash_table, "2"), ==, "2");
  g_assert_cmpstr (g_hash_table_lookup (hash_table, "a"), ==, "b");

  g_hash_table_destroy (hash_table);
}


static void
test_hash_misc (void)
{
  GHashTable *hash_table;
  gint i;
  gint value = 120;
  gint *pvalue;
  GList *keys, *values;
  gint keys_len, values_len;
  GHashTableIter iter;
  gpointer ikey, ivalue;
  int result_array[10000];
  int n_array[1];

  hash_table = g_hash_table_new (my_hash, my_hash_equal);
  fill_hash_table_and_array (hash_table);
  pvalue = g_hash_table_find (hash_table, find_first, &value);
  if (!pvalue || *pvalue != value)
    g_assert_not_reached();

  keys = g_hash_table_get_keys (hash_table);
  if (!keys)
    g_assert_not_reached ();

  values = g_hash_table_get_values (hash_table);
  if (!values)
    g_assert_not_reached ();

  keys_len = g_list_length (keys);
  values_len = g_list_length (values);
  if (values_len != keys_len &&  keys_len != g_hash_table_size (hash_table))
    g_assert_not_reached ();

  g_list_free (keys);
  g_list_free (values);

  init_result_array (result_array);
  g_hash_table_iter_init (&iter, hash_table);
  for (i = 0; i < 10000; i++)
    {
      g_assert (g_hash_table_iter_next (&iter, &ikey, &ivalue));

      handle_pair (ikey, ivalue, result_array);

      if (i % 2)
        g_hash_table_iter_remove (&iter);
    }
  g_assert (! g_hash_table_iter_next (&iter, &ikey, &ivalue));
  g_assert (g_hash_table_size (hash_table) == 5000);
  verify_result_array (result_array);

  fill_hash_table_and_array (hash_table);

  init_result_array (result_array);
  g_hash_table_foreach (hash_table, my_hash_callback, result_array);
  verify_result_array (result_array);

  for (i = 0; i < 10000; i++)
    g_hash_table_remove (hash_table, &array[i]);

  fill_hash_table_and_array (hash_table);

  if (g_hash_table_foreach_remove (hash_table, my_hash_callback_remove, NULL) != 5000 ||
      g_hash_table_size (hash_table) != 5000)
    g_assert_not_reached();

  g_hash_table_foreach (hash_table, my_hash_callback_remove_test, NULL);
  g_hash_table_destroy (hash_table);

  hash_table = g_hash_table_new (my_hash, my_hash_equal);
  fill_hash_table_and_array (hash_table);

  n_array[0] = 1;

  g_hash_table_iter_init (&iter, hash_table);
  for (i = 0; i < 10000; i++)
    {
      g_assert (g_hash_table_iter_next (&iter, &ikey, &ivalue));
      g_hash_table_iter_replace (&iter, &n_array[0]);
    }

  g_hash_table_iter_init (&iter, hash_table);
  for (i = 0; i < 10000; i++)
    {
      g_assert (g_hash_table_iter_next (&iter, &ikey, &ivalue));

      g_assert (ivalue == &n_array[0]);
    }

  g_hash_table_destroy (hash_table);
}

static gint destroy_counter;

static void
value_destroy (gpointer value)
{
  destroy_counter++;
}

static void
test_hash_ref (void)
{
  GHashTable *h;
  GHashTableIter iter;
  gchar *key, *value;
  gboolean abc_seen = FALSE;
  gboolean cde_seen = FALSE;
  gboolean xyz_seen = FALSE;

  h = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, value_destroy);
  g_hash_table_insert (h, "abc", "ABC");
  g_hash_table_insert (h, "cde", "CDE");
  g_hash_table_insert (h, "xyz", "XYZ");

  g_assert_cmpint (g_hash_table_size (h), == , 3);

  g_hash_table_iter_init (&iter, h);

  while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&value))
    {
      if (strcmp (key, "abc") == 0)
        {
          g_assert_cmpstr (value, ==, "ABC");
          abc_seen = TRUE;
          g_hash_table_iter_steal (&iter);
        }
      else if (strcmp (key, "cde") == 0)
        {
          g_assert_cmpstr (value, ==, "CDE");
          cde_seen = TRUE;
        }
      else if (strcmp (key, "xyz") == 0)
        {
          g_assert_cmpstr (value, ==, "XYZ");
          xyz_seen = TRUE;
        }
    }
  g_assert_cmpint (destroy_counter, ==, 0);

  g_assert (g_hash_table_iter_get_hash_table (&iter) == h);
  g_assert (abc_seen && cde_seen && xyz_seen);
  g_assert_cmpint (g_hash_table_size (h), == , 2);

  g_hash_table_ref (h);
  g_hash_table_destroy (h);
  g_assert_cmpint (g_hash_table_size (h), == , 0);
  g_assert_cmpint (destroy_counter, ==, 2);
  g_hash_table_insert (h, "uvw", "UVW");
  g_hash_table_unref (h);
  g_assert_cmpint (destroy_counter, ==, 3);
}

static guint
null_safe_str_hash (gconstpointer key)
{
  if (key == NULL)
    return 0;
  else
    return g_str_hash (key);
}

static gboolean
null_safe_str_equal (gconstpointer a, gconstpointer b)
{
  return g_strcmp0 (a, b) == 0;
}

static void
test_lookup_null_key (void)
{
  GHashTable *h;
  gboolean res;
  gpointer key;
  gpointer value;

  g_test_bug ("642944");

  h = g_hash_table_new (null_safe_str_hash, null_safe_str_equal);
  g_hash_table_insert (h, "abc", "ABC");

  res = g_hash_table_lookup_extended (h, NULL, &key, &value);
  g_assert (!res);

  g_hash_table_insert (h, NULL, "NULL");

  res = g_hash_table_lookup_extended (h, NULL, &key, &value);
  g_assert (res);
  g_assert_cmpstr (value, ==, "NULL");

  g_hash_table_unref (h);
}

static gint destroy_key_counter;

static void
key_destroy (gpointer key)
{
  destroy_key_counter++;
}

static void
test_remove_all (void)
{
  GHashTable *h;
  gboolean res;

  h = g_hash_table_new_full (g_str_hash, g_str_equal, key_destroy, value_destroy);

  g_hash_table_insert (h, "abc", "cde");
  g_hash_table_insert (h, "cde", "xyz");
  g_hash_table_insert (h, "xyz", "abc");

  destroy_counter = 0;
  destroy_key_counter = 0;

  g_hash_table_steal_all (h);
  g_assert_cmpint (destroy_counter, ==, 0);
  g_assert_cmpint (destroy_key_counter, ==, 0);

  g_hash_table_insert (h, "abc", "ABC");
  g_hash_table_insert (h, "cde", "CDE");
  g_hash_table_insert (h, "xyz", "XYZ");

  res = g_hash_table_steal (h, "nosuchkey");
  g_assert (!res);
  g_assert_cmpint (destroy_counter, ==, 0);
  g_assert_cmpint (destroy_key_counter, ==, 0);

  res = g_hash_table_steal (h, "xyz");
  g_assert (res);
  g_assert_cmpint (destroy_counter, ==, 0);
  g_assert_cmpint (destroy_key_counter, ==, 0);

  g_hash_table_remove_all (h);
  g_assert_cmpint (destroy_counter, ==, 2);
  g_assert_cmpint (destroy_key_counter, ==, 2);

  g_hash_table_remove_all (h);
  g_assert_cmpint (destroy_counter, ==, 2);
  g_assert_cmpint (destroy_key_counter, ==, 2);

  g_hash_table_unref (h);
}

GHashTable *recursive_destruction_table = NULL;
static void
recursive_value_destroy (gpointer value)
{
  destroy_counter++;

  if (recursive_destruction_table)
    g_hash_table_remove (recursive_destruction_table, value);
}

static void
test_recursive_remove_all_subprocess (void)
{
  GHashTable *h;

  h = g_hash_table_new_full (g_str_hash, g_str_equal, key_destroy, recursive_value_destroy);
  recursive_destruction_table = h;

  /* Add more items compared to test_remove_all, as it would not fail otherwise. */
  g_hash_table_insert (h, "abc", "cde");
  g_hash_table_insert (h, "cde", "fgh");
  g_hash_table_insert (h, "fgh", "ijk");
  g_hash_table_insert (h, "ijk", "lmn");
  g_hash_table_insert (h, "lmn", "opq");
  g_hash_table_insert (h, "opq", "rst");
  g_hash_table_insert (h, "rst", "uvw");
  g_hash_table_insert (h, "uvw", "xyz");
  g_hash_table_insert (h, "xyz", "abc");

  destroy_counter = 0;
  destroy_key_counter = 0;

  g_hash_table_remove_all (h);
  g_assert_cmpint (destroy_counter, ==, 9);
  g_assert_cmpint (destroy_key_counter, ==, 9);

  g_hash_table_unref (h);
}

static void
test_recursive_remove_all (void)
{
  g_test_trap_subprocess ("/hash/recursive-remove-all/subprocess", 1000000, 0);
  g_test_trap_assert_passed ();
}

typedef struct {
  gint ref_count;
  const gchar *key;
} RefCountedKey;

static guint
hash_func (gconstpointer key)
{
  const RefCountedKey *rkey = key;

  return g_str_hash (rkey->key);
}

static gboolean
eq_func (gconstpointer a, gconstpointer b)
{
  const RefCountedKey *aa = a;
  const RefCountedKey *bb = b;

  return g_strcmp0 (aa->key, bb->key) == 0;
}

static void
key_unref (gpointer data)
{
  RefCountedKey *key = data;

  g_assert (key->ref_count > 0);

  key->ref_count -= 1;

  if (key->ref_count == 0)
    g_free (key);
}

static RefCountedKey *
key_ref (RefCountedKey *key)
{
  key->ref_count += 1;

  return key;
}

static RefCountedKey *
key_new (const gchar *key)
{
  RefCountedKey *rkey;

  rkey = g_new (RefCountedKey, 1);

  rkey->ref_count = 1;
  rkey->key = key;

  return rkey;
}

static void
set_ref_hash_test (void)
{
  GHashTable *h;
  RefCountedKey *key1;
  RefCountedKey *key2;

  h = g_hash_table_new_full (hash_func, eq_func, key_unref, key_unref);

  key1 = key_new ("a");
  key2 = key_new ("a");

  g_assert_cmpint (key1->ref_count, ==, 1);
  g_assert_cmpint (key2->ref_count, ==, 1);

  g_hash_table_insert (h, key_ref (key1), key_ref (key1));

  g_assert_cmpint (key1->ref_count, ==, 3);
  g_assert_cmpint (key2->ref_count, ==, 1);

  g_hash_table_replace (h, key_ref (key2), key_ref (key2));

  g_assert_cmpint (key1->ref_count, ==, 1);
  g_assert_cmpint (key2->ref_count, ==, 3);

  g_hash_table_remove (h, key1);

  g_assert_cmpint (key1->ref_count, ==, 1);
  g_assert_cmpint (key2->ref_count, ==, 1);

  g_hash_table_unref (h);

  key_unref (key1);
  key_unref (key2);
}

GHashTable *h;

typedef struct {
    gchar *string;
    gboolean freed;
} FakeFreeData;

GPtrArray *fake_free_data;

static void
fake_free (gpointer dead)
{
  guint i;

  for (i = 0; i < fake_free_data->len; i++)
    {
      FakeFreeData *ffd = g_ptr_array_index (fake_free_data, i);

      if (ffd->string == (gchar *) dead)
        {
          g_assert (!ffd->freed);
          ffd->freed = TRUE;
          return;
        }
    }

  g_assert_not_reached ();
}

static void
value_destroy_insert (gpointer value)
{
  g_hash_table_remove_all (h);
}

static void
test_destroy_modify (void)
{
  FakeFreeData *ffd;
  guint i;

  g_test_bug ("650459");

  fake_free_data = g_ptr_array_new ();

  h = g_hash_table_new_full (g_str_hash, g_str_equal, fake_free, value_destroy_insert);

  ffd = g_new0 (FakeFreeData, 1);
  ffd->string = g_strdup ("a");
  g_ptr_array_add (fake_free_data, ffd);
  g_hash_table_insert (h, ffd->string, "b");

  ffd = g_new0 (FakeFreeData, 1);
  ffd->string = g_strdup ("c");
  g_ptr_array_add (fake_free_data, ffd);
  g_hash_table_insert (h, ffd->string, "d");

  ffd = g_new0 (FakeFreeData, 1);
  ffd->string = g_strdup ("e");
  g_ptr_array_add (fake_free_data, ffd);
  g_hash_table_insert (h, ffd->string, "f");

  ffd = g_new0 (FakeFreeData, 1);
  ffd->string = g_strdup ("g");
  g_ptr_array_add (fake_free_data, ffd);
  g_hash_table_insert (h, ffd->string, "h");

  ffd = g_new0 (FakeFreeData, 1);
  ffd->string = g_strdup ("h");
  g_ptr_array_add (fake_free_data, ffd);
  g_hash_table_insert (h, ffd->string, "k");

  ffd = g_new0 (FakeFreeData, 1);
  ffd->string = g_strdup ("a");
  g_ptr_array_add (fake_free_data, ffd);
  g_hash_table_insert (h, ffd->string, "c");

  g_hash_table_remove (h, "c");

  /* that removed everything... */
  for (i = 0; i < fake_free_data->len; i++)
    {
      FakeFreeData *ffd = g_ptr_array_index (fake_free_data, i);

      g_assert (ffd->freed);
      g_free (ffd->string);
      g_free (ffd);
    }

  g_ptr_array_unref (fake_free_data);

  /* ... so this is a no-op */
  g_hash_table_remove (h, "e");

  g_hash_table_unref (h);
}

static gboolean
find_str (gpointer key, gpointer value, gpointer data)
{
  return g_str_equal (key, data);
}

static void
test_find (void)
{
  GHashTable *hash;
  const gchar *value;

  hash = g_hash_table_new (g_str_hash, g_str_equal);

  g_hash_table_insert (hash, "a", "A");
  g_hash_table_insert (hash, "b", "B");
  g_hash_table_insert (hash, "c", "C");
  g_hash_table_insert (hash, "d", "D");
  g_hash_table_insert (hash, "e", "E");
  g_hash_table_insert (hash, "f", "F");

  value = g_hash_table_find (hash, find_str, "a");
  g_assert_cmpstr (value, ==, "A");

  value = g_hash_table_find (hash, find_str, "b");
  g_assert_cmpstr (value, ==, "B");

  value = g_hash_table_find (hash, find_str, "c");
  g_assert_cmpstr (value, ==, "C");

  value = g_hash_table_find (hash, find_str, "d");
  g_assert_cmpstr (value, ==, "D");

  value = g_hash_table_find (hash, find_str, "e");
  g_assert_cmpstr (value, ==, "E");

  value = g_hash_table_find (hash, find_str, "f");
  g_assert_cmpstr (value, ==, "F");

  value = g_hash_table_find (hash, find_str, "0");
  g_assert (value == NULL);

  g_hash_table_unref (hash);
}

gboolean seen_key[6];

static void
foreach_func (gpointer key, gpointer value, gpointer data)
{
  seen_key[((char*)key)[0] - 'a'] = TRUE;
}

static void
test_foreach (void)
{
  GHashTable *hash;
  gint i;

  hash = g_hash_table_new (g_str_hash, g_str_equal);

  g_hash_table_insert (hash, "a", "A");
  g_hash_table_insert (hash, "b", "B");
  g_hash_table_insert (hash, "c", "C");
  g_hash_table_insert (hash, "d", "D");
  g_hash_table_insert (hash, "e", "E");
  g_hash_table_insert (hash, "f", "F");

  for (i = 0; i < 6; i++)
    seen_key[i] = FALSE;

  g_hash_table_foreach (hash, foreach_func, NULL);

  for (i = 0; i < 6; i++)
    g_assert (seen_key[i]);

  g_hash_table_unref (hash);
}

static gboolean
foreach_steal_func (gpointer key, gpointer value, gpointer data)
{
  GHashTable *hash2 = data;

  if (strstr ("ace", (gchar*)key))
    {
      g_hash_table_insert (hash2, key, value);
      return TRUE;
    }

  return FALSE;
}


static void
test_foreach_steal (void)
{
  GHashTable *hash;
  GHashTable *hash2;

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  hash2 = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  g_hash_table_insert (hash, g_strdup ("a"), g_strdup ("A"));
  g_hash_table_insert (hash, g_strdup ("b"), g_strdup ("B"));
  g_hash_table_insert (hash, g_strdup ("c"), g_strdup ("C"));
  g_hash_table_insert (hash, g_strdup ("d"), g_strdup ("D"));
  g_hash_table_insert (hash, g_strdup ("e"), g_strdup ("E"));
  g_hash_table_insert (hash, g_strdup ("f"), g_strdup ("F"));

  g_hash_table_foreach_steal (hash, foreach_steal_func, hash2);

  g_assert_cmpint (g_hash_table_size (hash), ==, 3);
  g_assert_cmpint (g_hash_table_size (hash2), ==, 3);

  g_assert_cmpstr (g_hash_table_lookup (hash2, "a"), ==, "A");
  g_assert_cmpstr (g_hash_table_lookup (hash, "b"), ==, "B");
  g_assert_cmpstr (g_hash_table_lookup (hash2, "c"), ==, "C");
  g_assert_cmpstr (g_hash_table_lookup (hash, "d"), ==, "D");
  g_assert_cmpstr (g_hash_table_lookup (hash2, "e"), ==, "E");
  g_assert_cmpstr (g_hash_table_lookup (hash, "f"), ==, "F");

  g_hash_table_unref (hash);
  g_hash_table_unref (hash2);
}

struct _GHashTable
{
  gint             size;
  gint             mod;
  guint            mask;
  gint             nnodes;
  gint             noccupied;  /* nnodes + tombstones */

  gpointer        *keys;
  guint           *hashes;
  gpointer        *values;

  GHashFunc        hash_func;
  GEqualFunc       key_equal_func;
  volatile gint    ref_count;

#ifndef G_DISABLE_ASSERT
  int              version;
#endif
  GDestroyNotify   key_destroy_func;
  GDestroyNotify   value_destroy_func;
};

static void
count_keys (GHashTable *h, gint *unused, gint *occupied, gint *tombstones)
{
  gint i;

  *unused = 0;
  *occupied = 0;
  *tombstones = 0;
  for (i = 0; i < h->size; i++)
    {
      if (h->hashes[i] == 0)
        (*unused)++;
      else if (h->hashes[i] == 1)
        (*tombstones)++;
      else
        (*occupied)++;
    }
}

static void
check_data (GHashTable *h)
{
  gint i;

  for (i = 0; i < h->size; i++)
    {
      if (h->hashes[i] < 2)
        {
          g_assert (h->keys[i] == NULL);
          g_assert (h->values[i] == NULL);
        }
      else
        {
          g_assert_cmpint (h->hashes[i], ==, h->hash_func (h->keys[i]));
        }
    }
}

static void
check_consistency (GHashTable *h)
{
  gint unused;
  gint occupied;
  gint tombstones;

  count_keys (h, &unused, &occupied, &tombstones);

  g_assert_cmpint (occupied, ==, h->nnodes);
  g_assert_cmpint (occupied + tombstones, ==, h->noccupied);
  g_assert_cmpint (occupied + tombstones + unused, ==, h->size);

  check_data (h);
}

static void
check_counts (GHashTable *h, gint occupied, gint tombstones)
{
  g_assert_cmpint (occupied, ==, h->nnodes);
  g_assert_cmpint (occupied + tombstones, ==, h->noccupied);
}

static void
trivial_key_destroy (gpointer key)
{
}

static void
test_internal_consistency (void)
{
  GHashTable *h;

  h = g_hash_table_new_full (g_str_hash, g_str_equal, trivial_key_destroy, NULL);

  check_counts (h, 0, 0);
  check_consistency (h);

  g_hash_table_insert (h, "a", "A");
  g_hash_table_insert (h, "b", "B");
  g_hash_table_insert (h, "c", "C");
  g_hash_table_insert (h, "d", "D");
  g_hash_table_insert (h, "e", "E");
  g_hash_table_insert (h, "f", "F");

  check_counts (h, 6, 0);
  check_consistency (h);

  g_hash_table_remove (h, "a");
  check_counts (h, 5, 1);
  check_consistency (h);

  g_hash_table_remove (h, "b");
  check_counts (h, 4, 2);
  check_consistency (h);

  g_hash_table_insert (h, "c", "c");
  check_counts (h, 4, 2);
  check_consistency (h);

  g_hash_table_insert (h, "a", "A");
  check_counts (h, 5, 1);
  check_consistency (h);

  g_hash_table_remove_all (h);
  check_counts (h, 0, 0);
  check_consistency (h);

  g_hash_table_unref (h);
}

static void
my_key_free (gpointer v)
{
  gchar *s = v;
  g_assert (s[0] != 'x');
  s[0] = 'x';
  g_free (v);
}

static void
my_value_free (gpointer v)
{
  gchar *s = v;
  g_assert (s[0] != 'y');
  s[0] = 'y';
  g_free (v);
}

static void
test_iter_replace (void)
{
  GHashTable *h;
  GHashTableIter iter;
  gpointer k, v;
  gchar *s;

  g_test_bug ("662544");

  h = g_hash_table_new_full (g_str_hash, g_str_equal, my_key_free, my_value_free);

  g_hash_table_insert (h, g_strdup ("A"), g_strdup ("a"));
  g_hash_table_insert (h, g_strdup ("B"), g_strdup ("b"));
  g_hash_table_insert (h, g_strdup ("C"), g_strdup ("c"));

  g_hash_table_iter_init (&iter, h);

  while (g_hash_table_iter_next (&iter, &k, &v))
    {
       s = (gchar*)v;
       g_assert (g_ascii_islower (s[0]));
       g_hash_table_iter_replace (&iter, g_strdup (k));
    }

  g_hash_table_unref (h);
}

static void
replace_first_character (gchar *string)
{
  string[0] = 'b';
}

static void
test_set_insert_corruption (void)
{
  GHashTable *hash_table =
    g_hash_table_new_full (g_str_hash, g_str_equal,
        (GDestroyNotify) replace_first_character, NULL);
  GHashTableIter iter;
  gchar a[] = "foo";
  gchar b[] = "foo";
  gpointer key, value;

  g_test_bug ("692815");

  g_hash_table_insert (hash_table, a, a);
  g_assert (g_hash_table_contains (hash_table, "foo"));

  g_hash_table_insert (hash_table, b, b);

  g_assert_cmpuint (g_hash_table_size (hash_table), ==, 1);
  g_hash_table_iter_init (&iter, hash_table);
  if (!g_hash_table_iter_next (&iter, &key, &value))
    g_assert_not_reached();

  /* per the documentation to g_hash_table_insert(), 'b' has now been freed,
   * and the sole key in 'hash_table' should be 'a'.
   */
  g_assert (key != b);
  g_assert (key == a);

  g_assert_cmpstr (b, ==, "boo");

  /* g_hash_table_insert() also says that the value should now be 'b',
   * which is probably not what the caller intended but is precisely what they
   * asked for.
   */
  g_assert (value == b);

  /* even though the hash has now been de-set-ified: */
  g_assert (g_hash_table_contains (hash_table, "foo"));

  g_hash_table_unref (hash_table);
}

static void
test_set_to_strv (void)
{
  GHashTable *set;
  gchar **strv;
  guint n;

  set = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  g_hash_table_add (set, g_strdup ("xyz"));
  g_hash_table_add (set, g_strdup ("xyz"));
  g_hash_table_add (set, g_strdup ("abc"));
  strv = (gchar **) g_hash_table_get_keys_as_array (set, &n);
  g_hash_table_steal_all (set);
  g_hash_table_unref (set);
  g_assert_cmpint (n, ==, 2);
  n = g_strv_length (strv);
  g_assert_cmpint (n, ==, 2);
  if (g_str_equal (strv[0], "abc"))
    g_assert_cmpstr (strv[1], ==, "xyz");
  else
    {
    g_assert_cmpstr (strv[0], ==, "xyz");
    g_assert_cmpstr (strv[1], ==, "abc");
    }
  g_strfreev (strv);
}

static gboolean
is_prime (guint p)
{
  guint i;

  if (p % 2 == 0)
    return FALSE;

  i = 3;
  while (TRUE)
    {
      if (i * i > p)
        return TRUE;

      if (p % i == 0)
        return FALSE;

      i += 2;       
    }
}

static void
test_primes (void)
{
  guint p, q;
  gdouble r, min, max;

  max = 1.0;
  min = 10.0;
  q = 1;
  while (1) {
    p = q;
    q = g_spaced_primes_closest (p);
    g_assert (is_prime (q));
    if (p == 1) continue;
    if (q == p) break;
    r = q / (gdouble) p;
    min = MIN (min, r);
    max = MAX (max, r);
  };

  g_assert_cmpfloat (1.3, <, min);
  g_assert_cmpfloat (max, <, 2.0);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/hash/misc", test_hash_misc);
  g_test_add_data_func ("/hash/one", GINT_TO_POINTER (TRUE), second_hash_test);
  g_test_add_data_func ("/hash/honeyman", GINT_TO_POINTER (FALSE), second_hash_test);
  g_test_add_func ("/hash/direct", direct_hash_test);
  g_test_add_func ("/hash/direct2", direct_hash_test2);
  g_test_add_func ("/hash/int", int_hash_test);
  g_test_add_func ("/hash/int64", int64_hash_test);
  g_test_add_func ("/hash/double", double_hash_test);
  g_test_add_func ("/hash/string", string_hash_test);
  g_test_add_func ("/hash/set", set_hash_test);
  g_test_add_func ("/hash/set-ref", set_ref_hash_test);
  g_test_add_func ("/hash/ref", test_hash_ref);
  g_test_add_func ("/hash/remove-all", test_remove_all);
  g_test_add_func ("/hash/recursive-remove-all", test_recursive_remove_all);
  g_test_add_func ("/hash/recursive-remove-all/subprocess", test_recursive_remove_all_subprocess);
  g_test_add_func ("/hash/find", test_find);
  g_test_add_func ("/hash/foreach", test_foreach);
  g_test_add_func ("/hash/foreach-steal", test_foreach_steal);

  /* tests for individual bugs */
  g_test_add_func ("/hash/lookup-null-key", test_lookup_null_key);
  g_test_add_func ("/hash/destroy-modify", test_destroy_modify);
  g_test_add_func ("/hash/consistency", test_internal_consistency);
  g_test_add_func ("/hash/iter-replace", test_iter_replace);
  g_test_add_func ("/hash/set-insert-corruption", test_set_insert_corruption);
  g_test_add_func ("/hash/set-to-strv", test_set_to_strv);
  g_test_add_func ("/hash/primes", test_primes);

  return g_test_run ();

}
