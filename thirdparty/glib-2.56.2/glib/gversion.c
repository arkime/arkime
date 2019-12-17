/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1998  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "config.h"

#include "gversion.h"

/**
 * SECTION:version
 * @Title: Version Information
 * @Short_description: variables and functions to check the GLib version
 *
 * GLib provides version information, primarily useful in configure
 * checks for builds that have a configure script. Applications will
 * not typically use the features described here.
 *
 * The GLib headers annotate deprecated APIs in a way that produces
 * compiler warnings if these deprecated APIs are used. The warnings
 * can be turned off by defining the macro %GLIB_DISABLE_DEPRECATION_WARNINGS
 * before including the glib.h header.
 *
 * GLib also provides support for building applications against
 * defined subsets of deprecated or new GLib APIs. Define the macro
 * %GLIB_VERSION_MIN_REQUIRED to specify up to what version of GLib
 * you want to receive warnings about deprecated APIs. Define the
 * macro %GLIB_VERSION_MAX_ALLOWED to specify the newest version of
 * GLib whose API you want to use.
 */

/**
 * glib_major_version:
 *
 * The major version of the GLib library.
 *
 * An integer variable exported from the library linked
 * against at application run time.
 */

/**
 * GLIB_MAJOR_VERSION:
 *
 * The major version number of the GLib library.
 *
 * Like #glib_major_version, but from the headers used at
 * application compile time, rather than from the library
 * linked against at application run time.
 */

/**
 * glib_minor_version:
 *
 * The minor version number of the GLib library.
 *
 * An integer variable exported from the library linked
 * against at application run time.
 */

/**
 * GLIB_MINOR_VERSION:
 *
 * The minor version number of the GLib library.
 *
 * Like #gtk_minor_version, but from the headers used at
 * application compile time, rather than from the library
 * linked against at application run time.
 */

/**
 * glib_micro_version:
 *
 * The micro version number of the GLib library.
 *
 * An integer variable exported from the library linked
 * against at application run time.
 */

/**
 * GLIB_MICRO_VERSION:
 *
 * The micro version number of the GLib library.
 *
 * Like #gtk_micro_version, but from the headers used at
 * application compile time, rather than from the library
 * linked against at application run time.
 */

/**
 * GLIB_CHECK_VERSION:
 * @major: the major version to check for
 * @minor: the minor version to check for
 * @micro: the micro version to check for
 *
 * Checks the version of the GLib library that is being compiled
 * against. See glib_check_version() for a runtime check.
 *
 * Returns: %TRUE if the version of the GLib header files
 * is the same as or newer than the passed-in version.
 */

/**
 * glib_binary_age:
 *
 * The binary age of the GLib library.
 * Defines how far back backwards compatibility reaches.
 *
 * An integer variable exported from the library linked
 * against at application run time.
 */

/**
 * glib_interface_age:
 *
 * The interface age of the GLib library.
 * Defines how far back the API has last been extended.
 *
 * An integer variable exported from the library linked
 * against at application run time.
 */

const guint glib_major_version = GLIB_MAJOR_VERSION;
const guint glib_minor_version = GLIB_MINOR_VERSION;
const guint glib_micro_version = GLIB_MICRO_VERSION;
const guint glib_interface_age = GLIB_INTERFACE_AGE;
const guint glib_binary_age = GLIB_BINARY_AGE;

/**
 * glib_check_version:
 * @required_major: the required major version
 * @required_minor: the required minor version
 * @required_micro: the required micro version
 *
 * Checks that the GLib library in use is compatible with the
 * given version. Generally you would pass in the constants
 * #GLIB_MAJOR_VERSION, #GLIB_MINOR_VERSION, #GLIB_MICRO_VERSION
 * as the three arguments to this function; that produces
 * a check that the library in use is compatible with
 * the version of GLib the application or module was compiled
 * against.
 *
 * Compatibility is defined by two things: first the version
 * of the running library is newer than the version
 * @required_major.required_minor.@required_micro. Second
 * the running library must be binary compatible with the
 * version @required_major.required_minor.@required_micro
 * (same major version.)
 *
 * Returns: %NULL if the GLib library is compatible with the
 *     given version, or a string describing the version mismatch.
 *     The returned string is owned by GLib and must not be modified
 *     or freed.
 *
 * Since: 2.6
 */
const gchar *
glib_check_version (guint required_major,
                    guint required_minor,
                    guint required_micro)
{
  gint glib_effective_micro = 100 * GLIB_MINOR_VERSION + GLIB_MICRO_VERSION;
  gint required_effective_micro = 100 * required_minor + required_micro;

  if (required_major > GLIB_MAJOR_VERSION)
    return "GLib version too old (major mismatch)";
  if (required_major < GLIB_MAJOR_VERSION)
    return "GLib version too new (major mismatch)";
  if (required_effective_micro < glib_effective_micro - GLIB_BINARY_AGE)
    return "GLib version too new (micro mismatch)";
  if (required_effective_micro > glib_effective_micro)
    return "GLib version too old (micro mismatch)";
  return NULL;
}
