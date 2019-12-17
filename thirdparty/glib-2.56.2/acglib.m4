## Portability defines that help interoperate with classic and modern autoconfs
ifdef([AC_TR_SH],[
define([GLIB_TR_SH],[AC_TR_SH([$1])])
define([GLIB_TR_CPP],[AC_TR_CPP([$1])])
], [
define([GLIB_TR_SH],
       [patsubst(translit([[$1]], [*+], [pp]), [[^a-zA-Z0-9_]], [_])])
define([GLIB_TR_CPP],
       [patsubst(translit([[$1]],
  	                  [*abcdefghijklmnopqrstuvwxyz],
 			  [PABCDEFGHIJKLMNOPQRSTUVWXYZ]),
		 [[^A-Z0-9_]], [_])])
])

# GLIB_AC_DIVERT_BEFORE_HELP(STUFF)
# ---------------------------------
# Put STUFF early enough so that they are available for $ac_help expansion.
# Handle both classic (<= v2.13) and modern autoconf
AC_DEFUN([GLIB_AC_DIVERT_BEFORE_HELP],
[ifdef([m4_divert_text], [m4_divert_text([NOTICE],[$1])],
       [ifdef([AC_DIVERT], [AC_DIVERT([NOTICE],[$1])],
              [AC_DIVERT_PUSH(AC_DIVERSION_NOTICE)dnl
$1
AC_DIVERT_POP()])])])

dnl GLIB_IF_VAR_EQ (ENV_VAR, VALUE [, EQUALS_ACTION] [, ELSE_ACTION])
AC_DEFUN([GLIB_IF_VAR_EQ],[
        case "$[$1]" in
        "[$2]"[)]
                [$3]
                ;;
        *[)]
                [$4]
                ;;
        esac
])
dnl GLIB_STR_CONTAINS (SRC_STRING, SUB_STRING [, CONTAINS_ACTION] [, ELSE_ACTION])
AC_DEFUN([GLIB_STR_CONTAINS],[
        case "[$1]" in
        *"[$2]"*[)]
                [$3]
                ;;
        *[)]
                [$4]
                ;;
        esac
])
dnl GLIB_ADD_TO_VAR (ENV_VARIABLE, CHECK_STRING, ADD_STRING)
AC_DEFUN([GLIB_ADD_TO_VAR],[
        GLIB_STR_CONTAINS($[$1], [$2], [$1]="$[$1]", [$1]="$[$1] [$3]")
])

# GLIB_SIZEOF (INCLUDES, TYPE, ALIAS)
# ---------------------------------------------------------------
# The definition here is based of that of AC_CHECK_SIZEOF
AC_DEFUN([GLIB_SIZEOF],
[AS_LITERAL_IF([$3], [],
               [AC_FATAL([$0: requires literal arguments])])dnl
AC_CACHE_CHECK([size of $2], AS_TR_SH([glib_cv_sizeof_$3]),
[ # The cast to unsigned long works around a bug in the HP C Compiler
  # version HP92453-01 B.11.11.23709.GP, which incorrectly rejects
  # declarations like `int a3[[(sizeof (unsigned char)) >= 0]];'.
  # This bug is HP SR number 8606223364.
  _AC_COMPUTE_INT([(long) (sizeof ($2))],
                  [AS_TR_SH([glib_cv_sizeof_$3])],
                  [AC_INCLUDES_DEFAULT([$1])],
                  [AC_MSG_ERROR([cannot compute sizeof ($2), 77])])
])dnl
AC_DEFINE_UNQUOTED(GLIB_TR_CPP(glib_sizeof_$3), $AS_TR_SH([glib_cv_sizeof_$3]),
                   [The size of $3, as computed by sizeof.])
])# GLIB_SIZEOF

dnl GLIB_BYTE_CONTENTS (INCLUDES, TYPE, ALIAS, N_BYTES, INITIALIZER)
AC_DEFUN([GLIB_BYTE_CONTENTS],
[pushdef([glib_ByteContents], GLIB_TR_SH([glib_cv_byte_contents_$3]))dnl
AC_CACHE_CHECK([byte contents of $5], glib_ByteContents,
[AC_TRY_RUN([#include <stdio.h>
$1
main()
{
  static $2 tv = $5;
  char *p = (char*) &tv;
  int i;
  FILE *f=fopen("conftestval", "w");
  if (!f) exit(1);
  for (i = 0; i < $4; i++)
    fprintf(f, "%s%d", i?",":"", *(p++));
  fprintf(f, "\n");
  exit(0);
}], 
   [glib_ByteContents=`cat conftestval`  dnl''
], 
   [glib_ByteContents=no],
   [glib_ByteContents=no])])
AC_DEFINE_UNQUOTED(GLIB_TR_CPP(glib_byte_contents_$3), [$[]glib_ByteContents],
	[Byte contents of $3])
popdef([glib_ByteContents])dnl
])

# GLIB_CHECK_VALUE(SYMBOL, INCLUDES, ACTION-IF-FAIL)
# ---------------------------------------------------------------
AC_DEFUN([GLIB_CHECK_VALUE],
[AC_CACHE_CHECK([value of $1], AS_TR_SH([glib_cv_value_$1]),
  [_AC_COMPUTE_INT([$1], AS_TR_SH([glib_cv_value_$1]), [$2], [$3])])
])dnl

# GLIB_CHECK_COMPILE_WARNINGS(PROGRAM, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# ---------------------------------------------------------------------
# Try to compile PROGRAM, check for warnings
m4_define([GLIB_CHECK_COMPILE_WARNINGS],
[m4_ifvaln([$1], [AC_LANG_CONFTEST([$1])])dnl
rm -f conftest.$ac_objext
glib_ac_compile_save="$ac_compile"
ac_compile='$CC -c $CFLAGS $CPPFLAGS conftest.$ac_ext'
AS_IF([_AC_EVAL_STDERR($ac_compile) &&
         AC_TRY_COMMAND([(if test -s conftest.err; then false ; else true; fi)])],
      [$2],
      [echo "$as_me: failed program was:" >&AS_MESSAGE_LOG_FD
cat conftest.$ac_ext >&AS_MESSAGE_LOG_FD
m4_ifvaln([$3],[$3])dnl])
ac_compile="$glib_ac_compile_save"
rm -f conftest.$ac_objext conftest.err m4_ifval([$1], [conftest.$ac_ext])[]dnl
])# GLIB_CHECK_COMPILE_WARNINGS

# GLIB_ASSERT_SET(VARIABLE)
# -------------------------
AC_DEFUN([GLIB_ASSERT_SET],
[if test "x${$1+set}" != "xset" ; then
  AC_MSG_ERROR($1 [must be set in cache file when cross-compiling.])
fi
])dnl
