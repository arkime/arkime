# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.62])

dnl ***********************************
dnl *** include special GLib macros ***
dnl ***********************************

m4_define(glib_configure_ac)

#
# The following version number definitions apply to GLib, GModule, GObject,
# GThread and GIO as a whole, so if changes occurred in any of them, they are
# all treated with the same interface and binary age.
#
# Making releases:
#   glib_micro_version += 1;
#   glib_interface_age += 1;
#   glib_binary_age += 1;
# if any functions have been added, set glib_interface_age to 0.
# if backwards compatibility has been broken,
# set glib_binary_age _and_ glib_interface_age to 0.
#
# remember to add a GLIB_VERSION_2_xx macro every time the minor version is
# bumped, as well as the GLIB_DEPRECATED_IN and GLIB_AVAILABLE_IN macros
# for that version - see gversion.h for further information.
#
# in easier to understand terms:
#
# <mclasen> on the stable branch, interface age == micro
# <mclasen> on the unstable (ie master), interface age = 0

m4_define([glib_major_version], [2])
m4_define([glib_minor_version], [56])
m4_define([glib_micro_version], [2])
m4_define([glib_interface_age], [2])
m4_define([glib_binary_age],
          [m4_eval(100 * glib_minor_version + glib_micro_version)])
m4_define([glib_version],
          [glib_major_version.glib_minor_version.glib_micro_version])

# libtool version related macros
m4_define([glib_lt_release], [glib_major_version.glib_minor_version])
m4_define([glib_lt_current],
          [m4_eval(100 * glib_minor_version + glib_micro_version - glib_interface_age)])
m4_define([glib_lt_revision], [glib_interface_age])
m4_define([glib_lt_age], [m4_eval(glib_binary_age - glib_interface_age)])
m4_define([glib_lt_current_minus_age],
          [m4_eval(glib_lt_current - glib_lt_age)])

# if the minor version number is odd, then we want debugging.  Otherwise
# we only want minimal debugging support.
m4_define([glib_debug_default],
          [m4_if(m4_eval(glib_minor_version % 2), [1], [yes], [minimum])])dnl


AC_INIT(glib, [glib_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=glib])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([glib/glib.h])
AC_CONFIG_MACRO_DIR([m4macros])

# Save this value here, since automake will set cflags later
cflags_set=${CFLAGS:+set}

AM_INIT_AUTOMAKE([1.13.3 -Wno-portability no-define no-dist-gzip dist-xz tar-ustar subdir-objects])
AM_MAINTAINER_MODE([enable])

# Support silent build rules. Disable
# by either passing --disable-silent-rules to configure or passing V=1
# to make
AM_SILENT_RULES([yes])

GLIB_MAJOR_VERSION=glib_major_version
GLIB_MINOR_VERSION=glib_minor_version
GLIB_MICRO_VERSION=glib_micro_version
GLIB_INTERFACE_AGE=glib_interface_age
GLIB_BINARY_AGE=glib_binary_age
GLIB_VERSION=glib_version

AC_SUBST(GLIB_MAJOR_VERSION)
AC_SUBST(GLIB_MINOR_VERSION)
AC_SUBST(GLIB_MICRO_VERSION)
AC_SUBST(GLIB_VERSION)
AC_SUBST(GLIB_INTERFACE_AGE)
AC_SUBST(GLIB_BINARY_AGE)

AC_DEFINE(GLIB_MAJOR_VERSION, [glib_major_version],
	  [Define to the GLIB major version])
AC_DEFINE(GLIB_MINOR_VERSION, [glib_minor_version],
	  [Define to the GLIB minor version])
AC_DEFINE(GLIB_MICRO_VERSION, [glib_micro_version],
	  [Define to the GLIB micro version])
AC_DEFINE(GLIB_INTERFACE_AGE, [glib_interface_age],
	  [Define to the GLIB interface age])
AC_DEFINE(GLIB_BINARY_AGE, [glib_binary_age],
	  [Define to the GLIB binary age])

# libtool versioning
LT_RELEASE=glib_lt_release
LT_CURRENT=glib_lt_current
LT_REVISION=glib_lt_revision
LT_AGE=glib_lt_age
LT_CURRENT_MINUS_AGE=glib_lt_current_minus_age
AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)
AC_SUBST(LT_CURRENT_MINUS_AGE)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_USE_SYSTEM_EXTENSIONS

AM_CONDITIONAL(HAVE_GCC, [test "$GCC" = "yes"])

AC_CANONICAL_HOST

dnl

AC_MSG_CHECKING([for Win32])
LIB_EXE_MACHINE_FLAG=X86
case "$host" in
  *-*-mingw*)
    glib_native_win32=yes
    glib_pid_type='void *'
    glib_pid_format='p'
    glib_pollfd_format='%#x'
    glib_dir_separator='\\\\'
    glib_searchpath_separator=';'
    glib_cv_stack_grows=no
    # Unfortunately the mingw implementations of C99-style snprintf and vsnprintf
    # don't seem to be quite good enough, at least not in mingw-runtime-3.14.
    # (Sorry, I don't know exactly what is the problem, but it is related to
    # floating point formatting and decimal point vs. comma.)
    # The simple tests in AC_FUNC_VSNPRINTF_C99 and AC_FUNC_SNPRINTF_C99 aren't
    # rigorous enough to notice, though.
    # So preset the autoconf cache variables.
    ac_cv_func_vsnprintf_c99=no
    ac_cv_func_snprintf_c99=no
    case "$host" in
    x86_64-*-*)
      LIB_EXE_MACHINE_FLAG=X64
      glib_pollfd_format='%#I64x'
      ;;
    esac

    AC_DEFINE([_WIN32_WINNT], [0x0601], [Target the Windows 7 API])
    ;;
  *)
    glib_native_win32=no
    glib_pid_type=int
    glib_pid_format='i'
    glib_pollfd_format='%d'
    glib_dir_separator='/'
    glib_searchpath_separator=':'
    ;;
esac
case $host in
  *-*-linux*)
    glib_os_linux=yes
    ;;
esac

AC_MSG_RESULT([$glib_native_win32])

AC_MSG_CHECKING([for the Android])
case $host in
  *android*)
    glib_native_android="yes"
    ;;
  *)
    glib_native_android="no"
    ;;
esac
AC_MSG_RESULT([$glib_native_android])

AC_SUBST(LIB_EXE_MACHINE_FLAG)

glib_have_carbon=no
AC_MSG_CHECKING([for Mac OS X Carbon support])
AC_TRY_CPP([
#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>
], glib_have_carbon=yes)

AC_MSG_RESULT([$glib_have_carbon])

glib_have_cocoa=no
AC_MSG_CHECKING([for Mac OS X Cocoa support])
AC_TRY_CPP([
#include <Cocoa/Cocoa.h>
#ifdef GNUSTEP_BASE_VERSION
#error "Detected GNUstep, not Cocoa"
#endif
], glib_have_cocoa=yes)

AC_MSG_RESULT([$glib_have_cocoa])

AM_CONDITIONAL(OS_WIN32, [test "$glib_native_win32" = "yes"])
AM_CONDITIONAL(OS_WIN32_X64, [test "$LIB_EXE_MACHINE_FLAG" = "X64"])
AM_CONDITIONAL(OS_UNIX, [test "$glib_native_win32" != "yes"])
AM_CONDITIONAL(OS_LINUX, [test "$glib_os_linux" = "yes"])
AM_CONDITIONAL(OS_CARBON, [test "$glib_have_carbon" = "yes"])
AM_CONDITIONAL(OS_COCOA, [test "$glib_have_cocoa" = "yes"])

AS_IF([test "$glib_native_win32" = "yes"], [
  AC_CHECK_TOOL(WINDRES, windres, no)
  if test "$WINDRES" = no; then
    AC_MSG_ERROR([*** Could not find an implementation of windres in your PATH.])
  fi
  AC_CHECK_TOOL(NM, nm, no)
  if test "$NM" = no; then
    AC_MSG_ERROR([*** Could not find an implementation of nm in your PATH.])
  fi
  AC_CHECK_TOOL(RANLIB, ranlib, :)
  AC_CHECK_TOOL(DLLTOOL, dlltool, :)
  AC_CHECK_PROG(ms_librarian, [lib.exe], [yes], [no])
])
AM_CONDITIONAL(MS_LIB_AVAILABLE, [test x$ms_librarian = xyes])

AS_IF([test "x$glib_have_carbon" = "xyes"], [
  AC_DEFINE(HAVE_CARBON, 1, [define to 1 if Carbon is available])
  CARBON_LIBS="-Wl,-framework,Carbon"
  LDFLAGS="$LDFLAGS $CARBON_LIBS"
], [CARBON_LIBS=""])

AC_SUBST([CARBON_LIBS])
ac_cv_have_os_x_9_or_later="no"
AS_IF([test "x$glib_have_cocoa" = "xyes"], [
  AC_DEFINE(HAVE_COCOA, 1, [define to 1 if Cocoa is available])
  COCOA_LIBS="-Wl,-framework,Foundation -Wl,-framework,AppKit"
  LDFLAGS="$LDFLAGS $COCOA_LIBS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <AvailabilityMacros.h>
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1090
#error Compiling for minimum OS X version before 10.9
#endif
  ]])],[ac_cv_have_os_x_9_or_later="yes"])
], [COCOA_LIBS=""])
AM_CONDITIONAL([MAC_OS_X_9], [test "x$ac_cv_have_os_x_9_or_later" = xyes])
AC_SUBST([COCOA_LIBS])

dnl declare --enable-* args and collect ac_help strings
AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                             [turn on debugging @<:@default=glib_debug_default@:>@]),,
              enable_debug=glib_debug_default)

AC_ARG_ENABLE(gc_friendly,
              [AS_HELP_STRING([--enable-gc-friendly],
                              [turn on garbage collector friendliness [default=no]])],,
              [enable_gc_friendly=no])
AC_ARG_ENABLE(mem_pools,
              [AS_HELP_STRING([--disable-mem-pools],
			      [disable all glib memory pools])],,
	      [disable_mem_pools=no])

GLIB_TESTS

AC_MSG_CHECKING([whether to enable garbage collector friendliness])
AS_IF([test "x$enable_gc_friendly" = "xyes"], [
  AC_DEFINE(ENABLE_GC_FRIENDLY_DEFAULT, 1, [Whether to enable GC friendliness by default])
  AC_MSG_RESULT([yes])
], [ AC_MSG_RESULT([no]) ])

AC_MSG_CHECKING([whether to disable memory pools])
AS_IF([test "x$disable_mem_pools" = "xno"], [
  AC_MSG_RESULT([no])
], [
  AC_DEFINE(DISABLE_MEM_POOLS, [1], [Whether to disable memory pools])
  AC_MSG_RESULT([yes])
])

dnl location to install runtime libraries, e.g. ../../lib to install
dnl to /lib if libdir is /usr/lib
AC_ARG_WITH(runtime-libdir,
           [AS_HELP_STRING([--with-runtime-libdir=RELPATH],
                           [install runtime libraries relative to libdir])],
           [],
	   [with_runtime_libdir=""])
GLIB_RUNTIME_LIBDIR="$with_runtime_libdir"
AC_SUBST(GLIB_RUNTIME_LIBDIR)
AM_CONDITIONAL(HAVE_GLIB_RUNTIME_LIBDIR, [test "x$with_runtime_libdir" != "x"])

dnl Check for a working C++ compiler, but do not bail out, if none is found.
AC_CHECK_TOOLS(CXX, [$CCC c++ g++ gcc CC cxx cc++ cl], [gcc])
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_TRY_COMPILE(,[class a { int b; } c;], ,CXX=)
AM_CONDITIONAL(HAVE_CXX, [test "$CXX" != ""])
AC_LANG_RESTORE

AM_PROG_CC_C_O
AC_PROG_INSTALL

AC_SYS_LARGEFILE

PKG_PROG_PKG_CONFIG(0.16)

if test "x$enable_debug" = "xyes"; then
  if test "x$cflags_set" != "x" ; then
      case " $CFLAGS " in
      *[[\ \	]]-g[[\ \	]]*) ;;
      *) CFLAGS="$CFLAGS -g" ;;
      esac
  fi
  GLIB_DEBUG_FLAGS="-DG_ENABLE_DEBUG"
else
  GLIB_DEBUG_FLAGS="-DG_DISABLE_CAST_CHECKS"

  if test "x$enable_debug" = "xno"; then
    GLIB_DEBUG_FLAGS="$GLIB_DEBUG_FLAGS -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS"
  fi
fi

# Ensure MSVC-compatible struct packing convention is used when
# compiling for Win32 with gcc.
# What flag to depends on gcc version: gcc3 uses "-mms-bitfields", while
# gcc2 uses "-fnative-struct".
if test x"$glib_native_win32" = xyes; then
  if test x"$GCC" = xyes; then
    msnative_struct=''
    AC_MSG_CHECKING([how to get MSVC-compatible struct packing])
    if test -z "$ac_cv_prog_CC"; then
      our_gcc="$CC"
    else
      our_gcc="$ac_cv_prog_CC"
    fi
    case `$our_gcc --version | sed -e 's,\..*,.,' -e q` in
      2.)
	if $our_gcc -v --help 2>/dev/null | grep fnative-struct >/dev/null; then
	  msnative_struct='-fnative-struct'
	fi
	;;
      *)
	if $our_gcc -v --help 2>/dev/null | grep ms-bitfields >/dev/null; then
	  msnative_struct='-mms-bitfields'
	fi
	;;
    esac
    if test x"$msnative_struct" = x ; then
      AC_MSG_RESULT([no way])
      AC_MSG_WARN([produced libraries might be incompatible with MSVC-compiled code])
    else
      CFLAGS="$CFLAGS $msnative_struct"
      AC_MSG_RESULT([${msnative_struct}])
    fi
  fi
fi
GLIB_EXTRA_CFLAGS="${msnative_struct}"
AC_SUBST(GLIB_EXTRA_CFLAGS)

AC_EXEEXT

AC_PROG_AWK
AC_CHECK_PROGS(PERL, [perl5 perl])

# option to specify python interpreter to use; this just sets $PYTHON, so that
# we will fallback to reading $PYTHON if --with-python is not given, and
# python.m4 will get the expected input
AC_ARG_WITH(python,
            AS_HELP_STRING([--with-python=PATH],
                           [Path to Python interpreter; searches $PATH if only a program name is given; if not given, searches for a few standard names such as "python3" or "python2"]),
            [PYTHON="$withval"], [])
if test x"$PYTHON" = xyes; then
  AC_MSG_ERROR([--with-python option requires a path or program argument])
fi
AM_PATH_PYTHON(2.7,,PYTHON="python2.7")


dnl ***********************
dnl *** Tests for iconv ***
dnl ***********************
dnl
dnl We do this before the gettext checks, to avoid distortion

dnl On Windows we use a native implementation

AS_IF([ test x"$glib_native_win32" = xyes], [
  with_libiconv=native
], [
  AC_ARG_WITH(libiconv,
	      [AS_HELP_STRING([--with-libiconv=@<:@no/gnu/native@:>@],
			      [use the libiconv library])],,
	      [with_libiconv=maybe])

  found_iconv=no
  case $with_libiconv in
    maybe)
      # Check in the C library first
      AC_CHECK_FUNC(iconv_open, [with_libiconv=no; found_iconv=yes])
      # Check if we have GNU libiconv
      if test $found_iconv = "no"; then
	AC_CHECK_LIB(iconv, libiconv_open, [with_libiconv=gnu; found_iconv=yes])
      fi
      # Check if we have a iconv in -liconv, possibly from vendor
      if test $found_iconv = "no"; then
	AC_CHECK_LIB(iconv, iconv_open, [with_libiconv=native; found_iconv=yes])
      fi
      ;;
    no)
      AC_CHECK_FUNC(iconv_open, [with_libiconv=no; found_iconv=yes])
      ;;
    gnu|yes)
      AC_CHECK_LIB(iconv, libiconv_open, [with_libiconv=gnu; found_iconv=yes])
      ;;
    native)
      AC_CHECK_LIB(iconv, iconv_open, [with_libiconv=native; found_iconv=yes])
      ;;
  esac

  if test "x$found_iconv" = "xno" ; then
     AC_MSG_ERROR([*** No iconv() implementation found in C library or libiconv])
  fi
])

dnl
dnl zlib support
dnl
PKG_CHECK_MODULES([ZLIB], [zlib], [found_zlib=yes], [found_zlib=no])
AS_IF([test "x$found_zlib" = "xno"], [
  AC_CHECK_LIB(z, inflate, [AC_CHECK_HEADER(zlib.h, found_zlib=yes)])
  if test "x$found_zlib" = "xno" ; then
    AC_MSG_ERROR([*** Working zlib library and headers not found ***])
  fi
  ZLIB_LIBS='-lz'
  AC_SUBST(ZLIB_LIBS)
])

PKG_CHECK_MODULES(LIBFFI, [libffi >= 3.0.0])
AC_SUBST(LIBFFI_CFLAGS)
AC_SUBST(LIBFFI_LIBS)

dnl
dnl gettext support
dnl

ALL_LINGUAS="`grep -v '^#' "$srcdir/po/LINGUAS" | tr '\n' ' '`"
AC_SUBST([CONFIG_STATUS_DEPENDENCIES],['$(top_srcdir)/po/LINGUAS'])
GLIB_GNU_GETTEXT

if test "$gt_cv_have_gettext" != "yes" ; then
  AC_MSG_ERROR([
*** You must have either have gettext support in your C library, or use the
*** GNU gettext library. (http://www.gnu.org/software/gettext/gettext.html)
])
fi

LIBS="$INTLLIBS $LIBS"

GETTEXT_PACKAGE=glib20
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], 
  [Define the gettext package to be used])

GLIB_DEFINE_LOCALEDIR(GLIB_LOCALE_DIR)

dnl
dnl Now we are done with gettext checks, figure out ICONV_LIBS
dnl

AS_IF([test x"$glib_native_win32" != xyes], [
  if test x$with_libiconv != xno ; then
    case " $INTLLIBS " in
    *[[\ \	]]-liconv[[\ \	]]*) ;;
    *) ICONV_LIBS="-liconv" ;;
    esac
  fi
])
AC_SUBST(ICONV_LIBS)

case $with_libiconv in
  gnu)
    AC_DEFINE(USE_LIBICONV_GNU, 1, [Using GNU libiconv])
    ;;
  native)
    AC_DEFINE(USE_LIBICONV_NATIVE, 1, [Using a native implementation of iconv in a separate library])
    ;;
esac

dnl Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static win32-dll])
dnl when using libtool 2.x create libtool early, because it's used in configure
m4_ifdef([LT_OUTPUT], [LT_OUTPUT])


AS_IF([test "$glib_native_win32" = "yes"], [
  if test x$enable_static = xyes -a x$enable_shared = xyes; then
    AC_MSG_ERROR([Can not build both shared and static at the same time on Windows.])
  fi
  if test x$enable_static = xyes; then
    glib_win32_static_compilation=yes
    GLIB_WIN32_STATIC_COMPILATION_DEFINE="#define GLIB_STATIC_COMPILATION 1
#define GOBJECT_STATIC_COMPILATION 1"
    AC_SUBST(GLIB_WIN32_STATIC_COMPILATION_DEFINE)
  fi
])
AM_CONDITIONAL(OS_WIN32_AND_DLL_COMPILATION, [test x$glib_native_win32 = xyes -a x$glib_win32_static_compilation != xyes])

# Checks for library functions.
AC_FUNC_ALLOCA
AC_CHECK_FUNCS(mmap posix_memalign memalign valloc fsync pipe2 issetugid)
AC_CHECK_FUNCS(timegm gmtime_r)
AC_FUNC_STRERROR_R()

AC_CACHE_CHECK([for __libc_enable_secure], glib_cv_have_libc_enable_secure,
  [AC_TRY_LINK([#include <unistd.h>
    extern int __libc_enable_secure;],
    [return __libc_enable_secure;],
   glib_cv_have_libc_enable_secure=yes,
   glib_cv_have_libc_enable_secure=no)])
AS_IF([test x$glib_cv_have_libc_enable_secure = xyes], [
   AC_DEFINE(HAVE_LIBC_ENABLE_SECURE, 1,
     [Define if you have the __libc_enable_secure variable (GNU libc, eglibc)])
])

AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(void *)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(__int64)

AC_CACHE_CHECK([for sig_atomic_t], ac_cv_type_sig_atomic_t,
  [AC_TRY_LINK([#include <signal.h>
     #include <sys/types.h>
     sig_atomic_t val = 42;],
    [return val == 42 ? 0 : 1],
   ac_cv_type_sig_atomic_t=yes,
   ac_cv_type_sig_atomic_t=no)])
if test x$ac_cv_type_sig_atomic_t = xyes; then
   AC_DEFINE(HAVE_SIG_ATOMIC_T, 1,
     [Define if you have the 'sig_atomic_t' type.])
fi

if test x$ac_cv_sizeof_long = x8 || test x$ac_cv_sizeof_long_long = x8 || test x$ac_cv_sizeof___int64 = x8 ; then
  :
else
  AC_MSG_ERROR([
*** GLib requires a 64 bit type. You might want to consider
*** using the GNU C compiler.
])
fi

AS_IF([test x$glib_native_win32 != xyes && test x$ac_cv_sizeof_long_long = x8], [
	# long long is a 64 bit integer.
	AC_MSG_CHECKING(for format to printf and scanf a guint64)
	AC_CACHE_VAL(glib_cv_long_long_format,[
		for format in ll q I64; do
		  AC_TRY_RUN([#include <stdio.h>  
			int main()
			{
			  long long b, a = -0x3AFAFAFAFAFAFAFALL;
			  char buffer[1000];
			  sprintf (buffer, "%${format}u", a);
  			  sscanf (buffer, "%${format}u", &b);
			  exit (b!=a);
			}
			],
			[glib_cv_long_long_format=${format}
			break],
			[],[:])
		done])
	AS_IF([ test -n "$glib_cv_long_long_format"], [
	  AC_MSG_RESULT(%${glib_cv_long_long_format}u)
	  AC_DEFINE(HAVE_LONG_LONG_FORMAT,1,[define if system printf can print long long])
	  if test x"$glib_cv_long_long_format" = xI64; then
	    AC_DEFINE(HAVE_INT64_AND_I64,1,[define to support printing 64-bit integers with format I64])
          fi
	], [AC_MSG_RESULT(none)])
],[ test x$ac_cv_sizeof___int64 = x8], [
	# __int64 is a 64 bit integer.
	AC_MSG_CHECKING(for format to printf and scanf a guint64)
	# We know this is MSVCRT.DLL, and what the formats are
	glib_cv_long_long_format=I64
	AC_MSG_RESULT(%${glib_cv_long_long_format}u)
        AC_DEFINE(HAVE_LONG_LONG_FORMAT,1,[define if system printf can print long long])
	AC_DEFINE(HAVE_INT64_AND_I64,1,[define to support printing 64-bit integers with format I64])
])

AC_C_CONST

dnl
dnl check in which direction the stack grows
dnl
AC_CACHE_CHECK([for growing stack pointer],glib_cv_stack_grows,[
        AC_TRY_RUN([
	volatile int *a = 0, *b = 0;
	void f (int i) { volatile int x = 5; if (i == 0) b = &x; else f (i - 1); }
	int main () { volatile int y = 7; a = &y; f (100); return b > a ? 0 : 1; }
       	],
	glib_cv_stack_grows=yes
        ,
	glib_cv_stack_grows=no
        ,)
])

# check for flavours of varargs macros
AC_MSG_CHECKING(for ISO C99 varargs macros in C)
AC_TRY_COMPILE([],[
int a(int p1, int p2, int p3);
#define call_a(...) a(1,__VA_ARGS__)
call_a(2,3);
],g_have_iso_c_varargs=yes,g_have_iso_c_varargs=no)
AC_MSG_RESULT($g_have_iso_c_varargs)

AC_MSG_CHECKING(for ISO C99 varargs macros in C++)
AS_IF([test "$CXX" = ""], [
dnl No C++ compiler
  g_have_iso_cxx_varargs=no
else
  AC_LANG_CPLUSPLUS
  AC_TRY_COMPILE([],[
int a(int p1, int p2, int p3);
#define call_a(...) a(1,__VA_ARGS__)
call_a(2,3);
],g_have_iso_cxx_varargs=yes,g_have_iso_cxx_varargs=no)
  AC_LANG_C
])
AC_MSG_RESULT($g_have_iso_cxx_varargs)

AC_MSG_CHECKING(for GNUC varargs macros)
AC_TRY_COMPILE([],[
int a(int p1, int p2, int p3);
#define call_a(params...) a(1,params)
call_a(2,3);
],g_have_gnuc_varargs=yes,g_have_gnuc_varargs=no)
AC_MSG_RESULT($g_have_gnuc_varargs)

# check for GNUC visibility support
AC_MSG_CHECKING(for GNUC visibility attribute)
GLIB_CHECK_COMPILE_WARNINGS([AC_LANG_SOURCE([[
void
__attribute__ ((visibility ("hidden")))
     f_hidden (void)
{
}
void
__attribute__ ((visibility ("internal")))
     f_internal (void)
{
}
void
__attribute__ ((visibility ("protected")))
     f_protected (void)
{
}
void
__attribute__ ((visibility ("default")))
     f_default (void)
{
}
int main (void)
{
	f_hidden();
	f_internal();
	f_protected();
	f_default();
	return 0;
}
]])],g_have_gnuc_visibility=yes,g_have_gnuc_visibility=no)
AC_MSG_RESULT($g_have_gnuc_visibility)
AM_CONDITIONAL(HAVE_GNUC_VISIBILITY, [test x$g_have_gnuc_visibility = xyes])

AC_MSG_CHECKING([whether using Sun Studio C compiler])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if defined(__SUNPRO_C) || (__SUNPRO_C >= 0x550)
#else
# include "error: this is not Sun Studio."
#endif
]], [[]])], [ g_have_sunstudio_visibility=yes ], [ g_have_sunstudio_visibility=no ])
AC_MSG_RESULT($g_have_sunstudio_visibility)
AM_CONDITIONAL(HAVE_SUNSTUDIO_VISIBILITY, [test x$g_have_sunstudio_visibility = xyes])

# check for bytesex stuff
AC_C_BIGENDIAN
if test x$ac_cv_c_bigendian = xuniversal ; then
  AC_MSG_ERROR([Universal builds not supported: see https://bugzilla.gnome.org/show_bug.cgi?id=742548])
fi


# check for header files
AC_CHECK_HEADERS([sys/param.h sys/resource.h mach/mach_time.h])
AC_CHECK_HEADERS([sys/select.h stdint.h inttypes.h sched.h malloc.h])
AC_CHECK_HEADERS([sys/vfs.h sys/vmount.h sys/statfs.h sys/statvfs.h sys/filio.h])
AC_CHECK_HEADERS([mntent.h sys/mnttab.h sys/vfstab.h sys/mntctl.h fstab.h])
AC_CHECK_HEADERS([linux/magic.h])
AC_CHECK_HEADERS([termios.h])

# Some versions of MSC lack these
AC_CHECK_HEADERS([dirent.h sys/time.h])

# We don't care about this, but we need to keep including it in
# glibconfig.h for backward compatibility
AC_CHECK_HEADERS([values.h])

AC_CHECK_HEADERS([sys/mount.h sys/sysctl.h], [], [],
[#if HAVE_SYS_PARAM_H
 #include <sys/param.h>
 #endif
])
AC_CHECK_FUNCS(sysctlbyname)

AC_HEADER_MAJOR
AC_CHECK_HEADERS([xlocale.h])

# check for structure fields
AC_CHECK_MEMBERS([struct stat.st_mtimensec, struct stat.st_mtim.tv_nsec, struct stat.st_atimensec, struct stat.st_atim.tv_nsec, struct stat.st_ctimensec, struct stat.st_ctim.tv_nsec, struct stat.st_birthtime, struct stat.st_birthtimensec, struct stat.st_birthtim, struct stat.st_birthtim.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_blksize, struct stat.st_blocks, struct statfs.f_fstypename, struct statfs.f_bavail],,, [#include <sys/types.h>
#include <sys/stat.h>
#ifdef G_OS_UNIX
#include <unistd.h>
#endif
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif])
# struct statvfs.f_basetype is available on Solaris but not for Linux. 
AC_CHECK_MEMBERS([struct statvfs.f_basetype],,, [#include <sys/statvfs.h>])
AC_CHECK_MEMBERS([struct statvfs.f_fstypename],,, [#include <sys/statvfs.h>])
AC_CHECK_MEMBERS([struct tm.tm_gmtoff, struct tm.__tm_gmtoff],,,[#include <time.h>])

AC_STRUCT_DIRENT_D_TYPE

# Checks for libcharset
AM_LANGINFO_CODESET
gl_GLIBC21
AC_ARG_WITH(charsetalias-dir,
            AS_HELP_STRING([--with-charsetalias-dir=DIR], [directory for charset.alias file [LIBDIR]]),
           [],
           [with_charsetalias_dir='${libdir}'])
GLIB_CHARSETALIAS_DIR=$with_charsetalias_dir
AC_SUBST(GLIB_CHARSETALIAS_DIR)

# check additional type sizes
AC_CHECK_SIZEOF(size_t)

dnl Try to figure out whether gsize should be long or int
AC_MSG_CHECKING([for the appropriate definition for size_t])

case $ac_cv_sizeof_size_t in
  $ac_cv_sizeof_short) 
      glib_size_type=short
      ;;
  $ac_cv_sizeof_int) 
      glib_size_type=int
      ;;
  $ac_cv_sizeof_long) 
      glib_size_type=long
      ;;
  $ac_cv_sizeof_long_long)
      glib_size_type='long long'
      ;;
  $ac_cv_sizeof__int64)
      glib_size_type='__int64'
      ;;
  *)  AC_MSG_ERROR([No type matching size_t in size])
      ;;
esac

dnl If int/long are the same size, we see which one produces
dnl warnings when used in the location as size_t. (This matters
dnl on AIX with xlc)
dnl
AS_IF([test $ac_cv_sizeof_size_t = $ac_cv_sizeof_int &&
       test $ac_cv_sizeof_size_t = $ac_cv_sizeof_long], [
  GLIB_CHECK_COMPILE_WARNINGS([AC_LANG_SOURCE([[
#if defined(_AIX) && !defined(__GNUC__)
#pragma options langlvl=stdc89
#endif
#include <stddef.h> 
int main ()
{
  size_t s = 1;
  unsigned int *size_int = &s;
  return (int)*size_int;
}
    ]])],glib_size_type=int,
      [GLIB_CHECK_COMPILE_WARNINGS([AC_LANG_SOURCE([[
#if defined(_AIX) && !defined(__GNUC__)
#pragma options langlvl=stdc89
#endif
#include <stddef.h> 
int main ()
{
   size_t s = 1;
   unsigned long *size_long = &s;
   return (int)*size_long;
}
        ]])],glib_size_type=long)])
])

AC_MSG_RESULT(unsigned $glib_size_type)

AC_CHECK_SIZEOF(ssize_t)

dnl Try to figure out whether gssize should be long or int
AC_MSG_CHECKING([for the appropriate definition for ssize_t])

case $ac_cv_sizeof_ssize_t in
  $ac_cv_sizeof_short) 
      glib_ssize_type=short
      ;;
  $ac_cv_sizeof_int) 
      glib_ssize_type=int
      ;;
  $ac_cv_sizeof_long) 
      glib_ssize_type=long
      ;;
  $ac_cv_sizeof_long_long)
      glib_ssize_type='long long'
      ;;
  $ac_cv_sizeof__int64)
      glib_ssize_type='__int64'
      ;;
  *)  AC_MSG_ERROR([No type matching ssize_t in size])
      ;;
esac

dnl If int/long are the same size, we see which one produces
dnl warnings when used in the location as ssize_t. (This matters
dnl on Android where ssize_t is long and size_t is unsigned int)
dnl
AS_IF([test $ac_cv_sizeof_ssize_t = $ac_cv_sizeof_int &&
       test $ac_cv_sizeof_ssize_t = $ac_cv_sizeof_long], [
  GLIB_CHECK_COMPILE_WARNINGS([AC_LANG_SOURCE([[
#if defined(_AIX) && !defined(__GNUC__)
#pragma options langlvl=stdc89
#endif
#include <stddef.h>
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <sys/types.h>
int main ()
{
  ssize_t s = 1;
  int *size_int = &s;
  return (int)*size_int;
}
    ]])],glib_ssize_type=int,
      [GLIB_CHECK_COMPILE_WARNINGS([AC_LANG_SOURCE([[
#if defined(_AIX) && !defined(__GNUC__)
#pragma options langlvl=stdc89
#endif
#include <stddef.h> 
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <sys/types.h>
int main ()
{
   ssize_t s = 1;
   long *size_long = &s;
   return (int)*size_long;
}
        ]])],glib_ssize_type=long)])
])

AC_MSG_RESULT($glib_ssize_type)

# Check for some functions
AC_CHECK_FUNCS(lstat strsignal vsnprintf stpcpy strcasecmp strncasecmp poll vasprintf setenv unsetenv getc_unlocked readlink symlink fdwalk mkostemp link)
AC_CHECK_FUNCS(lchmod lchown fchmod fchown utimes getresuid)
AC_CHECK_FUNCS(getmntent_r setmntent endmntent hasmntopt getfsstat getvfsstat fallocate)
case $host_os in aix*) ac_cv_func_splice=no ;; esac # AIX splice() is something else
AC_CHECK_FUNCS(splice)
AC_CHECK_FUNCS(prlimit)

# To avoid finding a compatibility unusable statfs, which typically
# successfully compiles, but warns to use the newer statvfs interface:
AS_IF([test $ac_cv_header_sys_statvfs_h = yes], [AC_CHECK_FUNCS([statvfs])])
AS_IF([test $ac_cv_header_sys_statfs_h  = yes -o $ac_cv_header_sys_mount_h = yes], [AC_CHECK_FUNCS([statfs])])

AC_MSG_CHECKING([whether to use statfs or statvfs])
# Some systems have both statfs and statvfs, pick the most "native" for these
AS_IF([test x$ac_cv_func_statfs = xyes && test x$ac_cv_func_statvfs = xyes],
   [
   # on solaris and irix, statfs doesn't even have the f_bavail field
   AS_IF([test x$ac_cv_member_struct_statfs_f_bavail = xno],
      [ac_cv_func_statfs=no],
   # else, at least on linux, statfs is the actual syscall
      [ac_cv_func_statvfs=no])
   ])

AS_IF([test x$ac_cv_func_statfs = xyes],
      [
         AC_DEFINE([USE_STATFS], [1], [Define to use statfs()])
         AC_MSG_RESULT([statfs])
      ],
      [test x$ac_cv_func_statvfs = xyes],
      [
         AC_DEFINE([USE_STATVFS], [1], [Define to use statvfs()])
         AC_MSG_RESULT([statvfs])
      ],
      [  AC_MSG_RESULT([neither])])

AC_CHECK_HEADERS(crt_externs.h)
AC_CHECK_FUNCS(_NSGetEnviron)

AC_CHECK_FUNCS(newlocale uselocale strtod_l strtoll_l strtoull_l)

# Internet address families
if test $glib_native_win32 = yes; then
  glib_inet_includes=["
#include <winsock2.h>
  "]
else
  glib_inet_includes=["
#include <sys/types.h>
#include <sys/socket.h>
  "]
fi

glib_failed=false
GLIB_CHECK_VALUE(AF_INET, $glib_inet_includes, glib_failed=true)
GLIB_CHECK_VALUE(AF_INET6, $glib_inet_includes, glib_failed=true)
# winsock defines this even though it doesn't support it
GLIB_CHECK_VALUE(AF_UNIX, $glib_inet_includes, glib_failed=true)
if $glib_failed ; then
  AC_MSG_ERROR([Could not determine values for AF_INET* constants])
fi

glib_failed=false
GLIB_CHECK_VALUE(MSG_PEEK, $glib_inet_includes, glib_failed=true)
GLIB_CHECK_VALUE(MSG_OOB, $glib_inet_includes, glib_failed=true)
GLIB_CHECK_VALUE(MSG_DONTROUTE, $glib_inet_includes, glib_failed=true)
if $glib_failed ; then
  AC_MSG_ERROR([Could not determine values for MSG_* constants])
fi

AC_CHECK_FUNCS(endservent if_nametoindex if_indextoname sendmmsg recvmmsg)

AC_MSG_CHECKING([for SIOCGIFADDR])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[
        #include <sys/ioctl.h>
        #include <net/if.h>
    ]],
    [[
        struct ifreq ifr;
        ioctl(0, SIOCGIFADDR, &ifr);
    ]])], [
        AC_MSG_RESULT(yes)
        AC_DEFINE(HAVE_SIOCGIFADDR, 1, [SIOCGIFADDR is available])
    ], [
        AC_MSG_RESULT(no)
])

AC_MSG_CHECKING([if ip_mreq_source.imr_interface has s_addr member])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[
        #include <netinet/in.h>
    ]],
    [[
        struct ip_mreq_source mc_req_src;
        mc_req_src.imr_interface.s_addr = 0;
    ]])], [
        AC_MSG_RESULT(yes)
    ], [
        AC_MSG_RESULT(no)
        AC_DEFINE(BROKEN_IP_MREQ_SOURCE_STRUCT, 1, [struct ip_mreq_source definition is broken on Android NDK <= r16])
])

AS_IF([test $glib_native_win32 = yes], [
  # <wspiapi.h> in the Windows SDK and in mingw-w64 has wrappers for
  # inline workarounds for getaddrinfo, getnameinfo and freeaddrinfo if
  # they aren't present at run-time (on Windows 2000).
  AC_CHECK_HEADER([wspiapi.h], [WSPIAPI_INCLUDE="#include <wspiapi.h>"])
  AC_SUBST(WSPIAPI_INCLUDE)
], [
  # Android does not have C_IN in public headers, we define it wherever necessary
  AS_IF([test $glib_native_android != yes], [
    AC_MSG_CHECKING([if arpa/nameser_compat.h is needed])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include <sys/types.h>
                                        #include <arpa/nameser.h>],
                                       [int qclass = C_IN;])],
                      [AC_MSG_RESULT([no])],
                      [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include <sys/types.h>
                                                           #include <arpa/nameser.h>
                                                           #include <arpa/nameser_compat.h>],
                                                          [int qclass = C_IN;])],
                                         [AC_MSG_RESULT([yes])
                                          NAMESER_COMPAT_INCLUDE="#include <arpa/nameser_compat.h>"],
                                         [AC_MSG_ERROR([could not compile test program either way])])])])
  AC_SUBST(NAMESER_COMPAT_INCLUDE)

  # We can't just use AC_CHECK_FUNC/AC_CHECK_LIB here. Bug 586150
  NETWORK_LIBS=""
  AC_MSG_CHECKING([for res_query])
  AC_TRY_LINK([#include <sys/types.h>
	       #include <netinet/in.h>
	       #include <arpa/nameser.h>
	       #include <resolv.h>
              ],[
	       res_query("test", 0, 0, (void *)0, 0);
              ],[AC_MSG_RESULT([yes])],
              [save_libs="$LIBS"
               LIBS="-lresolv $LIBS"
               AC_TRY_LINK([#include <sys/types.h>
			    #include <netinet/in.h>
			    #include <arpa/nameser.h>
			    #include <resolv.h>
			   ],[
			    res_query("test", 0, 0, (void *)0, 0);
			   ],[AC_MSG_RESULT([in -lresolv])
                              NETWORK_LIBS="-lresolv $NETWORK_LIBS"],
                           [LIBS="-lbind $save_libs"
                            AC_TRY_LINK([#include <resolv.h>],
                                        [res_query("test", 0, 0, (void *)0, 0);],
                                        [AC_MSG_RESULT([in -lbind])
                                         NETWORK_LIBS="-lbind $NETWORK_LIBS"],
                                        [AC_MSG_ERROR(not found)])])
               LIBS="$save_libs"])
  AC_CHECK_FUNC(socket, :, AC_CHECK_LIB(socket, socket,
				        [NETWORK_LIBS="-lsocket $NETWORK_LIBS"],
				        [AC_MSG_ERROR(Could not find socket())]))
  save_libs="$LIBS"
  LIBS="$LIBS $NETWORK_LIBS"

  AC_MSG_CHECKING([for res_init])
  AC_TRY_LINK([#include <sys/types.h>
	       #include <netinet/in.h>
	       #include <arpa/nameser.h>
	       #include <resolv.h>
              ],[
	       res_init();
              ],[AC_MSG_RESULT([yes])
	         AC_DEFINE(HAVE_RES_INIT, 1, [Define to 1 if you have the 'res_init' function.])
	      ],[AC_MSG_RESULT([no])])

  AC_MSG_CHECKING([for res_nclose])
  AC_TRY_LINK([#include <sys/types.h>
	       #include <netinet/in.h>
	       #include <arpa/nameser.h>
	       #include <resolv.h>
              ],[
               struct __res_state res;
	       res_nclose(&res);
              ],[AC_MSG_RESULT([yes])
	         AC_DEFINE(HAVE_RES_NCLOSE, 1, [Define to 1 if you have the 'res_nclose' function.])
	      ],[AC_MSG_RESULT([no])])

  AC_MSG_CHECKING([for res_ndestroy])
  AC_TRY_LINK([#include <sys/types.h>
	       #include <netinet/in.h>
	       #include <arpa/nameser.h>
	       #include <resolv.h>
              ],[
               struct __res_state res;
	       res_ndestroy(&res);
              ],[AC_MSG_RESULT([yes])
	         AC_DEFINE(HAVE_RES_NDESTROY, 1, [Define to 1 if you have the 'res_ndestroy' function.])
	      ],[AC_MSG_RESULT([no])])

  AC_MSG_CHECKING([for res_ninit])
  AC_TRY_LINK([#include <sys/types.h>
	       #include <netinet/in.h>
	       #include <arpa/nameser.h>
	       #include <resolv.h>
              ],[
               struct __res_state res;
	       res_ninit(&res);
              ],[AC_MSG_RESULT([yes])
	         AC_DEFINE(HAVE_RES_NINIT, 1, [Define to 1 if you have the 'res_ninit' function.])
	      ],[AC_MSG_RESULT([no])])

  AC_MSG_CHECKING([for res_nquery])
  AC_TRY_LINK([#include <sys/types.h>
	       #include <netinet/in.h>
	       #include <arpa/nameser.h>
	       #include <resolv.h>
              ],[
               struct __res_state res;
	       res_nquery(&res, "test", 0, 0, (void *)0, 0);
              ],[AC_MSG_RESULT([yes])
	         AC_DEFINE(HAVE_RES_NQUERY, 1, [Define to 1 if you have the 'res_nquery' function.])
	      ],[AC_MSG_RESULT([no])])
  LIBS="$save_libs"
])
AC_SUBST(NETWORK_LIBS)

AC_CHECK_HEADER([linux/netlink.h],
		[AC_DEFINE(HAVE_NETLINK, 1, [We have AF_NETLINK sockets])],,
		[#include <sys/socket.h>])
AM_CONDITIONAL(HAVE_NETLINK, [test "$ac_cv_header_linux_netlink_h" = "yes"])

AC_CHECK_TYPE([struct ip_mreqn], [
              AC_DEFINE(HAVE_IP_MREQN,, [Define if we have struct ip_mreqn])],,
              [#include <netinet/in.h>])

case $host in
  *-*-solaris* )
     AC_DEFINE(_XOPEN_SOURCE_EXTENDED, 1, Needed to get declarations for msg_control and msg_controllen on Solaris)
     AC_DEFINE(_XOPEN_SOURCE,          2, Needed to get declarations for msg_control and msg_controllen on Solaris)
     AC_DEFINE(__EXTENSIONS__,         1, Needed to get declarations for msg_control and msg_controllen on Solaris)
     ;;
esac

dnl
dnl if statfs() takes 2 arguments (Posix) or 4 (Solaris)
dnl
AS_IF([test "$ac_cv_func_statfs" = yes], [
  AC_MSG_CHECKING([number of arguments to statfs()])
  AC_TRY_COMPILE([#include <unistd.h>
  #ifdef HAVE_SYS_PARAM_H
  #include <sys/param.h>
  #endif
  #ifdef HAVE_SYS_VFS_H
  #include <sys/vfs.h>
  #endif
  #ifdef HAVE_SYS_MOUNT_H
  #include <sys/mount.h>
  #endif
  #ifdef HAVE_SYS_STATFS_H
  #include <sys/statfs.h>
  #endif], [struct statfs st;
  statfs("/", &st);],[
    AC_MSG_RESULT([2])
    AC_DEFINE(STATFS_ARGS, 2, [Number of arguments to statfs()])],[
    AC_TRY_COMPILE([#include <unistd.h>
  #ifdef HAVE_SYS_PARAM_H
  #include <sys/param.h>
  #endif
  #ifdef HAVE_SYS_VFS_H
  #include <sys/vfs.h>
  #endif
  #ifdef HAVE_SYS_MOUNT_H
  #include <sys/mount.h>
  #endif
  #ifdef HAVE_SYS_STATFS_H
  #include <sys/statfs.h>
  #endif], [struct statfs st;
  statfs("/", &st, sizeof (st), 0);],[
      AC_MSG_RESULT([4])
      AC_DEFINE(STATFS_ARGS, 4, [Number of arguments to statfs()])],[
      AC_MSG_RESULT(unknown)
      AC_MSG_ERROR([unable to determine number of arguments to statfs()])])])
])

dnl
dnl open takes O_DIRECTORY as an option
dnl
AC_MSG_CHECKING([open() option O_DIRECTORY])
AC_TRY_COMPILE([#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>],
[open(0, O_DIRECTORY, 0);],[
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_OPEN_O_DIRECTORY, 1, [open option O_DIRECTORY])],[
	AC_MSG_RESULT([no])])

#
# Check whether to use an included printf
#
AC_FUNC_VSNPRINTF_C99
AC_FUNC_PRINTF_UNIX98

AC_ARG_ENABLE(included-printf,
              [AS_HELP_STRING([--enable-included-printf],
                              [use included printf [default=auto]])],
              enable_included_printf="$enableval")

need_included_printf=no
if test "x$enable_included_printf" = "xyes" ; then
  need_included_printf=yes
fi
if test "$ac_cv_func_vsnprintf_c99" != "yes" ; then
  need_included_printf=yes
fi
if test "$ac_cv_func_printf_unix98" != "yes" ; then
  need_included_printf=yes
fi
if test "x$ac_cv_sizeof_long_long" = "x8" &&
   test -z "$glib_cv_long_long_format" ; then
  need_included_printf=yes
fi

if test "x$enable_included_printf" = "xno" && 
   test "x$need_included_printf" = "xyes" ; then
  AC_MSG_ERROR([
*** Your C library's printf doesn't appear to have the features that
*** GLib needs, but you specified --enable-included-printf=no.])
fi

enable_included_printf=$need_included_printf

AM_CONDITIONAL(HAVE_GOOD_PRINTF, test "$enable_included_printf" != "yes")
AS_IF([test "$enable_included_printf" != "yes"], [
  AC_DEFINE(HAVE_GOOD_PRINTF,1,[define to use system printf])
], [
  if test -z "$glib_cv_long_long_format" ; then
    glib_cv_long_long_format="ll"
  fi
  AC_DEFINE(HAVE_VASPRINTF,1)
])

# Checks needed for gnulib vasnprintf
bh_C_SIGNED
jm_AC_TYPE_LONG_LONG
gt_TYPE_LONGDOUBLE
gt_TYPE_WCHAR_T
gt_TYPE_WINT_T
AC_TYPE_SIZE_T
AC_CHECK_TYPES(ptrdiff_t)
jm_AC_TYPE_INTMAX_T
AC_CHECK_FUNCS([snprintf strnlen wcslen wcsnlen mbrtowc wcrtomb])
AC_FUNC_SNPRINTF_C99

# Check if <sys/select.h> needs to be included for fd_set
AC_MSG_CHECKING([for fd_set])
AC_TRY_COMPILE([#include <sys/types.h>],
        [fd_set readMask, writeMask;], gtk_ok=yes, gtk_ok=no)
AS_IF([test "$gtk_ok" = "yes"], [
    AC_MSG_RESULT([yes, found in sys/types.h])
], [
    AC_EGREP_HEADER(fd_set, sys/select.h, gtk_ok=yes)
    if test "$gtk_ok" = "yes"; then
	# *** FIXME: give it a different name
        AC_DEFINE(HAVE_SYS_SELECT_H,1,[found fd_set in sys/select.h])
        AC_MSG_RESULT([yes, found in sys/select.h])
    else
	AC_DEFINE(NO_FD_SET,1,[didn't find fd_set])
	AC_MSG_RESULT(no)
    fi
])

dnl Check for nl_langinfo and CODESET
AC_LANG_SAVE
AC_LANG_C
AC_CACHE_CHECK([for nl_langinfo (CODESET)],glib_cv_langinfo_codeset,[
        AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <langinfo.h>],
                       [char *codeset = nl_langinfo (CODESET);])],
                [glib_cv_langinfo_codeset=yes],
                [glib_cv_langinfo_codeset=no])])
if test x$glib_cv_langinfo_codeset = xyes; then
  AC_DEFINE(HAVE_CODESET,1,[Have nl_langinfo (CODESET)])
fi

dnl Check for nl_langinfo and LC_TIME parts that are needed in gdatetime.c
AC_CACHE_CHECK([for nl_langinfo (PM_STR)],glib_cv_langinfo_time,[
        AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <langinfo.h>],
                 [char *str;
                 str = nl_langinfo (PM_STR);
                 str = nl_langinfo (D_T_FMT);
                 str = nl_langinfo (D_FMT);
                 str = nl_langinfo (T_FMT);
                 str = nl_langinfo (T_FMT_AMPM);
                 str = nl_langinfo (MON_1);
                 str = nl_langinfo (ABMON_12);
                 str = nl_langinfo (DAY_1);
                 str = nl_langinfo (ABDAY_7);])],
                [glib_cv_langinfo_time=yes],
                [glib_cv_langinfo_time=no])])
if test x$glib_cv_langinfo_time = xyes; then
  AC_DEFINE(HAVE_LANGINFO_TIME,1,[Have nl_langinfo (PM_STR)])
fi

dnl Check for nl_langinfo and _NL_CTYPE_OUTDIGITn_MB
AC_CACHE_CHECK([for nl_langinfo (_NL_CTYPE_OUTDIGITn_MB)], glib_cv_langinfo_outdigit,[
        AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <langinfo.h>],
                [char *str;
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT0_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT1_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT2_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT3_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT4_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT5_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT6_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT7_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT8_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT9_MB);])],
                [glib_cv_langinfo_outdigit=yes],
                [glib_cv_langinfo_outdigit=no])])
if test x$glib_cv_langinfo_outdigit = xyes; then
  AC_DEFINE(HAVE_LANGINFO_OUTDIGIT,1,[Have nl_langinfo (_NL_CTYPE_OUTDIGITn_MB)])
fi

dnl Check for nl_langinfo and ALTMON_n
AC_CACHE_CHECK([for nl_langinfo (ALTMON_n)], glib_cv_langinfo_altmon,[
        AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <langinfo.h>],
                [char *str;
                 str = nl_langinfo (ALTMON_1);
                 str = nl_langinfo (ALTMON_2);
                 str = nl_langinfo (ALTMON_3);
                 str = nl_langinfo (ALTMON_4);
                 str = nl_langinfo (ALTMON_5);
                 str = nl_langinfo (ALTMON_6);
                 str = nl_langinfo (ALTMON_7);
                 str = nl_langinfo (ALTMON_8);
                 str = nl_langinfo (ALTMON_9);
                 str = nl_langinfo (ALTMON_10);
                 str = nl_langinfo (ALTMON_11);
                 str = nl_langinfo (ALTMON_12);])],
                [glib_cv_langinfo_altmon=yes],
                [glib_cv_langinfo_altmon=no])])
if test x$glib_cv_langinfo_altmon = xyes; then
  AC_DEFINE(HAVE_LANGINFO_ALTMON,1,[Have nl_langinfo (ALTMON_n)])
fi

dnl Check for nl_langinfo and _NL_ABALTMON_n
AC_CACHE_CHECK([for nl_langinfo (_NL_ABALTMON_n)], glib_cv_langinfo_abaltmon,[
        AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <langinfo.h>],
                [char *str;
                 str = nl_langinfo (_NL_ABALTMON_1);
                 str = nl_langinfo (_NL_ABALTMON_2);
                 str = nl_langinfo (_NL_ABALTMON_3);
                 str = nl_langinfo (_NL_ABALTMON_4);
                 str = nl_langinfo (_NL_ABALTMON_5);
                 str = nl_langinfo (_NL_ABALTMON_6);
                 str = nl_langinfo (_NL_ABALTMON_7);
                 str = nl_langinfo (_NL_ABALTMON_8);
                 str = nl_langinfo (_NL_ABALTMON_9);
                 str = nl_langinfo (_NL_ABALTMON_10);
                 str = nl_langinfo (_NL_ABALTMON_11);
                 str = nl_langinfo (_NL_ABALTMON_12);])],
                [glib_cv_langinfo_abaltmon=yes],
                [glib_cv_langinfo_abaltmon=no])])
if test x$glib_cv_langinfo_abaltmon = xyes; then
  AC_DEFINE(HAVE_LANGINFO_ABALTMON,1,[Have nl_langinfo (_NL_ABALTMON_n)])
fi
AC_LANG_RESTORE

dnl ****************************************
dnl *** posix_memalign                   ***
dnl ****************************************
AC_MSG_CHECKING(for a compliant posix_memalign() implementation)
AC_CACHE_VAL(glib_cv_compliant_posix_memalign,[
    glib_cv_compliant_posix_memalign=0
    if test "$ac_cv_func_posix_memalign" = "yes" ; then
	AC_TRY_RUN([
                #define _XOPEN_SOURCE 600
                #include <stdlib.h> /* posix_memalign() should be defined here */
                /* some systems break if #include <malloc.h> used */
		static void test_memalign (size_t boundary, size_t size) {
		    void *mem = 0;
		    if (posix_memalign (&mem, boundary, size) != 0 || !mem)
			exit (1);
                    else
                      free (mem);
		}
		int main() {
		    test_memalign (  128,   128 - 2 * sizeof (void*));
		    test_memalign (  256,   256 - 2 * sizeof (void*));
		    test_memalign (  512,   512 - 2 * sizeof (void*));
		    test_memalign ( 1024,  1024 - 2 * sizeof (void*));
		    test_memalign ( 2048,  2048 - 2 * sizeof (void*));
		    test_memalign ( 4096,  4096 - 2 * sizeof (void*));
		    test_memalign ( 8192,  8192 - 2 * sizeof (void*));
		    test_memalign (16384, 16384 - 2 * sizeof (void*));
		    test_memalign (32768, 32768 - 2 * sizeof (void*));
		    exit (0); /* success */
		}
		],
	    [glib_cv_compliant_posix_memalign=1], [], [:])
      :
    fi
    ])
AS_IF([test "$glib_cv_compliant_posix_memalign" = "1"], [
    AC_MSG_RESULT(yes)
    AC_DEFINE(POSIX_MEMALIGN_WITH_COMPLIANT_ALLOCS, 1, [define if posix_memalign() can allocate any size])
], [
    AC_MSG_RESULT(no)
])


dnl ****************************************
dnl *** strlcpy/strlcat                  ***
dnl ****************************************
# Check for strlcpy
AC_CACHE_CHECK([for OpenBSD strlcpy/strlcat],glib_cv_have_strlcpy,[
AC_TRY_RUN([#include <stdlib.h>
#include <string.h>
int main() {
  char p[10];
  (void) strlcpy (p, "hi", 10);
  if (strlcat (p, "bye", 0) != 3) 
    return 1;
  return 0;
}], glib_cv_have_strlcpy=yes, 
    glib_cv_have_strlcpy=no,
    glib_cv_have_strlcpy=no)])
if test "$glib_cv_have_strlcpy" = "yes"; then
    AC_DEFINE(HAVE_STRLCPY,1,[Have functions strlcpy and strlcat])
fi
  

dnl **********************
dnl *** va_copy checks ***
dnl **********************
dnl we currently check for all three va_copy possibilities, so we get
dnl all results in config.log for bug reports.
AC_CACHE_CHECK([for an implementation of va_copy()],glib_cv_va_copy,[
	AC_LINK_IFELSE([AC_LANG_SOURCE([[#include <stdarg.h>
#include <stdlib.h>
	void f (int i, ...) {
	va_list args1, args2;
	va_start (args1, i);
	va_copy (args2, args1);
	if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
	  exit (1);
	va_end (args1); va_end (args2);
	}
	int main() {
	  f (0, 42);
	  return 0;
	}]])],
	[glib_cv_va_copy=yes],
	[glib_cv_va_copy=no])
])
AC_CACHE_CHECK([for an implementation of __va_copy()],glib_cv___va_copy,[
	AC_LINK_IFELSE([AC_LANG_SOURCE([[#include <stdarg.h>
#include <stdlib.h>
	void f (int i, ...) {
	va_list args1, args2;
	va_start (args1, i);
	__va_copy (args2, args1);
	if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
	  exit (1);
	va_end (args1); va_end (args2);
	}
	int main() {
	  f (0, 42);
	  return 0;
	}]])],
	[glib_cv___va_copy=yes],
	[glib_cv___va_copy=no])
])

if test "x$glib_cv_va_copy" = "xyes"; then
  g_va_copy_func=va_copy
else if test "x$glib_cv___va_copy" = "xyes"; then
  g_va_copy_func=__va_copy
fi
fi

if test -n "$g_va_copy_func"; then
  AC_DEFINE_UNQUOTED(G_VA_COPY,$g_va_copy_func,[A 'va_copy' style function])
fi

AC_CACHE_CHECK([whether va_lists can be copied by value],glib_cv_va_val_copy,[
	AC_TRY_RUN([#include <stdarg.h>
#include <stdlib.h> 
	void f (int i, ...) {
	va_list args1, args2;
	va_start (args1, i);
	args2 = args1;
	if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
	  exit (1);
	va_end (args1); va_end (args2);
	}
	int main() {
	  f (0, 42);
	  return 0;
	}],
	[glib_cv_va_val_copy=yes],
	[glib_cv_va_val_copy=no],
	[glib_cv_va_val_copy=yes])
])

AS_IF([ test "x$glib_cv_va_val_copy" = "xno"], [
  AC_DEFINE(G_VA_COPY_AS_ARRAY,1, ['va_lists' cannot be copies as values])
])

dnl ***********************
dnl *** g_module checks ***
dnl ***********************
G_MODULE_LIBS=
G_MODULE_LIBS_EXTRA=
G_MODULE_PLUGIN_LIBS=
if test x"$glib_native_win32" = xyes; then
  dnl No use for this on Win32
  G_MODULE_LDFLAGS=
else
  export SED
  eval G_MODULE_LDFLAGS=$export_dynamic_flag_spec
fi
dnl G_MODULE_IMPL= don't reset, so cmd-line can override
G_MODULE_NEED_USCORE=0
G_MODULE_BROKEN_RTLD_GLOBAL=0
G_MODULE_HAVE_DLERROR=0
dnl *** force native WIN32 shared lib loader 
if test -z "$G_MODULE_IMPL"; then
  case "$host" in
  *-*-mingw*|*-*-cygwin*) G_MODULE_IMPL=G_MODULE_IMPL_WIN32 ;;
  esac
fi
dnl *** force native AIX library loader
dnl *** dlopen() filepath must be of the form /path/libname.a(libname.so)
if test -z "$G_MODULE_IMPL"; then
  case "$host" in
  *-*-aix*) G_MODULE_IMPL=G_MODULE_IMPL_AR ;;
  esac
fi
dnl *** dlopen() and dlsym() in system libraries
AS_IF([ test -z "$G_MODULE_IMPL"], [
	AC_CHECK_FUNC(dlopen,
		      [AC_CHECK_FUNC(dlsym,
			             [G_MODULE_IMPL=G_MODULE_IMPL_DL],[])],
		      [])
])
dnl *** NSLinkModule (dyld) in system libraries (Darwin)
AS_IF([ test -z "$G_MODULE_IMPL" ], [
 	AC_CHECK_FUNC(NSLinkModule,
		      [G_MODULE_IMPL=G_MODULE_IMPL_DYLD
		       G_MODULE_NEED_USCORE=1],
		      [])
])
dnl *** dlopen() and dlsym() in libdl
AS_IF([ test -z "$G_MODULE_IMPL"], [
	AC_CHECK_LIB(dl, dlopen,
		     [AC_CHECK_LIB(dl, dlsym,
			           [G_MODULE_LIBS=-ldl
		                   G_MODULE_IMPL=G_MODULE_IMPL_DL],[])],
		     [])
])
dnl *** additional checks for G_MODULE_IMPL_DL
AS_IF([ test "$G_MODULE_IMPL" = "G_MODULE_IMPL_DL" ], [
	LIBS_orig="$LIBS"
	LDFLAGS_orig="$LDFLAGS"
	LIBS="$G_MODULE_LIBS $LIBS"
	LDFLAGS="$LDFLAGS $G_MODULE_LDFLAGS"
dnl *** check for OSF1/5.0 RTLD_GLOBAL brokenness
	echo "void glib_plugin_test(void) { }" > plugin.c
	${SHELL} ./libtool --mode=compile --tag=CC ${CC} ${CFLAGS} \
		${CPPFLAGS} -c -o plugin.lo plugin.c >/dev/null 2>&1
	${SHELL} ./libtool --mode=link --tag=CC ${CC} ${CFLAGS} \
		${LDFLAGS} -module -o plugin.la -export-dynamic \
		-shrext ".o" -avoid-version plugin.lo \
		-rpath /dont/care >/dev/null 2>&1
	AC_CACHE_CHECK([for RTLD_GLOBAL brokenness],
		glib_cv_rtldglobal_broken,[
		AC_TRY_RUN([
#include <dlfcn.h>
#ifndef RTLD_GLOBAL
#  define RTLD_GLOBAL 0
#endif
#ifndef RTLD_LAZY
#  define RTLD_LAZY 0
#endif
int glib_plugin_test;
int main () {
    void *handle, *global, *local;
    global = &glib_plugin_test;
    handle = dlopen ("./$objdir/plugin.o", RTLD_GLOBAL | RTLD_LAZY);
    if (!handle) return 0;
    local = dlsym (handle, "glib_plugin_test");
    return global == local;
}                       ],
			[glib_cv_rtldglobal_broken=no],
			[glib_cv_rtldglobal_broken=yes],
			[glib_cv_rtldglobal_broken=no])
		rm -f plugin.c plugin.o plugin.lo plugin.la ${objdir}/plugin.*
		rmdir ${objdir} 2>/dev/null
	])
	if test "x$glib_cv_rtldglobal_broken" = "xyes"; then
  		G_MODULE_BROKEN_RTLD_GLOBAL=1
	else
  		G_MODULE_BROKEN_RTLD_GLOBAL=0
	fi
dnl *** check whether we need preceeding underscores
	AC_CACHE_CHECK([for preceeding underscore in symbols],
		glib_cv_uscore,[
		AC_TRY_RUN([#include <dlfcn.h>
                int glib_underscore_test (void) { return 42; }
		int main() {
		  void *f1 = (void*)0, *f2 = (void*)0, *handle;
		  handle = dlopen ((void*)0, 0);
		  if (handle) {
		    f1 = dlsym (handle, "glib_underscore_test");
		    f2 = dlsym (handle, "_glib_underscore_test");
		  } return (!f2 || f1);
		}],
			[glib_cv_uscore=yes],
			[glib_cv_uscore=no],
			[])
		rm -f plugin.c plugin.$ac_objext plugin.lo
	])
        GLIB_ASSERT_SET(glib_cv_uscore)
	if test "x$glib_cv_uscore" = "xyes"; then
  		G_MODULE_NEED_USCORE=1
	else
  		G_MODULE_NEED_USCORE=0
	fi

        AC_CHECK_DECL([RTLD_LAZY],
                      [AC_DEFINE(HAVE_RTLD_LAZY, 1, [Define to 1 if RTLD_LAZY is available])],
                      [], [[#include <dlfcn.h>]])
        AC_CHECK_DECL([RTLD_NOW],
                      [AC_DEFINE(HAVE_RTLD_NOW, 1, [Define to 1 if RTLD_NOW is available])],
                      [], [[#include <dlfcn.h>]])
        AC_CHECK_DECL([RTLD_GLOBAL],
                      [AC_DEFINE(HAVE_RTLD_GLOBAL, 1, [Define to 1 if RTLD_GLOBAL is available])],
                      [], [[#include <dlfcn.h>]])

	LDFLAGS="$LDFLAGS_orig"
dnl *** check for having dlerror()
	AC_CHECK_FUNC(dlerror,
		[G_MODULE_HAVE_DLERROR=1],
		[G_MODULE_HAVE_DLERROR=0])
	LIBS="$LIBS_orig"
])
dnl *** done, have we got an implementation?
if test -z "$G_MODULE_IMPL"; then
	G_MODULE_IMPL=0
        G_MODULE_SUPPORTED=false
else
        G_MODULE_SUPPORTED=true
fi

AC_MSG_CHECKING(for the suffix of module shared libraries)
export SED
module=yes eval std_shrext=$shrext_cmds
# chop the initial dot
glib_gmodule_suffix=`echo $std_shrext | sed 's/^\.//'`
AC_MSG_RESULT(.$glib_gmodule_suffix)
# any reason it may fail?
if test "x$glib_gmodule_suffix" = x; then
	AC_MSG_ERROR(Cannot determine shared library suffix from libtool)
fi
 
AC_SUBST(G_MODULE_SUPPORTED)
AC_SUBST(G_MODULE_IMPL)
AC_SUBST(G_MODULE_LIBS)
AC_SUBST(G_MODULE_LIBS_EXTRA)
AC_SUBST(G_MODULE_PLUGIN_LIBS)
AC_SUBST(G_MODULE_LDFLAGS)
AC_SUBST(G_MODULE_HAVE_DLERROR)
AC_SUBST(G_MODULE_BROKEN_RTLD_GLOBAL)
AC_SUBST(G_MODULE_NEED_USCORE)
AC_SUBST(GLIB_DEBUG_FLAGS)

dnl **********************
dnl *** g_spawn checks ***
dnl **********************

AC_MSG_CHECKING(for gspawn implementation)
case "$host" in
  *-*-mingw*)
    GSPAWN=gspawn-win32.lo
    ;;
  *)
    GSPAWN=gspawn.lo
    ;;    
esac
AC_MSG_RESULT($GSPAWN)
AC_SUBST(GSPAWN)

dnl *************************
dnl *** GIOChannel checks ***
dnl *************************

AC_MSG_CHECKING(for GIOChannel implementation)
case "$host" in
  *-*-mingw*)
    GIO=giowin32.lo
    ;;
  *)
    GIO=giounix.lo
    ;;    
esac
AC_MSG_RESULT($GIO)
AC_SUBST(GIO)

dnl *********************************
dnl *** Directory for GIO modules ***
dnl *********************************

AC_ARG_WITH(gio-module-dir,
           [AS_HELP_STRING([--with-gio-module-dir=DIR],
                           [load gio modules from this directory [LIBDIR/gio/modules]])],
           [],
	   [with_gio_module_dir='${libdir}/gio/modules'])
GIO_MODULE_DIR=$with_gio_module_dir
AC_SUBST(GIO_MODULE_DIR)

dnl **********************************
dnl *** Check for libselinux (GIO) ***
dnl **********************************
AC_ARG_ENABLE(selinux,
              AS_HELP_STRING([--disable-selinux],
                             [build without selinux support]))
msg_selinux=no
SELINUX_LIBS=
AS_IF([ test "x$enable_selinux" != "xno"], [

 AC_CHECK_LIB(selinux, is_selinux_enabled,
   [AC_CHECK_HEADERS(selinux/selinux.h,
     [AC_CHECK_LIB(selinux, lgetfilecon_raw, 
       [AC_DEFINE(HAVE_SELINUX, 1, [Define to 1 if libselinux is available])
        SELINUX_LIBS="-lselinux"
        msg_selinux=yes])
     ])
   ])
])
AC_SUBST(SELINUX_LIBS)

dnl *****************************
dnl ** Check for inotify (GIO) **
dnl *****************************
inotify_support=no
AC_CHECK_HEADERS([sys/inotify.h],
[
  AC_CHECK_FUNCS(inotify_init1, [inotify_support=yes], [inotify_support=no])
])

AM_CONDITIONAL(HAVE_INOTIFY, [test "$inotify_support" = "yes"])

dnl ****************************
dnl ** Check for kqueue (GIO) **
dnl ****************************
kqueue_support=no
AC_CHECK_HEADERS([sys/event.h],
[
	AC_CHECK_FUNCS(kqueue kevent, [kqueue_support=yes])
])

AM_CONDITIONAL(HAVE_KQUEUE, [test "$kqueue_support" = "yes"])

dnl ****************************
dnl *** Checks for FAM (GIO) ***
dnl ****************************

should_disable_fam=no

AC_ARG_ENABLE(fam,
              AS_HELP_STRING([--disable-fam],
                             [don't use fam for file system monitoring]),
                         [
                                if test "x$enable_fam" = "xno"; then
                                        should_disable_fam=yes
                                fi
                         ]
                         )
fam_support=no
FAM_LIBS=
if test "x$should_disable_fam" = "xno"; then
AC_CHECK_LIB(fam, FAMOpen,
  [AC_CHECK_HEADERS(fam.h,
    [AC_DEFINE(HAVE_FAM, [], [Define if we have FAM])
     AC_CHECK_LIB(fam, FAMNoExists,
     		  AC_DEFINE(HAVE_FAM_NO_EXISTS, [], [Define if we have FAMNoExists in fam]))
     FAM_LIBS="-lfam"]
     fam_support=yes,
    AC_MSG_WARN(*** FAM support will not be built (header files not found) ***))],
  AC_MSG_WARN(*** FAM support will not be built (FAM library not found) ***))
AC_SUBST(FAM_LIBS)
fi
AM_CONDITIONAL(HAVE_FAM, [test "$fam_support" = "yes"])


dnl *****************************
dnl *** Check for xattr (GIO) ***
dnl *****************************
AC_ARG_ENABLE(xattr,
              AS_HELP_STRING([--disable-xattr], [build without xattr support]))
msg_xattr=no
XATTR_LIBS=
AS_IF([ test "x$enable_xattr" != "xno"], [

dnl either glibc or libattr can provide xattr support

dnl for both of them, we check for getxattr being in
dnl the library and a valid xattr header.

dnl try glibc
 AC_CHECK_LIB(c, getxattr,
   [AC_CHECK_HEADERS(sys/xattr.h,
     [AC_DEFINE(HAVE_XATTR, 1, [Define to 1 if xattr is available])
      msg_xattr=yes])
   ])

  AS_IF([ test "x$msg_xattr" != "xyes"], [
dnl   failure. try libattr
   AC_CHECK_LIB(attr, getxattr,
      [AC_CHECK_HEADERS(attr/xattr.h,
       [AC_DEFINE(HAVE_XATTR, 1, [Define to 1 if xattr is available])
        XATTR_LIBS="-lattr"
        msg_xattr=yes])
      ])
  ])

  AS_IF([ test "x$msg_xattr" = "xyes"], [
    AC_MSG_CHECKING([for XATTR_NOFOLLOW])
    AC_TRY_COMPILE([
      #include <stdio.h>
      #ifdef HAVE_SYS_TYPES_H
      #include <sys/types.h>
      #endif
      #ifdef HAVE_SYS_XATTR_H
      #include <sys/xattr.h>
      #elif HAVE_ATTR_XATTR_H
      #include <attr/xattr.h>
      #endif
    ],
    [ssize_t len = getxattr("", "", NULL, 0, 0, XATTR_NOFOLLOW);],
    [
      AC_DEFINE([HAVE_XATTR_NOFOLLOW], [1], [Define to 1 if xattr API uses XATTR_NOFOLLOW])
      AC_MSG_RESULT([yes])
    ],
    [AC_MSG_RESULT([no])]
    )
  ])
])
AC_SUBST(XATTR_LIBS)

dnl ************************
dnl *** check for libelf ***
dnl ************************
AC_ARG_ENABLE(libelf,
              AS_HELP_STRING([--disable-libelf], [build without libelf support]))
AS_IF([ test "x$enable_libelf" != "xno"],[
PKG_CHECK_MODULES([LIBELF], [libelf >= 0.8.12], [have_libelf=yes], [have_libelf=maybe])
AS_IF([ test $have_libelf = maybe ], [
  glib_save_LIBS=$LIBS
  AC_CHECK_LIB([elf], [elf_begin], [:], [have_libelf=no])
  AC_CHECK_LIB([elf], [elf_getshdrstrndx], [:], [have_libelf=no])
  AC_CHECK_LIB([elf], [elf_getshdrnum], [:], [have_libelf=no])
  AC_CHECK_HEADER([libelf.h], [:], [have_libelf=no])
  LIBS=$glib_save_LIBS

  if test $have_libelf != no; then
    LIBELF_LIBS=-lelf
    have_libelf=yes
  fi
])
])

if test x$have_libelf = xyes; then
  AC_DEFINE(HAVE_LIBELF, 1, [Define if libelf is available])
fi

dnl ************************
dnl *** check for libmount ***
dnl ************************

dnl The fallback code doesn't really implement the same behaviors - e.g.
dnl so on linux we want to require libmount unless specifically disabled
dnl
enable_libmount_default=${glib_os_linux:-no}
AC_ARG_ENABLE(libmount,
              [AS_HELP_STRING([--enable-libmount],
                              [build with libmount support [default for Linux]])],,
              [enable_libmount=$enable_libmount_default])
AS_IF([ test "x$enable_libmount" = "xyes"],[
PKG_CHECK_MODULES([LIBMOUNT], [mount >= 2.23], [have_libmount=yes], [have_libmount=no])
if test $have_libmount = no ; then
   AC_MSG_ERROR([*** Could not find libmount])
fi
])

if test x$have_libmount = xyes; then
  AC_DEFINE(HAVE_LIBMOUNT, 1, [Define if libmount is available])
fi
AM_CONDITIONAL(HAVE_LIBMOUNT, [test x$have_libmount = xyes])

dnl ****************************************
dnl *** platform dependent source checks ***
dnl ****************************************

AC_MSG_CHECKING(for platform-dependent source)
case "$host" in
  *-*-cygwin*|*-*-mingw*)
    PLATFORMDEP=gwin32.lo
    ;;
  *)
    PLATFORMDEP=
    ;;    
esac
AC_MSG_RESULT($PLATFORMDEP)
AC_SUBST(PLATFORMDEP)

AC_MSG_CHECKING([whether to compile timeloop])
case "$host" in
  *-*-cygwin*|*-*-mingw*|*-*-minix)
    enable_timeloop=no
    ;;
  *)
    enable_timeloop=yes
    ;;    
esac
AC_MSG_RESULT($enable_timeloop)
AM_CONDITIONAL(ENABLE_TIMELOOP, test x$enable_timeloop = xyes)

AC_MSG_CHECKING([if building for some Win32 platform])
case "$host" in
  *-*-mingw*|*-*-cygwin*)
    platform_win32=yes
    ;;
  *)
    platform_win32=no
    ;;
esac
AC_MSG_RESULT($platform_win32)
AM_CONDITIONAL(PLATFORM_WIN32, test x$platform_win32 = xyes)

dnl ***********************
dnl *** g_thread checks ***
dnl ***********************

AC_ARG_WITH(threads,
           [AS_HELP_STRING([--with-threads=@<:@posix/win32@:>@],
                           [specify a thread implementation to use])],
           [],
           [with_threads=yes])

dnl error and warning message
dnl *************************

THREAD_NO_IMPLEMENTATION="No thread implementation found."

FLAG_DOES_NOT_WORK="I can't find the MACRO to enable thread safety on your
                platform (normally it's "_REENTRANT"). I'll not use any flag on
                compilation now, but then your programs might not work.
                Please provide information on how it is done on your system."

LIBS_NOT_FOUND_1="I can't find the libraries for the thread implementation
                 "

LIBS_NOT_FOUND_2=". Please choose another thread implementation or
                  provide information on your thread implementation."

FUNC_NO_GETPWUID_R="the 'g_get_(user_name|real_name|home_dir|tmp_dir)'
		functions will not be MT-safe during their first call because
		there is no working 'getpwuid_r' on your system."

FUNC_NO_LOCALTIME_R="the 'g_date_set_time' function will not be MT-safe
		because there is no 'localtime_r' on your system."

AIX_COMPILE_INFO="AIX's C compiler needs to be called by a different name, when
		linking threaded applications. As GLib cannot do that 
		automatically, you will get an linkg error everytime you are 
		not using the right compiler. In that case you have to relink 
		with the right compiler. Ususally just '_r' is appended 
		to the compiler name."

dnl determination of thread implementation
dnl ***************************************

AC_MSG_CHECKING(for thread implementation)

have_threads=no
AS_IF([ test "x$with_threads" = xyes || test "x$with_threads" = xposix], [
        AS_IF([ test "x$have_threads" = xno], [
                AC_TRY_COMPILE([#include <pthread.h>],
			[pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;],
			have_threads=posix)
	])
	# Tru64Unix requires -pthread to find pthread.h. See #103020
	if test "x$have_threads" = xno; then
		glib_save_CPPFLAGS="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS -pthread"
		AC_TRY_COMPILE([#include <pthread.h>],
		       [pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;],
		       have_threads=posix)
		CPPFLAGS="$glib_save_CPPFLAGS"
        fi
])
if test "x$with_threads" = xyes || test "x$with_threads" = xwin32; then
        case $host in
                *-*-mingw*)
                have_threads=win32
                ;;
        esac
fi

if test "x$have_threads" = xno; then
        AC_MSG_RESULT(none available)
        AC_MSG_ERROR($THREAD_NO_IMPLEMENTATION)
else
        AC_MSG_RESULT($have_threads)
fi


dnl determination of G_THREAD_CFLAGS
dnl ********************************

G_THREAD_LIBS=
G_THREAD_LIBS_EXTRA=
G_THREAD_CFLAGS=

dnl
dnl Test program for basic POSIX threads functionality
dnl
m4_define([glib_thread_test],[
#include <pthread.h> 
int check_me = 0;
void* func(void* data) {check_me = 42; return &check_me;}
int main()
 { pthread_t t; 
   void *ret;
   pthread_create (&t, $1, func, 0);
   pthread_join (t, &ret);
   return (check_me != 42 || ret != &check_me);
}])

AS_IF([ test x"$have_threads" = xposix], [
  # First we test for posix, whether -pthread or -pthreads do the trick as 
  # both CPPFLAG and LIBS. 
  # One of them does for most gcc versions and some other platforms/compilers
  # too and could be considered as the canonical way to go. 
  case $host in
    *-*-cygwin*|*-*-darwin*)
       # skip cygwin and darwin -pthread or -pthreads test
       ;;
    *-solaris*)
      # These compiler/linker flags work with both Sun Studio and gcc
      # Sun Studio expands -mt to -D_REENTRANT and -lthread
      # gcc expands -pthreads to -D_REENTRANT -D_PTHREADS -lpthread
      G_THREAD_CFLAGS="-D_REENTRANT -D_PTHREADS"
      G_THREAD_LIBS="-lpthread -lthread"
      ;;
    *)
      for flag in pthread pthreads mt; do
        glib_save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS -$flag"
        AC_TRY_RUN(glib_thread_test(0),
                   glib_flag_works=yes,
                   glib_flag_works=no,
                   [AC_LINK_IFELSE([AC_LANG_SOURCE(glib_thread_test(0))],
                                   glib_flag_works=yes,
                                   glib_flag_works=no)])
        CFLAGS="$glib_save_CFLAGS"
        if test $glib_flag_works = yes ; then
           G_THREAD_CFLAGS=-$flag
        G_THREAD_LIBS=-$flag
        break;
        fi
      done
       ;;
  esac
])

AS_IF([ test x"$G_THREAD_CFLAGS" = x], [

  # The canonical -pthread[s] does not work. Try something different.

  case $host in
	*-aix*)
		if test x"$GCC" = xyes; then
			# GCC 3.0 and above needs -pthread. 
			# Should be coverd by the case above.
			# GCC 2.x and below needs -mthreads
			G_THREAD_CFLAGS="-mthreads"		
			G_THREAD_LIBS=$G_THREAD_CFLAGS
		else 
			# We are probably using the aix compiler. Normaly a 
			# program would have to be compiled with the _r variant
			# of the corresponding compiler, but we as GLib cannot 
			# do that: but the good news is that for compiling the
			# only difference is the added -D_THREAD_SAFE compile 
			# option. This is according to the "C for AIX User's 
			# Guide".
			G_THREAD_CFLAGS="-D_THREAD_SAFE"
		fi
		;;
	*-sysv5uw7*) # UnixWare 7 
		# We are not using gcc with -pthread. Catched above.
		G_THREAD_CFLAGS="-Kthread"
		G_THREAD_LIBS=$G_THREAD_CFLAGS
		;;
	*-mingw*)
		# No flag needed when using MSVCRT.DLL
		G_THREAD_CFLAGS=""
		;;
	*)
		G_THREAD_CFLAGS="-D_REENTRANT" # good default guess otherwise
		;;
  esac
])

# if we are not finding the localtime_r function, then we probably are
# not using the proper multithread flag

glib_save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $G_THREAD_CFLAGS"

# First we test, whether localtime_r is declared in time.h
# directly. Then we test whether a macro localtime_r exists, in
# which case localtime_r in the test program is replaced and thus
# if we still find localtime_r in the output, it is not defined as 
# a macro.

AC_EGREP_CPP([[^a-zA-Z1-9_]localtime_r[^a-zA-Z1-9_]], [#include <time.h>], ,
  [AC_EGREP_CPP([[^a-zA-Z1-9_]localtime_r[^a-zA-Z1-9_]], [#include <time.h> 
							   localtime_r(a,b)],
  		   AC_MSG_WARN($FLAG_DOES_NOT_WORK))])

CPPFLAGS="$glib_save_CPPFLAGS"

AC_MSG_CHECKING(thread related cflags)
AC_MSG_RESULT($G_THREAD_CFLAGS)
CPPFLAGS="$CPPFLAGS $G_THREAD_CFLAGS"

dnl determination of G_THREAD_LIBS
dnl ******************************

AS_IF([test x$have_threads = xposix], [
	  glib_save_CPPFLAGS="$CPPFLAGS"
	  CPPFLAGS="$CPPFLAGS $GTHREAD_COMPILE_IMPL_DEFINES"
          AS_IF([ test x"$G_THREAD_LIBS" = x ], [
            case $host in
              *-aix*)
                # We are not using gcc (would have set G_THREAD_LIBS) and thus 
                # probably using the aix compiler.
		AC_MSG_WARN($AIX_COMPILE_INFO)
                ;;
              *)
                G_THREAD_LIBS=error
	        glib_save_LIBS="$LIBS"
	        for thread_lib in "" pthread pthread32 pthreads thread; do
			if test x"$thread_lib" = x; then
				add_thread_lib=""
				IN=""
			else
				add_thread_lib="-l$thread_lib"
				IN=" in -l$thread_lib"
			fi
			if test x"$have_threads" = xposix; then
				defattr=0
			else
				defattr=pthread_attr_default
			fi
			
			LIBS="$add_thread_lib $glib_save_LIBS"
			
			AC_MSG_CHECKING(for pthread_create/pthread_join$IN)
			AC_TRY_RUN(glib_thread_test($defattr),
                                   glib_result=yes,
                                   glib_result=no,
                                   [AC_LINK_IFELSE([AC_LANG_SOURCE(glib_thread_test($defattr))],
                                                   glib_result=yes,
                                                   glib_result=no)])
                        AC_MSG_RESULT($glib_result)
			
                        if test "$glib_result" = "yes" ; then
			  G_THREAD_LIBS="$add_thread_lib"
                          break
                        fi
		done
		if test "x$G_THREAD_LIBS" = xerror; then
		  AC_MSG_ERROR($LIBS_NOT_FOUND_1$have_threads$LIBS_NOT_FOUND_2)
		fi 
		LIBS="$glib_save_LIBS"
                ;;
            esac
	  ])

	  g_threads_impl="POSIX"
          AC_DEFINE([THREADS_POSIX], [1], [Use pthreads])
	  AC_SUBST(GTHREAD_COMPILE_IMPL_DEFINES)
          CPPFLAGS="$glib_save_CPPFLAGS"
], [test x$have_threads = xwin32], [
           AC_DEFINE([THREADS_WIN32], [1], [Use w32 threads])
	   g_threads_impl="WIN32"
], [
           AC_DEFINE([THREADS_NONE], [1], [Use no threads])
	   g_threads_impl="NONE"
           G_THREAD_LIBS=error
])
AM_CONDITIONAL(THREADS_POSIX, [test "$g_threads_impl" = "POSIX"])
AM_CONDITIONAL(THREADS_WIN32, [test "$g_threads_impl" = "WIN32"])
AM_CONDITIONAL(THREADS_NONE, [test "$g_threads_impl" = "NONE"])

if test "x$G_THREAD_LIBS" = xerror; then
        AC_MSG_ERROR($LIBS_NOT_FOUND_1$have_threads$LIBS_NOT_FOUND_2)
fi

AC_MSG_CHECKING(thread related libraries)
AC_MSG_RESULT($G_THREAD_LIBS)

dnl check for mt safe function variants and some posix functions
dnl ************************************************************

glib_save_LIBS="$LIBS"
# we are not doing the following for now, as this might require glib 
# to always be linked with the thread libs on some platforms. 
# LIBS="$LIBS $G_THREAD_LIBS"
AC_CHECK_FUNCS(localtime_r gmtime_r getpwuid_r getgrgid_r)

LIBS="$G_THREAD_LIBS $LIBS"
AS_IF([ test x"$have_threads" = xposix], [
        glib_save_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $GTHREAD_COMPILE_IMPL_DEFINES"
        # This is not AC_CHECK_FUNC to also work with function
        # name mangling in header files.
        AC_MSG_CHECKING(for pthread_attr_setstacksize)
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
                [#include <pthread.h>],
                [pthread_attr_t t; pthread_attr_setstacksize(&t,0)])],
            [AC_MSG_RESULT(yes)
             AC_DEFINE(HAVE_PTHREAD_ATTR_SETSTACKSIZE,1,
                [Have function pthread_attr_setstacksize])],
            [AC_MSG_RESULT(no)])
        AC_MSG_CHECKING(for pthread_condattr_setclock)
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
                [#include <pthread.h>],
                [pthread_condattr_t a; pthread_condattr_setclock(&a,0)])],
            [AC_MSG_RESULT(yes)
             AC_DEFINE(HAVE_PTHREAD_CONDATTR_SETCLOCK,1,
                [Have function pthread_condattr_setclock])],
            [AC_MSG_RESULT(no)])
        AC_MSG_CHECKING(for pthread_cond_timedwait_relative_np)
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
                [#include <pthread.h>],
                [pthread_cond_timedwait_relative_np(NULL, NULL, NULL)])],
            [AC_MSG_RESULT(yes)
             AC_DEFINE(HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP,1,
                [Have function pthread_cond_timedwait_relative_np])],
            [AC_MSG_RESULT(no)])
        dnl Sets thread names on OS X 10.6, iOS 3.2 (and higher)
        AC_MSG_CHECKING(for pthread_setname_np(const char*))
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
                [#include <pthread.h>],
                [pthread_setname_np("example")])],
            [AC_MSG_RESULT(yes)
             AC_DEFINE(HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID,1,
                [Have function pthread_setname_np(const char*)])],
            [AC_MSG_RESULT(no)])
        dnl Sets thread names on Solaris 11.3 & higher
        AC_MSG_CHECKING(for pthread_setname_np(pthread_t, const char*))
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
                [#include <pthread.h>],
                [pthread_setname_np(pthread_self(), "example")])],
            [AC_MSG_RESULT(yes)
             AC_DEFINE(HAVE_PTHREAD_SETNAME_NP_WITH_TID,1,
                [Have function pthread_setname_np(pthread_t, const char*)])],
            [AC_MSG_RESULT(no)])
        AC_MSG_CHECKING(for pthread_getname_np(pthread_t, name, len))
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
                [#include <pthread.h>],
                [[char name[16]; pthread_getname_np(pthread_self(), name, 16);]])],
            [AC_MSG_RESULT(yes)
             AC_DEFINE(HAVE_PTHREAD_GETNAME_NP,1,
                [Have function pthread_getname_np(pthread_t, name, len)])],
            [AC_MSG_RESULT(no)])
        CPPFLAGS="$glib_save_CPPFLAGS"
])

LIBS="$glib_save_LIBS"

# now spit out all the warnings.
if test "$ac_cv_func_getpwuid_r" != "yes"; then
	AC_MSG_WARN($FUNC_NO_GETPWUID_R)
fi
if test "$ac_cv_func_localtime_r" != "yes"; then
	AC_MSG_WARN($FUNC_NO_LOCALTIME_R)
fi

#
# Hack to deal with:
# 
#  a) GCC < 3.3 for Linux doesn't include -lpthread when
#     building shared libraries with linux.
#  b) FreeBSD doesn't do this either.
#
case $host in
  *android*)
    G_THREAD_LIBS_FOR_GTHREAD="$G_THREAD_LIBS"
    ;;
  *-*-freebsd*|*-*-linux*)
    G_THREAD_LIBS_FOR_GTHREAD="`echo $G_THREAD_LIBS | sed s/-pthread/-lpthread/`"
    ;;
  *)
    G_THREAD_LIBS_FOR_GTHREAD="$G_THREAD_LIBS"
    ;;
esac

AC_SUBST(G_THREAD_CFLAGS)
AC_SUBST(G_THREAD_LIBS)
AC_SUBST(G_THREAD_LIBS_FOR_GTHREAD)
AC_SUBST(G_THREAD_LIBS_EXTRA)

AC_CHECK_FUNCS(clock_gettime, [], [
  AC_CHECK_LIB(rt, clock_gettime, [
    G_THREAD_LIBS="$G_THREAD_LIBS -lrt"
    G_THREAD_LIBS_FOR_GTHREAD="$G_THREAD_LIBS_FOR_GTHREAD -lrt"
  ])
])


dnl ************************
dnl *** g_atomic_* tests ***
dnl ************************

dnl We need to decide at configure time if GLib will use real atomic
dnl operations ("lock free") or emulated ones with a mutex.  This is
dnl because we must put this information in glibconfig.h so we know if
dnl it is safe or not to inline using compiler intrinsics directly from
dnl the header.
dnl
dnl We also publish the information via G_ATOMIC_LOCK_FREE in case the
dnl user is interested in knowing if they can use the atomic ops across
dnl processes.
dnl
dnl We can currently support the atomic ops natively when building GLib
dnl with recent versions of GCC or MSVC.  MSVC doesn't run ./configure,
dnl so we skip that case here and define G_ATOMIC_LOCK_FREE exactly when
dnl we are using GCC (and not mingw*).
dnl
dnl Note that the atomic ops are only available with GCC on x86 when
dnl using -march=i486 or higher.  If we detect that the atomic ops are
dnl not available but would be available given the right flags, we want
dnl to abort and advise the user to fix their CFLAGS.  It's better to do
dnl that then to silently fall back on emulated atomic ops just because
dnl the user had the wrong build environment.

dnl We may add other compilers here in the future...

AC_CACHE_CHECK([for lock-free atomic intrinsics], glib_cv_g_atomic_lock_free, [
  case $host in
    *-*-mingw*)
      glib_cv_g_atomic_lock_free=yes
      ;;
    *)
      AC_TRY_LINK([],
                     [volatile int atomic = 2;\
                      __sync_bool_compare_and_swap (&atomic, 2, 3);],
                     [glib_cv_g_atomic_lock_free=yes],
                     [glib_cv_g_atomic_lock_free=no])
      if test "$glib_cv_g_atomic_lock_free" = "no"; then
        SAVE_CFLAGS="${CFLAGS}"
        CFLAGS="-march=i486"
        AC_TRY_LINK([],
                       [volatile int atomic = 2;\
                        __sync_bool_compare_and_swap (&atomic, 2, 3);],
                       [AC_MSG_ERROR([GLib must be built with -march=i486 or later.])],
                       [])
        CFLAGS="${SAVE_CFLAGS}"
      fi
      ;;
  esac])


case $host in
  *-*-mingw*)
    ;;
  *)
    # Some compilers support atomic operations but do not define
    # __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4, like clang
    if test x"$glib_cv_g_atomic_lock_free" = xyes; then
      AC_TRY_LINK([],
                     [__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4;],
                     [],
                     [AC_DEFINE(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4, 1, [ compiler supports atomic operations])])
    fi
    ;;
 esac

dnl We need a more robust approach here...
case $host_cpu in
  i?86|x86_64|s390|s390x|arm*|crisv32*|etrax*)
    glib_memory_barrier_needed=no
    ;;
  sparc*|alpha*|powerpc*|ia64)
    glib_memory_barrier_needed=yes
    ;;
  *)
    glib_memory_barrier_needed=yes
    ;;
esac

dnl ************************
dnl ** Check for futex(2) **
dnl ************************
AC_CACHE_CHECK(for futex(2) system call,
    glib_cv_futex,AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
],[
  syscall (__NR_futex, NULL, FUTEX_WAKE, FUTEX_WAIT);
])],glib_cv_futex=yes,glib_cv_futex=no))
if test x"$glib_cv_futex" = xyes; then
  AC_DEFINE(HAVE_FUTEX, 1, [we have the futex(2) system call])
fi

AC_CACHE_CHECK(for eventfd(2) system call,
    glib_cv_eventfd,AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <sys/eventfd.h>
#include <unistd.h>
],[
  eventfd (0, EFD_CLOEXEC);
])],glib_cv_eventfd=yes,glib_cv_eventfd=no))
if test x"$glib_cv_eventfd" = x"yes"; then
  AC_DEFINE(HAVE_EVENTFD, 1, [we have the eventfd(2) system call])
fi
AM_CONDITIONAL(HAVE_EVENTFD, [test "$glib_cv_eventfd" = "yes"])

dnl ****************************************
dnl *** GLib POLL* compatibility defines ***
dnl ****************************************

glib_poll_includes=["
#include <sys/types.h>
#include <poll.h>
"]

AS_IF([ test $ac_cv_header_sys_types_h = yes &&
   test $ac_cv_func_poll = yes ], [
  glib_failed=false
  GLIB_CHECK_VALUE(POLLIN, $glib_poll_includes, glib_failed=true)
  GLIB_CHECK_VALUE(POLLOUT, $glib_poll_includes, glib_failed=true)
  GLIB_CHECK_VALUE(POLLPRI, $glib_poll_includes, glib_failed=true)
  GLIB_CHECK_VALUE(POLLERR, $glib_poll_includes, glib_failed=true)
  GLIB_CHECK_VALUE(POLLHUP, $glib_poll_includes, glib_failed=true)
  GLIB_CHECK_VALUE(POLLNVAL, $glib_poll_includes, glib_failed=true)
  if $glib_failed ; then
    AC_MSG_ERROR([Could not determine values for POLL* constants])
  fi
], [
  glib_cv_value_POLLIN=1
  glib_cv_value_POLLOUT=4
  glib_cv_value_POLLPRI=2
  glib_cv_value_POLLERR=8
  glib_cv_value_POLLHUP=16
  glib_cv_value_POLLNVAL=32
])

AC_MSG_CHECKING([for broken poll])
AC_RUN_IFELSE([AC_LANG_SOURCE([[
	#include <stdlib.h>
	#include <fcntl.h>
	#include <poll.h>
	int main(void) {
	  struct pollfd fds[1];
	  int fd;
	  fd = open("/dev/null", 1);
	  fds[0].fd = fd;
	  fds[0].events = POLLIN;
	  fds[0].revents = 0;
	  if (poll(fds, 1, 0) < 0 || (fds[0].revents & POLLNVAL) != 0) {
		exit(1);  /* Does not work for devices -- fail */
	  }
	  exit(0);
	}]])],
  [broken_poll=no],
  [broken_poll=yes
   AC_DEFINE(BROKEN_POLL,1,[poll doesn't work on devices])],
  [broken_poll="no (cross compiling)"])
AC_MSG_RESULT($broken_poll)

dnl *********************
dnl *** GRegex checks ***
dnl *********************

PCRE_REQUIRED_VERSION=8.13

# Check if we should use the internal or the system-supplied pcre
AC_ARG_WITH(pcre,
            [AS_HELP_STRING([--with-pcre=@<:@internal/system@:>@],
                            [whether to use system PCRE [default=system]])],
            [],
            [with_pcre=system])

AM_CONDITIONAL(USE_SYSTEM_PCRE, [test "x$with_pcre" = xsystem])

AS_IF([ test "x$with_pcre" = xsystem], [
  PKG_CHECK_MODULES(PCRE,
                    libpcre >= $PCRE_REQUIRED_VERSION)
  AC_CACHE_CHECK([for Unicode support in PCRE],glib_cv_pcre_has_unicode,[
                  glib_save_CFLAGS="$CFLAGS"
                  glib_save_LIBS="$LIBS"
                  CFLAGS="$CFLAGS $PCRE_CFLAGS" LIBS="$PCRE_LIBS"
                  AC_TRY_RUN([#include <pcre.h>
                              int main () {
                                int support;
                                pcre_config (PCRE_CONFIG_UTF8, &support);
                                if (!support)
                                  return 1;
                                pcre_config (PCRE_CONFIG_UNICODE_PROPERTIES, &support);
                                if (!support)
                                  return 1;
                                return 0;
                              }],
                  glib_cv_pcre_has_unicode=yes,
                  glib_cv_pcre_has_unicode=no,
                  glib_cv_pcre_has_unicode=yes)
                  CFLAGS="$glib_save_CFLAGS"
                  LIBS="$glib_save_LIBS"
      ])
  if test "$glib_cv_pcre_has_unicode" = "no"; then
    AC_MSG_ERROR([*** The system-supplied PCRE does not support Unicode properties or UTF-8.])
  fi
  AC_SUBST(PCRE_CFLAGS)
  AC_SUBST(PCRE_LIBS)
  AC_DEFINE(USE_SYSTEM_PCRE, [], [using the system-supplied PCRE library])
  PCRE_REQUIRES=libpcre
  AC_SUBST(PCRE_REQUIRES)
], [
  # If using gcc 4 pass -Wno-pointer-sign when compiling the internal PCRE
  AS_IF([ test x"$GCC" = xyes], [
    AC_MSG_CHECKING([whether compiler understands -Wno-pointer-sign])
    save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -Wno-pointer-sign"
    AC_TRY_COMPILE([],[],[PCRE_WARN_CFLAGS="$PCRE_WARN_CFLAGS -Wno-pointer-sign"
                          AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    CFLAGS="$save_CFLAGS"
  ])
])
AC_SUBST(PCRE_WARN_CFLAGS)

dnl **********************
dnl *** Win32 API libs ***
dnl **********************

case $host in
  *-*-cygwin*)
	G_LIBS_EXTRA="-luser32 -lkernel32"
    ;;
  *-*-mingw*)
	G_LIBS_EXTRA="-lws2_32 -lole32 -lwinmm -lshlwapi"
    ;;
  *)
	G_LIBS_EXTRA=""
    ;;
esac
AC_SUBST(G_LIBS_EXTRA)

dnl If the system doesn't define EILSEQ, we should define EILSEQ ourselves
dnl since we need it for g_iconv()

AC_MSG_CHECKING([for EILSEQ])
AC_TRY_COMPILE([
#include <errno.h>
],
[
int error = EILSEQ;
], have_eilseq=yes, have_eilseq=no);
AC_MSG_RESULT($have_eilseq)

dnl Add a conditional we can use when cross-compiling, so we avoid running
dnl binaries
AM_CONDITIONAL(CROSS_COMPILING, test $cross_compiling = yes)

dnl **************************
dnl *** Checks for gtk-doc ***
dnl **************************
# gtkdocize greps for ^GTK_DOC_CHECK and parses it, so you need to have
# it on it's own line.
m4_ifdef([GTK_DOC_CHECK], [
GTK_DOC_CHECK([1.20], [--flavour no-tmpl])
],[
AM_CONDITIONAL([ENABLE_GTK_DOC],[false])
])

AC_ARG_ENABLE(man,
              [AS_HELP_STRING([--enable-man],
                              [generate man pages [default=auto]])],,
              enable_man=maybe)

AS_IF([test "$enable_man" != no], [
  AC_PATH_PROG([XSLTPROC], [xsltproc])
  AS_IF([test -z "$XSLTPROC"], [
    AS_IF([test "$enable_man" = yes], [
      AC_MSG_ERROR([xsltproc is required for --enable-man])
    ])
    enable_man=no
  ])
])

AS_IF([ test "$enable_man" != no ], [
  dnl check for DocBook DTD in the local catalog
  JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.1.2//EN],
     [DocBook XML DTD V4.1.2], [have_docbook_dtd=yes], [have_docbook_dtd=no])
  AS_IF([test "$have_docbook_dtd" != yes], [
    AS_IF([test "$enable_man" = yes ], [
      AC_MSG_ERROR([DocBook DTD is required for --enable-man])
    ])
    enable_man=no
  ])
])

AS_IF([test "$enable_man" != no], [
  dnl check for DocBook XSL stylesheets in the local catalog
  JH_CHECK_XML_CATALOG([http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl],
     [DocBook XSL Stylesheets], [have_docbook_style=yes],[have_docbook_style=no])
  AS_IF([ test "$have_docbook_style" != yes ], [
    AS_IF([ test "$enable_man" = yes ], [
      AC_MSG_ERROR([DocBook XSL Stylesheets are required for --enable-man])
    ])
    enable_man=no
  ])
])

AM_CONDITIONAL(ENABLE_MAN, test "$enable_man" != no)

AC_MSG_CHECKING([whether to generate man pages])
AS_IF([ test "$enable_man" != no ], [
  AC_MSG_RESULT([yes])
], [
  AC_MSG_RESULT([no])
])

dnl
dnl Tracing
dnl

AC_ARG_ENABLE([dtrace],
             [AS_HELP_STRING([--enable-dtrace],
                              [include tracing support for dtrace])])
have_dtrace=no
AC_MSG_CHECKING([whether to include dtrace tracing support])
AS_IF([ test "x$enable_dtrace" != xno], [
  if test x$glib_have_carbon = xyes; then
    AC_MSG_RESULT([no (not yet compatible with MacOS dtrace)])
  else 
    AC_MSG_RESULT([yes])
    AC_CHECK_PROGS(DTRACE, dtrace)
    if test -z "$DTRACE"; then
      if test "x$enable_dtrace" = xyes; then
        AC_MSG_ERROR([dtrace not found])
      fi
    else
      AC_CHECK_HEADER([sys/sdt.h],have_dtrace=yes,
                      [if test "x$enable_dtrace" = xyes; then
                        AC_MSG_ERROR([dtrace support needs sys/sdt.h header])
                       fi])
    fi
  fi
], [
  AC_MSG_RESULT([no])
])
if test "x$have_dtrace" = xyes; then
  AC_DEFINE([HAVE_DTRACE], [1], [Define to 1 if using dtrace probes.])
fi
AM_CONDITIONAL([ENABLE_DTRACE], [test x$have_dtrace = xyes ])

AC_MSG_CHECKING([whether to include systemtap tracing support])
AC_ARG_ENABLE([systemtap],
             [AS_HELP_STRING([--enable-systemtap],
                              [include tracing support for systemtap])])
have_systemtap=no
if test "x$enable_systemtap" != xno -a "x$have_dtrace" = xyes; then
  have_systemtap=yes
fi
AC_MSG_RESULT(${have_systemtap})

AM_CONDITIONAL([ENABLE_SYSTEMTAP], [test x$have_systemtap = xyes])

AC_ARG_WITH([tapset-install-dir],
            AS_HELP_STRING([--with-tapset-install-dir=DIR],
                            [path where systemtap tapsets are installed [DATADIR/systemtap/tapset/HOST_CPU]]),
            [if test "x${withval}" = x; then
	       ABS_TAPSET_DIR="\$(datadir)/systemtap/tapset/${host_cpu}"
	     else
	       ABS_TAPSET_DIR="${withval}"
	      fi],
	    [ABS_TAPSET_DIR="\$(datadir)/systemtap/tapset/${host_cpu}"])
AC_SUBST(ABS_TAPSET_DIR)

dnl ************************************
dnl *** Enable lcov coverage reports ***
dnl ************************************

AC_ARG_ENABLE(coverage,
  AS_HELP_STRING([--enable-coverage],
		 [enable coverage testing with gcov]),
  [use_gcov=$enableval], [use_gcov=no])

AS_IF([ test "x$use_gcov" = "xyes"], [
  dnl we need gcc:
  if test "$GCC" != "yes"; then
    AC_MSG_ERROR([GCC is required for --enable-coverage])
  fi

  dnl Check if ccache is being used
  AC_CHECK_PROG(SHTOOL, shtool, shtool)
  case `$SHTOOL path $CC` in
    *ccache*[)] gcc_ccache=yes;;
    *[)] gcc_ccache=no;;
  esac

  if test "$gcc_ccache" = "yes" && (test -z "$CCACHE_DISABLE" || test "$CCACHE_DISABLE" != "1"); then
    AC_MSG_ERROR([ccache must be disabled when --enable-coverage option is used. You can disable ccache by setting environment variable CCACHE_DISABLE=1.])
  fi

  ltp_version_list="1.6 1.7 1.8 1.9 1.10 1.12"
  AC_CHECK_PROG(LTP, lcov, lcov)
  AC_CHECK_PROG(LTP_GENHTML, genhtml, genhtml)

  AS_IF([ test "$LTP" ], [
    AC_CACHE_CHECK([for ltp version], glib_cv_ltp_version, [
      glib_cv_ltp_version=invalid
      ltp_version=`$LTP -v 2>/dev/null | $SED -e 's/^.* //'`
      for ltp_check_version in $ltp_version_list; do
        if test "$ltp_version" = "$ltp_check_version"; then
          glib_cv_ltp_version="$ltp_check_version (ok)"
        fi
      done
    ])
  ], [
    ltp_msg="To enable code coverage reporting you must have one of the following LTP versions installed: $ltp_version_list"
    AC_MSG_ERROR([$ltp_msg])
  ])

  case $glib_cv_ltp_version in
    ""|invalid[)]
      ltp_msg="You must have one of the following versions of LTP: $ltp_version_list (found: $ltp_version)."
      AC_MSG_ERROR([$ltp_msg])
      LTP="exit 0;"
      ;;
  esac

  if test -z "$LTP_GENHTML"; then
    AC_MSG_ERROR([Could not find genhtml from the LTP package])
  fi

  dnl Remove all optimization flags from CFLAGS
  changequote({,})
  CFLAGS=`echo "$CFLAGS" | $SED -e 's/-O[0-9]*//g'`
  changequote([,])

  dnl Add the special gcc flags
  CFLAGS="$CFLAGS -O0 -fprofile-arcs -ftest-coverage"
  LDFLAGS="$LDFLAGS -lgcov"
])

dnl *******************************
dnl *** Disable strict aliasing ***
dnl *******************************
dnl See https://bugzilla.gnome.org/show_bug.cgi?id=791622
AS_IF([test "${GCC}" = "yes"],[
  CFLAGS="$CFLAGS -fno-strict-aliasing"
])

dnl ******************************
dnl *** output the whole stuff ***
dnl ******************************

dnl this section will only be run if config.status is invoked with no
dnl arguments, or with "glib/glibconfig.h" as an argument.
AC_CONFIG_COMMANDS([glib/glibconfig.h],
[
	outfile=glib/glibconfig.h-tmp
	cat > $outfile <<\_______EOF
/* glibconfig.h
 *
 * This is a generated file.  Please modify 'configure.ac'
 */

#ifndef __GLIBCONFIG_H__
#define __GLIBCONFIG_H__

#include <glib/gmacros.h>

_______EOF

	echo '#include <limits.h>' >> $outfile
	echo '#include <float.h>' >> $outfile
	if test x$glib_values_h = xyes; then
	  echo '#include <values.h>' >> $outfile
	fi
	if test "$glib_header_alloca_h" = "yes"; then
	  echo '#define GLIB_HAVE_ALLOCA_H' >> $outfile
	fi
	if test x$glib_included_printf != xyes; then
          echo "
/* Specifies that GLib's g_print*() functions wrap the
 * system printf functions.  This is useful to know, for example,
 * when using glibc's register_printf_function().
 */" >> $outfile
	  echo '#define GLIB_USING_SYSTEM_PRINTF' >> $outfile
	fi

	cat >> $outfile <<_______EOF

G_BEGIN_DECLS

#define G_MINFLOAT	FLT_MIN
#define G_MAXFLOAT	FLT_MAX
#define G_MINDOUBLE	DBL_MIN
#define G_MAXDOUBLE	DBL_MAX
#define G_MINSHORT	SHRT_MIN
#define G_MAXSHORT	SHRT_MAX
#define G_MAXUSHORT	USHRT_MAX
#define G_MININT	INT_MIN
#define G_MAXINT	INT_MAX
#define G_MAXUINT	UINT_MAX
#define G_MINLONG	LONG_MIN
#define G_MAXLONG	LONG_MAX
#define G_MAXULONG	ULONG_MAX

_______EOF


	### this should always be true in a modern C/C++ compiler
	### and is statically asserted by glib-init.c
	cat >>$outfile <<_______EOF
typedef signed char gint8;
typedef unsigned char guint8;
_______EOF


	if test -n "$gint16"; then
	  cat >>$outfile <<_______EOF
typedef signed $gint16 gint16;
typedef unsigned $gint16 guint16;
#define G_GINT16_MODIFIER $gint16_modifier
#define G_GINT16_FORMAT $gint16_format
#define G_GUINT16_FORMAT $guint16_format
_______EOF
	fi


	if test -n "$gint32"; then
	  cat >>$outfile <<_______EOF
typedef signed $gint32 gint32;
typedef unsigned $gint32 guint32;
#define G_GINT32_MODIFIER $gint32_modifier
#define G_GINT32_FORMAT $gint32_format
#define G_GUINT32_FORMAT $guint32_format
_______EOF
	fi

	cat >>$outfile <<_______EOF
#define G_HAVE_GINT64 1          /* deprecated, always true */

${glib_extension}typedef signed $gint64 gint64;
${glib_extension}typedef unsigned $gint64 guint64;

#define G_GINT64_CONSTANT(val)	$gint64_constant
#define G_GUINT64_CONSTANT(val)	$guint64_constant
_______EOF

	if test x$gint64_format != x ; then
	  cat >>$outfile <<_______EOF
#define G_GINT64_MODIFIER $gint64_modifier
#define G_GINT64_FORMAT $gint64_format
#define G_GUINT64_FORMAT $guint64_format
_______EOF
        else
	  cat >>$outfile <<_______EOF
#undef G_GINT64_MODIFIER
#undef G_GINT64_FORMAT
#undef G_GUINT64_FORMAT
_______EOF
        fi           

        cat >>$outfile <<_______EOF

#define GLIB_SIZEOF_VOID_P $glib_void_p
#define GLIB_SIZEOF_LONG   $glib_long
#define GLIB_SIZEOF_SIZE_T $glib_size_t
#define GLIB_SIZEOF_SSIZE_T $glib_ssize_t

_______EOF

        cat >>$outfile <<_______EOF
typedef signed $glib_ssize_type_define gssize;
typedef unsigned $glib_size_type_define gsize;
#define G_GSIZE_MODIFIER $gsize_modifier
#define G_GSSIZE_MODIFIER $gssize_modifier
#define G_GSIZE_FORMAT $gsize_format
#define G_GSSIZE_FORMAT $gssize_format

#define G_MAXSIZE	G_MAXU$glib_msize_type
#define G_MINSSIZE	G_MIN$glib_mssize_type
#define G_MAXSSIZE	G_MAX$glib_mssize_type

typedef gint64 goffset;
#define G_MINOFFSET	G_MININT64
#define G_MAXOFFSET	G_MAXINT64

#define G_GOFFSET_MODIFIER      G_GINT64_MODIFIER
#define G_GOFFSET_FORMAT        G_GINT64_FORMAT
#define G_GOFFSET_CONSTANT(val) G_GINT64_CONSTANT(val)

#define G_POLLFD_FORMAT $g_pollfd_format

_______EOF

	if test -z "$glib_unknown_void_p"; then
	  cat >>$outfile <<_______EOF

#define GPOINTER_TO_INT(p)	((gint)  ${glib_gpi_cast} (p))
#define GPOINTER_TO_UINT(p)	((guint) ${glib_gpui_cast} (p))

#define GINT_TO_POINTER(i)	((gpointer) ${glib_gpi_cast} (i))
#define GUINT_TO_POINTER(u)	((gpointer) ${glib_gpui_cast} (u))

typedef signed $glib_intptr_type_define gintptr;
typedef unsigned $glib_intptr_type_define guintptr;

#define G_GINTPTR_MODIFIER      $gintptr_modifier
#define G_GINTPTR_FORMAT        $gintptr_format
#define G_GUINTPTR_FORMAT       $guintptr_format
_______EOF
	else
	  echo '#error SIZEOF_VOID_P unknown - This should never happen' >>$outfile
	fi



	cat >>$outfile <<_______EOF
#ifndef G_DISABLE_DEPRECATED
#define g_ATEXIT(proc)	(atexit (proc))
#define g_memmove(dest,src,len) G_STMT_START { memmove ((dest), (src), (len)); } G_STMT_END
#endif
$glib_defines
$glib_os
$glib_static_compilation

$glib_vacopy

_______EOF

	if test x$g_have_iso_c_varargs = xyes ; then
		cat >>$outfile <<_______EOF
#ifndef __cplusplus
# define G_HAVE_ISO_VARARGS 1
#endif
_______EOF
	fi
	if test x$g_have_iso_cxx_varargs = xyes ; then
		cat >>$outfile <<_______EOF
#ifdef __cplusplus
# define G_HAVE_ISO_VARARGS 1
#endif
_______EOF
	fi
	if test x$g_have_gnuc_varargs = xyes ; then
		cat >>$outfile <<_______EOF

/* gcc-2.95.x supports both gnu style and ISO varargs, but if -ansi
 * is passed ISO vararg support is turned off, and there is no work
 * around to turn it on, so we unconditionally turn it off.
 */
#if __GNUC__ == 2 && __GNUC_MINOR__ == 95
#  undef G_HAVE_ISO_VARARGS
#endif

#define G_HAVE_GNUC_VARARGS 1
_______EOF
	fi

	case x$g_stack_grows in
	xyes) echo "#define G_HAVE_GROWING_STACK 1" >>$outfile ;;
	*)    echo "#define G_HAVE_GROWING_STACK 0" >>$outfile ;;
	esac


	echo >>$outfile
	if test x$g_have_eilseq = xno; then
		cat >>$outfile <<_______EOF
#ifndef EILSEQ
/* On some pre-C99 systems, EILSEQ is not defined.
 * The correspondence between this and the corresponding definition
 * in libiconv is essential.
 */
#  define EILSEQ ENOENT
#endif
_______EOF

	fi

	if test x$g_have_gnuc_visibility = xyes; then
		cat >>$outfile <<_______EOF
#define G_HAVE_GNUC_VISIBILITY 1
_______EOF
	fi
		cat >>$outfile <<_______EOF
#if defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define G_GNUC_INTERNAL __attribute__((visibility("hidden")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
#define G_GNUC_INTERNAL __hidden
#elif defined (__GNUC__) && defined (G_HAVE_GNUC_VISIBILITY)
#define G_GNUC_INTERNAL __attribute__((visibility("hidden")))
#else
#define G_GNUC_INTERNAL
#endif 
_______EOF

	echo >>$outfile
	cat >>$outfile <<_______EOF
#define G_THREADS_ENABLED
#define G_THREADS_IMPL_$g_threads_impl_def
_______EOF

	if test x"$g_memory_barrier_needed" != xno; then
	  echo >>$outfile
	  echo "#define G_ATOMIC_OP_MEMORY_BARRIER_NEEDED 1" >>$outfile
	fi
	if test x"$g_atomic_lock_free" = xyes; then
          echo >>$outfile
          echo "#define G_ATOMIC_LOCK_FREE" >>$outfile
        fi
	echo >>$outfile
	g_bit_sizes="16 32 64"
	for bits in $g_bit_sizes; do
	  cat >>$outfile <<_______EOF
#define GINT${bits}_TO_${g_bs_native}(val)	((gint${bits}) (val))
#define GUINT${bits}_TO_${g_bs_native}(val)	((guint${bits}) (val))
#define GINT${bits}_TO_${g_bs_alien}(val)	((gint${bits}) GUINT${bits}_SWAP_LE_BE (val))
#define GUINT${bits}_TO_${g_bs_alien}(val)	(GUINT${bits}_SWAP_LE_BE (val))
_______EOF
	done

	cat >>$outfile <<_______EOF
#define GLONG_TO_LE(val)	((glong) GINT${glongbits}_TO_LE (val))
#define GULONG_TO_LE(val)	((gulong) GUINT${glongbits}_TO_LE (val))
#define GLONG_TO_BE(val)	((glong) GINT${glongbits}_TO_BE (val))
#define GULONG_TO_BE(val)	((gulong) GUINT${glongbits}_TO_BE (val))
#define GINT_TO_LE(val)		((gint) GINT${gintbits}_TO_LE (val))
#define GUINT_TO_LE(val)	((guint) GUINT${gintbits}_TO_LE (val))
#define GINT_TO_BE(val)		((gint) GINT${gintbits}_TO_BE (val))
#define GUINT_TO_BE(val)	((guint) GUINT${gintbits}_TO_BE (val))
#define GSIZE_TO_LE(val)	((gsize) GUINT${gsizebits}_TO_LE (val))
#define GSSIZE_TO_LE(val)	((gssize) GINT${gssizebits}_TO_LE (val))
#define GSIZE_TO_BE(val)	((gsize) GUINT${gsizebits}_TO_BE (val))
#define GSSIZE_TO_BE(val)	((gssize) GINT${gssizebits}_TO_BE (val))
#define G_BYTE_ORDER $g_byte_order

#define GLIB_SYSDEF_POLLIN =$g_pollin
#define GLIB_SYSDEF_POLLOUT =$g_pollout
#define GLIB_SYSDEF_POLLPRI =$g_pollpri
#define GLIB_SYSDEF_POLLHUP =$g_pollhup
#define GLIB_SYSDEF_POLLERR =$g_pollerr
#define GLIB_SYSDEF_POLLNVAL =$g_pollnval

#define G_MODULE_SUFFIX "$g_module_suffix"

typedef $g_pid_type GPid;
#define G_PID_FORMAT $g_pid_format

#define GLIB_SYSDEF_AF_UNIX $g_af_unix
#define GLIB_SYSDEF_AF_INET $g_af_inet
#define GLIB_SYSDEF_AF_INET6 $g_af_inet6

#define GLIB_SYSDEF_MSG_OOB $g_msg_oob
#define GLIB_SYSDEF_MSG_PEEK $g_msg_peek
#define GLIB_SYSDEF_MSG_DONTROUTE $g_msg_dontroute

#define G_DIR_SEPARATOR '$g_dir_separator'
#define G_DIR_SEPARATOR_S "$g_dir_separator"
#define G_SEARCHPATH_SEPARATOR '$g_searchpath_separator'
#define G_SEARCHPATH_SEPARATOR_S "$g_searchpath_separator"

G_END_DECLS

#endif /* __GLIBCONFIG_H__ */
_______EOF


	if cmp -s $outfile glib/glibconfig.h; then
	  AC_MSG_NOTICE([glib/glibconfig.h is unchanged])
	  rm -f $outfile
	else
	  mv $outfile glib/glibconfig.h
	fi
],[

# Note that if two cases are the same, case goes with the first one.
# Note also that this is inside an AC_OUTPUT_COMMAND.  We do not depend
# on variable expansion in case labels.  Look at the generated config.status
# for a hint.

if test "x${ac_cv_working_alloca_h+set}" = xset ; then
  glib_header_alloca_h="$ac_cv_working_alloca_h"
else
  glib_header_alloca_h="$ac_cv_header_alloca_h"
fi

if test x$enable_included_printf = xyes ; then
  glib_included_printf=yes
fi

case 2 in
$ac_cv_sizeof_short)		
  gint16=short
  gint16_modifier='"h"'
  gint16_format='"hi"'
  guint16_format='"hu"'
  ;;
$ac_cv_sizeof_int)		
  gint16=int
  gint16_modifier='""'
  gint16_format='"i"'
  guint16_format='"u"'
  ;;
esac
case 4 in
$ac_cv_sizeof_short)		
  gint32=short
  gint32_modifier='"h"'
  gint32_format='"hi"'
  guint32_format='"hu"'
  ;;
$ac_cv_sizeof_int)		
  gint32=int
  gint32_modifier='""'
  gint32_format='"i"'
  guint32_format='"u"'
  ;;
$ac_cv_sizeof_long)		
  gint32=long
  gint32_modifier='"l"'
  gint32_format='"li"'
  guint32_format='"lu"'
  ;;
esac
case 8 in
$ac_cv_sizeof_int)
  gint64=int
  gint64_modifier='""'
  gint64_format='"i"'
  guint64_format='"u"'
  glib_extension=
  gint64_constant='(val)'
  guint64_constant='(val)'
  ;;
$ac_cv_sizeof_long)
  gint64=long
  gint64_modifier='"l"'
  gint64_format='"li"'
  guint64_format='"lu"'
  glib_extension=
  gint64_constant='(val##L)'
  guint64_constant='(val##UL)'
  ;;
$ac_cv_sizeof_long_long)
  gint64='long long'
  if test -n "$glib_cv_long_long_format"; then
    gint64_modifier='"'$glib_cv_long_long_format'"'
    gint64_format='"'$glib_cv_long_long_format'i"'
    guint64_format='"'$glib_cv_long_long_format'u"'
  fi
  glib_extension='G_GNUC_EXTENSION '
  gint64_constant='(G_GNUC_EXTENSION (val##LL))'
  guint64_constant='(G_GNUC_EXTENSION (val##ULL))'
  ;;
$ac_cv_sizeof___int64)
  gint64='__int64'
  if test -n "$glib_cv_long_long_format"; then
    gint64_modifier='"'$glib_cv_long_long_format'"'
    gint64_format='"'$glib_cv_long_long_format'i"'
    guint64_format='"'$glib_cv_long_long_format'u"'
  fi
  glib_extension=
  gint64_constant='(val##i64)'
  guint64_constant='(val##ui64)'
  ;;
esac
glib_size_t=$ac_cv_sizeof_size_t
glib_ssize_t=$ac_cv_sizeof_ssize_t
glib_size_type_define="$glib_size_type"
glib_ssize_type_define="$glib_ssize_type"
glib_void_p=$ac_cv_sizeof_void_p
glib_long=$ac_cv_sizeof_long

case "$glib_size_type" in
short)
  gsize_modifier='"h"'
  gsize_format='"hu"'
  glib_msize_type='SHRT'
  ;;
int)
  gsize_modifier='""'
  gsize_format='"u"'
  glib_msize_type='INT'
  ;;
long)
  gsize_modifier='"l"'
  gsize_format='"lu"'
  glib_msize_type='LONG'
  ;;
"long long"|__int64)
  gsize_modifier='"I64"'
  gsize_format='"I64u"'
  glib_msize_type='INT64'
  ;;
esac

case "$glib_ssize_type" in
short)
  gssize_modifier='"h"'
  gssize_format='"hi"'
  glib_mssize_type='SHRT'
  ;;
int)
  gssize_modifier='""'
  gssize_format='"i"'
  glib_mssize_type='INT'
  ;;
long)
  gssize_modifier='"l"'
  gssize_format='"li"'
  glib_mssize_type='LONG'
  ;;
"long long"|__int64)
  gssize_modifier='"I64"'
  gssize_format='"I64i"'
  glib_mssize_type='INT64'
  ;;
esac

gintbits=`expr $ac_cv_sizeof_int \* 8`
glongbits=`expr $ac_cv_sizeof_long \* 8`
gsizebits=`expr $ac_cv_sizeof_size_t \* 8`
gssizebits=`expr $ac_cv_sizeof_ssize_t \* 8`

case $ac_cv_sizeof_void_p in
$ac_cv_sizeof_int)
  glib_intptr_type_define=int
  gintptr_modifier='""'
  gintptr_format='"i"'
  guintptr_format='"u"'
  glib_gpi_cast='(gint)'
  glib_gpui_cast='(guint)'
  ;;
$ac_cv_sizeof_long)
  glib_intptr_type_define=long
  gintptr_modifier='"l"'
  gintptr_format='"li"'
  guintptr_format='"lu"'
  glib_gpi_cast='(glong)'
  glib_gpui_cast='(gulong)'
  ;;
$ac_cv_sizeof_long_long)
  glib_intptr_type_define='long long'
  gintptr_modifier='"I64"'
  gintptr_format='"I64i"'
  guintptr_format='"I64u"'
  glib_gpi_cast='(gint64)'
  glib_gpui_cast='(guint64)'
  ;;
$ac_cv_sizeof___int64)
  glib_intptr_type_define=__int64
  gintptr_modifier='"I64"'
  gintptr_format='"I64i"'
  guintptr_format='"I64u"'
  glib_gpi_cast='(gint64)'
  glib_gpui_cast='(guint64)'
  ;;
*)
  glib_unknown_void_p=yes
  ;;
esac


glib_defines="
#define GLIB_MAJOR_VERSION $GLIB_MAJOR_VERSION
#define GLIB_MINOR_VERSION $GLIB_MINOR_VERSION
#define GLIB_MICRO_VERSION $GLIB_MICRO_VERSION
"

case xyes in
x$glib_cv_va_copy)	glib_vacopy='#define G_VA_COPY	va_copy' ;;
x$glib_cv___va_copy)	glib_vacopy='#define G_VA_COPY	__va_copy' ;;
*)			glib_vacopy=''
esac

if test x$glib_cv_va_val_copy = xno; then
  glib_vacopy="\$glib_vacopy
#define G_VA_COPY_AS_ARRAY 1"
fi

g_have_gnuc_varargs=$g_have_gnuc_varargs
g_have_iso_c_varargs=$g_have_iso_c_varargs
g_have_iso_cxx_varargs=$g_have_iso_cxx_varargs

g_have_gnuc_visibility=$g_have_gnuc_visibility
g_have_sunstudio_visibility=$g_have_sunstudio_visibility

if test x$ac_cv_c_bigendian = xyes; then
  g_byte_order=G_BIG_ENDIAN
  g_bs_native=BE
  g_bs_alien=LE
else
  g_byte_order=G_LITTLE_ENDIAN
  g_bs_native=LE
  g_bs_alien=BE
fi

g_pollin=$glib_cv_value_POLLIN
g_pollout=$glib_cv_value_POLLOUT
g_pollpri=$glib_cv_value_POLLPRI
g_pollhup=$glib_cv_value_POLLHUP
g_pollerr=$glib_cv_value_POLLERR
g_pollnval=$glib_cv_value_POLLNVAL

# If a family is not found on the system, define that family to
# a negative value, picking a different one for each undefined
# family (-1 for AF_UNIX, -2 for the next one, -3 ...)
# This is needed because glib-mkenums doesn't handle optional
# values in enums, and thus we have to have all existing values
# defined in the enum.
if test "x$glib_cv_value_AF_UNIX" != "x"; then
  g_af_unix=$glib_cv_value_AF_UNIX
else
  g_af_unix=-1
fi
g_af_inet=$glib_cv_value_AF_INET
g_af_inet6=$glib_cv_value_AF_INET6

g_msg_peek=$glib_cv_value_MSG_PEEK
g_msg_oob=$glib_cv_value_MSG_OOB
g_msg_dontroute=$glib_cv_value_MSG_DONTROUTE

g_stack_grows=$glib_cv_stack_grows

g_have_eilseq=$have_eilseq

g_threads_impl_def=$g_threads_impl

g_atomic_lock_free="$glib_cv_g_atomic_lock_free"
g_memory_barrier_needed="$glib_memory_barrier_needed"
g_gcc_atomic_ops="$glib_cv_gcc_has_builtin_atomic_operations"

g_module_suffix="$glib_gmodule_suffix"
g_dir_separator="$glib_dir_separator"
g_searchpath_separator="$glib_searchpath_separator"

g_pid_type="$glib_pid_type"
g_pid_format="\"$glib_pid_format\""
g_pollfd_format="\"$glib_pollfd_format\""

case $host in
  *-*-cygwin*)
    glib_os="#define G_OS_UNIX
#define G_PLATFORM_WIN32
#define G_WITH_CYGWIN"
    ;;
  *-*-mingw*)
    glib_os="#define G_OS_WIN32
#define G_PLATFORM_WIN32"
    ;;
  *)
    glib_os="#define G_OS_UNIX"
    ;;
esac
glib_static_compilation=""
if test x$glib_win32_static_compilation = xyes; then
  glib_static_compilation="#define GLIB_STATIC_COMPILATION 1
#define GOBJECT_STATIC_COMPILATION 1"
fi
])

# Redo enough to get guint32 and guint64 for the alignment checks below
case 4 in
$ac_cv_sizeof_short)
  gint32=short
  ;;
$ac_cv_sizeof_int)
  gint32=int
  ;;
$ac_cv_sizeof_long)
  gint32=long
  ;;
esac
case 8 in
$ac_cv_sizeof_int)
  gint64=int
  ;;
$ac_cv_sizeof_long)
  gint64=long
  ;;
$ac_cv_sizeof_long_long)
  gint64='long long'
  ;;
$ac_cv_sizeof___int64)
  gint64='__int64'
  ;;
esac

AC_CHECK_TYPE([guint32],,,[typedef unsigned $gint32 guint32;])
AC_CHECK_ALIGNOF([guint32], [AC_INCLUDES_DEFAULT
typedef unsigned $gint32 guint32;])
AC_CHECK_TYPE([guint64],,,[typedef unsigned $gint64 guint64;])
AC_CHECK_ALIGNOF([guint64], [AC_INCLUDES_DEFAULT
typedef unsigned $gint64 guint64;])
AC_CHECK_TYPE([unsigned long])
AC_CHECK_ALIGNOF([unsigned long])

# Check for libdbus1 - Optional - is only used in the GDBus test cases
#
# 1.2.14 required for dbus_message_set_serial
PKG_CHECK_MODULES(DBUS1,
     dbus-1 >= 1.2.14,
     [AC_DEFINE(HAVE_DBUS1, 1, [Define if dbus-1 is available]) have_dbus1=yes],
     have_dbus1=no)
AC_SUBST(DBUS1_CFLAGS)
AC_SUBST(DBUS1_LIBS)
AM_CONDITIONAL(HAVE_DBUS1, [test "x$have_dbus1" = "xyes"])

AC_CHECK_PROGS([DBUS_DAEMON], [dbus-daemon])
AM_CONDITIONAL([HAVE_DBUS_DAEMON], [test x$DBUS_DAEMON = xdbus-daemon ])

# Check whether MSVC toolset is explicitly set
AM_CONDITIONAL(MSVC_BASE_NO_TOOLSET_SET, [test x$MSVC_BASE_TOOLSET = x])
AM_CONDITIONAL(MSVC_NO_TOOLSET_SET, [test x$MSVC_TOOLSET = x])

dnl
dnl Check for -Bsymbolic-functions linker flag used to avoid
dnl intra-library PLT jumps, if available.
dnl

AC_ARG_ENABLE(Bsymbolic,
              [AS_HELP_STRING([--disable-Bsymbolic],
                              [avoid linking with -Bsymbolic])],,
              [SAVED_LDFLAGS="${LDFLAGS}" SAVED_LIBS="${LIBS}"
               AC_MSG_CHECKING([for -Bsymbolic-functions linker flag])
               LDFLAGS=-Wl,-Bsymbolic-functions
               LIBS=
               AC_TRY_LINK([], [return 0],
                           AC_MSG_RESULT(yes)
                           enable_Bsymbolic=yes,
                           AC_MSG_RESULT(no)
                           enable_Bsymbolic=no)
               LDFLAGS="${SAVED_LDFLAGS}" LIBS="${SAVED_LIBS}"])

if test "x${enable_Bsymbolic}" = "xyes"; then
  GLIB_LINK_FLAGS=-Wl,-Bsymbolic-functions
fi

dnl
dnl Check for -z,nodelete linker flag: the type system assumes that
dnl libgobject stays loaded for the lifetime of the process.
dnl Since ld.bfd does not treat wrong -z options as fatal by default,
dnl we also try to check for the --fatal-warnings linker flag if
dnl auto-detecting.
dnl

AC_ARG_ENABLE([znodelete],
              [AS_HELP_STRING([--disable-znodelete],
                              [avoid linking with -z,nodelete])],,
              [SAVED_CFLAGS="${CFLAGS}" SAVED_LDFLAGS="${LDFLAGS}" SAVED_LIBS="${LIBS}"
               AC_MSG_CHECKING([for --fatal-warnings linker flag])
               CFLAGS="${SAVED_CFLAGS} ${shared_flag:- -shared}"
               LDFLAGS=-Wl,--fatal-warnings
               LIBS=
               AC_TRY_LINK([], [return 0],
                           AC_MSG_RESULT(yes)
                           [ldflags_fatal=-Wl,--fatal-warnings],
                           AC_MSG_RESULT(no)
                           ldflags_fatal=)
               AC_MSG_CHECKING([for -z,nodelete linker flag])
               LDFLAGS="$ldflags_fatal -Wl,-z,nodelete"
               AC_TRY_LINK([], [return 0],
                           AC_MSG_RESULT(yes)
                           enable_znodelete=yes,
                           AC_MSG_RESULT(no)
                           enable_znodelete=no)
               CFLAGS="${SAVED_CFLAGS}" LDFLAGS="${SAVED_LDFLAGS}" LIBS="${SAVED_LIBS}"])

if test "x${enable_znodelete}" = "xyes"; then
  GLIB_LINK_FLAGS="$GLIB_LINK_FLAGS -Wl,-z,nodelete"
fi

AC_SUBST(GLIB_LINK_FLAGS)

dnl
dnl Check for -fvisibility=hidden to determine if we can do GNU-style
dnl visibility attributes for symbol export control
dnl
GLIB_HIDDEN_VISIBILITY_CFLAGS=""
case "$host" in
  *-*-mingw*)
    dnl on mingw32 we do -fvisibility=hidden and __declspec(dllexport)
    AC_DEFINE([_GLIB_EXTERN], [__attribute__((visibility("default"))) __declspec(dllexport) extern],
              [defines how to decorate public symbols while building])
    CFLAGS="${CFLAGS} -fvisibility=hidden"
    ;;
  *)
    dnl on other compilers, check if we can do -fvisibility=hidden
    SAVED_CFLAGS="${CFLAGS}"
    CFLAGS="-fvisibility=hidden"
    AC_MSG_CHECKING([for -fvisibility=hidden compiler flag])
    AC_TRY_COMPILE([], [return 0],
                   AC_MSG_RESULT(yes)
                   enable_fvisibility_hidden=yes,
                   AC_MSG_RESULT(no)
                   enable_fvisibility_hidden=no)
    CFLAGS="${SAVED_CFLAGS}"

    AS_IF([test "${enable_fvisibility_hidden}" = "yes"], [
      AC_DEFINE([_GLIB_EXTERN], [__attribute__((visibility("default"))) extern],
                [defines how to decorate public symbols while building])
      GLIB_HIDDEN_VISIBILITY_CFLAGS="-fvisibility=hidden"
    ])
    ;;
esac
AC_SUBST(GLIB_HIDDEN_VISIBILITY_CFLAGS)

dnl Compiler flags; macro originates from systemd
dnl See https://bugzilla.gnome.org/show_bug.cgi?id=608953
AC_ARG_ENABLE(compile-warnings,
              [AS_HELP_STRING([--disable-compile-warnings],
                              [Don't use builtin compiler warnings])],,
			      enable_compile_warnings=yes)
AS_IF([test "x$enable_compile_warnings" != xno], [
  CC_CHECK_FLAGS_APPEND([GLIB_WARN_CFLAGS], [CFLAGS], [\
   -Wall -Wstrict-prototypes -Wduplicated-branches \
   -Werror=declaration-after-statement \
   -Werror=missing-prototypes -Werror=implicit-function-declaration \
   -Werror=pointer-arith -Werror=init-self -Werror=format-security \
   -Werror=format=2 -Werror=missing-include-dirs])
])
AC_SUBST(GLIB_WARN_CFLAGS)

#
# Define variables corresponding to the correct include paths to use for
# in-tree building.
#

# for config.h:
config_h_INCLUDES='-I$(top_builddir)'
AC_SUBST(config_h_INCLUDES)

# glib:
#   config.h
#   $(top_builddir)/glib: for glibconfig.h
#   $(top_srcdir)/glib:   for glib.h
#   $(top_srcdir):        for everything
glib_INCLUDES='$(config_h_INCLUDES) -I$(top_builddir)/glib -I$(top_srcdir)/glib -I$(top_srcdir)'
AC_SUBST(glib_INCLUDES)

# gobject:
#   same as glib
gobject_INCLUDES='$(glib_INCLUDES)'
AC_SUBST(gobject_INCLUDES)

# gmodule:
#   glib includes
#   $(top_srcdir)/gmodule: for gmodule.h
gmodule_INCLUDES='$(glib_INCLUDES) -I$(top_srcdir)/gmodule'
AC_SUBST(gmodule_INCLUDES)

# gio:
#   same as gmodule
gio_INCLUDES='$(gmodule_INCLUDES)'
AC_SUBST(gio_INCLUDES)


AC_CONFIG_FILES([
glib-2.0.pc
gmodule-2.0.pc
gmodule-export-2.0.pc
gmodule-no-export-2.0.pc
gthread-2.0.pc
gobject-2.0.pc
gio-2.0.pc
gio-unix-2.0.pc
gio-windows-2.0.pc
glib-zip
glib-gettextize
Makefile
build/Makefile
build/win32/Makefile
build/win32/dirent/Makefile
win32/Makefile
win32/vs9/Makefile
win32/vs9/glib-version-paths.vsprops
win32/vs10/Makefile
win32/vs10/glib-version-paths.props
win32/vs11/Makefile
win32/vs12/Makefile
win32/vs14/Makefile
win32/vs15/Makefile
glib/Makefile
glib/libcharset/Makefile
glib/gnulib/Makefile
glib/pcre/Makefile
glib/update-pcre/Makefile
glib/tests/Makefile
gmodule/Makefile
gmodule/gmoduleconf.h
gobject/Makefile
gobject/glib-genmarshal
gobject/glib-mkenums
gobject/tests/Makefile
gthread/Makefile
gio/Makefile
gio/gdbus-2.0/codegen/Makefile
gio/gdbus-2.0/codegen/config.py
gio/gnetworking.h
gio/xdgmime/Makefile
gio/inotify/Makefile
gio/kqueue/Makefile
gio/fam/Makefile
gio/win32/Makefile
gio/tests/Makefile
gio/tests/gdbus-object-manager-example/Makefile
gio/tests/services/Makefile
gio/tests/services/org.gtk.GDBus.Examples.ObjectManager.service
gio/tests/modules/Makefile
po/Makefile.in
docs/Makefile
docs/reference/Makefile
docs/reference/glib/Makefile
docs/reference/glib/version.xml
docs/reference/gobject/Makefile
docs/reference/gobject/version.xml
docs/reference/gio/Makefile
docs/reference/gio/version.xml
tests/Makefile
tests/gobject/Makefile
tests/refcount/Makefile
m4macros/Makefile
])

# We need this command because the configure script will not preserve
# the same attributes of the template files
AC_CONFIG_COMMANDS([chmod-scripts],
[chmod 0755 glib-zip
chmod 0755 glib-gettextize
chmod 0755 gobject/glib-genmarshal
chmod 0755 gobject/glib-mkenums])

# we want to invoke this macro solely so that the config.status script
# and automake generated makefiles know about these generated files.
# They are only needed to distcheck the package
if false; then
  AC_CONFIG_FILES([
    INSTALL
    README
    config.h.win32
    glib/glibconfig.h.win32
    glib/glib.rc
    gmodule/gmodule.rc
    gobject/gobject.rc
    gthread/gthread.rc
    gio/gio.rc
  ])
fi

AC_OUTPUT
