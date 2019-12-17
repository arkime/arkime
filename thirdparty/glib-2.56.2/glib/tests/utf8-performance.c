/* GLIB - Library of useful routines for C programming
 *
 * Copyright (C) 2010 Mikhail Zabaluev <mikhail.zabaluev@gmail.com>
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

#include <string.h>

#include <glib.h>

#define NUM_ITERATIONS 500000

static const char str_ascii[] =
    "The quick brown fox jumps over the lazy dog";

static const gchar str_latin1[] =
    "Zwölf Boxkämpfer jagen Viktor quer über den großen Sylter Deich";

/* Energizing GOELRO-talk in Russian, used by KDE */
static const char str_cyrillic[] =
    "Широкая электрификация южных губерний даст мощный толчок подъёму "
    "сельского хозяйства.";

/* First sentence from the Wikipedia article:
 * http://zh.wikipedia.org/w/index.php?title=%E6%B1%89%E5%AD%97&oldid=13053137 */
static const char str_han[] =
    "漢字，亦稱中文字、中国字，在台灣又被稱為國字，是漢字文化圈廣泛使用的一種文字，屬於表意文字的詞素音節文字";

typedef int (* GrindFunc) (const char *, gsize);

#define GRIND_LOOP_BEGIN                 \
  {                                      \
    int i;                               \
    for (i = 0; i < NUM_ITERATIONS; i++)

#define GRIND_LOOP_END \
  }

static int
grind_get_char (const char *str, gsize len)
{
  gunichar acc = 0;
  GRIND_LOOP_BEGIN
    {
      const char *p = str;
      while (*p)
        {
          acc += g_utf8_get_char (p);
          p = g_utf8_next_char (p);
        }
    }
  GRIND_LOOP_END;
  return acc;
}

static int
grind_get_char_validated (const char *str, gsize len)
{
  gunichar acc = 0;
  GRIND_LOOP_BEGIN
    {
      const char *p = str;
      while (*p)
        {
          acc += g_utf8_get_char_validated (p, -1);
          p = g_utf8_next_char (p);
        }
    }
  GRIND_LOOP_END;
  return acc;
}

static int
grind_utf8_to_ucs4 (const char *str, gsize len)
{
  GRIND_LOOP_BEGIN
    {
      gunichar *ustr;
      ustr = g_utf8_to_ucs4 (str, -1, NULL, NULL, NULL);
      g_free (ustr);
    }
  GRIND_LOOP_END;
  return 0;
}

static int
grind_get_char_backwards (const char *str, gsize len)
{
  gunichar acc = 0;
  GRIND_LOOP_BEGIN
    {
      const char *p = str + len;
      do
	{
	  p = g_utf8_prev_char (p);
	  acc += g_utf8_get_char (p);
        }
      while (p != str);
    }
  GRIND_LOOP_END;
  return acc;
}

static int
grind_utf8_to_ucs4_sized (const char *str, gsize len)
{
  GRIND_LOOP_BEGIN
    {
      gunichar *ustr;
      ustr = g_utf8_to_ucs4 (str, len, NULL, NULL, NULL);
      g_free (ustr);
    }
  GRIND_LOOP_END;
  return 0;
}

static int
grind_utf8_to_ucs4_fast (const char *str, gsize len)
{
  GRIND_LOOP_BEGIN
    {
      gunichar *ustr;
      ustr = g_utf8_to_ucs4_fast (str, -1, NULL);
      g_free (ustr);
    }
  GRIND_LOOP_END;
  return 0;
}

static int
grind_utf8_to_ucs4_fast_sized (const char *str, gsize len)
{
  GRIND_LOOP_BEGIN
    {
      gunichar *ustr;
      ustr = g_utf8_to_ucs4_fast (str, len, NULL);
      g_free (ustr);
    }
  GRIND_LOOP_END;
  return 0;
}

static int
grind_utf8_validate (const char *str, gsize len)
{
  GRIND_LOOP_BEGIN
    g_utf8_validate (str, -1, NULL);
  GRIND_LOOP_END;
  return 0;
}

static int
grind_utf8_validate_sized (const char *str, gsize len)
{
  GRIND_LOOP_BEGIN
    g_utf8_validate (str, len, NULL);
  GRIND_LOOP_END;
  return 0;
}

typedef struct _GrindData {
  GrindFunc func;
  const char *str;
} GrindData;

static void
perform (gconstpointer data)
{
  GrindData *gd = (GrindData *) data;
  GrindFunc grind_func = gd->func;
  const char *str = gd->str;
  gsize len;
  gulong bytes_ground;
  gdouble time_elapsed;
  gdouble result;

  len = strlen (str);
  bytes_ground = (gulong) len * NUM_ITERATIONS;

  g_test_timer_start ();

  grind_func (str, len);

  time_elapsed = g_test_timer_elapsed ();

  result = ((gdouble) bytes_ground / time_elapsed) * 1.0e-6;

  g_test_maximized_result (result, "%7.1f MB/s", result);

  g_slice_free (GrindData, gd);
}

static void
add_cases(const char *path, GrindFunc func)
{
#define ADD_CASE(script)                              \
  G_STMT_START {                                      \
    GrindData *gd;                                    \
    gchar *full_path;                                 \
    gd = g_slice_new0(GrindData);                     \
    gd->func = func;                                  \
    gd->str = str_##script;                           \
    full_path = g_strdup_printf("%s/" #script, path); \
    g_test_add_data_func (full_path, gd, perform);    \
    g_free (full_path);                               \
  } G_STMT_END

  ADD_CASE(ascii);
  ADD_CASE(latin1);
  ADD_CASE(cyrillic);
  ADD_CASE(han);

#undef ADD_CASE
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  if (g_test_perf ())
    {
      add_cases ("/utf8/perf/get_char", grind_get_char);
      add_cases ("/utf8/perf/get_char-backwards", grind_get_char_backwards);
      add_cases ("/utf8/perf/get_char_validated", grind_get_char_validated);
      add_cases ("/utf8/perf/utf8_to_ucs4", grind_utf8_to_ucs4);
      add_cases ("/utf8/perf/utf8_to_ucs4-sized", grind_utf8_to_ucs4_sized);
      add_cases ("/utf8/perf/utf8_to_ucs4_fast", grind_utf8_to_ucs4_fast);
      add_cases ("/utf8/perf/utf8_to_ucs4_fast-sized", grind_utf8_to_ucs4_fast_sized);
      add_cases ("/utf8/perf/utf8_validate", grind_utf8_validate);
      add_cases ("/utf8/perf/utf8_validate-sized", grind_utf8_validate_sized);
    }

  return g_test_run ();
}
