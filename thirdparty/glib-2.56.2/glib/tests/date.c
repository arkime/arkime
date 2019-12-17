#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

/* We are testing some deprecated APIs here */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "glib.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <time.h>

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static void
test_basic (void)
{
  g_assert_cmpint (sizeof (GDate), <,  9);
  g_assert (!g_date_valid_month (G_DATE_BAD_MONTH));
  g_assert (!g_date_valid_month (13));
  g_assert (!g_date_valid_day (G_DATE_BAD_DAY));
  g_assert (!g_date_valid_day (32));
  g_assert (!g_date_valid_year (G_DATE_BAD_YEAR));
  g_assert (!g_date_valid_julian (G_DATE_BAD_JULIAN));
  g_assert (!g_date_valid_weekday (G_DATE_BAD_WEEKDAY));
  g_assert (g_date_is_leap_year (2000));
  g_assert (!g_date_is_leap_year (1999));
  g_assert (g_date_is_leap_year (1996));
  g_assert (g_date_is_leap_year (1600));
  g_assert (!g_date_is_leap_year (2100));
  g_assert (!g_date_is_leap_year (1800));
}

static void
test_empty_constructor (void)
{
  GDate *d;

  d = g_date_new ();
  g_assert (!g_date_valid (d));
  g_date_free (d);
}

static void
test_dmy_constructor (void)
{
  GDate *d;
  guint32 j;

  d = g_date_new_dmy (1, 1, 1);
  g_assert (g_date_valid (d));
  j = g_date_get_julian (d);
  g_assert_cmpint (j, ==, 1);
  g_assert_cmpint (g_date_get_month (d), ==, G_DATE_JANUARY);
  g_assert_cmpint (g_date_get_day (d), ==, 1);
  g_assert_cmpint (g_date_get_year (d), ==, 1);
  g_date_free (d);
}

static void
test_julian_constructor (void)
{
  GDate *d1;
  GDate *d2;

  d1 = g_date_new_julian (4000);
  d2 = g_date_new_julian (5000);
  g_assert_cmpint (g_date_get_julian (d1), ==, 4000);
  g_assert_cmpint (g_date_days_between (d1, d2), ==, 1000);
  g_assert_cmpint (g_date_get_year (d1), ==, 11);
  g_assert_cmpint (g_date_get_day (d2), ==, 9);
  g_date_free (d1);
  g_date_free (d2);
}

static void
test_dates (void)
{
  GDate *d;
  GTimeVal tv;

  d = g_date_new ();

  /* today */
  g_date_set_time (d, time (NULL));
  g_assert (g_date_valid (d));

  /* Unix epoch */
  g_date_set_time (d, 1);
  g_assert (g_date_valid (d));

  tv.tv_sec = 0;
  tv.tv_usec = 0;
  g_date_set_time_val (d, &tv);
  g_assert (g_date_valid (d));

  /* Julian day 1 */
  g_date_set_julian (d, 1);
  g_assert (g_date_valid (d));

  g_date_set_year (d, 3);
  g_date_set_day (d, 3);
  g_date_set_month (d, 3);
  g_assert (g_date_valid (d));
  g_assert_cmpint (g_date_get_year (d), ==, 3);
  g_assert_cmpint (g_date_get_month (d), ==, 3);
  g_assert_cmpint (g_date_get_day (d), ==, 3);
  g_assert (!g_date_is_first_of_month (d));
  g_assert (!g_date_is_last_of_month (d));
  g_date_set_day (d, 1);
  g_assert (g_date_is_first_of_month (d));
  g_date_subtract_days (d, 1);
  g_assert (g_date_is_last_of_month (d));

  g_date_free (d);
}

static void
test_parse (void)
{
  GDate *d;
  gchar buf[101];

  d = g_date_new ();

  g_date_set_dmy (d, 10, 1, 2000);
  g_date_strftime (buf, 100, "%x", d);

  g_date_set_parse (d, buf);
  g_assert (g_date_valid (d));
  g_assert_cmpint (g_date_get_month (d), ==, 1);
  g_assert_cmpint (g_date_get_day (d), ==, 10);
  g_assert_cmpint (g_date_get_year (d), ==, 2000);

  g_date_set_parse (d, "2001 10 1");
  g_assert (g_date_valid (d));
  g_assert_cmpint (g_date_get_month (d), ==, 10);
  g_assert_cmpint (g_date_get_day (d), ==, 1);
  g_assert_cmpint (g_date_get_year (d), ==, 2001);

  g_date_set_parse (d, "2001 10");
  g_assert (!g_date_valid (d));

  g_date_set_parse (d, "2001 10 1 1");
  g_assert (!g_date_valid (d));

  g_date_set_parse (d, "March 1999");
  g_assert (g_date_valid (d));
  g_assert_cmpint (g_date_get_month (d), ==, 3);
  g_assert_cmpint (g_date_get_day (d), ==, 1);
  g_assert_cmpint (g_date_get_year (d), ==, 1999);

  g_date_set_parse (d, "10 Sep 1087");
  g_assert (g_date_valid (d));
  g_assert_cmpint (g_date_get_month (d), ==, 9);
  g_assert_cmpint (g_date_get_day (d), ==, 10);
  g_assert_cmpint (g_date_get_year (d), ==, 1087);

  g_date_set_parse (d, "19990301");
  g_assert (g_date_valid (d));
  g_assert_cmpint (g_date_get_month (d), ==, 3);
  g_assert_cmpint (g_date_get_day (d), ==, 1);
  g_assert_cmpint (g_date_get_year (d), ==, 1999);

  g_date_set_parse (d, "20011320");
  g_assert (!g_date_valid (d));

  g_date_free (d);
}

static void
test_month_names (void)
{
#if defined(HAVE_LANGINFO_ABALTMON) || defined(G_OS_WIN32)
  GDate *gdate;
  gchar buf[101];
  gchar *oldlocale;
#ifdef G_OS_WIN32
  LCID old_lcid;
#endif
#endif  /* defined(HAVE_LANGINFO_ABALTMON) || defined(G_OS_WIN32) */

  g_test_bug ("749206");

  /* This test can only work (on non-Windows platforms) if libc supports
   * the %OB (etc.) format placeholders. If it doesn’t, strftime() (and hence
   * g_date_strftime()) will return the placeholder unsubstituted.
   * g_date_strftime() explicitly documents that it doesn’t provide any more
   * format placeholders than the system strftime(), so we should skip the test
   * in that case. If people need %OB support, they should depend on a suitable
   * version of libc, or use g_date_time_format(). Note: a test for a support
   * of _NL_ABALTMON_* is not strictly the same as checking for %OB support.
   * Some platforms (BSD, OS X) support %OB while _NL_ABALTMON_* and %Ob
   * are supported only by glibc 2.27 and newer. But we don’t care about BSD
   * here, the aim of this test is to make sure that our custom implementation
   * for Windows works the same as glibc 2.27 native implementation. */
#if !defined(HAVE_LANGINFO_ABALTMON) && !defined(G_OS_WIN32)
  g_test_skip ("libc doesn’t support all alternative month names");
#else

#define TEST_DATE(d,m,y,f,o)                                    \
  g_date_set_dmy (gdate, d, m, y);                              \
  g_date_strftime (buf, 100, f, gdate);                         \
  g_assert_cmpstr (buf, ==, (o));                               \
  g_date_set_parse (gdate, buf);                                \
  g_assert (g_date_valid (gdate));                              \
  g_assert_cmpint (g_date_get_day (gdate), ==, d);              \
  g_assert_cmpint (g_date_get_month (gdate), ==, m);            \
  g_assert_cmpint (g_date_get_year (gdate), ==, y);

  oldlocale = g_strdup (setlocale (LC_ALL, NULL));
#ifdef G_OS_WIN32
  old_lcid = GetThreadLocale ();
#endif

  gdate = g_date_new ();

  /* Note: Windows implementation of g_date_strftime() does not support
   * "-" format modifier (e.g., "%-d", "%-e") so we will not use it.
   */

  /* Make sure that nothing has been changed in western European languages.  */
  setlocale (LC_ALL, "en_GB.utf-8");
#ifdef G_OS_WIN32
  SetThreadLocale (MAKELCID (MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_UK), SORT_DEFAULT));
#endif
  if (strstr (setlocale (LC_ALL, NULL), "en_GB") != NULL)
    {
      TEST_DATE (1,  1, 2018, "%B %d, %Y", "January 01, 2018");
      TEST_DATE (1,  2, 2018,    "%OB %Y",    "February 2018");
      TEST_DATE (1,  3, 2018,  "%e %b %Y",      " 1 Mar 2018");
      TEST_DATE (1,  4, 2018,    "%Ob %Y",         "Apr 2018");
      TEST_DATE (1,  5, 2018,  "%d %h %Y",      "01 May 2018");
      TEST_DATE (1,  6, 2018,    "%Oh %Y",         "Jun 2018");
    }
  else
    g_test_incomplete ("locale en_GB not available, skipping English month names test");

  setlocale (LC_ALL, "de_DE.utf-8");
#ifdef G_OS_WIN32
  SetThreadLocale (MAKELCID (MAKELANGID (LANG_GERMAN, SUBLANG_GERMAN), SORT_DEFAULT));
#endif
  if (strstr (setlocale (LC_ALL, NULL), "de_DE") != NULL)
    {
      TEST_DATE (16,  7, 2018, "%d. %B %Y", "16. Juli 2018");
      TEST_DATE ( 1,  8, 2018,    "%OB %Y",   "August 2018");
      TEST_DATE (18,  9, 2018, "%e. %b %Y",  "18. Sep 2018");
      TEST_DATE ( 1, 10, 2018,    "%Ob %Y",      "Okt 2018");
      TEST_DATE (20, 11, 2018, "%d. %h %Y",  "20. Nov 2018");
      TEST_DATE ( 1, 12, 2018,    "%Oh %Y",      "Dez 2018");
    }
  else
    g_test_incomplete ("locale de_DE not available, skipping German month names test");


  setlocale (LC_ALL, "es_ES.utf-8");
#ifdef G_OS_WIN32
  SetThreadLocale (MAKELCID (MAKELANGID (LANG_SPANISH, SUBLANG_SPANISH_MODERN), SORT_DEFAULT));
#endif
  if (strstr (setlocale (LC_ALL, NULL), "es_ES") != NULL)
    {
      TEST_DATE ( 9,  1, 2018, "%d de %B de %Y", "09 de enero de 2018");
      TEST_DATE ( 1,  2, 2018,      "%OB de %Y",     "febrero de 2018");
      TEST_DATE (10,  3, 2018, "%e de %b de %Y",   "10 de mar de 2018");
      TEST_DATE ( 1,  4, 2018,      "%Ob de %Y",         "abr de 2018");
      TEST_DATE (11,  5, 2018, "%d de %h de %Y",   "11 de may de 2018");
      TEST_DATE ( 1,  6, 2018,      "%Oh de %Y",         "jun de 2018");
    }
  else
    g_test_incomplete ("locale es_ES not available, skipping Spanish month names test");

  setlocale (LC_ALL, "fr_FR.utf-8");
#ifdef G_OS_WIN32
  SetThreadLocale (MAKELCID (MAKELANGID (LANG_FRENCH, SUBLANG_FRENCH), SORT_DEFAULT));
#endif
  if (strstr (setlocale (LC_ALL, NULL), "fr_FR") != NULL)
    {
      TEST_DATE (31,  7, 2018, "%d %B %Y", "31 juillet 2018");
      TEST_DATE ( 1,  8, 2018,   "%OB %Y",       "août 2018");
      TEST_DATE (30,  9, 2018, "%e %b %Y",   "30 sept. 2018");
      TEST_DATE ( 1, 10, 2018,   "%Ob %Y",       "oct. 2018");
      TEST_DATE (29, 11, 2018, "%d %h %Y",    "29 nov. 2018");
      TEST_DATE ( 1, 12, 2018,   "%Oh %Y",       "déc. 2018");
    }
  else
    g_test_incomplete ("locale fr_FR not available, skipping French month names test");

  /* Make sure that there are visible changes in some European languages.  */
  setlocale (LC_ALL, "el_GR.utf-8");
#ifdef G_OS_WIN32
  SetThreadLocale (MAKELCID (MAKELANGID (LANG_GREEK, SUBLANG_GREEK_GREECE), SORT_DEFAULT));
#endif
  if (strstr (setlocale (LC_ALL, NULL), "el_GR") != NULL)
    {
      TEST_DATE ( 2,  1, 2018, "%d %B %Y",  "02 Ιανουαρίου 2018");
      TEST_DATE ( 4,  2, 2018, "%e %B %Y", " 4 Φεβρουαρίου 2018");
      TEST_DATE (15,  3, 2018, "%d %B %Y",     "15 Μαρτίου 2018");
      TEST_DATE ( 1,  4, 2018,   "%OB %Y",       "Απρίλιος 2018");
      TEST_DATE ( 1,  5, 2018,   "%OB %Y",          "Μάιος 2018");
      TEST_DATE ( 1,  6, 2018,   "%OB %Y",        "Ιούνιος 2018");
      TEST_DATE (16,  7, 2018, "%e %b %Y",        "16 Ιούλ 2018");
      TEST_DATE ( 1,  8, 2018,   "%Ob %Y",            "Αύγ 2018");
    }
  else
    g_test_incomplete ("locale el_GR not available, skipping Greek month names test");

  setlocale (LC_ALL, "hr_HR.utf-8");
#ifdef G_OS_WIN32
  SetThreadLocale (MAKELCID (MAKELANGID (LANG_CROATIAN, SUBLANG_CROATIAN_CROATIA), SORT_DEFAULT));
#endif
  if (strstr (setlocale (LC_ALL, NULL), "hr_HR") != NULL)
    {
      TEST_DATE ( 8,  5, 2018, "%d. %B %Y", "08. svibnja 2018");
      TEST_DATE ( 9,  6, 2018, "%e. %B %Y",  " 9. lipnja 2018");
      TEST_DATE (10,  7, 2018, "%d. %B %Y",  "10. srpnja 2018");
      TEST_DATE ( 1,  8, 2018,    "%OB %Y",     "Kolovoz 2018");
      TEST_DATE ( 1,  9, 2018,    "%OB %Y",       "Rujan 2018");
      TEST_DATE ( 1, 10, 2018,    "%OB %Y",    "Listopad 2018");
      TEST_DATE (11, 11, 2018, "%e. %b %Y",     "11. Stu 2018");
      TEST_DATE ( 1, 12, 2018,    "%Ob %Y",         "Pro 2018");
    }
  else
    g_test_incomplete ("locale hr_HR not available, skipping Croatian month names test");

  setlocale (LC_ALL, "lt_LT.utf-8");
#ifdef G_OS_WIN32
  SetThreadLocale (MAKELCID (MAKELANGID (LANG_LITHUANIAN, SUBLANG_LITHUANIAN_LITHUANIA), SORT_DEFAULT));
#endif
  if (strstr (setlocale (LC_ALL, NULL), "lt_LT") != NULL)
    {
      TEST_DATE ( 1,  1, 2018, "%Y m. %B %d d.",  "2018 m. sausio 01 d.");
      TEST_DATE ( 2,  2, 2018, "%Y m. %B %e d.", "2018 m. vasario  2 d.");
      TEST_DATE ( 3,  3, 2018, "%Y m. %B %d d.",    "2018 m. kovo 03 d.");
      TEST_DATE ( 1,  4, 2018,      "%Y m. %OB",      "2018 m. balandis");
      TEST_DATE ( 1,  5, 2018,      "%Y m. %OB",        "2018 m. gegužė");
      TEST_DATE ( 1,  6, 2018,      "%Y m. %OB",      "2018 m. birželis");
      TEST_DATE (17,  7, 2018, "%Y m. %b %e d.",     "2018 m. Lie 17 d.");
      TEST_DATE ( 1,  8, 2018,      "%Y m. %Ob",           "2018 m. Rgp");
    }
  else
    g_test_incomplete ("locale lt_LT not available, skipping Lithuanian month names test");

  setlocale (LC_ALL, "pl_PL.utf-8");
#ifdef G_OS_WIN32
  SetThreadLocale (MAKELCID (MAKELANGID (LANG_POLISH, SUBLANG_POLISH_POLAND), SORT_DEFAULT));
#endif
  if (strstr (setlocale (LC_ALL, NULL), "pl_PL") != NULL)
    {
      TEST_DATE ( 3,  5, 2018, "%d %B %Y",     "03 maja 2018");
      TEST_DATE ( 4,  6, 2018, "%e %B %Y",  " 4 czerwca 2018");
      TEST_DATE (20,  7, 2018, "%d %B %Y",    "20 lipca 2018");
      TEST_DATE ( 1,  8, 2018,   "%OB %Y",    "sierpień 2018");
      TEST_DATE ( 1,  9, 2018,   "%OB %Y",    "wrzesień 2018");
      TEST_DATE ( 1, 10, 2018,   "%OB %Y", "październik 2018");
      TEST_DATE (25, 11, 2018, "%e %b %Y",      "25 lis 2018");
      TEST_DATE ( 1, 12, 2018,   "%Ob %Y",         "gru 2018");
    }
  else
    g_test_incomplete ("locale pl_PL not available, skipping Polish month names test");

  setlocale (LC_ALL, "ru_RU.utf-8");
#ifdef G_OS_WIN32
  SetThreadLocale (MAKELCID (MAKELANGID (LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA), SORT_DEFAULT));
#endif
  if (strstr (setlocale (LC_ALL, NULL), "ru_RU") != NULL)
    {
      TEST_DATE ( 3,  1, 2018,      "%d %B %Y",  "03 января 2018");
      TEST_DATE ( 4,  2, 2018,      "%e %B %Y", " 4 февраля 2018");
      TEST_DATE (23,  3, 2018,      "%d %B %Y",   "23 марта 2018");
      TEST_DATE ( 1,  4, 2018,        "%OB %Y",     "Апрель 2018");
      TEST_DATE ( 1,  5, 2018,        "%OB %Y",        "Май 2018");
      TEST_DATE ( 1,  6, 2018,        "%OB %Y",       "Июнь 2018");
      TEST_DATE (24,  7, 2018,      "%e %b %Y",     "24 июл 2018");
      TEST_DATE ( 1,  8, 2018,        "%Ob %Y",        "авг 2018");
      /* This difference is very important in Russian:  */
      TEST_DATE (19,  5, 2018,      "%e %b %Y",     "19 мая 2018");
      TEST_DATE (20,  5, 2018, "%Ob, %d-е, %Y", "май, 20-е, 2018");
    }
  else
    g_test_incomplete ("locale ru_RU not available, skipping Russian month names test");

  g_date_free (gdate);

  setlocale (LC_ALL, oldlocale);
#ifdef G_OS_WIN32
  SetThreadLocale (old_lcid);
#endif
  g_free (oldlocale);
#endif  /* defined(HAVE_LANGINFO_ABALTMON) || defined(G_OS_WIN32) */
}

static void
test_year (gconstpointer t)
{
  GDateYear y = GPOINTER_TO_INT (t);
  GDateMonth m;
  GDateDay day;
  guint32 j;
  GDate *d;
  gint i;
  GDate tmp;

  guint32 first_day_of_year = G_DATE_BAD_JULIAN;
  guint16 days_in_year = g_date_is_leap_year (y) ? 366 : 365;
  guint   sunday_week_of_year = 0;
  guint   sunday_weeks_in_year = g_date_get_sunday_weeks_in_year (y);
  guint   monday_week_of_year = 0;
  guint   monday_weeks_in_year = g_date_get_monday_weeks_in_year (y);
  guint   iso8601_week_of_year = 0;

  g_assert (g_date_valid_year (y));
  /* Years ought to have roundabout 52 weeks */
  g_assert (sunday_weeks_in_year == 52 || sunday_weeks_in_year == 53);
  g_assert (monday_weeks_in_year == 52 || monday_weeks_in_year == 53);

  m = 1;
  while (m < 13)
    {
      guint8 dim = g_date_get_days_in_month (m, y);
      GDate days[31];

      g_date_clear (days, 31);

      g_assert (dim > 0 && dim < 32);
      g_assert (g_date_valid_month (m));

      day = 1;
      while (day <= dim)
        {
          g_assert (g_date_valid_dmy (day, m, y));

          d = &days[day - 1];
          //g_assert (!g_date_valid (d));

          g_date_set_dmy (d, day, m, y);

          g_assert (g_date_valid (d));

          if (m == G_DATE_JANUARY && day == 1)
            first_day_of_year = g_date_get_julian (d);

          g_assert (first_day_of_year != G_DATE_BAD_JULIAN);

          g_assert_cmpint (g_date_get_month (d), ==, m);
          g_assert_cmpint (g_date_get_year (d), ==, y);
          g_assert_cmpint (g_date_get_day (d), ==, day);

          g_assert (g_date_get_julian (d) + 1 - first_day_of_year ==
                    g_date_get_day_of_year (d));

          if (m == G_DATE_DECEMBER && day == 31)
            g_assert_cmpint (g_date_get_day_of_year (d), ==, days_in_year);

          g_assert_cmpint (g_date_get_day_of_year (d), <=, days_in_year);
          g_assert_cmpint (g_date_get_monday_week_of_year (d), <=, monday_weeks_in_year);
          g_assert_cmpint (g_date_get_monday_week_of_year (d), >=, monday_week_of_year);

          if (g_date_get_weekday(d) == G_DATE_MONDAY)
            {
              g_assert_cmpint (g_date_get_monday_week_of_year (d) - monday_week_of_year, ==, 1);
              if ((m == G_DATE_JANUARY && day <= 4) ||
                  (m == G_DATE_DECEMBER && day >= 29))
                 g_assert_cmpint (g_date_get_iso8601_week_of_year (d), ==, 1);
              else
                g_assert_cmpint (g_date_get_iso8601_week_of_year (d) - iso8601_week_of_year, ==, 1);
            }
          else
            {
              g_assert_cmpint (g_date_get_monday_week_of_year(d) - monday_week_of_year, ==, 0);
              if (!(day == 1 && m == G_DATE_JANUARY))
                g_assert_cmpint (g_date_get_iso8601_week_of_year(d) - iso8601_week_of_year, ==, 0);
            }

          monday_week_of_year = g_date_get_monday_week_of_year (d);
          iso8601_week_of_year = g_date_get_iso8601_week_of_year (d);

          g_assert_cmpint (g_date_get_sunday_week_of_year (d), <=, sunday_weeks_in_year);
          g_assert_cmpint (g_date_get_sunday_week_of_year (d), >=, sunday_week_of_year);
          if (g_date_get_weekday(d) == G_DATE_SUNDAY)
            g_assert_cmpint (g_date_get_sunday_week_of_year (d) - sunday_week_of_year, ==, 1);
          else
            g_assert_cmpint (g_date_get_sunday_week_of_year (d) - sunday_week_of_year, ==, 0);

          sunday_week_of_year = g_date_get_sunday_week_of_year (d);

          g_assert_cmpint (g_date_compare (d, d), ==, 0);

          i = 1;
          while (i < 402) /* Need to get 400 year increments in */
            {
              tmp = *d;
              g_date_add_days (d, i);
              g_assert_cmpint (g_date_compare (d, &tmp), >, 0);
              g_date_subtract_days (d, i);
              g_assert_cmpint (g_date_get_day (d), ==, day);
              g_assert_cmpint (g_date_get_month (d), ==, m);
              g_assert_cmpint (g_date_get_year (d), ==, y);

              tmp = *d;
              g_date_add_months (d, i);
              g_assert_cmpint (g_date_compare (d, &tmp), >, 0);
              g_date_subtract_months (d, i);
              g_assert_cmpint (g_date_get_month (d), ==, m);
              g_assert_cmpint (g_date_get_year (d), ==, y);

              if (day < 29)
                g_assert_cmpint (g_date_get_day (d), ==, day);
              else
                g_date_set_day (d, day);

              tmp = *d;
              g_date_add_years (d, i);
              g_assert_cmpint (g_date_compare (d, &tmp), >, 0);
              g_date_subtract_years (d, i);
              g_assert_cmpint (g_date_get_month (d), ==, m);
              g_assert_cmpint (g_date_get_year (d), ==, y);

              if (m != 2 && day != 29)
                g_assert_cmpint (g_date_get_day (d), ==, day);
              else
                g_date_set_day (d, day); /* reset */

              i += 10;
            }

          j = g_date_get_julian (d);

          ++day;
        }
      ++m;
   }

  /* at this point, d is the last day of year y */
  g_date_set_dmy (&tmp, 1, 1, y + 1);
  g_assert_cmpint (j + 1, ==, g_date_get_julian (&tmp));

  g_date_add_days (&tmp, 1);
  g_assert_cmpint (j + 2, ==, g_date_get_julian (&tmp));
}

static void
test_clamp (void)
{
  GDate d1, d2, d, o;

  g_date_set_dmy (&d1, 1, 1, 1970);
  g_date_set_dmy (&d2, 1, 1, 1980);
  g_date_set_dmy (&d, 1, 1, 1);

  o = d;
  g_date_clamp (&o, NULL, NULL);
  g_assert (g_date_compare (&o, &d) == 0);

  g_date_clamp (&o,  &d1, &d2);
  g_assert (g_date_compare (&o, &d1) == 0);

  g_date_set_dmy (&o, 1, 1, 2000);

  g_date_clamp (&o,  &d1, &d2);
  g_assert (g_date_compare (&o, &d2) == 0);
}

static void
test_order (void)
{
  GDate d1, d2;

  g_date_set_dmy (&d1, 1, 1, 1970);
  g_date_set_dmy (&d2, 1, 1, 1980);

  g_assert (g_date_compare (&d1, &d2) == -1);
  g_date_order (&d2, &d1);
  g_assert (g_date_compare (&d1, &d2) == 1);
}

static void
test_copy (void)
{
  GDate *d;
  GDate *c;

  d = g_date_new ();
  g_assert_false (g_date_valid (d));

  c = g_date_copy (d);
  g_assert_nonnull (c);
  g_assert_false (g_date_valid (c));
  g_date_free (c);

  g_date_set_day (d, 10);

  c = g_date_copy (d);
  g_date_set_month (c, 1);
  g_date_set_year (c, 2015);
  g_assert_true (g_date_valid (c));
  g_assert_cmpuint (g_date_get_day (c), ==, 10);
  g_date_free (c);

  g_date_free (d);
}

int
main (int argc, char** argv)
{
  gchar *path;
  gint i;

  /* Try to get all the leap year cases. */
  int check_years[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 98, 99, 100, 101, 102, 103, 397,
    398, 399, 400, 401, 402, 403, 404, 405, 406,
    1598, 1599, 1600, 1601, 1602, 1650, 1651,
    1897, 1898, 1899, 1900, 1901, 1902, 1903,
    1961, 1962, 1963, 1964, 1965, 1967,
    1968, 1969, 1970, 1971, 1972, 1973, 1974, 1975, 1976,
    1977, 1978, 1979, 1980, 1981, 1982, 1983, 1984, 1985,
    1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994,
    1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
    2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012,
    3000, 3001, 3002, 3998, 3999, 4000, 4001, 4002, 4003
  };

  g_setenv ("LC_ALL", "en_US.utf-8", TRUE);
  setlocale (LC_ALL, "");
#ifdef G_OS_WIN32
  SetThreadLocale (MAKELCID (MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT));
#endif

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/date/basic", test_basic);
  g_test_add_func ("/date/empty", test_empty_constructor);
  g_test_add_func ("/date/dmy", test_dmy_constructor);
  g_test_add_func ("/date/julian", test_julian_constructor);
  g_test_add_func ("/date/dates", test_dates);
  g_test_add_func ("/date/parse", test_parse);
  g_test_add_func ("/date/month_names", test_month_names);
  g_test_add_func ("/date/clamp", test_clamp);
  g_test_add_func ("/date/order", test_order);
  for (i = 0; i < G_N_ELEMENTS (check_years); i++)
    {
      path = g_strdup_printf ("/date/year/%d", check_years[i]);
      g_test_add_data_func (path, GINT_TO_POINTER(check_years[i]), test_year);
      g_free (path);
    }
  g_test_add_func ("/date/copy", test_copy);

  return g_test_run ();
}


