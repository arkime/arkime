/* guuid.c
 *
 * Copyright (C) 2013-2015, 2017 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#undef G_DISABLE_ASSERT

#include <glib.h>
#include <string.h>

static void
test_guuid_string (void)
{
  g_assert_false (g_uuid_string_is_valid ("00010203-0405-0607-0809"));
  g_assert_false (g_uuid_string_is_valid ("zzzzzzzz-zzzz-zzzz-zzzz-zzzzzzzzzzzz"));
  g_assert_false (g_uuid_string_is_valid ("000102030405060708090a0b0c0d0e0f"));
  g_assert_false (g_uuid_string_is_valid ("{urn:uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6}"));
  g_assert_false (g_uuid_string_is_valid ("urn:uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6"));

  g_assert_true (g_uuid_string_is_valid ("00010203-0405-0607-0809-0a0b0c0d0e0f"));
  g_assert_true (g_uuid_string_is_valid ("7d444840-9dc0-11d1-b245-5ffdce74fad2"));
  g_assert_true (g_uuid_string_is_valid ("e902893a-9d22-3c7e-a7b8-d6e313b71d9f"));
  g_assert_true (g_uuid_string_is_valid ("6ba7b810-9dad-11d1-80b4-00c04fd430c8"));
}

static void
test_guuid_random (void)
{
  gchar *str1, *str2;

  str1 = g_uuid_string_random ();
  g_assert_cmpuint (strlen (str1), ==, 36);
  g_assert_true (g_uuid_string_is_valid (str1));

  str2 = g_uuid_string_random ();
  g_assert_cmpuint (strlen (str2), ==, 36);
  g_assert_true (g_uuid_string_is_valid (str2));
  g_assert_cmpstr (str1, !=, str2);

  g_free (str1);
  g_free (str2);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  /* GUuid Tests */
  g_test_add_func ("/uuid/string", test_guuid_string);
  g_test_add_func ("/uuid/random", test_guuid_random);

  return g_test_run ();
}
