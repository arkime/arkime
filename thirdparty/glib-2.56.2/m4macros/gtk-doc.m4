# -*- mode: autoconf -*-
#
# gtk-doc.m4 - configure macro to check for gtk-doc
# Copyright (C) 2003 James Henstridge
#               2007-2017  Stefan Sauer
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# As a special exception, the above copyright owner gives unlimited
# permission to copy, distribute and modify the configure scripts that
# are the output of Autoconf when processing the Macro. You need not
# follow the terms of the GNU General Public License when using or
# distributing such scripts, even though portions of the text of the
# Macro appear in them. The GNU General Public License (GPL) does govern
# all other use of the material that constitutes the Autoconf Macro.

# serial 2

dnl Usage:
dnl   GTK_DOC_CHECK([minimum-gtk-doc-version])
AC_DEFUN([GTK_DOC_CHECK],
[
  AC_REQUIRE([PKG_PROG_PKG_CONFIG])
  AC_BEFORE([AC_PROG_LIBTOOL],[$0])dnl setup libtool first
  AC_BEFORE([AM_PROG_LIBTOOL],[$0])dnl setup libtool first

  ifelse([$1],[],[gtk_doc_requires="gtk-doc"],[gtk_doc_requires="gtk-doc >= $1"])
  AC_MSG_CHECKING([for gtk-doc])
  PKG_CHECK_EXISTS([$gtk_doc_requires],[have_gtk_doc=yes],[have_gtk_doc=no])
  AC_MSG_RESULT($have_gtk_doc)

  if test "$have_gtk_doc" = "no"; then
      AC_MSG_WARN([
  You will not be able to create source packages with 'make dist'
  because $gtk_doc_requires is not found.])
  fi

  dnl check for tools we added during development
  dnl Use AC_CHECK_PROG to avoid the check target using an absolute path that
  dnl may not be writable by the user. Currently, automake requires that the
  dnl test name must end in '.test'.
  dnl https://bugzilla.gnome.org/show_bug.cgi?id=701638
  AC_CHECK_PROG([GTKDOC_CHECK],[gtkdoc-check],[gtkdoc-check.test])
  AC_PATH_PROG([GTKDOC_CHECK_PATH],[gtkdoc-check])
  AC_PATH_PROGS([GTKDOC_REBASE],[gtkdoc-rebase],[true])
  AC_PATH_PROG([GTKDOC_MKPDF],[gtkdoc-mkpdf])

  dnl for overriding the documentation installation directory
  AC_ARG_WITH([html-dir],
    AS_HELP_STRING([--with-html-dir=PATH], [path to installed docs]),,
    [with_html_dir='${datadir}/gtk-doc/html'])
  HTML_DIR="$with_html_dir"
  AC_SUBST([HTML_DIR])

  dnl enable/disable documentation building
  AC_ARG_ENABLE([gtk-doc],
    AS_HELP_STRING([--enable-gtk-doc],
                   [use gtk-doc to build documentation [[default=no]]]),,
    [enable_gtk_doc=no])

  AC_MSG_CHECKING([whether to build gtk-doc documentation])
  AC_MSG_RESULT($enable_gtk_doc)

  if test "x$enable_gtk_doc" = "xyes" && test "$have_gtk_doc" = "no"; then
    AC_MSG_ERROR([
  You must have $gtk_doc_requires installed to build documentation for
  $PACKAGE_NAME. Please install gtk-doc or disable building the
  documentation by adding '--disable-gtk-doc' to '[$]0'.])
  fi

  dnl don't check for glib if we build glib
  if test "x$PACKAGE_NAME" != "xglib"; then
    dnl don't fail if someone does not have glib
    PKG_CHECK_MODULES(GTKDOC_DEPS, glib-2.0 >= 2.10.0 gobject-2.0  >= 2.10.0,,[:])
  fi

  dnl enable/disable output formats
  AC_ARG_ENABLE([gtk-doc-html],
    AS_HELP_STRING([--enable-gtk-doc-html],
                   [build documentation in html format [[default=yes]]]),,
    [enable_gtk_doc_html=yes])
    AC_ARG_ENABLE([gtk-doc-pdf],
      AS_HELP_STRING([--enable-gtk-doc-pdf],
                     [build documentation in pdf format [[default=no]]]),,
      [enable_gtk_doc_pdf=no])

  if test -z "$GTKDOC_MKPDF"; then
    enable_gtk_doc_pdf=no
  fi

  if test -z "$AM_DEFAULT_VERBOSITY"; then
    AM_DEFAULT_VERBOSITY=1
  fi
  AC_SUBST([AM_DEFAULT_VERBOSITY])

  AM_CONDITIONAL([HAVE_GTK_DOC], [test x$have_gtk_doc = xyes])
  AM_CONDITIONAL([ENABLE_GTK_DOC], [test x$enable_gtk_doc = xyes])
  AM_CONDITIONAL([GTK_DOC_BUILD_HTML], [test x$enable_gtk_doc_html = xyes])
  AM_CONDITIONAL([GTK_DOC_BUILD_PDF], [test x$enable_gtk_doc_pdf = xyes])
  AM_CONDITIONAL([GTK_DOC_USE_LIBTOOL], [test -n "$LIBTOOL"])
  AM_CONDITIONAL([GTK_DOC_USE_REBASE], [test -n "$GTKDOC_REBASE"])
])
