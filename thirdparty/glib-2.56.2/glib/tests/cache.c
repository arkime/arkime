/* Copyright (C) 2011 Red Hat, Inc.
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

/* We are testing some deprecated APIs here */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <glib.h>

static gint value_create_count = 0;
static gint value_destroy_count = 0;

static gpointer
value_create (gpointer key)
{
  gint *value;

  value_create_count++;

  value = g_new (gint, 1);
  *value = *(gint*)key * 2;

  return value;
}

static void
value_destroy (gpointer value)
{
  value_destroy_count++;
  g_free (value);
}

static gpointer
key_dup (gpointer key)
{
  gint *newkey;

  newkey = g_new (gint, 1);
  *newkey = *(gint*)key;

  return newkey;
}

static void
key_destroy (gpointer key)
{
  g_free (key);
}

static guint
key_hash (gconstpointer key)
{
  return *(guint*)key;
}

static guint
value_hash (gconstpointer value)
{
  return *(guint*)value;
}

static gboolean
key_equal (gconstpointer key1, gconstpointer key2)
{
  return *(gint*)key1 == *(gint*)key2;
}

static void
key_foreach (gpointer valuep, gpointer keyp, gpointer data)
{
  gint *count = data;
  gint *key = keyp;

  (*count)++;

  g_assert_cmpint (*key, ==, 2);
}

static void
value_foreach (gpointer keyp, gpointer nodep, gpointer data)
{
  gint *count = data;
  gint *key = keyp;

  (*count)++;

  g_assert_cmpint (*key, ==, 2);
}

static void
test_cache_basic (void)
{
  GCache *c;
  gint *key;
  gint *value;
  gint count;

  value_create_count = 0;
  value_destroy_count = 0;

  c = g_cache_new (value_create, value_destroy,
                   key_dup, key_destroy,
                   key_hash, value_hash, key_equal);

  key = g_new (gint, 1);
  *key = 2;

  value = g_cache_insert (c, key);
  g_assert_cmpint (*value, ==, 4);
  g_assert_cmpint (value_create_count, ==, 1);
  g_assert_cmpint (value_destroy_count, ==, 0);

  count = 0;
  g_cache_key_foreach (c, key_foreach, &count);
  g_assert_cmpint (count, ==, 1);

  count = 0;
  g_cache_value_foreach (c, value_foreach, &count);
  g_assert_cmpint (count, ==, 1);

  value = g_cache_insert (c, key);
  g_assert_cmpint (*value, ==, 4);
  g_assert_cmpint (value_create_count, ==, 1);
  g_assert_cmpint (value_destroy_count, ==, 0);

  g_cache_remove (c, value);
  g_assert_cmpint (value_create_count, ==, 1);
  g_assert_cmpint (value_destroy_count, ==, 0);

  g_cache_remove (c, value);
  g_assert_cmpint (value_create_count, ==, 1);
  g_assert_cmpint (value_destroy_count, ==, 1);

  value = g_cache_insert (c, key);
  g_assert_cmpint (*value, ==, 4);
  g_assert_cmpint (value_create_count, ==, 2);
  g_assert_cmpint (value_destroy_count, ==, 1);

  g_cache_remove (c, value);
  g_cache_destroy (c);
  g_free (key);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/cache/basic", test_cache_basic);

  return g_test_run ();

}
