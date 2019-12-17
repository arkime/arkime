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

#include <string.h>
#include <glib.h>

/* Test conversions between offsets and pointers */

static void test_utf8 (gconstpointer d)
{
  gint num_chars;
  const gchar **p;
  gint i, j;
  const gchar *string = d;

  g_assert (g_utf8_validate (string, -1, NULL));

  num_chars = g_utf8_strlen (string, -1);

  p = (const gchar **) g_malloc (num_chars * sizeof (gchar *));

  p[0] = string;
  for (i = 1; i < num_chars; i++)
    p[i] = g_utf8_next_char (p[i-1]);

  for (i = 0; i < num_chars; i++)
    for (j = 0; j < num_chars; j++)
      {
        g_assert (g_utf8_offset_to_pointer (p[i], j - i) == p[j]);
        g_assert (g_utf8_pointer_to_offset (p[i], p[j]) == j - i);
      }

  g_free (p);
}

gchar *longline = "asdasdas dsaf asfd as fdasdf asfd asdf as dfas dfasdf a"
"asd fasdf asdf asdf asd fasfd as fdasfd asdf as fdççççççççças ffsd asfd as fdASASASAs As"
"Asfdsf sdfg sdfg dsfg dfg sdfgsdfgsdfgsdfg sdfgsdfg sdfg sdfg sdf gsdfg sdfg sd"
"asd fasdf asdf asdf asd fasfd as fdaèèèèèèè òòòòòòòòòòòòsfd asdf as fdas ffsd asfd as fdASASASAs D"
"Asfdsf sdfg sdfg dsfg dfg sdfgsdfgsdfgsdfg sdfgsdfg sdfgùùùùùùùùùùùùùù sdfg sdf gsdfg sdfg sd"
"asd fasdf asdf asdf asd fasfd as fdasfd asd@@@@@@@f as fdas ffsd asfd as fdASASASAs D "
"Asfdsf sdfg sdfg dsfg dfg sdfgsdfgsdfgsdfg sdfgsdf€€€€€€€€€€€€€€€€€€g sdfg sdfg sdf gsdfg sdfg sd"
"asd fasdf asdf asdf asd fasfd as fdasfd asdf as fdas ffsd asfd as fdASASASAs D"
"Asfdsf sdfg sdfg dsfg dfg sdfgsdfgsdfgsdfg sdfgsdfg sdfg sdfg sdf gsdfg sdfg sd\n\nlalala\n";

static void
test_length (void)
{
  g_assert (g_utf8_strlen ("1234", -1) == 4);
  g_assert (g_utf8_strlen ("1234", 0) == 0);
  g_assert (g_utf8_strlen ("1234", 1) == 1);
  g_assert (g_utf8_strlen ("1234", 2) == 2);
  g_assert (g_utf8_strlen ("1234", 3) == 3);
  g_assert (g_utf8_strlen ("1234", 4) == 4);
  g_assert (g_utf8_strlen ("1234", 5) == 4);

  g_assert (g_utf8_strlen (longline, -1) == 762);
  g_assert (g_utf8_strlen (longline, strlen (longline)) == 762);
  g_assert (g_utf8_strlen (longline, 1024) == 762);

  g_assert (g_utf8_strlen (NULL, 0) == 0);

  g_assert (g_utf8_strlen ("a\340\250\201c", -1) == 3);
  g_assert (g_utf8_strlen ("a\340\250\201c", 1) == 1);
  g_assert (g_utf8_strlen ("a\340\250\201c", 2) == 1);
  g_assert (g_utf8_strlen ("a\340\250\201c", 3) == 1);
  g_assert (g_utf8_strlen ("a\340\250\201c", 4) == 2);
  g_assert (g_utf8_strlen ("a\340\250\201c", 5) == 3);
}

static void
test_find (void)
{
  /* U+0B0B Oriya Letter Vocalic R (\340\254\213)
   * U+10900 Phoenician Letter Alf (\360\220\244\200)
   * U+0041 Latin Capital Letter A (\101)
   * U+1EB6 Latin Capital Letter A With Breve And Dot Below (\341\272\266)
   */
  const gchar *str = "\340\254\213\360\220\244\200\101\341\272\266\0\101";
  const gchar *p = str + strlen (str);
  const gchar *q;

  q = g_utf8_find_prev_char (str, p);
  g_assert (q == str + 8);
  q = g_utf8_find_prev_char (str, q);
  g_assert (q == str + 7);
  q = g_utf8_find_prev_char (str, q);
  g_assert (q == str + 3);
  q = g_utf8_find_prev_char (str, q);
  g_assert (q == str);
  q = g_utf8_find_prev_char (str, q);
  g_assert (q == NULL);

  p = str + 2;
  q = g_utf8_find_next_char (p, NULL);
  g_assert (q == str + 3);
  q = g_utf8_find_next_char (q, NULL);
  g_assert (q == str + 7);

  q = g_utf8_find_next_char (p, str + 6);
  g_assert (q == str + 3);
  q = g_utf8_find_next_char (q, str + 6);
  g_assert (q == NULL);

  q = g_utf8_find_next_char (str, str);
  g_assert (q == NULL);

  q = g_utf8_find_next_char (str + strlen (str), NULL);
  g_assert (q == str + strlen (str) + 1);

  /* Check return values when reaching the end of the string,
   * with @end set and unset. */
  q = g_utf8_find_next_char (str + 10, NULL);
  g_assert_nonnull (q);
  g_assert (*q == '\0');

  q = g_utf8_find_next_char (str + 10, str + 11);
  g_assert_null (q);
}

int main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_data_func ("/utf8/offsets", longline, test_utf8);
  g_test_add_func ("/utf8/lengths", test_length);
  g_test_add_func ("/utf8/find", test_find);

  return g_test_run ();
}
