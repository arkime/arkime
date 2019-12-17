dnl @synopsis AC_FUNC_VSNPRINTF_C99
dnl
dnl Check whether there is a vsnprintf() function with C99 semantics installed.
dnl
AC_DEFUN([AC_FUNC_VSNPRINTF_C99],
[AC_CACHE_CHECK(for C99 vsnprintf,
  ac_cv_func_vsnprintf_c99,
[AC_TRY_RUN(
[#include <stdio.h>
#include <stdarg.h>

int
doit(char * s, ...)
{
  char buffer[32];
  va_list args;
  int r;

  va_start(args, s);
  r = vsnprintf(buffer, 5, s, args);
  va_end(args);

  if (r != 7)
    exit(1);

  /* AIX 5.1 and Solaris seems to have a half-baked vsnprintf()
     implementation. The above will return 7 but if you replace
     the size of the buffer with 0, it borks! */
  va_start(args, s);
  r = vsnprintf(buffer, 0, s, args);
  va_end(args);

  if (r != 7)
    exit(1);

  exit(0);
}

int
main(void)
{
  doit("1234567");
  exit(1);
}], ac_cv_func_vsnprintf_c99=yes, ac_cv_func_vsnprintf_c99=no, ac_cv_func_vsnprintf_c99=no)])
dnl Note that the default is to be pessimistic in the case of cross compilation.
dnl If you know that the target has a C99 vsnprintf(), you can get around this
dnl by setting ac_func_vsnprintf_c99 to yes, as described in the Autoconf manual.
if test $ac_cv_func_vsnprintf_c99 = yes; then
  AC_DEFINE(HAVE_C99_VSNPRINTF, 1,
            [Define if you have a version of the vsnprintf function
             with semantics as specified by the ISO C99 standard.])
fi
])# AC_FUNC_VSNPRINTF_C99


dnl @synopsis AC_FUNC_SNPRINTF_C99
dnl
dnl Check whether there is a snprintf() function with C99 semantics installed.
dnl
AC_DEFUN([AC_FUNC_SNPRINTF_C99],
[AC_CACHE_CHECK(for C99 snprintf,
  ac_cv_func_snprintf_c99,
[AC_TRY_RUN(
[#include <stdio.h>
#include <stdarg.h>

int
doit()
{
  char buffer[32];
  va_list args;
  int r;

  r = snprintf(buffer, 5, "1234567");

  if (r != 7)
    exit(1);

  r = snprintf(buffer, 0, "1234567");

  if (r != 7)
    exit(1);

  r = snprintf(NULL, 0, "1234567");

  if (r != 7)
    exit(1);

  exit(0);
}

int
main(void)
{
  doit();
  exit(1);
}], ac_cv_func_snprintf_c99=yes, ac_cv_func_snprintf_c99=no, ac_cv_func_snprintf_c99=no)])
dnl Note that the default is to be pessimistic in the case of cross compilation.
dnl If you know that the target has a C99 snprintf(), you can get around this
dnl by setting ac_func_snprintf_c99 to yes, as described in the Autoconf manual.
if test $ac_cv_func_snprintf_c99 = yes; then
  AC_DEFINE(HAVE_C99_SNPRINTF, 1,
            [Define if you have a version of the snprintf function
             with semantics as specified by the ISO C99 standard.])
fi
])# AC_FUNC_SNPRINTF_C99


dnl @synopsis AC_FUNC_PRINTF_UNIX98
dnl
dnl Check whether the printf() family supports Unix98 %n$ positional parameters 
dnl
AC_DEFUN([AC_FUNC_PRINTF_UNIX98],
[AC_CACHE_CHECK(whether printf supports positional parameters,
  ac_cv_func_printf_unix98,
[AC_TRY_RUN(
[#include <stdio.h>

int
main (void)
{
  char buffer[128];

  sprintf (buffer, "%2\$d %3\$d %1\$d", 1, 2, 3);
  if (strcmp ("2 3 1", buffer) == 0)
    exit (0);
  exit (1);
}], ac_cv_func_printf_unix98=yes, ac_cv_func_printf_unix98=no, ac_cv_func_printf_unix98=no)])
dnl Note that the default is to be pessimistic in the case of cross compilation.
dnl If you know that the target printf() supports positional parameters, you can get around 
dnl this by setting ac_func_printf_unix98 to yes, as described in the Autoconf manual.
if test $ac_cv_func_printf_unix98 = yes; then
  AC_DEFINE(HAVE_UNIX98_PRINTF, 1,
            [Define if your printf function family supports positional parameters
             as specified by Unix98.])
fi
])# AC_FUNC_PRINTF_UNIX98

# Checks the location of the XML Catalog
# Usage:
#   JH_PATH_XML_CATALOG([ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# Defines XMLCATALOG and XML_CATALOG_FILE substitutions
AC_DEFUN([JH_PATH_XML_CATALOG],
[
  # check for the presence of the XML catalog
  AC_ARG_WITH([xml-catalog],
              AC_HELP_STRING([--with-xml-catalog=CATALOG],
                             [path to xml catalog to use]),,
              [with_xml_catalog=/etc/xml/catalog])
  jh_found_xmlcatalog=true
  XML_CATALOG_FILE="$with_xml_catalog"
  AC_SUBST([XML_CATALOG_FILE])
  AC_MSG_CHECKING([for XML catalog ($XML_CATALOG_FILE)])
  if test -f "$XML_CATALOG_FILE"; then
    AC_MSG_RESULT([found])
  else
    jh_found_xmlcatalog=false
    AC_MSG_RESULT([not found])
  fi

  # check for the xmlcatalog program
  AC_PATH_PROG(XMLCATALOG, xmlcatalog, no)
  if test "x$XMLCATALOG" = xno; then
    jh_found_xmlcatalog=false
  fi

  if $jh_found_xmlcatalog; then
    ifelse([$1],,[:],[$1])
  else
    ifelse([$2],,[AC_MSG_ERROR([could not find XML catalog])],[$2])
  fi
])

# Checks if a particular URI appears in the XML catalog
# Usage:
#   JH_CHECK_XML_CATALOG(URI, [FRIENDLY-NAME], [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
AC_DEFUN([JH_CHECK_XML_CATALOG],
[
  AC_REQUIRE([JH_PATH_XML_CATALOG],[JH_PATH_XML_CATALOG(,[:])])dnl
  AC_MSG_CHECKING([for ifelse([$2],,[$1],[$2]) in XML catalog])
  if $jh_found_xmlcatalog && \
     AC_RUN_LOG([$XMLCATALOG --noout "$XML_CATALOG_FILE" "$1" >&2]); then
    AC_MSG_RESULT([found])
    ifelse([$3],,,[$3
])dnl
  else
    AC_MSG_RESULT([not found])
    ifelse([$4],,
       [AC_MSG_ERROR([could not find ifelse([$2],,[$1],[$2]) in XML catalog])],
       [$4])
  fi
])


# signed.m4 serial 1 (gettext-0.10.40)
dnl Copyright (C) 2001-2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Bruno Haible.

AC_DEFUN([bh_C_SIGNED],
[
  AC_CACHE_CHECK([for signed], bh_cv_c_signed,
   [AC_TRY_COMPILE(, [signed char x;], bh_cv_c_signed=yes, bh_cv_c_signed=no)])
  if test $bh_cv_c_signed = no; then
    AC_DEFINE(signed, ,
              [Define to empty if the C compiler doesn't support this keyword.])
  fi
])


# longlong.m4 serial 4
dnl Copyright (C) 1999-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Paul Eggert.

# Define HAVE_LONG_LONG if 'long long' works.

AC_DEFUN([jm_AC_TYPE_LONG_LONG],
[
  AC_CACHE_CHECK([for long long], ac_cv_type_long_long,
  [AC_TRY_LINK([long long ll = 1LL; int i = 63;],
    [long long llmax = (long long) -1;
     return ll << i | ll >> i | llmax / ll | llmax % ll;],
    ac_cv_type_long_long=yes,
    ac_cv_type_long_long=no)])
  if test $ac_cv_type_long_long = yes; then
    AC_DEFINE(HAVE_LONG_LONG, 1,
      [Define if you have the 'long long' type.])
  fi
])


# longdouble.m4 serial 1 (gettext-0.11.6)
dnl Copyright (C) 2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Bruno Haible.
dnl Test whether the compiler supports the 'long double' type.
dnl Prerequisite: AC_PROG_CC

AC_DEFUN([gt_TYPE_LONGDOUBLE],
[
  AC_CACHE_CHECK([for long double], gt_cv_c_long_double,
    [if test "$GCC" = yes; then
       gt_cv_c_long_double=yes
     else
       AC_TRY_COMPILE([
         /* The Stardent Vistra knows sizeof(long double), but does not support it.  */
         long double foo = 0.0;
         /* On Ultrix 4.3 cc, long double is 4 and double is 8.  */
         int array [2*(sizeof(long double) >= sizeof(double)) - 1];
         ], ,
         gt_cv_c_long_double=yes, gt_cv_c_long_double=no)
     fi])
  if test $gt_cv_c_long_double = yes; then
    AC_DEFINE(HAVE_LONG_DOUBLE, 1, [Define if you have the 'long double' type.])
  fi
])



# wchar_t.m4 serial 1 (gettext-0.11.6)
dnl Copyright (C) 2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Bruno Haible.
dnl Test whether <stddef.h> has the 'wchar_t' type.
dnl Prerequisite: AC_PROG_CC

AC_DEFUN([gt_TYPE_WCHAR_T],
[
  AC_CACHE_CHECK([for wchar_t], gt_cv_c_wchar_t,
    [AC_TRY_COMPILE([#include <stddef.h>
       wchar_t foo = (wchar_t)'\0';], ,
       gt_cv_c_wchar_t=yes, gt_cv_c_wchar_t=no)])
  if test $gt_cv_c_wchar_t = yes; then
    AC_DEFINE(HAVE_WCHAR_T, 1, [Define if you have the 'wchar_t' type.])
  fi
])


# wint_t.m4 serial 1
dnl Copyright (C) 2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Bruno Haible.
dnl Test whether <wchar.h> has the 'wint_t' type.
dnl Prerequisite: AC_PROG_CC

AC_DEFUN([gt_TYPE_WINT_T],
[
  AC_CACHE_CHECK([for wint_t], gt_cv_c_wint_t,
    [AC_TRY_COMPILE([#include <wchar.h>
       wint_t foo = (wchar_t)'\0';], ,
       gt_cv_c_wint_t=yes, gt_cv_c_wint_t=no)])
  if test $gt_cv_c_wint_t = yes; then
    AC_DEFINE(HAVE_WINT_T, 1, [Define if you have the 'wint_t' type.])
  fi
])


# intmax_t.m4 serial 1
dnl Copyright (C) 1997-2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Paul Eggert.

AC_PREREQ(2.13)

# Define intmax_t to 'long' or 'long long'
# if it is not already defined in <stdint.h> or <inttypes.h>.

AC_DEFUN([jm_AC_TYPE_INTMAX_T],
[
  dnl For simplicity, we assume that a header file defines 'intmax_t' if and
  dnl only if it defines 'uintmax_t'.
  AC_REQUIRE([jm_AC_HEADER_INTTYPES_H])
  AC_REQUIRE([jm_AC_HEADER_STDINT_H])
  if test $jm_ac_cv_header_inttypes_h = no && test $jm_ac_cv_header_stdint_h = no; then
    AC_REQUIRE([jm_AC_TYPE_LONG_LONG])
    test $ac_cv_type_long_long = yes \
      && ac_type='long long' \
      || ac_type='long'
    AC_DEFINE_UNQUOTED(intmax_t, $ac_type,
     [Define to long or long long if <inttypes.h> and <stdint.h> don't define.])
  else
    AC_DEFINE(HAVE_INTMAX_T, 1,
      [Define if you have the 'intmax_t' type in <stdint.h> or <inttypes.h>.])
  fi
])

dnl An alternative would be to explicitly test for 'intmax_t'.

AC_DEFUN([gt_AC_TYPE_INTMAX_T],
[
  AC_REQUIRE([jm_AC_HEADER_INTTYPES_H])
  AC_REQUIRE([jm_AC_HEADER_STDINT_H])
  AC_CACHE_CHECK(for intmax_t, gt_cv_c_intmax_t,
    [AC_TRY_COMPILE([
#include <stddef.h> 
#include <stdlib.h>
#if HAVE_STDINT_H_WITH_UINTMAX
#include <stdint.h>
#endif
#if HAVE_INTTYPES_H_WITH_UINTMAX
#include <inttypes.h>
#endif
], [intmax_t x = -1;], gt_cv_c_intmax_t=yes, gt_cv_c_intmax_t=no)])
  if test $gt_cv_c_intmax_t = yes; then
    AC_DEFINE(HAVE_INTMAX_T, 1,
      [Define if you have the 'intmax_t' type in <stdint.h> or <inttypes.h>.])
  else
    AC_REQUIRE([jm_AC_TYPE_LONG_LONG])
    test $ac_cv_type_long_long = yes \
      && ac_type='long long' \
      || ac_type='long'
    AC_DEFINE_UNQUOTED(intmax_t, $ac_type,
     [Define to long or long long if <stdint.h> and <inttypes.h> don't define.])
  fi
])


# stdint_h.m4 serial 3 (gettext-0.11.6)
dnl Copyright (C) 1997-2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Paul Eggert.

# Define HAVE_STDINT_H_WITH_UINTMAX if <stdint.h> exists,
# doesn't clash with <sys/types.h>, and declares uintmax_t.

AC_DEFUN([jm_AC_HEADER_STDINT_H],
[
  AC_CACHE_CHECK([for stdint.h], jm_ac_cv_header_stdint_h,
  [AC_TRY_COMPILE(
    [#include <sys/types.h>
#include <stdint.h>],
    [uintmax_t i = (uintmax_t) -1;],
    jm_ac_cv_header_stdint_h=yes,
    jm_ac_cv_header_stdint_h=no)])
  if test $jm_ac_cv_header_stdint_h = yes; then
    AC_DEFINE_UNQUOTED(HAVE_STDINT_H_WITH_UINTMAX, 1,
      [Define if <stdint.h> exists, doesn't clash with <sys/types.h>,
       and declares uintmax_t. ])
  fi
])


# inttypes_h.m4 serial 5 (gettext-0.11.6)
dnl Copyright (C) 1997-2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Paul Eggert.

# Define HAVE_INTTYPES_H_WITH_UINTMAX if <inttypes.h> exists,
# doesn't clash with <sys/types.h>, and declares uintmax_t.

AC_DEFUN([jm_AC_HEADER_INTTYPES_H],
[
  AC_CACHE_CHECK([for inttypes.h], jm_ac_cv_header_inttypes_h,
  [AC_TRY_COMPILE(
    [#include <sys/types.h>
#include <inttypes.h>],
    [uintmax_t i = (uintmax_t) -1;],
    jm_ac_cv_header_inttypes_h=yes,
    jm_ac_cv_header_inttypes_h=no)])
  if test $jm_ac_cv_header_inttypes_h = yes; then
    AC_DEFINE_UNQUOTED(HAVE_INTTYPES_H_WITH_UINTMAX, 1,
      [Define if <inttypes.h> exists, doesn't clash with <sys/types.h>,
       and declares uintmax_t. ])
  fi
])


m4_include(acglib.m4)dnl
m4_include(glib/libcharset/codeset.m4)dnl
m4_include(glib/libcharset/glibc21.m4)dnl
m4_include(m4macros/glib-gettext.m4)dnl
