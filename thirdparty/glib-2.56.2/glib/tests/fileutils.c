/* Unit tests for gfileutils
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

#include "config.h"
#include <string.h>
#include <errno.h>

/* We are testing some deprecated APIs here */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <glib.h>

/* Test our stdio wrappers here */
#define G_STDIO_NO_WRAP_ON_UNIX
#include <glib/gstdio.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <fcntl.h>
#include <utime.h>
#ifdef G_OS_WIN32
#include <windows.h>
#endif

#define S G_DIR_SEPARATOR_S

static void
check_string (gchar *str, const gchar *expected)
{
  g_assert_nonnull (str);
  g_assert_cmpstr (str, ==, expected);
  g_free (str);
}

static void
test_build_path (void)
{
/*  check_string (g_build_path ("", NULL), "");*/
  check_string (g_build_path ("", "", NULL), "");
  check_string (g_build_path ("", "x", NULL), "x");
  check_string (g_build_path ("", "x", "y",  NULL), "xy");
  check_string (g_build_path ("", "x", "y", "z", NULL), "xyz");

/*  check_string (g_build_path (":", NULL), "");*/
  check_string (g_build_path (":", ":", NULL), ":");
  check_string (g_build_path (":", ":x", NULL), ":x");
  check_string (g_build_path (":", "x:", NULL), "x:");
  check_string (g_build_path (":", "", "x", NULL), "x");
  check_string (g_build_path (":", "", ":x", NULL), ":x");
  check_string (g_build_path (":", ":", "x", NULL), ":x");
  check_string (g_build_path (":", "::", "x", NULL), "::x");
  check_string (g_build_path (":", "x", "", NULL), "x");
  check_string (g_build_path (":", "x:", "", NULL), "x:");
  check_string (g_build_path (":", "x", ":", NULL), "x:");
  check_string (g_build_path (":", "x", "::", NULL), "x::");
  check_string (g_build_path (":", "x", "y",  NULL), "x:y");
  check_string (g_build_path (":", ":x", "y", NULL), ":x:y");
  check_string (g_build_path (":", "x", "y:", NULL), "x:y:");
  check_string (g_build_path (":", ":x:", ":y:", NULL), ":x:y:");
  check_string (g_build_path (":", ":x::", "::y:", NULL), ":x:y:");
  check_string (g_build_path (":", "x", "","y",  NULL), "x:y");
  check_string (g_build_path (":", "x", ":", "y",  NULL), "x:y");
  check_string (g_build_path (":", "x", "::", "y",  NULL), "x:y");
  check_string (g_build_path (":", "x", "y", "z", NULL), "x:y:z");
  check_string (g_build_path (":", ":x:", ":y:", ":z:", NULL), ":x:y:z:");
  check_string (g_build_path (":", "::x::", "::y::", "::z::", NULL), "::x:y:z::");

/*  check_string (g_build_path ("::", NULL), "");*/
  check_string (g_build_path ("::", "::", NULL), "::");
  check_string (g_build_path ("::", ":::", NULL), ":::");
  check_string (g_build_path ("::", "::x", NULL), "::x");
  check_string (g_build_path ("::", "x::", NULL), "x::");
  check_string (g_build_path ("::", "", "x", NULL), "x");
  check_string (g_build_path ("::", "", "::x", NULL), "::x");
  check_string (g_build_path ("::", "::", "x", NULL), "::x");
  check_string (g_build_path ("::", "::::", "x", NULL), "::::x");
  check_string (g_build_path ("::", "x", "", NULL), "x");
  check_string (g_build_path ("::", "x::", "", NULL), "x::");
  check_string (g_build_path ("::", "x", "::", NULL), "x::");

  /* This following is weird, but keeps the definition simple */
  check_string (g_build_path ("::", "x", ":::", NULL), "x:::::");
  check_string (g_build_path ("::", "x", "::::", NULL), "x::::");
  check_string (g_build_path ("::", "x", "y",  NULL), "x::y");
  check_string (g_build_path ("::", "::x", "y", NULL), "::x::y");
  check_string (g_build_path ("::", "x", "y::", NULL), "x::y::");
  check_string (g_build_path ("::", "::x::", "::y::", NULL), "::x::y::");
  check_string (g_build_path ("::", "::x:::", ":::y::", NULL), "::x::::y::");
  check_string (g_build_path ("::", "::x::::", "::::y::", NULL), "::x::y::");
  check_string (g_build_path ("::", "x", "", "y",  NULL), "x::y");
  check_string (g_build_path ("::", "x", "::", "y",  NULL), "x::y");
  check_string (g_build_path ("::", "x", "::::", "y",  NULL), "x::y");
  check_string (g_build_path ("::", "x", "y", "z", NULL), "x::y::z");
  check_string (g_build_path ("::", "::x::", "::y::", "::z::", NULL), "::x::y::z::");
  check_string (g_build_path ("::", ":::x:::", ":::y:::", ":::z:::", NULL), ":::x::::y::::z:::");
  check_string (g_build_path ("::", "::::x::::", "::::y::::", "::::z::::", NULL), "::::x::y::z::::");
}

static void
test_build_pathv (void)
{
  gchar *args[10];

  g_assert_null (g_build_pathv ("", NULL));
  args[0] = NULL;
  check_string (g_build_pathv ("", args), "");
  args[0] = ""; args[1] = NULL;
  check_string (g_build_pathv ("", args), "");
  args[0] = "x"; args[1] = NULL;
  check_string (g_build_pathv ("", args), "x");
  args[0] = "x"; args[1] = "y"; args[2] = NULL;
  check_string (g_build_pathv ("", args), "xy");
  args[0] = "x"; args[1] = "y"; args[2] = "z", args[3] = NULL;
  check_string (g_build_pathv ("", args), "xyz");

  args[0] = NULL;
  check_string (g_build_pathv (":", args), "");
  args[0] = ":"; args[1] = NULL;
  check_string (g_build_pathv (":", args), ":");
  args[0] = ":x"; args[1] = NULL;
  check_string (g_build_pathv (":", args), ":x");
  args[0] = "x:"; args[1] = NULL;
  check_string (g_build_pathv (":", args), "x:");
  args[0] = ""; args[1] = "x"; args[2] = NULL;
  check_string (g_build_pathv (":", args), "x");
  args[0] = ""; args[1] = ":x"; args[2] = NULL;
  check_string (g_build_pathv (":", args), ":x");
  args[0] = ":"; args[1] = "x"; args[2] = NULL;
  check_string (g_build_pathv (":", args), ":x");
  args[0] = "::"; args[1] = "x"; args[2] = NULL;
  check_string (g_build_pathv (":", args), "::x");
  args[0] = "x"; args[1] = ""; args[2] = NULL;
  check_string (g_build_pathv (":", args), "x");
  args[0] = "x:"; args[1] = ""; args[2] = NULL;
  check_string (g_build_pathv (":", args), "x:");
  args[0] = "x"; args[1] = ":"; args[2] = NULL;
  check_string (g_build_pathv (":", args), "x:");
  args[0] = "x"; args[1] = "::"; args[2] = NULL;
  check_string (g_build_pathv (":", args), "x::");
  args[0] = "x"; args[1] = "y"; args[2] = NULL;
  check_string (g_build_pathv (":", args), "x:y");
  args[0] = ":x"; args[1] = "y"; args[2] = NULL;
  check_string (g_build_pathv (":", args), ":x:y");
  args[0] = "x"; args[1] = "y:"; args[2] = NULL;
  check_string (g_build_pathv (":", args), "x:y:");
  args[0] = ":x:"; args[1] = ":y:"; args[2] = NULL;
  check_string (g_build_pathv (":", args), ":x:y:");
  args[0] = ":x::"; args[1] = "::y:"; args[2] = NULL;
  check_string (g_build_pathv (":", args), ":x:y:");
  args[0] = "x"; args[1] = ""; args[2] = "y"; args[3] = NULL;
  check_string (g_build_pathv (":", args), "x:y");
  args[0] = "x"; args[1] = ":"; args[2] = "y"; args[3] = NULL;
  check_string (g_build_pathv (":", args), "x:y");
  args[0] = "x"; args[1] = "::"; args[2] = "y"; args[3] = NULL;
  check_string (g_build_pathv (":", args), "x:y");
  args[0] = "x"; args[1] = "y"; args[2] = "z"; args[3] = NULL;
  check_string (g_build_pathv (":", args), "x:y:z");
  args[0] = ":x:"; args[1] = ":y:"; args[2] = ":z:"; args[3] = NULL;
  check_string (g_build_pathv (":", args), ":x:y:z:");
  args[0] = "::x::"; args[1] = "::y::"; args[2] = "::z::"; args[3] = NULL;
  check_string (g_build_pathv (":", args), "::x:y:z::");

  args[0] = NULL;
  check_string (g_build_pathv ("::", args), "");
  args[0] = "::"; args[1] = NULL;
  check_string (g_build_pathv ("::", args), "::");
  args[0] = ":::"; args[1] = NULL;
  check_string (g_build_pathv ("::", args), ":::");
  args[0] = "::x"; args[1] = NULL;
  check_string (g_build_pathv ("::", args), "::x");
  args[0] = "x::"; args[1] = NULL;
  check_string (g_build_pathv ("::", args), "x::");
  args[0] = ""; args[1] = "x"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "x");
  args[0] = ""; args[1] = "::x"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "::x");
  args[0] = "::"; args[1] = "x"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "::x");
  args[0] = "::::"; args[1] = "x"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "::::x");
  args[0] = "x"; args[1] = ""; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "x");
  args[0] = "x::"; args[1] = ""; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "x::");
  args[0] = "x"; args[1] = "::"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "x::");
  /* This following is weird, but keeps the definition simple */
  args[0] = "x"; args[1] = ":::"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "x:::::");
  args[0] = "x"; args[1] = "::::"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "x::::");
  args[0] = "x"; args[1] = "y"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "x::y");
  args[0] = "::x"; args[1] = "y"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "::x::y");
  args[0] = "x"; args[1] = "y::"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "x::y::");
  args[0] = "::x::"; args[1] = "::y::"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "::x::y::");
  args[0] = "::x:::"; args[1] = ":::y::"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "::x::::y::");
  args[0] = "::x::::"; args[1] = "::::y::"; args[2] = NULL;
  check_string (g_build_pathv ("::", args), "::x::y::");
  args[0] = "x"; args[1] = ""; args[2] = "y"; args[3] = NULL;
  check_string (g_build_pathv ("::", args), "x::y");
  args[0] = "x"; args[1] = "::"; args[2] = "y"; args[3] = NULL;
  check_string (g_build_pathv ("::", args), "x::y");
  args[0] = "x"; args[1] = "::::"; args[2] = "y"; args[3] = NULL;
  check_string (g_build_pathv ("::", args), "x::y");
  args[0] = "x"; args[1] = "y"; args[2] = "z"; args[3] = NULL;
  check_string (g_build_pathv ("::", args), "x::y::z");
  args[0] = "::x::"; args[1] = "::y::"; args[2] = "::z::"; args[3] = NULL;
  check_string (g_build_pathv ("::", args), "::x::y::z::");
  args[0] = ":::x:::"; args[1] = ":::y:::"; args[2] = ":::z:::"; args[3] = NULL;
  check_string (g_build_pathv ("::", args), ":::x::::y::::z:::");
  args[0] = "::::x::::"; args[1] = "::::y::::"; args[2] = "::::z::::"; args[3] = NULL;
  check_string (g_build_pathv ("::", args), "::::x::y::z::::");
}

static void
test_build_filename (void)
{
/*  check_string (g_build_filename (NULL), "");*/
  check_string (g_build_filename (S, NULL), S);
  check_string (g_build_filename (S"x", NULL), S"x");
  check_string (g_build_filename ("x"S, NULL), "x"S);
  check_string (g_build_filename ("", "x", NULL), "x");
  check_string (g_build_filename ("", S"x", NULL), S"x");
  check_string (g_build_filename (S, "x", NULL), S"x");
  check_string (g_build_filename (S S, "x", NULL), S S"x");
  check_string (g_build_filename ("x", "", NULL), "x");
  check_string (g_build_filename ("x"S, "", NULL), "x"S);
  check_string (g_build_filename ("x", S, NULL), "x"S);
  check_string (g_build_filename ("x", S S, NULL), "x"S S);
  check_string (g_build_filename ("x", "y",  NULL), "x"S"y");
  check_string (g_build_filename (S"x", "y", NULL), S"x"S"y");
  check_string (g_build_filename ("x", "y"S, NULL), "x"S"y"S);
  check_string (g_build_filename (S"x"S, S"y"S, NULL), S"x"S"y"S);
  check_string (g_build_filename (S"x"S S, S S"y"S, NULL), S"x"S"y"S);
  check_string (g_build_filename ("x", "", "y",  NULL), "x"S"y");
  check_string (g_build_filename ("x", S, "y",  NULL), "x"S"y");
  check_string (g_build_filename ("x", S S, "y",  NULL), "x"S"y");
  check_string (g_build_filename ("x", "y", "z", NULL), "x"S"y"S"z");
  check_string (g_build_filename (S"x"S, S"y"S, S"z"S, NULL), S"x"S"y"S"z"S);
  check_string (g_build_filename (S S"x"S S, S S"y"S S, S S"z"S S, NULL), S S"x"S"y"S"z"S S);

#ifdef G_OS_WIN32

  /* Test also using the slash as file name separator */
#define U "/"
  /* check_string (g_build_filename (NULL), ""); */
  check_string (g_build_filename (U, NULL), U);
  check_string (g_build_filename (U"x", NULL), U"x");
  check_string (g_build_filename ("x"U, NULL), "x"U);
  check_string (g_build_filename ("", U"x", NULL), U"x");
  check_string (g_build_filename ("", U"x", NULL), U"x");
  check_string (g_build_filename (U, "x", NULL), U"x");
  check_string (g_build_filename (U U, "x", NULL), U U"x");
  check_string (g_build_filename (U S, "x", NULL), U S"x");
  check_string (g_build_filename ("x"U, "", NULL), "x"U);
  check_string (g_build_filename ("x"S"y", "z"U"a", NULL), "x"S"y"S"z"U"a");
  check_string (g_build_filename ("x", U, NULL), "x"U);
  check_string (g_build_filename ("x", U U, NULL), "x"U U);
  check_string (g_build_filename ("x", S U, NULL), "x"S U);
  check_string (g_build_filename (U"x", "y", NULL), U"x"U"y");
  check_string (g_build_filename ("x", "y"U, NULL), "x"U"y"U);
  check_string (g_build_filename (U"x"U, U"y"U, NULL), U"x"U"y"U);
  check_string (g_build_filename (U"x"U U, U U"y"U, NULL), U"x"U"y"U);
  check_string (g_build_filename ("x", U, "y",  NULL), "x"U"y");
  check_string (g_build_filename ("x", U U, "y",  NULL), "x"U"y");
  check_string (g_build_filename ("x", U S, "y",  NULL), "x"S"y");
  check_string (g_build_filename ("x", S U, "y",  NULL), "x"U"y");
  check_string (g_build_filename ("x", U "y", "z", NULL), "x"U"y"U"z");
  check_string (g_build_filename ("x", S "y", "z", NULL), "x"S"y"S"z");
  check_string (g_build_filename ("x", S "y", "z", U, "a", "b", NULL), "x"S"y"S"z"U"a"U"b");
  check_string (g_build_filename (U"x"U, U"y"U, U"z"U, NULL), U"x"U"y"U"z"U);
  check_string (g_build_filename (U U"x"U U, U U"y"U U, U U"z"U U, NULL), U U"x"U"y"U"z"U U);

#undef U

#endif /* G_OS_WIN32 */

}

static void
test_build_filenamev (void)
{
  gchar *args[10];

  args[0] = NULL;
  check_string (g_build_filenamev (args), "");
  args[0] = S; args[1] = NULL;
  check_string (g_build_filenamev (args), S);
  args[0] = S"x"; args[1] = NULL;
  check_string (g_build_filenamev (args), S"x");
  args[0] = "x"S; args[1] = NULL;
  check_string (g_build_filenamev (args), "x"S);
  args[0] = ""; args[1] = "x"; args[2] = NULL;
  check_string (g_build_filenamev (args), "x");
  args[0] = ""; args[1] = S"x"; args[2] = NULL;
  check_string (g_build_filenamev (args), S"x");
  args[0] = S; args[1] = "x"; args[2] = NULL;
  check_string (g_build_filenamev (args), S"x");
  args[0] = S S; args[1] = "x"; args[2] = NULL;
  check_string (g_build_filenamev (args), S S"x");
  args[0] = "x"; args[1] = ""; args[2] = NULL;
  check_string (g_build_filenamev (args), "x");
  args[0] = "x"S; args[1] = ""; args[2] = NULL;
  check_string (g_build_filenamev (args), "x"S);
  args[0] = "x"; args[1] = S; args[2] = NULL;
  check_string (g_build_filenamev (args), "x"S);
  args[0] = "x"; args[1] = S S; args[2] = NULL;
  check_string (g_build_filenamev (args), "x"S S);
  args[0] = "x"; args[1] = "y"; args[2] = NULL;
  check_string (g_build_filenamev (args), "x"S"y");
  args[0] = S"x"; args[1] = "y"; args[2] = NULL;
  check_string (g_build_filenamev (args), S"x"S"y");
  args[0] = "x"; args[1] = "y"S; args[2] = NULL;
  check_string (g_build_filenamev (args), "x"S"y"S);
  args[0] = S"x"S; args[1] = S"y"S; args[2] = NULL;
  check_string (g_build_filenamev (args), S"x"S"y"S);
  args[0] = S"x"S S; args[1] = S S"y"S; args[2] = NULL;
  check_string (g_build_filenamev (args), S"x"S"y"S);
  args[0] = "x"; args[1] = ""; args[2] = "y"; args[3] = NULL;
  check_string (g_build_filenamev (args), "x"S"y");
  args[0] = "x"; args[1] = S; args[2] = "y"; args[3] = NULL;
  check_string (g_build_filenamev (args), "x"S"y");
  args[0] = "x"; args[1] = S S; args[2] = "y"; args[3] = NULL;
  check_string (g_build_filenamev (args), "x"S"y");
  args[0] = "x"; args[1] = "y"; args[2] = "z"; args[3] = NULL;
  check_string (g_build_filenamev (args), "x"S"y"S"z");
  args[0] = S"x"S; args[1] = S"y"S; args[2] = S"z"S; args[3] = NULL;
  check_string (g_build_filenamev (args), S"x"S"y"S"z"S);
  args[0] = S S"x"S S; args[1] = S S"y"S S; args[2] = S S"z"S S; args[3] = NULL;
  check_string (g_build_filenamev (args), S S"x"S"y"S"z"S S);

#ifdef G_OS_WIN32

  /* Test also using the slash as file name separator */
#define U "/"
  args[0] = NULL;
  check_string (g_build_filenamev (args), "");
  args[0] = U; args[1] = NULL;
  check_string (g_build_filenamev (args), U);
  args[0] = U"x"; args[1] = NULL;
  check_string (g_build_filenamev (args), U"x");
  args[0] = "x"U; args[1] = NULL;
  check_string (g_build_filenamev (args), "x"U);
  args[0] = ""; args[1] = U"x"; args[2] = NULL;
  check_string (g_build_filenamev (args), U"x");
  args[0] = ""; args[1] = U"x"; args[2] = NULL;
  check_string (g_build_filenamev (args), U"x");
  args[0] = U; args[1] = "x"; args[2] = NULL;
  check_string (g_build_filenamev (args), U"x");
  args[0] = U U; args[1] = "x"; args[2] = NULL;
  check_string (g_build_filenamev (args), U U"x");
  args[0] = U S; args[1] = "x"; args[2] = NULL;
  check_string (g_build_filenamev (args), U S"x");
  args[0] = "x"U; args[1] = ""; args[2] = NULL;
  check_string (g_build_filenamev (args), "x"U);
  args[0] = "x"S"y"; args[1] = "z"U"a"; args[2] = NULL;
  check_string (g_build_filenamev (args), "x"S"y"S"z"U"a");
  args[0] = "x"; args[1] = U; args[2] = NULL;
  check_string (g_build_filenamev (args), "x"U);
  args[0] = "x"; args[1] = U U; args[2] = NULL;
  check_string (g_build_filenamev (args), "x"U U);
  args[0] = "x"; args[1] = S U; args[2] = NULL;
  check_string (g_build_filenamev (args), "x"S U);
  args[0] = U"x"; args[1] = "y"; args[2] = NULL;
  check_string (g_build_filenamev (args), U"x"U"y");
  args[0] = "x"; args[1] = "y"U; args[2] = NULL;
  check_string (g_build_filenamev (args), "x"U"y"U);
  args[0] = U"x"U; args[1] = U"y"U; args[2] = NULL;
  check_string (g_build_filenamev (args), U"x"U"y"U);
  args[0] = U"x"U U; args[1] = U U"y"U; args[2] = NULL;
  check_string (g_build_filenamev (args), U"x"U"y"U);
  args[0] = "x"; args[1] = U; args[2] = "y", args[3] = NULL;
  check_string (g_build_filenamev (args), "x"U"y");
  args[0] = "x"; args[1] = U U; args[2] = "y", args[3] = NULL;
  check_string (g_build_filenamev (args), "x"U"y");
  args[0] = "x"; args[1] = U S; args[2] = "y", args[3] = NULL;
  check_string (g_build_filenamev (args), "x"S"y");
  args[0] = "x"; args[1] = S U; args[2] = "y", args[3] = NULL;
  check_string (g_build_filenamev (args), "x"U"y");
  args[0] = "x"; args[1] = U "y"; args[2] = "z", args[3] = NULL;
  check_string (g_build_filenamev (args), "x"U"y"U"z");
  args[0] = "x"; args[1] = S "y"; args[2] = "z", args[3] = NULL;
  check_string (g_build_filenamev (args), "x"S"y"S"z");
  args[0] = "x"; args[1] = S "y"; args[2] = "z", args[3] = U;
  args[4] = "a"; args[5] = "b"; args[6] = NULL;
  check_string (g_build_filenamev (args), "x"S"y"S"z"U"a"U"b");
  args[0] = U"x"U; args[1] = U"y"U; args[2] = U"z"U, args[3] = NULL;
  check_string (g_build_filenamev (args), U"x"U"y"U"z"U);
  args[0] = U U"x"U U; args[1] = U U"y"U U; args[2] = U U"z"U U, args[3] = NULL;
  check_string (g_build_filenamev (args), U U"x"U"y"U"z"U U);

#undef U

#endif /* G_OS_WIN32 */
}

#undef S

static void
test_mkdir_with_parents_1 (const gchar *base)
{
  char *p0 = g_build_filename (base, "fum", NULL);
  char *p1 = g_build_filename (p0, "tem", NULL);
  char *p2 = g_build_filename (p1, "zap", NULL);
  FILE *f;

  g_remove (p2);
  g_remove (p1);
  g_remove (p0);

  if (g_file_test (p0, G_FILE_TEST_EXISTS))
    g_error ("failed, %s exists, cannot test g_mkdir_with_parents\n", p0);

  if (g_file_test (p1, G_FILE_TEST_EXISTS))
    g_error ("failed, %s exists, cannot test g_mkdir_with_parents\n", p1);

  if (g_file_test (p2, G_FILE_TEST_EXISTS))
    g_error ("failed, %s exists, cannot test g_mkdir_with_parents\n", p2);

  if (g_mkdir_with_parents (p2, 0777) == -1)
    {
      int errsv = errno;
      g_error ("failed, g_mkdir_with_parents(%s) failed: %s\n", p2, g_strerror (errsv));
    }

  if (!g_file_test (p2, G_FILE_TEST_IS_DIR))
    g_error ("failed, g_mkdir_with_parents(%s) succeeded, but %s is not a directory\n", p2, p2);

  if (!g_file_test (p1, G_FILE_TEST_IS_DIR))
    g_error ("failed, g_mkdir_with_parents(%s) succeeded, but %s is not a directory\n", p2, p1);

  if (!g_file_test (p0, G_FILE_TEST_IS_DIR))
    g_error ("failed, g_mkdir_with_parents(%s) succeeded, but %s is not a directory\n", p2, p0);

  g_rmdir (p2);
  if (g_file_test (p2, G_FILE_TEST_EXISTS))
    g_error ("failed, did g_rmdir(%s), but %s is still there\n", p2, p2);

  g_rmdir (p1);
  if (g_file_test (p1, G_FILE_TEST_EXISTS))
    g_error ("failed, did g_rmdir(%s), but %s is still there\n", p1, p1);

  f = g_fopen (p1, "w");
  if (f == NULL)
    g_error ("failed, couldn't create file %s\n", p1);
  fclose (f);

  if (g_mkdir_with_parents (p1, 0666) == 0)
    g_error ("failed, g_mkdir_with_parents(%s) succeeded, even if %s is a file\n", p1, p1);

  if (g_mkdir_with_parents (p2, 0666) == 0)
    g_error("failed, g_mkdir_with_parents(%s) succeeded, even if %s is a file\n", p2, p1);

  g_remove (p2);
  g_remove (p1);
  g_remove (p0);

  g_free (p2);
  g_free (p1);
  g_free (p0);
}

static void
test_mkdir_with_parents (void)
{
  gchar *cwd;
  if (g_test_verbose())
    g_printerr ("checking g_mkdir_with_parents() in subdir ./hum/");
  test_mkdir_with_parents_1 ("hum");
  g_remove ("hum");
  if (g_test_verbose())
    g_printerr ("checking g_mkdir_with_parents() in subdir ./hii///haa/hee/");
  test_mkdir_with_parents_1 ("hii///haa/hee");
  g_remove ("hii/haa/hee");
  g_remove ("hii/haa");
  g_remove ("hii");
  cwd = g_get_current_dir ();
  if (g_test_verbose())
    g_printerr ("checking g_mkdir_with_parents() in cwd: %s", cwd);
  test_mkdir_with_parents_1 (cwd);
  g_free (cwd);

  g_assert_cmpint (g_mkdir_with_parents (NULL, 0), ==, -1);
  g_assert_cmpint (errno, ==, EINVAL);
}

static void
test_format_size_for_display (void)
{
#ifdef G_OS_WIN32
  SetThreadLocale (MAKELCID (MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT));
#endif
  /* nobody called setlocale(), so we should get "C" behaviour... */
  check_string (g_format_size_for_display (0), "0 bytes");
  check_string (g_format_size_for_display (1), "1 byte");
  check_string (g_format_size_for_display (2), "2 bytes");
  check_string (g_format_size_for_display (1024), "1.0 KB");
  check_string (g_format_size_for_display (1024 * 1024), "1.0 MB");
  check_string (g_format_size_for_display (1024 * 1024 * 1024), "1.0 GB");
  check_string (g_format_size_for_display (1024ULL * 1024 * 1024 * 1024), "1.0 TB");
  check_string (g_format_size_for_display (1024ULL * 1024 * 1024 * 1024 * 1024), "1.0 PB");
  check_string (g_format_size_for_display (1024ULL * 1024 * 1024 * 1024 * 1024 * 1024), "1.0 EB");

  check_string (g_format_size (0), "0 bytes");
  check_string (g_format_size (1), "1 byte");
  check_string (g_format_size (2), "2 bytes");
  check_string (g_format_size (1000ULL), "1.0 kB");
  check_string (g_format_size (1000ULL * 1000), "1.0 MB");
  check_string (g_format_size (1000ULL * 1000 * 1000), "1.0 GB");
  check_string (g_format_size (1000ULL * 1000 * 1000 * 1000), "1.0 TB");
  check_string (g_format_size (1000ULL * 1000 * 1000 * 1000 * 1000), "1.0 PB");
  check_string (g_format_size (1000ULL * 1000 * 1000 * 1000 * 1000 * 1000), "1.0 EB");

  check_string (g_format_size_full (0, G_FORMAT_SIZE_IEC_UNITS), "0 bytes");
  check_string (g_format_size_full (1, G_FORMAT_SIZE_IEC_UNITS), "1 byte");
  check_string (g_format_size_full (2, G_FORMAT_SIZE_IEC_UNITS), "2 bytes");

  check_string (g_format_size_full (2048ULL, G_FORMAT_SIZE_IEC_UNITS), "2.0 KiB");
  check_string (g_format_size_full (2048ULL * 1024, G_FORMAT_SIZE_IEC_UNITS), "2.0 MiB");
  check_string (g_format_size_full (2048ULL * 1024 * 1024, G_FORMAT_SIZE_IEC_UNITS), "2.0 GiB");
  check_string (g_format_size_full (2048ULL * 1024 * 1024 * 1024, G_FORMAT_SIZE_IEC_UNITS), "2.0 TiB");
  check_string (g_format_size_full (2048ULL * 1024 * 1024 * 1024 * 1024, G_FORMAT_SIZE_IEC_UNITS), "2.0 PiB");
  check_string (g_format_size_full (2048ULL * 1024 * 1024 * 1024 * 1024 * 1024, G_FORMAT_SIZE_IEC_UNITS), "2.0 EiB");

  check_string (g_format_size_full (238472938, G_FORMAT_SIZE_IEC_UNITS), "227.4 MiB");
  check_string (g_format_size_full (238472938, G_FORMAT_SIZE_DEFAULT), "238.5 MB");
  check_string (g_format_size_full (238472938, G_FORMAT_SIZE_LONG_FORMAT), "238.5 MB (238472938 bytes)");


  check_string (g_format_size_full (0, G_FORMAT_SIZE_BITS), "0 bits");
  check_string (g_format_size_full (1, G_FORMAT_SIZE_BITS), "1 bit");
  check_string (g_format_size_full (2, G_FORMAT_SIZE_BITS), "2 bits");

  check_string (g_format_size_full (2000ULL, G_FORMAT_SIZE_BITS), "2.0 kb");
  check_string (g_format_size_full (2000ULL * 1000, G_FORMAT_SIZE_BITS), "2.0 Mb");
  check_string (g_format_size_full (2000ULL * 1000 * 1000, G_FORMAT_SIZE_BITS), "2.0 Gb");
  check_string (g_format_size_full (2000ULL * 1000 * 1000 * 1000, G_FORMAT_SIZE_BITS), "2.0 Tb");
  check_string (g_format_size_full (2000ULL * 1000 * 1000 * 1000 * 1000, G_FORMAT_SIZE_BITS), "2.0 Pb");
  check_string (g_format_size_full (2000ULL * 1000 * 1000 * 1000 * 1000 * 1000, G_FORMAT_SIZE_BITS), "2.0 Eb");

  check_string (g_format_size_full (238472938, G_FORMAT_SIZE_BITS), "238.5 Mb");
  check_string (g_format_size_full (238472938, G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_LONG_FORMAT), "238.5 Mb (238472938 bits)");


  check_string (g_format_size_full (0, G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_IEC_UNITS), "0 bits");
  check_string (g_format_size_full (1, G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_IEC_UNITS), "1 bit");
  check_string (g_format_size_full (2, G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_IEC_UNITS), "2 bits");

  check_string (g_format_size_full (2048ULL, G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_IEC_UNITS), "2.0 Kib");
  check_string (g_format_size_full (2048ULL * 1024, G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_IEC_UNITS), "2.0 Mib");
  check_string (g_format_size_full (2048ULL * 1024 * 1024, G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_IEC_UNITS), "2.0 Gib");
  check_string (g_format_size_full (2048ULL * 1024 * 1024 * 1024, G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_IEC_UNITS), "2.0 Tib");
  check_string (g_format_size_full (2048ULL * 1024 * 1024 * 1024 * 1024, G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_IEC_UNITS), "2.0 Pib");
  check_string (g_format_size_full (2048ULL * 1024 * 1024 * 1024 * 1024 * 1024, G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_IEC_UNITS), "2.0 Eib");

  check_string (g_format_size_full (238472938, G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_IEC_UNITS), "227.4 Mib");
  check_string (g_format_size_full (238472938, G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_IEC_UNITS | G_FORMAT_SIZE_LONG_FORMAT), "227.4 Mib (238472938 bits)");
}

static void
test_file_errors (void)
{
#ifdef EEXIST
  g_assert_cmpint (g_file_error_from_errno (EEXIST), ==, G_FILE_ERROR_EXIST);
#endif
#ifdef EISDIR
  g_assert_cmpint (g_file_error_from_errno (EISDIR), ==, G_FILE_ERROR_ISDIR);
#endif
#ifdef EACCES
  g_assert_cmpint (g_file_error_from_errno (EACCES), ==, G_FILE_ERROR_ACCES);
#endif
#ifdef ENAMETOOLONG
  g_assert_cmpint (g_file_error_from_errno (ENAMETOOLONG), ==, G_FILE_ERROR_NAMETOOLONG);
#endif
#ifdef ENOENT
  g_assert_cmpint (g_file_error_from_errno (ENOENT), ==, G_FILE_ERROR_NOENT);
#endif
#ifdef ENOTDIR
  g_assert_cmpint (g_file_error_from_errno (ENOTDIR), ==, G_FILE_ERROR_NOTDIR);
#endif
#ifdef ENXIO
  g_assert_cmpint (g_file_error_from_errno (ENXIO), ==, G_FILE_ERROR_NXIO);
#endif
#ifdef ENODEV
  g_assert_cmpint (g_file_error_from_errno (ENODEV), ==, G_FILE_ERROR_NODEV);
#endif
#ifdef EROFS
  g_assert_cmpint (g_file_error_from_errno (EROFS), ==, G_FILE_ERROR_ROFS);
#endif
#ifdef ETXTBSY
  g_assert_cmpint (g_file_error_from_errno (ETXTBSY), ==, G_FILE_ERROR_TXTBSY);
#endif
#ifdef EFAULT
  g_assert_cmpint (g_file_error_from_errno (EFAULT), ==, G_FILE_ERROR_FAULT);
#endif
#ifdef ELOOP
  g_assert_cmpint (g_file_error_from_errno (ELOOP), ==, G_FILE_ERROR_LOOP);
#endif
#ifdef ENOSPC
  g_assert_cmpint (g_file_error_from_errno (ENOSPC), ==, G_FILE_ERROR_NOSPC);
#endif
#ifdef ENOMEM
  g_assert_cmpint (g_file_error_from_errno (ENOMEM), ==, G_FILE_ERROR_NOMEM);
#endif
#ifdef EMFILE
  g_assert_cmpint (g_file_error_from_errno (EMFILE), ==, G_FILE_ERROR_MFILE);
#endif
#ifdef ENFILE
  g_assert_cmpint (g_file_error_from_errno (ENFILE), ==, G_FILE_ERROR_NFILE);
#endif
#ifdef EBADF
  g_assert_cmpint (g_file_error_from_errno (EBADF), ==, G_FILE_ERROR_BADF);
#endif
#ifdef EINVAL
  g_assert_cmpint (g_file_error_from_errno (EINVAL), ==, G_FILE_ERROR_INVAL);
#endif
#ifdef EPIPE
  g_assert_cmpint (g_file_error_from_errno (EPIPE), ==, G_FILE_ERROR_PIPE);
#endif
#ifdef EAGAIN
  g_assert_cmpint (g_file_error_from_errno (EAGAIN), ==, G_FILE_ERROR_AGAIN);
#endif
#ifdef EINTR
  g_assert_cmpint (g_file_error_from_errno (EINTR), ==, G_FILE_ERROR_INTR);
#endif
#ifdef EIO
  g_assert_cmpint (g_file_error_from_errno (EIO), ==, G_FILE_ERROR_IO);
#endif
#ifdef EPERM
  g_assert_cmpint (g_file_error_from_errno (EPERM), ==, G_FILE_ERROR_PERM);
#endif
#ifdef ENOSYS
  g_assert_cmpint (g_file_error_from_errno (ENOSYS), ==, G_FILE_ERROR_NOSYS);
#endif
}

static void
test_basename (void)
{
  gchar *b;

  b = g_path_get_basename ("");
  g_assert_cmpstr (b, ==, ".");
  g_free (b);

  b = g_path_get_basename ("///");
  g_assert_cmpstr (b, ==, G_DIR_SEPARATOR_S);
  g_free (b);

  b = g_path_get_basename ("/a/b/c/d");
  g_assert_cmpstr (b, ==, "d");
  g_free (b);
}

static void
test_dir_make_tmp (void)
{
  gchar *name;
  GError *error = NULL;
  gint ret;

  name = g_dir_make_tmp ("testXXXXXXtest", &error);
  g_assert_no_error (error);
  g_assert_true (g_file_test (name, G_FILE_TEST_IS_DIR));
  ret = g_rmdir (name);
  g_assert_cmpint (ret, ==, 0);
  g_free (name);

  name = g_dir_make_tmp (NULL, &error);
  g_assert_no_error (error);
  g_assert_true (g_file_test (name, G_FILE_TEST_IS_DIR));
  ret = g_rmdir (name);
  g_assert_cmpint (ret, ==, 0);
  g_free (name);

  name = g_dir_make_tmp ("test/XXXXXX", &error);
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED);
  g_clear_error (&error);
  g_assert_null (name);

  name = g_dir_make_tmp ("XXXXxX", &error);
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED);
  g_clear_error (&error);
  g_assert_null (name);
}

static void
test_file_open_tmp (void)
{
  gchar *name = NULL;
  GError *error = NULL;
  gint fd;

  fd = g_file_open_tmp ("testXXXXXXtest", &name, &error);
  g_assert_cmpint (fd, !=, -1);
  g_assert_no_error (error);
  g_assert_nonnull (name);
  unlink (name);
  g_free (name);
  close (fd);

  fd = g_file_open_tmp (NULL, &name, &error);
  g_assert_cmpint (fd, !=, -1);
  g_assert_no_error (error);
  g_assert_nonnull (name);
  g_unlink (name);
  g_free (name);
  close (fd);

  name = NULL;
  fd = g_file_open_tmp ("test/XXXXXX", &name, &error);
  g_assert_cmpint (fd, ==, -1);
  g_assert_null (name);
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED);
  g_clear_error (&error);

  fd = g_file_open_tmp ("XXXXxX", &name, &error);
  g_assert_cmpint (fd, ==, -1);
  g_assert_null (name);
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED);
  g_clear_error (&error);
}

static void
test_mkstemp (void)
{
  gchar *name;
  gint fd;

  name = g_strdup ("testXXXXXXtest"),
  fd = g_mkstemp (name);
  g_assert_cmpint (fd, !=, -1);
  g_assert_null (strstr (name, "XXXXXX"));
  unlink (name);
  close (fd);
  g_free (name);

  name = g_strdup ("testYYYYYYtest"),
  fd = g_mkstemp (name);
  g_assert_cmpint (fd, ==, -1);
  g_free (name);
}

static void
test_mkdtemp (void)
{
  gchar *name;
  gchar *ret;

  name = g_strdup ("testXXXXXXtest"),
  ret = g_mkdtemp (name);
  g_assert (ret == name);
  g_assert_null (strstr (name, "XXXXXX"));
  g_rmdir (name);
  g_free (name);

  name = g_strdup ("testYYYYYYtest"),
  ret = g_mkdtemp (name);
  g_assert_null (ret);
  g_free (name);
}

static void
test_set_contents (void)
{
  GError *error = NULL;
  gint fd;
  gchar *name;
  gchar *buf;
  gsize len;
  gboolean ret;

  fd = g_file_open_tmp (NULL, &name, &error);
  g_assert_no_error (error);
  write (fd, "a", 1);
  close (fd);

  ret = g_file_get_contents (name, &buf, &len, &error);
  g_assert_true (ret);
  g_assert_no_error (error);
  g_assert_cmpstr (buf, ==, "a");
  g_free (buf);

  ret = g_file_set_contents (name, "b", 1, &error);
  g_assert_true (ret);
  g_assert_no_error (error);

  ret = g_file_get_contents (name, &buf, &len, &error);
  g_assert_true (ret);
  g_assert_no_error (error);
  g_assert_cmpstr (buf, ==, "b");
  g_free (buf);

  g_remove (name);
  g_free (name);
}

static void
test_read_link (void)
{
#ifdef HAVE_READLINK
#ifdef G_OS_UNIX
  int ret;
  const gchar *oldpath;
  gchar *cwd;
  gchar *newpath;
  gchar *badpath;
  gchar *path;
  GError *error = NULL;

  cwd = g_get_current_dir ();

  oldpath = g_test_get_filename (G_TEST_DIST, "4096-random-bytes", NULL);
  newpath = g_build_filename (cwd, "page-of-junk", NULL);
  badpath = g_build_filename (cwd, "4097-random-bytes", NULL);
  remove (newpath);
  ret = symlink (oldpath, newpath);
  g_assert_cmpint (ret, ==, 0);
  path = g_file_read_link (newpath, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (path, ==, oldpath);
  g_free (path);

  remove (newpath);
  ret = symlink (badpath, newpath);
  g_assert_cmpint (ret, ==, 0);
  path = g_file_read_link (newpath, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (path, ==, badpath);
  g_free (path);

  path = g_file_read_link (oldpath, &error);
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL);
  g_assert_null (path);
  g_error_free (error);

  remove (newpath);
  g_free (cwd);
  g_free (newpath);
  g_free (badpath);

#endif
#else
  g_test_skip ("Symbolic links not supported");
#endif
}

static void
test_stdio_wrappers (void)
{
  GStatBuf buf;
  gchar *cwd, *path;
  gint ret;
  struct utimbuf ut;
  GError *error = NULL;

  g_remove ("mkdir-test/test-create");
  ret = g_rmdir ("mkdir-test");
  g_assert (ret == 0 || errno == ENOENT);

  ret = g_stat ("mkdir-test", &buf);
  g_assert_cmpint (ret, ==, -1);
  ret = g_mkdir ("mkdir-test", 0666);
  g_assert_cmpint (ret, ==, 0);
  ret = g_stat ("mkdir-test", &buf);
  g_assert_cmpint (ret, ==, 0);
  g_assert_cmpint (S_ISDIR (buf.st_mode), !=, 0);

  cwd = g_get_current_dir ();
  path = g_build_filename (cwd, "mkdir-test", NULL);
  g_free (cwd);
  ret = g_chdir (path);
  g_assert_cmpint (errno, ==, EACCES);
  g_assert_cmpint (ret, ==, -1);
  ret = g_chmod (path, 0777);
  g_assert_cmpint (ret, ==, 0);
  ret = g_chdir (path);
  g_assert_cmpint (ret, ==, 0);
  cwd = g_get_current_dir ();
  g_assert_true (g_str_equal (cwd, path));
  g_free (cwd);
  g_free (path);

  ret = g_creat ("test-creat", 0555);
  g_close (ret, &error);
  g_assert_no_error (error);

  ret = g_access ("test-creat", F_OK);
  g_assert_cmpint (ret, ==, 0);

  ret = g_rename ("test-creat", "test-create");
  g_assert_cmpint (ret, ==, 0);

  ret = g_open ("test-create", O_RDONLY, 0666);
  g_close (ret, &error);
  g_assert_no_error (error);

  ut.actime = ut.modtime = (time_t)0;
  ret = g_utime ("test-create", &ut);
  g_assert_cmpint (ret, ==, 0);

  ret = g_lstat ("test-create", &buf);
  g_assert_cmpint (ret, ==, 0);
  g_assert_cmpint (buf.st_atime, ==, (time_t)0);
  g_assert_cmpint (buf.st_mtime, ==, (time_t)0);

  g_chdir ("..");
  g_remove ("mkdir-test/test-create");
  g_rmdir ("mkdir-test");
}

int
main (int   argc,
      char *argv[])
{
  g_setenv ("LC_ALL", "C", TRUE);
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/fileutils/build-path", test_build_path);
  g_test_add_func ("/fileutils/build-pathv", test_build_pathv);
  g_test_add_func ("/fileutils/build-filename", test_build_filename);
  g_test_add_func ("/fileutils/build-filenamev", test_build_filenamev);
  g_test_add_func ("/fileutils/mkdir-with-parents", test_mkdir_with_parents);
  g_test_add_func ("/fileutils/format-size-for-display", test_format_size_for_display);
  g_test_add_func ("/fileutils/errors", test_file_errors);
  g_test_add_func ("/fileutils/basename", test_basename);
  g_test_add_func ("/fileutils/dir-make-tmp", test_dir_make_tmp);
  g_test_add_func ("/fileutils/file-open-tmp", test_file_open_tmp);
  g_test_add_func ("/fileutils/mkstemp", test_mkstemp);
  g_test_add_func ("/fileutils/mkdtemp", test_mkdtemp);
  g_test_add_func ("/fileutils/set-contents", test_set_contents);
  g_test_add_func ("/fileutils/read-link", test_read_link);
  g_test_add_func ("/fileutils/stdio-wrappers", test_stdio_wrappers);

  return g_test_run ();
}
