/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gdir.c: Simplified wrapper around the DIRENT functions.
 *
 * Copyright 2001 Hans Breuer
 * Copyright 2004 Tor Lillqvist
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

#include "config.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef HAVE_DIRENT_H
#include <sys/types.h>
#include <dirent.h>
#endif

#include "gdir.h"

#include "gconvert.h"
#include "gfileutils.h"
#include "gstrfuncs.h"
#include "gtestutils.h"
#include "glibintl.h"

#if defined (_MSC_VER) && !defined (HAVE_DIRENT_H)
#include "../build/win32/dirent/dirent.h"
#include "../build/win32/dirent/wdirent.c"
#endif

#include "glib-private.h" /* g_dir_open_with_errno, g_dir_new_from_dirp */

/**
 * GDir:
 *
 * An opaque structure representing an opened directory.
 */

struct _GDir
{
#ifdef G_OS_WIN32
  _WDIR *wdirp;
#else
  DIR *dirp;
#endif
#ifdef G_OS_WIN32
  gchar utf8_buf[FILENAME_MAX*4];
#endif
};

/*< private >
 * g_dir_open_with_errno:
 * @path: the path to the directory you are interested in.
 * @flags: Currently must be set to 0. Reserved for future use.
 *
 * Opens a directory for reading.
 *
 * This function is equivalent to g_dir_open() except in the error case,
 * errno will be set accordingly.
 *
 * This is useful if you want to construct your own error message.
 *
 * Returns: a newly allocated #GDir on success, or %NULL on failure,
 *   with errno set accordingly.
 *
 * Since: 2.38
 */
GDir *
g_dir_open_with_errno (const gchar *path,
                       guint        flags)
{
  GDir dir;
#ifdef G_OS_WIN32
  gint saved_errno;
  wchar_t *wpath;
#endif

  g_return_val_if_fail (path != NULL, NULL);

#ifdef G_OS_WIN32
  wpath = g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);

  g_return_val_if_fail (wpath != NULL, NULL);

  dir.wdirp = _wopendir (wpath);
  saved_errno = errno;
  g_free (wpath);
  errno = saved_errno;

  if (dir.wdirp == NULL)
    return NULL;
#else
  dir.dirp = opendir (path);

  if (dir.dirp == NULL)
    return NULL;
#endif

  return g_memdup (&dir, sizeof dir);
}

/**
 * g_dir_open:
 * @path: the path to the directory you are interested in. On Unix
 *         in the on-disk encoding. On Windows in UTF-8
 * @flags: Currently must be set to 0. Reserved for future use.
 * @error: return location for a #GError, or %NULL.
 *         If non-%NULL, an error will be set if and only if
 *         g_dir_open() fails.
 *
 * Opens a directory for reading. The names of the files in the
 * directory can then be retrieved using g_dir_read_name().  Note
 * that the ordering is not defined.
 *
 * Returns: a newly allocated #GDir on success, %NULL on failure.
 *   If non-%NULL, you must free the result with g_dir_close()
 *   when you are finished with it.
 **/
GDir *
g_dir_open (const gchar  *path,
            guint         flags,
            GError      **error)
{
  gint saved_errno;
  GDir *dir;

  dir = g_dir_open_with_errno (path, flags);

  if (dir == NULL)
    {
      gchar *utf8_path;

      saved_errno = errno;

      utf8_path = g_filename_to_utf8 (path, -1, NULL, NULL, NULL);

      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (saved_errno),
                   _("Error opening directory “%s”: %s"), utf8_path, g_strerror (saved_errno));
      g_free (utf8_path);
    }

  return dir;
}

/*< private >
 * g_dir_new_from_dirp:
 * @dirp: a #DIR* created by opendir() or fdopendir()
 *
 * Creates a #GDir object from the DIR object that is created using
 * opendir() or fdopendir().  The created #GDir assumes ownership of the
 * passed-in #DIR pointer.
 *
 * @dirp must not be %NULL.
 *
 * This function never fails.
 *
 * Returns: a newly allocated #GDir, which should be closed using
 *     g_dir_close().
 *
 * Since: 2.38
 **/
GDir *
g_dir_new_from_dirp (gpointer dirp)
{
#ifdef G_OS_UNIX
  GDir *dir;

  g_return_val_if_fail (dirp != NULL, NULL);

  dir = g_new (GDir, 1);
  dir->dirp = dirp;

  return dir;
#else
  g_assert_not_reached ();
#endif
}

/**
 * g_dir_read_name:
 * @dir: a #GDir* created by g_dir_open()
 *
 * Retrieves the name of another entry in the directory, or %NULL.
 * The order of entries returned from this function is not defined,
 * and may vary by file system or other operating-system dependent
 * factors.
 *
 * %NULL may also be returned in case of errors. On Unix, you can
 * check `errno` to find out if %NULL was returned because of an error.
 *
 * On Unix, the '.' and '..' entries are omitted, and the returned
 * name is in the on-disk encoding.
 *
 * On Windows, as is true of all GLib functions which operate on
 * filenames, the returned name is in UTF-8.
 *
 * Returns: (type filename): The entry's name or %NULL if there are no
 *   more entries. The return value is owned by GLib and
 *   must not be modified or freed.
 **/
const gchar *
g_dir_read_name (GDir *dir)
{
#ifdef G_OS_WIN32
  gchar *utf8_name;
  struct _wdirent *wentry;
#else
  struct dirent *entry;
#endif

  g_return_val_if_fail (dir != NULL, NULL);

#ifdef G_OS_WIN32
  while (1)
    {
      wentry = _wreaddir (dir->wdirp);
      while (wentry 
	     && (0 == wcscmp (wentry->d_name, L".") ||
		 0 == wcscmp (wentry->d_name, L"..")))
	wentry = _wreaddir (dir->wdirp);

      if (wentry == NULL)
	return NULL;

      utf8_name = g_utf16_to_utf8 (wentry->d_name, -1, NULL, NULL, NULL);

      if (utf8_name == NULL)
	continue;		/* Huh, impossible? Skip it anyway */

      strcpy (dir->utf8_buf, utf8_name);
      g_free (utf8_name);

      return dir->utf8_buf;
    }
#else
  entry = readdir (dir->dirp);
  while (entry 
         && (0 == strcmp (entry->d_name, ".") ||
             0 == strcmp (entry->d_name, "..")))
    entry = readdir (dir->dirp);

  if (entry)
    return entry->d_name;
  else
    return NULL;
#endif
}

/**
 * g_dir_rewind:
 * @dir: a #GDir* created by g_dir_open()
 *
 * Resets the given directory. The next call to g_dir_read_name()
 * will return the first entry again.
 **/
void
g_dir_rewind (GDir *dir)
{
  g_return_if_fail (dir != NULL);
  
#ifdef G_OS_WIN32
  _wrewinddir (dir->wdirp);
#else
  rewinddir (dir->dirp);
#endif
}

/**
 * g_dir_close:
 * @dir: a #GDir* created by g_dir_open()
 *
 * Closes the directory and deallocates all related resources.
 **/
void
g_dir_close (GDir *dir)
{
  g_return_if_fail (dir != NULL);

#ifdef G_OS_WIN32
  _wclosedir (dir->wdirp);
#else
  closedir (dir->dirp);
#endif
  g_free (dir);
}

#ifdef G_OS_WIN32

/* Binary compatibility versions. Not for newly compiled code. */

_GLIB_EXTERN GDir        *g_dir_open_utf8      (const gchar  *path,
                                                guint         flags,
                                                GError      **error);
_GLIB_EXTERN const gchar *g_dir_read_name_utf8 (GDir         *dir);

GDir *
g_dir_open_utf8 (const gchar  *path,
                 guint         flags,
                 GError      **error)
{
  return g_dir_open (path, flags, error);
}

const gchar *
g_dir_read_name_utf8 (GDir *dir)
{
  return g_dir_read_name (dir);
}

#endif
