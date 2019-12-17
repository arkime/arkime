/* Unit tests for gprintf
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "glib.h"
#include "gstdio.h"

static void
test_retval_and_trunc (void)
{
  gchar buf[128];
  gint res;

  res = g_snprintf (buf, 0, "abc");
  g_assert_cmpint (res, ==, 3);

  res = g_snprintf (NULL, 0, "abc");
  g_assert_cmpint (res, ==, 3);

  res = g_snprintf (buf, 5, "abc");
  g_assert_cmpint (res, ==, 3);

  res = g_snprintf (buf, 1, "abc");
  g_assert_cmpint (res, ==, 3);
  g_assert (buf[0] == '\0');
  g_assert_cmpstr (buf, ==, "");

  res = g_snprintf (buf, 2, "abc");
  g_assert_cmpint (res, ==, 3);
  g_assert (buf[1] == '\0');
  g_assert_cmpstr (buf, ==, "a");

  res = g_snprintf (buf, 3, "abc");
  g_assert_cmpint (res, ==, 3);
  g_assert (buf[2] == '\0');
  g_assert_cmpstr (buf, ==, "ab");

  res = g_snprintf (buf, 4, "abc");
  g_assert_cmpint (res, ==, 3);
  g_assert (buf[3] == '\0');
  g_assert_cmpstr (buf, ==, "abc");

  res = g_snprintf (buf, 5, "abc");
  g_assert_cmpint (res, ==, 3);
  g_assert (buf[3] == '\0');
  g_assert_cmpstr (buf, ==, "abc");
}

static void
test_d (void)
{
  gchar buf[128];
  gint res;

  /* %d basic formatting */

  res = g_snprintf (buf, 128, "%d", 5);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "5");

  res = g_snprintf (buf, 128, "%d", 0);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "0");

  res = g_snprintf (buf, 128, "%.0d", 0);
  g_assert_cmpint (res, ==, 0);
  g_assert_cmpstr (buf, ==, "");

  res = g_snprintf (buf, 128, "%.0d", 1);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "1");

  res = g_snprintf (buf, 128, "%.d", 2);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "2");

  res = g_snprintf (buf, 128, "%d", -1);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "-1");

  res = g_snprintf (buf, 128, "%.3d", 5);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "005");

  res = g_snprintf (buf, 128, "%.3d", -5);
  g_assert_cmpint (res, ==, 4);
  g_assert_cmpstr (buf, ==, "-005");

  res = g_snprintf (buf, 128, "%5.3d", 5);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "  005");

  res = g_snprintf (buf, 128, "%-5.3d", -5);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "-005 ");

  /* %d, length modifiers */

  res = g_snprintf (buf, 128, "%" G_GINT16_FORMAT, (gint16)-5);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "-5");

  res = g_snprintf (buf, 128, "%" G_GUINT16_FORMAT, (guint16)5);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "5");

  res = g_snprintf (buf, 128, "%" G_GINT32_FORMAT, (gint32)-5);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "-5");

  res = g_snprintf (buf, 128, "%" G_GUINT32_FORMAT, (guint32)5);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "5");

  res = g_snprintf (buf, 128, "%" G_GINT64_FORMAT, (gint64)-5);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "-5");

  res = g_snprintf (buf, 128, "%" G_GUINT64_FORMAT, (guint64)5);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "5");

  res = g_snprintf (buf, 128, "%" G_GSSIZE_FORMAT, (gssize)-5);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "-5");

  res = g_snprintf (buf, 128, "%" G_GSIZE_FORMAT, (gsize)5);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "5");

  /* %d, flags */

  res = g_snprintf (buf, 128, "%-d", 5);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "5");

  res = g_snprintf (buf, 128, "%-+d", 5);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "+5");

  res = g_snprintf (buf, 128, "%+-d", 5);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "+5");

  res = g_snprintf (buf, 128, "%+d", -5);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "-5");

  res = g_snprintf (buf, 128, "% d", 5);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, " 5");

  res = g_snprintf (buf, 128, "% .0d", 0);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, " ");

  res = g_snprintf (buf, 128, "%03d", 5);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "005");

  res = g_snprintf (buf, 128, "%03d", -5);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "-05");
}

/* gcc emits warnings for the following formats, since the C spec
 * says some of the flags must be ignored. (The " " in "% +d" and
 * the "0" in "%-03d".) But we need to test that our printf gets
 * those rules right. So we fool gcc into not warning.
 *
 * These have to be in a separate function in order to use #pragma.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static void
test_d_invalid (void)
{
  const gchar *fmt;
  gchar buf[128];
  gint res;

  fmt = "% +d";
  res = g_snprintf (buf, 128, fmt, 5);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "+5");

  fmt = "%-03d";
  res = g_snprintf (buf, 128, fmt, -5);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "-5 ");
}
#pragma GCC diagnostic pop

static void
test_o (void)
{
  gchar buf[128];
  gint res;

  /* %o basic formatting */

  res = g_snprintf (buf, 128, "%o", 5);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "5");

  res = g_snprintf (buf, 128, "%o", 8);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "10");

  res = g_snprintf (buf, 128, "%o", 0);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "0");

  res = g_snprintf (buf, 128, "%.0o", 0);
  g_assert_cmpint (res, ==, 0);
  g_assert_cmpstr (buf, ==, "");

  res = g_snprintf (buf, 128, "%.0o", 1);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "1");

  res = g_snprintf (buf, 128, "%.3o", 5);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "005");

  res = g_snprintf (buf, 128, "%.3o", 8);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "010");

  res = g_snprintf (buf, 128, "%5.3o", 5);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "  005");
}

static void
test_u (void)
{
  gchar buf[128];
  gint res;

  /* %u, basic formatting */

  res = g_snprintf (buf, 128, "%u", 5);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "5");

  res = g_snprintf (buf, 128, "%u", 0);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "0");

  res = g_snprintf (buf, 128, "%.0u", 0);
  g_assert_cmpint (res, ==, 0);
  g_assert_cmpstr (buf, ==, "");

  res = g_snprintf (buf, 128, "%.0u", 1);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "1");

  res = g_snprintf (buf, 128, "%.3u", 5);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "005");

  res = g_snprintf (buf, 128, "%5.3u", 5);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "  005");
}

static void
test_x (void)
{
  gchar buf[128];
  gint res;

  /* %x, basic formatting */

  res = g_snprintf (buf, 128, "%x", 5);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "5");

  res = g_snprintf (buf, 128, "%x", 31);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "1f");

  res = g_snprintf (buf, 128, "%x", 0);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "0");

  res = g_snprintf (buf, 128, "%.0x", 0);
  g_assert_cmpint (res, ==, 0);
  g_assert_cmpstr (buf, ==, "");

  res = g_snprintf (buf, 128, "%.0x", 1);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "1");

  res = g_snprintf (buf, 128, "%.3x", 5);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "005");

  res = g_snprintf (buf, 128, "%.3x", 31);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "01f");

  res = g_snprintf (buf, 128, "%5.3x", 5);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "  005");

  /* %x, flags */

  res = g_snprintf (buf, 128, "%-x", 5);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "5");

  res = g_snprintf (buf, 128, "%03x", 5);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "005");

  res = g_snprintf (buf, 128, "%#x", 31);
  g_assert_cmpint (res, ==, 4);
  g_assert_cmpstr (buf, ==, "0x1f");

  res = g_snprintf (buf, 128, "%#x", 0);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "0");
}

static void
test_X (void)
{
  gchar buf[128];
  gint res;

  /* %X, basic formatting */

  res = g_snprintf (buf, 128, "%X", 5);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "5");

  res = g_snprintf (buf, 128, "%X", 31);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "1F");

  res = g_snprintf (buf, 128, "%X", 0);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "0");

  res = g_snprintf (buf, 128, "%.0X", 0);
  g_assert_cmpint (res, ==, 0);
  g_assert_cmpstr (buf, ==, "");

  res = g_snprintf (buf, 128, "%.0X", 1);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "1");

  res = g_snprintf (buf, 128, "%.3X", 5);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "005");

  res = g_snprintf (buf, 128, "%.3X", 31);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "01F");

  res = g_snprintf (buf, 128, "%5.3X", 5);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "  005");

  /* %X, flags */

  res = g_snprintf (buf, 128, "%-X", 5);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "5");

  res = g_snprintf (buf, 128, "%03X", 5);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "005");

  res = g_snprintf (buf, 128, "%#X", 31);
  g_assert_cmpint (res, ==, 4);
  g_assert_cmpstr (buf, ==, "0X1F");

  res = g_snprintf (buf, 128, "%#X", 0);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "0");
}

static void
test_f (void)
{
  gchar buf[128];
  gint res;

  /* %f, basic formattting */

  res = g_snprintf (buf, 128, "%f", G_PI);
  g_assert_cmpint (res, ==, 8);
  g_assert (0 == strncmp (buf, "3.14159", 7));

  res = g_snprintf (buf, 128, "%.8f", G_PI);
  g_assert_cmpint (res, ==, 10);
  g_assert (0 == strncmp (buf, "3.1415926", 9));

  res = g_snprintf (buf, 128, "%.0f", G_PI);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "3");

  res = g_snprintf (buf, 128, "%1.f", G_PI);
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "3");

  res = g_snprintf (buf, 128, "%3.f", G_PI);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "  3");

  /* %f, flags */

  res = g_snprintf (buf, 128, "%+f", G_PI);
  g_assert_cmpint (res, ==, 9);
  g_assert (0 == strncmp (buf, "+3.14159", 8));

  res = g_snprintf (buf, 128, "% f", G_PI);
  g_assert_cmpint (res, ==, 9);
  g_assert (0 == strncmp (buf, " 3.14159", 8));

  res = g_snprintf (buf, 128, "%#.0f", G_PI);
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "3.");

  res = g_snprintf (buf, 128, "%05.2f", G_PI);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "03.14");
}

static gboolean
same_value (const gchar *actual, 
            const gchar *expected)
{
  gdouble actual_value, expected_value;

  actual_value = g_ascii_strtod (actual, NULL);
  expected_value = g_ascii_strtod (expected, NULL);

  return actual_value == expected_value;
}

static void
test_e (void)
{
  gchar buf[128];
  gint res;

  /* %e, basic formatting */
  /* for %e we can't expect to reproduce exact strings and lengths, since SUS
   * only guarantees that the exponent shall always contain at least two 
   * digits. On Windows, it seems to be at least three digits long.
   * Therefore, we compare the results of parsing the expected result and the
   * actual result.
   */

  res = g_snprintf (buf, 128, "%e", G_PI);
  g_assert_cmpint (res, >=, 12);
  g_assert (same_value (buf, "3.141593e+00"));

  res = g_snprintf (buf, 128, "%.8e", G_PI);
  g_assert_cmpint (res, >=, 14);
  g_assert (same_value (buf, "3.14159265e+00"));

  res = g_snprintf (buf, 128, "%.0e", G_PI);
  g_assert_cmpint (res, >=, 5);
  g_assert (same_value (buf, "3e+00"));

  res = g_snprintf (buf, 128, "%.1e", 0.0);
  g_assert_cmpint (res, >=, 7);
  g_assert (same_value (buf, "0.0e+00"));

  res = g_snprintf (buf, 128, "%.1e", 0.00001);
  g_assert_cmpint (res, >=, 7);
  g_assert (same_value (buf, "1.0e-05"));

  res = g_snprintf (buf, 128, "%.1e", 10000.0);
  g_assert_cmpint (res, >=, 7);
  g_assert (same_value (buf, "1.0e+04"));

  /* %e, flags */

  res = g_snprintf (buf, 128, "%+e", G_PI);
  g_assert_cmpint (res, >=, 13);
  g_assert (same_value (buf, "+3.141593e+00"));

  res = g_snprintf (buf, 128, "% e", G_PI);
  g_assert_cmpint (res, >=, 13);
  g_assert (same_value (buf, " 3.141593e+00"));

  res = g_snprintf (buf, 128, "%#.0e", G_PI);
  g_assert_cmpint (res, >=, 6);
  g_assert (same_value (buf, "3.e+00"));

  res = g_snprintf (buf, 128, "%09.2e", G_PI);
  g_assert_cmpint (res, >=, 9);
  g_assert (same_value (buf, "03.14e+00"));
}

static void
test_c (void)
{
  gchar buf[128];
  gint res;

  res = g_snprintf (buf, 128, "%c", 'a');
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "a");
}

static void
test_s (void)
{
  gchar buf[128];
  gint res;

  res = g_snprintf (buf, 128, "%.2s", "abc");
  g_assert_cmpint (res, ==, 2);
  g_assert_cmpstr (buf, ==, "ab");

  res = g_snprintf (buf, 128, "%.6s", "abc");
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "abc");

  res = g_snprintf (buf, 128, "%5s", "abc");
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "  abc");

  res = g_snprintf (buf, 128, "%-5s", "abc");
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "abc  ");

  res = g_snprintf (buf, 128, "%5.2s", "abc");
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "   ab");

  res = g_snprintf (buf, 128, "%*s", 5, "abc");
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "  abc");

  res = g_snprintf (buf, 128, "%*s", -5, "abc");
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "abc  ");

  res = g_snprintf (buf, 128, "%*.*s", 5, 2, "abc");
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "   ab");
}

static void
test_n (void)
{
  gchar buf[128];
  gint res;
  gint i;
  glong l;

  res = g_snprintf (buf, 128, "abc%n", &i);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "abc");
  g_assert_cmpint (i, ==, 3);

  res = g_snprintf (buf, 128, "abc%ln", &l);
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "abc");
  g_assert_cmpint (l, ==, 3);
}

static void
test_percent (void)
{
  gchar buf[128];
  gint res;

  res = g_snprintf (buf, 128, "%%");
  g_assert_cmpint (res, ==, 1);
  g_assert_cmpstr (buf, ==, "%");
}

static void
test_positional_params (void)
{
  gchar buf[128];
  gint res;

  res = g_snprintf (buf, 128, "%2$c %1$c", 'b', 'a');
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "a b");

  res = g_snprintf (buf, 128, "%1$*2$.*3$s", "abc", 5, 2);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "   ab");

  res = g_snprintf (buf, 128, "%1$s%1$s", "abc");
  g_assert_cmpint (res, ==, 6);
  g_assert_cmpstr (buf, ==, "abcabc");
}

static void
test_positional_params2 (void)
{
  if (g_test_subprocess ())
    {
      gint res;

      res = g_printf ("%2$c %1$c\n", 'b', 'a');
      g_assert_cmpint (res, ==, 4);

      res = g_printf ("%1$*2$.*3$s\n", "abc", 5, 2);
      g_assert_cmpint (res, ==, 6);

      res = g_printf ("%1$s%1$s\n", "abc");
      g_assert_cmpint (res, ==, 7);
      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_passed ();
#ifndef G_OS_WIN32
  g_test_trap_assert_stdout ("a b\n   ab\nabcabc\n");
#else
  g_test_trap_assert_stdout ("a b\r\n   ab\r\nabcabc\r\n");
#endif
}

static void
test_positional_params3 (void)
{
  gchar buf[128];
  gint res;

  res = g_sprintf (buf, "%2$c %1$c", 'b', 'a');
  g_assert_cmpint (res, ==, 3);
  g_assert_cmpstr (buf, ==, "a b");

  res = g_sprintf (buf, "%1$*2$.*3$s", "abc", 5, 2);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "   ab");

  res = g_sprintf (buf, "%1$s%1$s", "abc");
  g_assert_cmpint (res, ==, 6);
  g_assert_cmpstr (buf, ==, "abcabc");
}

static void
test_percent2 (void)
{
  if (g_test_subprocess ())
    {
      gint res;

      res = g_printf ("%%");
      g_assert_cmpint (res, ==, 1);
      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*%*");
}

static void
test_64bit (void)
{
  gchar buf[128];
  gint res;

  res = g_snprintf (buf, 128, "%" G_GINT64_FORMAT, (gint64)123456);
  g_assert_cmpint (res, ==, 6);
  g_assert_cmpstr (buf, ==, "123456");

  res = g_snprintf (buf, 128, "%" G_GINT64_FORMAT, (gint64)-123456);
  g_assert_cmpint (res, ==, 7);
  g_assert_cmpstr (buf, ==, "-123456");

  res = g_snprintf (buf, 128, "%" G_GUINT64_FORMAT, (guint64)123456);
  g_assert_cmpint (res, ==, 6);
  g_assert_cmpstr (buf, ==, "123456");

  res = g_snprintf (buf, 128, "%" G_GINT64_MODIFIER "o", (gint64)123456);
  g_assert_cmpint (res, ==, 6);
  g_assert_cmpstr (buf, ==, "361100");

  res = g_snprintf (buf, 128, "%#" G_GINT64_MODIFIER "o", (gint64)123456);
  g_assert_cmpint (res, ==, 7);
  g_assert_cmpstr (buf, ==, "0361100");

  res = g_snprintf (buf, 128, "%" G_GINT64_MODIFIER "x", (gint64)123456);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "1e240");

  res = g_snprintf (buf, 128, "%#" G_GINT64_MODIFIER "x", (gint64)123456);
  g_assert_cmpint (res, ==, 7);
  g_assert_cmpstr (buf, ==, "0x1e240");

  res = g_snprintf (buf, 128, "%" G_GINT64_MODIFIER "X", (gint64)123456);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "1E240");

#ifdef G_OS_WIN32
  /* On Win32, test that the "ll" modifier also works, for backward
   * compatibility. One really should use the G_GINT64_MODIFIER (which
   * on Win32 is the "I64" that the (msvcrt) C library's printf uses),
   * but "ll" used to work with the "trio" g_printf implementation in
   * GLib 2.2, so it's best if it continues to work.
   */

  /* However, gcc doesn't know about this, so we need to disable printf
   * format warnings...
   */
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
_Pragma ("GCC diagnostic push")
_Pragma ("GCC diagnostic ignored \"-Wformat\"")
_Pragma ("GCC diagnostic ignored \"-Wformat-extra-args\"")
#endif

  res = g_snprintf (buf, 128, "%" "lli", (gint64)123456);
  g_assert_cmpint (res, ==, 6);
  g_assert_cmpstr (buf, ==, "123456");

  res = g_snprintf (buf, 128, "%" "lli", (gint64)-123456);
  g_assert_cmpint (res, ==, 7);
  g_assert_cmpstr (buf, ==, "-123456");

  res = g_snprintf (buf, 128, "%" "llu", (guint64)123456);
  g_assert_cmpint (res, ==, 6);
  g_assert_cmpstr (buf, ==, "123456");

  res = g_snprintf (buf, 128, "%" "ll" "o", (gint64)123456);
  g_assert_cmpint (res, ==, 6);
  g_assert_cmpstr (buf, ==, "361100");

  res = g_snprintf (buf, 128, "%#" "ll" "o", (gint64)123456);
  g_assert_cmpint (res, ==, 7);
  g_assert_cmpstr (buf, ==, "0361100");

  res = g_snprintf (buf, 128, "%" "ll" "x", (gint64)123456);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "1e240");

  res = g_snprintf (buf, 128, "%#" "ll" "x", (gint64)123456);
  g_assert_cmpint (res, ==, 7);
  g_assert_cmpstr (buf, ==, "0x1e240");

  res = g_snprintf (buf, 128, "%" "ll" "X", (gint64)123456);
  g_assert_cmpint (res, ==, 5);
  g_assert_cmpstr (buf, ==, "1E240");

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
_Pragma ("GCC diagnostic pop")
#endif

#endif
}

static void
test_64bit2_base (void)
{
  gint res;

  res = g_printf ("%" G_GINT64_FORMAT "\n", (gint64)123456);
  g_assert_cmpint (res, ==, 7);

  res = g_printf ("%" G_GINT64_FORMAT "\n", (gint64)-123456);
  g_assert_cmpint (res, ==, 8);

  res = g_printf ("%" G_GUINT64_FORMAT "\n", (guint64)123456);
  g_assert_cmpint (res, ==, 7);

  res = g_printf ("%" G_GINT64_MODIFIER "o\n", (gint64)123456);
  g_assert_cmpint (res, ==, 7);

  res = g_printf ("%#" G_GINT64_MODIFIER "o\n", (gint64)123456);
  g_assert_cmpint (res, ==, 8);

  res = g_printf ("%" G_GINT64_MODIFIER "x\n", (gint64)123456);
  g_assert_cmpint (res, ==, 6);

  res = g_printf ("%#" G_GINT64_MODIFIER "x\n", (gint64)123456);
  g_assert_cmpint (res, ==, 8);

  res = g_printf ("%" G_GINT64_MODIFIER "X\n", (gint64)123456);
  g_assert_cmpint (res, ==, 6);
}

#ifdef G_OS_WIN32
static void
test_64bit2_win32 (void)
{
  gint res;

  /* On Win32, test that the "ll" modifier also works, for backward
   * compatibility. One really should use the G_GINT64_MODIFIER (which
   * on Win32 is the "I64" that the (msvcrt) C library's printf uses),
   * but "ll" used to work with the "trio" g_printf implementation in
   * GLib 2.2, so it's best if it continues to work.
   */

  /* However, gcc doesn't know about this, so we need to disable printf
   * format warnings...
   */
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
_Pragma ("GCC diagnostic push")
_Pragma ("GCC diagnostic ignored \"-Wformat\"")
_Pragma ("GCC diagnostic ignored \"-Wformat-extra-args\"")
#endif

  res = g_printf ("%" "lli\n", (gint64)123456);
  g_assert_cmpint (res, ==, 7);

  res = g_printf ("%" "lli\n", (gint64)-123456);
  g_assert_cmpint (res, ==, 8);

  res = g_printf ("%" "llu\n", (guint64)123456);
  g_assert_cmpint (res, ==, 7);

  res = g_printf ("%" "ll" "o\n", (gint64)123456);
  g_assert_cmpint (res, ==, 7);

  res = g_printf ("%#" "ll" "o\n", (gint64)123456);
  g_assert_cmpint (res, ==, 8);

  res = g_printf ("%" "ll" "x\n", (gint64)123456);
  g_assert_cmpint (res, ==, 6);

  res = g_printf ("%#" "ll" "x\n", (gint64)123456);
  g_assert_cmpint (res, ==, 8);

  res = g_printf ("%" "ll" "X\n", (gint64)123456);
  g_assert_cmpint (res, ==, 6);

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
_Pragma ("GCC diagnostic pop")
#endif
}
#endif

static void
test_64bit2 (void)
{
#ifndef G_OS_WIN32
  g_test_trap_subprocess ("/printf/test-64bit/subprocess/base", 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("123456\n-123456\n123456\n"
                             "361100\n0361100\n1e240\n"
                             "0x1e240\n1E240\n");

#else
  g_test_trap_subprocess ("/printf/test-64bit/subprocess/base", 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("123456\r\n-123456\r\n123456\r\n"
                             "361100\r\n0361100\r\n1e240\r\n"
                             "0x1e240\r\n1E240\r\n");

  g_test_trap_subprocess ("/printf/test-64bit/subprocess/win32", 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("123456\r\n-123456\r\n123456\r\n"
                             "361100\r\n0361100\r\n1e240\r\n"
                             "0x1e240\r\n1E240\r\n");
#endif
}

G_GNUC_PRINTF(1, 2)
static gsize
upper_bound (const gchar *format, ...)
{
  va_list args;
  gsize res;

  va_start (args, format);
  res = g_printf_string_upper_bound (format, args);
  va_end (args);

  return res;
}

static void
test_upper_bound (void)
{
  gsize res;

  res = upper_bound ("bla %s %d: %g\n", "bla", 123, 0.123);
  g_assert_cmpint (res, ==, 20);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/snprintf/retval-and-trunc", test_retval_and_trunc);
  g_test_add_func ("/snprintf/%d", test_d);
  g_test_add_func ("/snprintf/%d-invalid", test_d_invalid);
  g_test_add_func ("/snprintf/%o", test_o);
  g_test_add_func ("/snprintf/%u", test_u);
  g_test_add_func ("/snprintf/%x", test_x);
  g_test_add_func ("/snprintf/%X", test_X);
  g_test_add_func ("/snprintf/%f", test_f);
  g_test_add_func ("/snprintf/%e", test_e);
  g_test_add_func ("/snprintf/%c", test_c);
  g_test_add_func ("/snprintf/%s", test_s);
  g_test_add_func ("/snprintf/%n", test_n);
  g_test_add_func ("/snprintf/test-percent", test_percent);
  g_test_add_func ("/snprintf/test-positional-params", test_positional_params);
  g_test_add_func ("/snprintf/test-64bit", test_64bit);

  g_test_add_func ("/printf/test-percent", test_percent2);
  g_test_add_func ("/printf/test-positional-params", test_positional_params2);
  g_test_add_func ("/printf/test-64bit", test_64bit2);
  g_test_add_func ("/printf/test-64bit/subprocess/base", test_64bit2_base);
#ifdef G_OS_WIN32
  g_test_add_func ("/printf/test-64bit/subprocess/win32", test_64bit2_win32);
#endif

  g_test_add_func ("/sprintf/test-positional-params", test_positional_params3);
  g_test_add_func ("/sprintf/upper-bound", test_upper_bound);

  return g_test_run();
}
