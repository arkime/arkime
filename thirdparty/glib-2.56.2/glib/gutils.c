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

/* 
 * MT safe for the unix part, FIXME: make the win32 part MT safe as well.
 */

#include "config.h"

#include "gutils.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <string.h>
#include <ctype.h>		/* For tolower() */
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef G_OS_UNIX
#include <pwd.h>
#include <unistd.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_CRT_EXTERNS_H 
#include <crt_externs.h> /* for _NSGetEnviron */
#endif

#include "glib-init.h"
#include "glib-private.h"
#include "genviron.h"
#include "gfileutils.h"
#include "ggettext.h"
#include "ghash.h"
#include "gthread.h"
#include "gtestutils.h"
#include "gunicode.h"
#include "gstrfuncs.h"
#include "garray.h"
#include "glibintl.h"
#include "gstdio.h"

#ifdef G_PLATFORM_WIN32
#include "gconvert.h"
#include "gwin32.h"
#endif


/**
 * SECTION:misc_utils
 * @title: Miscellaneous Utility Functions
 * @short_description: a selection of portable utility functions
 *
 * These are portable utility functions.
 */

#ifdef G_PLATFORM_WIN32
#  include <windows.h>
#  ifndef GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
#    define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 2
#    define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 4
#  endif
#  include <lmcons.h>		/* For UNLEN */
#endif /* G_PLATFORM_WIN32 */

#ifdef G_OS_WIN32
#  include <direct.h>
#  include <shlobj.h>
   /* older SDK (e.g. msvc 5.0) does not have these*/
#  ifndef CSIDL_MYMUSIC
#    define CSIDL_MYMUSIC 13
#  endif
#  ifndef CSIDL_MYVIDEO
#    define CSIDL_MYVIDEO 14
#  endif
#  ifndef CSIDL_INTERNET_CACHE
#    define CSIDL_INTERNET_CACHE 32
#  endif
#  ifndef CSIDL_COMMON_APPDATA
#    define CSIDL_COMMON_APPDATA 35
#  endif
#  ifndef CSIDL_MYPICTURES
#    define CSIDL_MYPICTURES 0x27
#  endif
#  ifndef CSIDL_COMMON_DOCUMENTS
#    define CSIDL_COMMON_DOCUMENTS 46
#  endif
#  ifndef CSIDL_PROFILE
#    define CSIDL_PROFILE 40
#  endif
#  include <process.h>
#endif

#ifdef HAVE_CARBON
#include <CoreServices/CoreServices.h>
#endif

#ifdef HAVE_CODESET
#include <langinfo.h>
#endif

#ifdef G_PLATFORM_WIN32

gchar *
_glib_get_dll_directory (void)
{
  gchar *retval;
  gchar *p;
  wchar_t wc_fn[MAX_PATH];

#ifdef DLL_EXPORT
  if (glib_dll == NULL)
    return NULL;
#endif

  /* This code is different from that in
   * g_win32_get_package_installation_directory_of_module() in that
   * here we return the actual folder where the GLib DLL is. We don't
   * do the check for it being in a "bin" or "lib" subfolder and then
   * returning the parent of that.
   *
   * In a statically built GLib, glib_dll will be NULL and we will
   * thus look up the application's .exe file's location.
   */
  if (!GetModuleFileNameW (glib_dll, wc_fn, MAX_PATH))
    return NULL;

  retval = g_utf16_to_utf8 (wc_fn, -1, NULL, NULL, NULL);

  p = strrchr (retval, G_DIR_SEPARATOR);
  if (p == NULL)
    {
      /* Wtf? */
      return NULL;
    }
  *p = '\0';

  return retval;
}

#endif

/**
 * g_memmove: 
 * @dest: the destination address to copy the bytes to.
 * @src: the source address to copy the bytes from.
 * @len: the number of bytes to copy.
 *
 * Copies a block of memory @len bytes long, from @src to @dest.
 * The source and destination areas may overlap.
 *
 * Deprecated:2.40: Just use memmove().
 */

#ifdef G_OS_WIN32
#undef g_atexit
#endif

/**
 * g_atexit:
 * @func: (scope async): the function to call on normal program termination.
 * 
 * Specifies a function to be called at normal program termination.
 *
 * Since GLib 2.8.2, on Windows g_atexit() actually is a preprocessor
 * macro that maps to a call to the atexit() function in the C
 * library. This means that in case the code that calls g_atexit(),
 * i.e. atexit(), is in a DLL, the function will be called when the
 * DLL is detached from the program. This typically makes more sense
 * than that the function is called when the GLib DLL is detached,
 * which happened earlier when g_atexit() was a function in the GLib
 * DLL.
 *
 * The behaviour of atexit() in the context of dynamically loaded
 * modules is not formally specified and varies wildly.
 *
 * On POSIX systems, calling g_atexit() (or atexit()) in a dynamically
 * loaded module which is unloaded before the program terminates might
 * well cause a crash at program exit.
 *
 * Some POSIX systems implement atexit() like Windows, and have each
 * dynamically loaded module maintain an own atexit chain that is
 * called when the module is unloaded.
 *
 * On other POSIX systems, before a dynamically loaded module is
 * unloaded, the registered atexit functions (if any) residing in that
 * module are called, regardless where the code that registered them
 * resided. This is presumably the most robust approach.
 *
 * As can be seen from the above, for portability it's best to avoid
 * calling g_atexit() (or atexit()) except in the main executable of a
 * program.
 *
 * Deprecated:2.32: It is best to avoid g_atexit().
 */
void
g_atexit (GVoidFunc func)
{
  gint result;
  int errsv;

  result = atexit ((void (*)(void)) func);
  errsv = errno;
  if (result)
    {
      g_error ("Could not register atexit() function: %s",
               g_strerror (errsv));
    }
}

/* Based on execvp() from GNU Libc.
 * Some of this code is cut-and-pasted into gspawn.c
 */

static gchar*
my_strchrnul (const gchar *str, 
	      gchar        c)
{
  gchar *p = (gchar*)str;
  while (*p && (*p != c))
    ++p;

  return p;
}

#ifdef G_OS_WIN32

static gchar *inner_find_program_in_path (const gchar *program);

gchar*
g_find_program_in_path (const gchar *program)
{
  const gchar *last_dot = strrchr (program, '.');

  if (last_dot == NULL ||
      strchr (last_dot, '\\') != NULL ||
      strchr (last_dot, '/') != NULL)
    {
      const gint program_length = strlen (program);
      gchar *pathext = g_build_path (";",
				     ".exe;.cmd;.bat;.com",
				     g_getenv ("PATHEXT"),
				     NULL);
      gchar *p;
      gchar *decorated_program;
      gchar *retval;

      p = pathext;
      do
	{
	  gchar *q = my_strchrnul (p, ';');

	  decorated_program = g_malloc (program_length + (q-p) + 1);
	  memcpy (decorated_program, program, program_length);
	  memcpy (decorated_program+program_length, p, q-p);
	  decorated_program [program_length + (q-p)] = '\0';
	  
	  retval = inner_find_program_in_path (decorated_program);
	  g_free (decorated_program);

	  if (retval != NULL)
	    {
	      g_free (pathext);
	      return retval;
	    }
	  p = q;
	} while (*p++ != '\0');
      g_free (pathext);
      return NULL;
    }
  else
    return inner_find_program_in_path (program);
}

#endif

/**
 * g_find_program_in_path:
 * @program: (type filename): a program name in the GLib file name encoding
 * 
 * Locates the first executable named @program in the user's path, in the
 * same way that execvp() would locate it. Returns an allocated string
 * with the absolute path name, or %NULL if the program is not found in
 * the path. If @program is already an absolute path, returns a copy of
 * @program if @program exists and is executable, and %NULL otherwise.
 *  
 * On Windows, if @program does not have a file type suffix, tries
 * with the suffixes .exe, .cmd, .bat and .com, and the suffixes in
 * the `PATHEXT` environment variable. 
 * 
 * On Windows, it looks for the file in the same way as CreateProcess() 
 * would. This means first in the directory where the executing
 * program was loaded from, then in the current directory, then in the
 * Windows 32-bit system directory, then in the Windows directory, and
 * finally in the directories in the `PATH` environment variable. If
 * the program is found, the return value contains the full name
 * including the type suffix.
 *
 * Returns: (type filename): a newly-allocated string with the absolute path,
 *     or %NULL
 **/
#ifdef G_OS_WIN32
static gchar *
inner_find_program_in_path (const gchar *program)
#else
gchar*
g_find_program_in_path (const gchar *program)
#endif
{
  const gchar *path, *p;
  gchar *name, *freeme;
#ifdef G_OS_WIN32
  const gchar *path_copy;
  gchar *filename = NULL, *appdir = NULL;
  gchar *sysdir = NULL, *windir = NULL;
  int n;
  wchar_t wfilename[MAXPATHLEN], wsysdir[MAXPATHLEN],
    wwindir[MAXPATHLEN];
#endif
  gsize len;
  gsize pathlen;

  g_return_val_if_fail (program != NULL, NULL);

  /* If it is an absolute path, or a relative path including subdirectories,
   * don't look in PATH.
   */
  if (g_path_is_absolute (program)
      || strchr (program, G_DIR_SEPARATOR) != NULL
#ifdef G_OS_WIN32
      || strchr (program, '/') != NULL
#endif
      )
    {
      if (g_file_test (program, G_FILE_TEST_IS_EXECUTABLE) &&
	  !g_file_test (program, G_FILE_TEST_IS_DIR))
        return g_strdup (program);
      else
        return NULL;
    }
  
  path = g_getenv ("PATH");
#if defined(G_OS_UNIX)
  if (path == NULL)
    {
      /* There is no 'PATH' in the environment.  The default
       * search path in GNU libc is the current directory followed by
       * the path 'confstr' returns for '_CS_PATH'.
       */
      
      /* In GLib we put . last, for security, and don't use the
       * unportable confstr(); UNIX98 does not actually specify
       * what to search if PATH is unset. POSIX may, dunno.
       */
      
      path = "/bin:/usr/bin:.";
    }
#else
  n = GetModuleFileNameW (NULL, wfilename, MAXPATHLEN);
  if (n > 0 && n < MAXPATHLEN)
    filename = g_utf16_to_utf8 (wfilename, -1, NULL, NULL, NULL);
  
  n = GetSystemDirectoryW (wsysdir, MAXPATHLEN);
  if (n > 0 && n < MAXPATHLEN)
    sysdir = g_utf16_to_utf8 (wsysdir, -1, NULL, NULL, NULL);
  
  n = GetWindowsDirectoryW (wwindir, MAXPATHLEN);
  if (n > 0 && n < MAXPATHLEN)
    windir = g_utf16_to_utf8 (wwindir, -1, NULL, NULL, NULL);
  
  if (filename)
    {
      appdir = g_path_get_dirname (filename);
      g_free (filename);
    }
  
  path = g_strdup (path);

  if (windir)
    {
      const gchar *tem = path;
      path = g_strconcat (windir, ";", path, NULL);
      g_free ((gchar *) tem);
      g_free (windir);
    }
  
  if (sysdir)
    {
      const gchar *tem = path;
      path = g_strconcat (sysdir, ";", path, NULL);
      g_free ((gchar *) tem);
      g_free (sysdir);
    }
  
  {
    const gchar *tem = path;
    path = g_strconcat (".;", path, NULL);
    g_free ((gchar *) tem);
  }
  
  if (appdir)
    {
      const gchar *tem = path;
      path = g_strconcat (appdir, ";", path, NULL);
      g_free ((gchar *) tem);
      g_free (appdir);
    }

  path_copy = path;
#endif
  
  len = strlen (program) + 1;
  pathlen = strlen (path);
  freeme = name = g_malloc (pathlen + len + 1);
  
  /* Copy the file name at the top, including '\0'  */
  memcpy (name + pathlen + 1, program, len);
  name = name + pathlen;
  /* And add the slash before the filename  */
  *name = G_DIR_SEPARATOR;
  
  p = path;
  do
    {
      char *startp;

      path = p;
      p = my_strchrnul (path, G_SEARCHPATH_SEPARATOR);

      if (p == path)
        /* Two adjacent colons, or a colon at the beginning or the end
         * of 'PATH' means to search the current directory.
         */
        startp = name + 1;
      else
        startp = memcpy (name - (p - path), path, p - path);

      if (g_file_test (startp, G_FILE_TEST_IS_EXECUTABLE) &&
	  !g_file_test (startp, G_FILE_TEST_IS_DIR))
        {
          gchar *ret;
          ret = g_strdup (startp);
          g_free (freeme);
#ifdef G_OS_WIN32
	  g_free ((gchar *) path_copy);
#endif
          return ret;
        }
    }
  while (*p++ != '\0');
  
  g_free (freeme);
#ifdef G_OS_WIN32
  g_free ((gchar *) path_copy);
#endif

  return NULL;
}

/* The functions below are defined this way for compatibility reasons.
 * See the note in gutils.h.
 */

/**
 * g_bit_nth_lsf:
 * @mask: a #gulong containing flags
 * @nth_bit: the index of the bit to start the search from
 *
 * Find the position of the first bit set in @mask, searching
 * from (but not including) @nth_bit upwards. Bits are numbered
 * from 0 (least significant) to sizeof(#gulong) * 8 - 1 (31 or 63,
 * usually). To start searching from the 0th bit, set @nth_bit to -1.
 *
 * Returns: the index of the first bit set which is higher than @nth_bit, or -1
 *    if no higher bits are set
 */
gint
(g_bit_nth_lsf) (gulong mask,
                 gint   nth_bit)
{
  return g_bit_nth_lsf_impl (mask, nth_bit);
}

/**
 * g_bit_nth_msf:
 * @mask: a #gulong containing flags
 * @nth_bit: the index of the bit to start the search from
 *
 * Find the position of the first bit set in @mask, searching
 * from (but not including) @nth_bit downwards. Bits are numbered
 * from 0 (least significant) to sizeof(#gulong) * 8 - 1 (31 or 63,
 * usually). To start searching from the last bit, set @nth_bit to
 * -1 or GLIB_SIZEOF_LONG * 8.
 *
 * Returns: the index of the first bit set which is lower than @nth_bit, or -1
 *    if no lower bits are set
 */
gint
(g_bit_nth_msf) (gulong mask,
                 gint   nth_bit)
{
  return g_bit_nth_msf_impl (mask, nth_bit);
}


/**
 * g_bit_storage:
 * @number: a #guint
 *
 * Gets the number of bits used to hold @number,
 * e.g. if @number is 4, 3 bits are needed.
 *
 * Returns: the number of bits used to hold @number
 */
guint
(g_bit_storage) (gulong number)
{
  return g_bit_storage_impl (number);
}

G_LOCK_DEFINE_STATIC (g_utils_global);

typedef struct
{
  gchar *user_name;
  gchar *real_name;
  gchar *home_dir;
} UserDatabaseEntry;

static  gchar   *g_user_data_dir = NULL;
static  gchar  **g_system_data_dirs = NULL;
static  gchar   *g_user_cache_dir = NULL;
static  gchar   *g_user_config_dir = NULL;
static  gchar  **g_system_config_dirs = NULL;

static  gchar  **g_user_special_dirs = NULL;

/* fifteen minutes of fame for everybody */
#define G_USER_DIRS_EXPIRE      15 * 60

#ifdef G_OS_WIN32

static gchar *
get_special_folder (int csidl)
{
  wchar_t path[MAX_PATH+1];
  HRESULT hr;
  LPITEMIDLIST pidl = NULL;
  BOOL b;
  gchar *retval = NULL;

  hr = SHGetSpecialFolderLocation (NULL, csidl, &pidl);
  if (hr == S_OK)
    {
      b = SHGetPathFromIDListW (pidl, path);
      if (b)
	retval = g_utf16_to_utf8 (path, -1, NULL, NULL, NULL);
      CoTaskMemFree (pidl);
    }
  return retval;
}

static char *
get_windows_directory_root (void)
{
  wchar_t wwindowsdir[MAX_PATH];

  if (GetWindowsDirectoryW (wwindowsdir, G_N_ELEMENTS (wwindowsdir)))
    {
      /* Usually X:\Windows, but in terminal server environments
       * might be an UNC path, AFAIK.
       */
      char *windowsdir = g_utf16_to_utf8 (wwindowsdir, -1, NULL, NULL, NULL);
      char *p;

      if (windowsdir == NULL)
	return g_strdup ("C:\\");

      p = (char *) g_path_skip_root (windowsdir);
      if (G_IS_DIR_SEPARATOR (p[-1]) && p[-2] != ':')
	p--;
      *p = '\0';
      return windowsdir;
    }
  else
    return g_strdup ("C:\\");
}

#endif

/* HOLDS: g_utils_global_lock */
static UserDatabaseEntry *
g_get_user_database_entry (void)
{
  static UserDatabaseEntry *entry;

  if (g_once_init_enter (&entry))
    {
      static UserDatabaseEntry e;

#ifdef G_OS_UNIX
      {
        struct passwd *pw = NULL;
        gpointer buffer = NULL;
        gint error;
        gchar *logname;

#  if defined (HAVE_GETPWUID_R)
        struct passwd pwd;
#    ifdef _SC_GETPW_R_SIZE_MAX
        /* This reurns the maximum length */
        glong bufsize = sysconf (_SC_GETPW_R_SIZE_MAX);

        if (bufsize < 0)
          bufsize = 64;
#    else /* _SC_GETPW_R_SIZE_MAX */
        glong bufsize = 64;
#    endif /* _SC_GETPW_R_SIZE_MAX */

        logname = (gchar *) g_getenv ("LOGNAME");

        do
          {
            g_free (buffer);
            /* we allocate 6 extra bytes to work around a bug in
             * Mac OS < 10.3. See #156446
             */
            buffer = g_malloc (bufsize + 6);
            errno = 0;

            if (logname) {
              error = getpwnam_r (logname, &pwd, buffer, bufsize, &pw);
              if (!pw || (pw->pw_uid != getuid ())) {
                /* LOGNAME is lying, fall back to looking up the uid */
                error = getpwuid_r (getuid (), &pwd, buffer, bufsize, &pw);
              }
            } else {
              error = getpwuid_r (getuid (), &pwd, buffer, bufsize, &pw);
            }
            error = error < 0 ? errno : error;

            if (!pw)
              {
                /* we bail out prematurely if the user id can't be found
                 * (should be pretty rare case actually), or if the buffer
                 * should be sufficiently big and lookups are still not
                 * successful.
                 */
                if (error == 0 || error == ENOENT)
                  {
                    g_warning ("getpwuid_r(): failed due to unknown user id (%lu)",
                               (gulong) getuid ());
                    break;
                  }
                if (bufsize > 32 * 1024)
                  {
                    g_warning ("getpwuid_r(): failed due to: %s.",
                               g_strerror (error));
                    break;
                  }

                bufsize *= 2;
              }
          }
        while (!pw);
#  endif /* HAVE_GETPWUID_R */

        if (!pw)
          {
            pw = getpwuid (getuid ());
          }
        if (pw)
          {
            e.user_name = g_strdup (pw->pw_name);

#ifndef __BIONIC__
            if (pw->pw_gecos && *pw->pw_gecos != '\0')
              {
                gchar **gecos_fields;
                gchar **name_parts;

                /* split the gecos field and substitute '&' */
                gecos_fields = g_strsplit (pw->pw_gecos, ",", 0);
                name_parts = g_strsplit (gecos_fields[0], "&", 0);
                pw->pw_name[0] = g_ascii_toupper (pw->pw_name[0]);
                e.real_name = g_strjoinv (pw->pw_name, name_parts);
                g_strfreev (gecos_fields);
                g_strfreev (name_parts);
              }
#endif

            if (!e.home_dir)
              e.home_dir = g_strdup (pw->pw_dir);
          }
        g_free (buffer);
      }

#endif /* G_OS_UNIX */

#ifdef G_OS_WIN32
      {
        guint len = UNLEN+1;
        wchar_t buffer[UNLEN+1];

        if (GetUserNameW (buffer, (LPDWORD) &len))
          {
            e.user_name = g_utf16_to_utf8 (buffer, -1, NULL, NULL, NULL);
            e.real_name = g_strdup (e.user_name);
          }
      }
#endif /* G_OS_WIN32 */

      if (!e.user_name)
        e.user_name = g_strdup ("somebody");
      if (!e.real_name)
        e.real_name = g_strdup ("Unknown");

      g_once_init_leave (&entry, &e);
    }

  return entry;
}

/**
 * g_get_user_name:
 *
 * Gets the user name of the current user. The encoding of the returned
 * string is system-defined. On UNIX, it might be the preferred file name
 * encoding, or something else, and there is no guarantee that it is even
 * consistent on a machine. On Windows, it is always UTF-8.
 *
 * Returns: (type filename): the user name of the current user.
 */
const gchar *
g_get_user_name (void)
{
  UserDatabaseEntry *entry;

  entry = g_get_user_database_entry ();

  return entry->user_name;
}

/**
 * g_get_real_name:
 *
 * Gets the real name of the user. This usually comes from the user's
 * entry in the `passwd` file. The encoding of the returned string is
 * system-defined. (On Windows, it is, however, always UTF-8.) If the
 * real user name cannot be determined, the string "Unknown" is 
 * returned.
 *
 * Returns: (type filename): the user's real name.
 */
const gchar *
g_get_real_name (void)
{
  UserDatabaseEntry *entry;

  entry = g_get_user_database_entry ();

  return entry->real_name;
}

/**
 * g_get_home_dir:
 *
 * Gets the current user's home directory.
 *
 * As with most UNIX tools, this function will return the value of the
 * `HOME` environment variable if it is set to an existing absolute path
 * name, falling back to the `passwd` file in the case that it is unset.
 *
 * If the path given in `HOME` is non-absolute, does not exist, or is
 * not a directory, the result is undefined.
 *
 * Before version 2.36 this function would ignore the `HOME` environment
 * variable, taking the value from the `passwd` database instead. This was
 * changed to increase the compatibility of GLib with other programs (and
 * the XDG basedir specification) and to increase testability of programs
 * based on GLib (by making it easier to run them from test frameworks).
 *
 * If your program has a strong requirement for either the new or the
 * old behaviour (and if you don't wish to increase your GLib
 * dependency to ensure that the new behaviour is in effect) then you
 * should either directly check the `HOME` environment variable yourself
 * or unset it before calling any functions in GLib.
 *
 * Returns: (type filename): the current user's home directory
 */
const gchar *
g_get_home_dir (void)
{
  static gchar *home_dir;

  if (g_once_init_enter (&home_dir))
    {
      gchar *tmp;

      /* We first check HOME and use it if it is set */
      tmp = g_strdup (g_getenv ("HOME"));

#ifdef G_OS_WIN32
      /* Only believe HOME if it is an absolute path and exists.
       *
       * We only do this check on Windows for a couple of reasons.
       * Historically, we only did it there because we used to ignore $HOME
       * on UNIX.  There are concerns about enabling it now on UNIX because
       * of things like autofs.  In short, if the user has a bogus value in
       * $HOME then they get what they pay for...
       */
      if (tmp)
        {
          if (!(g_path_is_absolute (tmp) &&
                g_file_test (tmp, G_FILE_TEST_IS_DIR)))
            {
              g_free (tmp);
              tmp = NULL;
            }
        }

      /* In case HOME is Unix-style (it happens), convert it to
       * Windows style.
       */
      if (tmp)
        {
          gchar *p;
          while ((p = strchr (tmp, '/')) != NULL)
            *p = '\\';
        }

      if (!tmp)
        {
          /* USERPROFILE is probably the closest equivalent to $HOME? */
          if (g_getenv ("USERPROFILE") != NULL)
            tmp = g_strdup (g_getenv ("USERPROFILE"));
        }

      if (!tmp)
        tmp = get_special_folder (CSIDL_PROFILE);

      if (!tmp)
        tmp = get_windows_directory_root ();
#endif /* G_OS_WIN32 */

      if (!tmp)
        {
          /* If we didn't get it from any of those methods, we will have
           * to read the user database entry.
           */
          UserDatabaseEntry *entry;

          entry = g_get_user_database_entry ();

          /* Strictly speaking, we should copy this, but we know that
           * neither will ever be freed, so don't bother...
           */
          tmp = entry->home_dir;
        }

      /* If we have been denied access to /etc/passwd (for example, by an
       * overly-zealous LSM), make up a junk value. The return value at this
       * point is explicitly documented as ‘undefined’. Memory management is as
       * immediately above: strictly this should be copied, but we know not
       * copying it is OK. */
      if (tmp == NULL)
        {
          g_warning ("Could not find home directory: $HOME is not set, and "
                     "user database could not be read.");
          tmp = "/";
        }

      g_once_init_leave (&home_dir, tmp);
    }

  return home_dir;
}

/**
 * g_get_tmp_dir:
 *
 * Gets the directory to use for temporary files.
 *
 * On UNIX, this is taken from the `TMPDIR` environment variable.
 * If the variable is not set, `P_tmpdir` is
 * used, as defined by the system C library. Failing that, a
 * hard-coded default of "/tmp" is returned.
 *
 * On Windows, the `TEMP` environment variable is used, with the
 * root directory of the Windows installation (eg: "C:\") used
 * as a default.
 *
 * The encoding of the returned string is system-defined. On Windows,
 * it is always UTF-8. The return value is never %NULL or the empty
 * string.
 *
 * Returns: (type filename): the directory to use for temporary files.
 */
const gchar *
g_get_tmp_dir (void)
{
  static gchar *tmp_dir;

  if (g_once_init_enter (&tmp_dir))
    {
      gchar *tmp;

#ifdef G_OS_WIN32
      tmp = g_strdup (g_getenv ("TEMP"));

      if (tmp == NULL || *tmp == '\0')
        {
          g_free (tmp);
          tmp = get_windows_directory_root ();
        }
#else /* G_OS_WIN32 */
      tmp = g_strdup (g_getenv ("TMPDIR"));

#ifdef P_tmpdir
      if (tmp == NULL || *tmp == '\0')
        {
          gsize k;
          g_free (tmp);
          tmp = g_strdup (P_tmpdir);
          k = strlen (tmp);
          if (k > 1 && G_IS_DIR_SEPARATOR (tmp[k - 1]))
            tmp[k - 1] = '\0';
        }
#endif /* P_tmpdir */

      if (tmp == NULL || *tmp == '\0')
        {
          g_free (tmp);
          tmp = g_strdup ("/tmp");
        }
#endif /* !G_OS_WIN32 */

      g_once_init_leave (&tmp_dir, tmp);
    }

  return tmp_dir;
}

/**
 * g_get_host_name:
 *
 * Return a name for the machine. 
 *
 * The returned name is not necessarily a fully-qualified domain name,
 * or even present in DNS or some other name service at all. It need
 * not even be unique on your local network or site, but usually it
 * is. Callers should not rely on the return value having any specific
 * properties like uniqueness for security purposes. Even if the name
 * of the machine is changed while an application is running, the
 * return value from this function does not change. The returned
 * string is owned by GLib and should not be modified or freed. If no
 * name can be determined, a default fixed string "localhost" is
 * returned.
 *
 * The encoding of the returned string is UTF-8.
 *
 * Returns: the host name of the machine.
 *
 * Since: 2.8
 */
const gchar *
g_get_host_name (void)
{
  static gchar *hostname;

  if (g_once_init_enter (&hostname))
    {
      gboolean failed;
      gchar *utmp;

#ifndef G_OS_WIN32
      gchar *tmp = g_malloc (sizeof (gchar) * 100);
      failed = (gethostname (tmp, sizeof (gchar) * 100) == -1);
      if (failed)
        g_clear_pointer (&tmp, g_free);
      utmp = tmp;
#else
      wchar_t tmp[MAX_COMPUTERNAME_LENGTH + 1];
      DWORD size = sizeof (tmp) / sizeof (tmp[0]);
      failed = (!GetComputerNameW (tmp, &size));
      if (!failed)
        utmp = g_utf16_to_utf8 (tmp, size, NULL, NULL, NULL);
      if (utmp == NULL)
        failed = TRUE;
#endif

      g_once_init_leave (&hostname, failed ? g_strdup ("localhost") : utmp);
    }

  return hostname;
}

G_LOCK_DEFINE_STATIC (g_prgname);
static gchar *g_prgname = NULL;

/**
 * g_get_prgname:
 *
 * Gets the name of the program. This name should not be localized,
 * in contrast to g_get_application_name().
 *
 * If you are using #GApplication the program name is set in
 * g_application_run(). In case of GDK or GTK+ it is set in
 * gdk_init(), which is called by gtk_init() and the
 * #GtkApplication::startup handler. The program name is found by
 * taking the last component of @argv[0].
 *
 * Returns: the name of the program. The returned string belongs 
 *     to GLib and must not be modified or freed.
 */
const gchar*
g_get_prgname (void)
{
  gchar* retval;

  G_LOCK (g_prgname);
#ifdef G_OS_WIN32
  if (g_prgname == NULL)
    {
      static gboolean beenhere = FALSE;

      if (!beenhere)
	{
	  gchar *utf8_buf = NULL;
	  wchar_t buf[MAX_PATH+1];

	  beenhere = TRUE;
	  if (GetModuleFileNameW (GetModuleHandle (NULL),
				  buf, G_N_ELEMENTS (buf)) > 0)
	    utf8_buf = g_utf16_to_utf8 (buf, -1, NULL, NULL, NULL);

	  if (utf8_buf)
	    {
	      g_prgname = g_path_get_basename (utf8_buf);
	      g_free (utf8_buf);
	    }
	}
    }
#endif
  retval = g_prgname;
  G_UNLOCK (g_prgname);

  return retval;
}

/**
 * g_set_prgname:
 * @prgname: the name of the program.
 *
 * Sets the name of the program. This name should not be localized,
 * in contrast to g_set_application_name().
 *
 * If you are using #GApplication the program name is set in
 * g_application_run(). In case of GDK or GTK+ it is set in
 * gdk_init(), which is called by gtk_init() and the
 * #GtkApplication::startup handler. The program name is found by
 * taking the last component of @argv[0].
 *
 * Note that for thread-safety reasons this function can only be called once.
 */
void
g_set_prgname (const gchar *prgname)
{
  G_LOCK (g_prgname);
  g_free (g_prgname);
  g_prgname = g_strdup (prgname);
  G_UNLOCK (g_prgname);
}

G_LOCK_DEFINE_STATIC (g_application_name);
static gchar *g_application_name = NULL;

/**
 * g_get_application_name:
 * 
 * Gets a human-readable name for the application, as set by
 * g_set_application_name(). This name should be localized if
 * possible, and is intended for display to the user.  Contrast with
 * g_get_prgname(), which gets a non-localized name. If
 * g_set_application_name() has not been called, returns the result of
 * g_get_prgname() (which may be %NULL if g_set_prgname() has also not
 * been called).
 * 
 * Returns: human-readable application name. may return %NULL
 *
 * Since: 2.2
 **/
const gchar *
g_get_application_name (void)
{
  gchar* retval;

  G_LOCK (g_application_name);
  retval = g_application_name;
  G_UNLOCK (g_application_name);

  if (retval == NULL)
    return g_get_prgname ();
  
  return retval;
}

/**
 * g_set_application_name:
 * @application_name: localized name of the application
 *
 * Sets a human-readable name for the application. This name should be
 * localized if possible, and is intended for display to the user.
 * Contrast with g_set_prgname(), which sets a non-localized name.
 * g_set_prgname() will be called automatically by gtk_init(),
 * but g_set_application_name() will not.
 *
 * Note that for thread safety reasons, this function can only
 * be called once.
 *
 * The application name will be used in contexts such as error messages,
 * or when displaying an application's name in the task list.
 * 
 * Since: 2.2
 **/
void
g_set_application_name (const gchar *application_name)
{
  gboolean already_set = FALSE;
	
  G_LOCK (g_application_name);
  if (g_application_name)
    already_set = TRUE;
  else
    g_application_name = g_strdup (application_name);
  G_UNLOCK (g_application_name);

  if (already_set)
    g_warning ("g_set_application_name() called multiple times");
}

/**
 * g_get_user_data_dir:
 * 
 * Returns a base directory in which to access application data such
 * as icons that is customized for a particular user.  
 *
 * On UNIX platforms this is determined using the mechanisms described
 * in the
 * [XDG Base Directory Specification](http://www.freedesktop.org/Standards/basedir-spec).
 * In this case the directory retrieved will be `XDG_DATA_HOME`.
 *
 * On Windows it follows XDG Base Directory Specification if `XDG_DATA_HOME`
 * is defined. If `XDG_DATA_HOME` is undefined, the folder to use for local (as
 * opposed to roaming) application data is used instead. See the
 * [documentation for `CSIDL_LOCAL_APPDATA`](https://msdn.microsoft.com/en-us/library/windows/desktop/bb762494%28v=vs.85%29.aspx#csidl_local_appdata).
 * Note that in this case on Windows it will be the same
 * as what g_get_user_config_dir() returns.
 *
 * Returns: (type filename): a string owned by GLib that must not be modified
 *               or freed.
 * Since: 2.6
 **/
const gchar *
g_get_user_data_dir (void)
{
  gchar *data_dir = NULL;

  G_LOCK (g_utils_global);

  if (!g_user_data_dir)
    {
      const gchar *data_dir_env = g_getenv ("XDG_DATA_HOME");

      if (data_dir_env && data_dir_env[0])
        data_dir = g_strdup (data_dir_env);
#ifdef G_OS_WIN32
      else
        data_dir = get_special_folder (CSIDL_LOCAL_APPDATA);
#endif
      if (!data_dir || !data_dir[0])
	{
          const gchar *home_dir = g_get_home_dir ();

          if (home_dir)
            data_dir = g_build_filename (home_dir, ".local", "share", NULL);
	  else
            data_dir = g_build_filename (g_get_tmp_dir (), g_get_user_name (), ".local", "share", NULL);
	}

      g_user_data_dir = data_dir;
    }
  else
    data_dir = g_user_data_dir;

  G_UNLOCK (g_utils_global);

  return data_dir;
}

static void
g_init_user_config_dir (void)
{
  gchar *config_dir = NULL;

  if (!g_user_config_dir)
    {
      const gchar *config_dir_env = g_getenv ("XDG_CONFIG_HOME");

      if (config_dir_env && config_dir_env[0])
	config_dir = g_strdup (config_dir_env);
#ifdef G_OS_WIN32
      else
        config_dir = get_special_folder (CSIDL_LOCAL_APPDATA);
#endif
      if (!config_dir || !config_dir[0])
	{
          const gchar *home_dir = g_get_home_dir ();

          if (home_dir)
            config_dir = g_build_filename (home_dir, ".config", NULL);
	  else
            config_dir = g_build_filename (g_get_tmp_dir (), g_get_user_name (), ".config", NULL);
	}

      g_user_config_dir = config_dir;
    }
}

/**
 * g_get_user_config_dir:
 * 
 * Returns a base directory in which to store user-specific application 
 * configuration information such as user preferences and settings. 
 *
 * On UNIX platforms this is determined using the mechanisms described
 * in the
 * [XDG Base Directory Specification](http://www.freedesktop.org/Standards/basedir-spec).
 * In this case the directory retrieved will be `XDG_CONFIG_HOME`.
 *
 * On Windows it follows XDG Base Directory Specification if `XDG_CONFIG_HOME` is defined.
 * If `XDG_CONFIG_HOME` is undefined, the folder to use for local (as opposed
 * to roaming) application data is used instead. See the
 * [documentation for `CSIDL_LOCAL_APPDATA`](https://msdn.microsoft.com/en-us/library/windows/desktop/bb762494%28v=vs.85%29.aspx#csidl_local_appdata).
 * Note that in this case on Windows it will be  the same
 * as what g_get_user_data_dir() returns.
 *
 * Returns: (type filename): a string owned by GLib that must not be modified
 *               or freed.
 * Since: 2.6
 **/
const gchar *
g_get_user_config_dir (void)
{
  G_LOCK (g_utils_global);

  g_init_user_config_dir ();

  G_UNLOCK (g_utils_global);

  return g_user_config_dir;
}

/**
 * g_get_user_cache_dir:
 * 
 * Returns a base directory in which to store non-essential, cached
 * data specific to particular user.
 *
 * On UNIX platforms this is determined using the mechanisms described
 * in the
 * [XDG Base Directory Specification](http://www.freedesktop.org/Standards/basedir-spec).
 * In this case the directory retrieved will be `XDG_CACHE_HOME`.
 *
 * On Windows it follows XDG Base Directory Specification if `XDG_CACHE_HOME` is defined.
 * If `XDG_CACHE_HOME` is undefined, the directory that serves as a common
 * repository for temporary Internet files is used instead. A typical path is
 * `C:\Documents and Settings\username\Local Settings\Temporary Internet Files`.
 * See the [documentation for `CSIDL_INTERNET_CACHE`](https://msdn.microsoft.com/en-us/library/windows/desktop/bb762494%28v=vs.85%29.aspx#csidl_internet_cache).
 *
 * Returns: (type filename): a string owned by GLib that must not be modified
 *               or freed.
 * Since: 2.6
 **/
const gchar *
g_get_user_cache_dir (void)
{
  gchar *cache_dir = NULL;

  G_LOCK (g_utils_global);

  if (!g_user_cache_dir)
    {
      const gchar *cache_dir_env = g_getenv ("XDG_CACHE_HOME");

      if (cache_dir_env && cache_dir_env[0])
        cache_dir = g_strdup (cache_dir_env);
#ifdef G_OS_WIN32
      else
        cache_dir = get_special_folder (CSIDL_INTERNET_CACHE);
#endif
      if (!cache_dir || !cache_dir[0])
	{
          const gchar *home_dir = g_get_home_dir ();

          if (home_dir)
            cache_dir = g_build_filename (home_dir, ".cache", NULL);
	  else
            cache_dir = g_build_filename (g_get_tmp_dir (), g_get_user_name (), ".cache", NULL);
	}
      g_user_cache_dir = cache_dir;
    }
  else
    cache_dir = g_user_cache_dir;

  G_UNLOCK (g_utils_global);

  return cache_dir;
}

/**
 * g_get_user_runtime_dir:
 *
 * Returns a directory that is unique to the current user on the local
 * system.
 *
 * This is determined using the mechanisms described
 * in the 
 * [XDG Base Directory Specification](http://www.freedesktop.org/Standards/basedir-spec).
 * This is the directory
 * specified in the `XDG_RUNTIME_DIR` environment variable.
 * In the case that this variable is not set, we return the value of
 * g_get_user_cache_dir(), after verifying that it exists.
 *
 * Returns: (type filename): a string owned by GLib that must not be
 *     modified or freed.
 *
 * Since: 2.28
 **/
const gchar *
g_get_user_runtime_dir (void)
{
  static const gchar *runtime_dir;

  if (g_once_init_enter (&runtime_dir))
    {
      const gchar *dir;

      dir = g_strdup (getenv ("XDG_RUNTIME_DIR"));

      if (dir == NULL)
        {
          /* No need to strdup this one since it is valid forever. */
          dir = g_get_user_cache_dir ();

          /* The user should be able to rely on the directory existing
           * when the function returns.  Probably it already does, but
           * let's make sure.  Just do mkdir() directly since it will be
           * no more expensive than a stat() in the case that the
           * directory already exists and is a lot easier.
           *
           * $XDG_CACHE_HOME is probably ~/.cache/ so as long as $HOME
           * exists this will work.  If the user changed $XDG_CACHE_HOME
           * then they can make sure that it exists...
           */
          (void) g_mkdir (dir, 0700);
        }

      g_assert (dir != NULL);

      g_once_init_leave (&runtime_dir, dir);
    }

  return runtime_dir;
}

#ifdef HAVE_CARBON

static gchar *
find_folder (OSType type)
{
  gchar *filename = NULL;
  FSRef  found;

  if (FSFindFolder (kUserDomain, type, kDontCreateFolder, &found) == noErr)
    {
      CFURLRef url = CFURLCreateFromFSRef (kCFAllocatorSystemDefault, &found);

      if (url)
	{
	  CFStringRef path = CFURLCopyFileSystemPath (url, kCFURLPOSIXPathStyle);

	  if (path)
	    {
	      filename = g_strdup (CFStringGetCStringPtr (path, kCFStringEncodingUTF8));

	      if (! filename)
		{
		  filename = g_new0 (gchar, CFStringGetLength (path) * 3 + 1);

		  CFStringGetCString (path, filename,
				      CFStringGetLength (path) * 3 + 1,
				      kCFStringEncodingUTF8);
		}

	      CFRelease (path);
	    }

	  CFRelease (url);
	}
    }

  return filename;
}

static void
load_user_special_dirs (void)
{
  g_user_special_dirs[G_USER_DIRECTORY_DESKTOP] = find_folder (kDesktopFolderType);
  g_user_special_dirs[G_USER_DIRECTORY_DOCUMENTS] = find_folder (kDocumentsFolderType);
  g_user_special_dirs[G_USER_DIRECTORY_DOWNLOAD] = find_folder (kDesktopFolderType); /* XXX correct ? */
  g_user_special_dirs[G_USER_DIRECTORY_MUSIC] = find_folder (kMusicDocumentsFolderType);
  g_user_special_dirs[G_USER_DIRECTORY_PICTURES] = find_folder (kPictureDocumentsFolderType);
  g_user_special_dirs[G_USER_DIRECTORY_PUBLIC_SHARE] = NULL;
  g_user_special_dirs[G_USER_DIRECTORY_TEMPLATES] = NULL;
  g_user_special_dirs[G_USER_DIRECTORY_VIDEOS] = find_folder (kMovieDocumentsFolderType);
}

#elif defined(G_OS_WIN32)

static void
load_user_special_dirs (void)
{
  typedef HRESULT (WINAPI *t_SHGetKnownFolderPath) (const GUID *rfid,
						    DWORD dwFlags,
						    HANDLE hToken,
						    PWSTR *ppszPath);
  t_SHGetKnownFolderPath p_SHGetKnownFolderPath;

  static const GUID FOLDERID_Downloads =
    { 0x374de290, 0x123f, 0x4565, { 0x91, 0x64, 0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b } };
  static const GUID FOLDERID_Public =
    { 0xDFDF76A2, 0xC82A, 0x4D63, { 0x90, 0x6A, 0x56, 0x44, 0xAC, 0x45, 0x73, 0x85 } };

  wchar_t *wcp;

  p_SHGetKnownFolderPath = (t_SHGetKnownFolderPath) GetProcAddress (GetModuleHandle ("shell32.dll"),
								    "SHGetKnownFolderPath");

  g_user_special_dirs[G_USER_DIRECTORY_DESKTOP] = get_special_folder (CSIDL_DESKTOPDIRECTORY);
  g_user_special_dirs[G_USER_DIRECTORY_DOCUMENTS] = get_special_folder (CSIDL_PERSONAL);

  if (p_SHGetKnownFolderPath == NULL)
    {
      g_user_special_dirs[G_USER_DIRECTORY_DOWNLOAD] = get_special_folder (CSIDL_DESKTOPDIRECTORY);
    }
  else
    {
      wcp = NULL;
      (*p_SHGetKnownFolderPath) (&FOLDERID_Downloads, 0, NULL, &wcp);
      if (wcp)
        {
          g_user_special_dirs[G_USER_DIRECTORY_DOWNLOAD] = g_utf16_to_utf8 (wcp, -1, NULL, NULL, NULL);
          if (g_user_special_dirs[G_USER_DIRECTORY_DOWNLOAD] == NULL)
              g_user_special_dirs[G_USER_DIRECTORY_DOWNLOAD] = get_special_folder (CSIDL_DESKTOPDIRECTORY);
          CoTaskMemFree (wcp);
        }
      else
          g_user_special_dirs[G_USER_DIRECTORY_DOWNLOAD] = get_special_folder (CSIDL_DESKTOPDIRECTORY);
    }

  g_user_special_dirs[G_USER_DIRECTORY_MUSIC] = get_special_folder (CSIDL_MYMUSIC);
  g_user_special_dirs[G_USER_DIRECTORY_PICTURES] = get_special_folder (CSIDL_MYPICTURES);

  if (p_SHGetKnownFolderPath == NULL)
    {
      /* XXX */
      g_user_special_dirs[G_USER_DIRECTORY_PUBLIC_SHARE] = get_special_folder (CSIDL_COMMON_DOCUMENTS);
    }
  else
    {
      wcp = NULL;
      (*p_SHGetKnownFolderPath) (&FOLDERID_Public, 0, NULL, &wcp);
      if (wcp)
        {
          g_user_special_dirs[G_USER_DIRECTORY_PUBLIC_SHARE] = g_utf16_to_utf8 (wcp, -1, NULL, NULL, NULL);
          if (g_user_special_dirs[G_USER_DIRECTORY_PUBLIC_SHARE] == NULL)
              g_user_special_dirs[G_USER_DIRECTORY_PUBLIC_SHARE] = get_special_folder (CSIDL_COMMON_DOCUMENTS);
          CoTaskMemFree (wcp);
        }
      else
          g_user_special_dirs[G_USER_DIRECTORY_PUBLIC_SHARE] = get_special_folder (CSIDL_COMMON_DOCUMENTS);
    }
  
  g_user_special_dirs[G_USER_DIRECTORY_TEMPLATES] = get_special_folder (CSIDL_TEMPLATES);
  g_user_special_dirs[G_USER_DIRECTORY_VIDEOS] = get_special_folder (CSIDL_MYVIDEO);
}

#else /* default is unix */

/* adapted from xdg-user-dir-lookup.c
 *
 * Copyright (C) 2007 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions: 
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software. 
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
static void
load_user_special_dirs (void)
{
  gchar *config_file;
  gchar *data;
  gchar **lines;
  gint n_lines, i;
  
  g_init_user_config_dir ();
  config_file = g_build_filename (g_user_config_dir,
                                  "user-dirs.dirs",
                                  NULL);
  
  if (!g_file_get_contents (config_file, &data, NULL, NULL))
    {
      g_free (config_file);
      return;
    }

  lines = g_strsplit (data, "\n", -1);
  n_lines = g_strv_length (lines);
  g_free (data);
  
  for (i = 0; i < n_lines; i++)
    {
      gchar *buffer = lines[i];
      gchar *d, *p;
      gint len;
      gboolean is_relative = FALSE;
      GUserDirectory directory;

      /* Remove newline at end */
      len = strlen (buffer);
      if (len > 0 && buffer[len - 1] == '\n')
	buffer[len - 1] = 0;
      
      p = buffer;
      while (*p == ' ' || *p == '\t')
	p++;
      
      if (strncmp (p, "XDG_DESKTOP_DIR", strlen ("XDG_DESKTOP_DIR")) == 0)
        {
          directory = G_USER_DIRECTORY_DESKTOP;
          p += strlen ("XDG_DESKTOP_DIR");
        }
      else if (strncmp (p, "XDG_DOCUMENTS_DIR", strlen ("XDG_DOCUMENTS_DIR")) == 0)
        {
          directory = G_USER_DIRECTORY_DOCUMENTS;
          p += strlen ("XDG_DOCUMENTS_DIR");
        }
      else if (strncmp (p, "XDG_DOWNLOAD_DIR", strlen ("XDG_DOWNLOAD_DIR")) == 0)
        {
          directory = G_USER_DIRECTORY_DOWNLOAD;
          p += strlen ("XDG_DOWNLOAD_DIR");
        }
      else if (strncmp (p, "XDG_MUSIC_DIR", strlen ("XDG_MUSIC_DIR")) == 0)
        {
          directory = G_USER_DIRECTORY_MUSIC;
          p += strlen ("XDG_MUSIC_DIR");
        }
      else if (strncmp (p, "XDG_PICTURES_DIR", strlen ("XDG_PICTURES_DIR")) == 0)
        {
          directory = G_USER_DIRECTORY_PICTURES;
          p += strlen ("XDG_PICTURES_DIR");
        }
      else if (strncmp (p, "XDG_PUBLICSHARE_DIR", strlen ("XDG_PUBLICSHARE_DIR")) == 0)
        {
          directory = G_USER_DIRECTORY_PUBLIC_SHARE;
          p += strlen ("XDG_PUBLICSHARE_DIR");
        }
      else if (strncmp (p, "XDG_TEMPLATES_DIR", strlen ("XDG_TEMPLATES_DIR")) == 0)
        {
          directory = G_USER_DIRECTORY_TEMPLATES;
          p += strlen ("XDG_TEMPLATES_DIR");
        }
      else if (strncmp (p, "XDG_VIDEOS_DIR", strlen ("XDG_VIDEOS_DIR")) == 0)
        {
          directory = G_USER_DIRECTORY_VIDEOS;
          p += strlen ("XDG_VIDEOS_DIR");
        }
      else
	continue;

      while (*p == ' ' || *p == '\t')
	p++;

      if (*p != '=')
	continue;
      p++;

      while (*p == ' ' || *p == '\t')
	p++;

      if (*p != '"')
	continue;
      p++;

      if (strncmp (p, "$HOME", 5) == 0)
	{
	  p += 5;
	  is_relative = TRUE;
	}
      else if (*p != '/')
	continue;

      d = strrchr (p, '"');
      if (!d)
        continue;
      *d = 0;

      d = p;
      
      /* remove trailing slashes */
      len = strlen (d);
      if (d[len - 1] == '/')
        d[len - 1] = 0;
      
      if (is_relative)
        {
          g_user_special_dirs[directory] = g_build_filename (g_get_home_dir (), d, NULL);
        }
      else
	g_user_special_dirs[directory] = g_strdup (d);
    }

  g_strfreev (lines);
  g_free (config_file);
}

#endif /* platform-specific load_user_special_dirs implementations */


/**
 * g_reload_user_special_dirs_cache:
 *
 * Resets the cache used for g_get_user_special_dir(), so
 * that the latest on-disk version is used. Call this only
 * if you just changed the data on disk yourself.
 *
 * Due to threadsafety issues this may cause leaking of strings
 * that were previously returned from g_get_user_special_dir()
 * that can't be freed. We ensure to only leak the data for
 * the directories that actually changed value though.
 *
 * Since: 2.22
 */
void
g_reload_user_special_dirs_cache (void)
{
  int i;

  G_LOCK (g_utils_global);

  if (g_user_special_dirs != NULL)
    {
      /* save a copy of the pointer, to check if some memory can be preserved */
      char **old_g_user_special_dirs = g_user_special_dirs;
      char *old_val;

      /* recreate and reload our cache */
      g_user_special_dirs = g_new0 (gchar *, G_USER_N_DIRECTORIES);
      load_user_special_dirs ();

      /* only leak changed directories */
      for (i = 0; i < G_USER_N_DIRECTORIES; i++)
        {
          old_val = old_g_user_special_dirs[i];
          if (g_user_special_dirs[i] == NULL)
            {
              g_user_special_dirs[i] = old_val;
            }
          else if (g_strcmp0 (old_val, g_user_special_dirs[i]) == 0)
            {
              /* don't leak */
              g_free (g_user_special_dirs[i]);
              g_user_special_dirs[i] = old_val;
            }
          else
            g_free (old_val);
        }

      /* free the old array */
      g_free (old_g_user_special_dirs);
    }

  G_UNLOCK (g_utils_global);
}

/**
 * g_get_user_special_dir:
 * @directory: the logical id of special directory
 *
 * Returns the full path of a special directory using its logical id.
 *
 * On UNIX this is done using the XDG special user directories.
 * For compatibility with existing practise, %G_USER_DIRECTORY_DESKTOP
 * falls back to `$HOME/Desktop` when XDG special user directories have
 * not been set up. 
 *
 * Depending on the platform, the user might be able to change the path
 * of the special directory without requiring the session to restart; GLib
 * will not reflect any change once the special directories are loaded.
 *
 * Returns: (type filename): the path to the specified special directory, or
 *   %NULL if the logical id was not found. The returned string is owned by
 *   GLib and should not be modified or freed.
 *
 * Since: 2.14
 */
const gchar *
g_get_user_special_dir (GUserDirectory directory)
{
  g_return_val_if_fail (directory >= G_USER_DIRECTORY_DESKTOP &&
                        directory < G_USER_N_DIRECTORIES, NULL);

  G_LOCK (g_utils_global);

  if (G_UNLIKELY (g_user_special_dirs == NULL))
    {
      g_user_special_dirs = g_new0 (gchar *, G_USER_N_DIRECTORIES);

      load_user_special_dirs ();

      /* Special-case desktop for historical compatibility */
      if (g_user_special_dirs[G_USER_DIRECTORY_DESKTOP] == NULL)
        g_user_special_dirs[G_USER_DIRECTORY_DESKTOP] = g_build_filename (g_get_home_dir (), "Desktop", NULL);
    }

  G_UNLOCK (g_utils_global);

  return g_user_special_dirs[directory];
}

#ifdef G_OS_WIN32

#undef g_get_system_data_dirs

static HMODULE
get_module_for_address (gconstpointer address)
{
  /* Holds the g_utils_global lock */

  static gboolean beenhere = FALSE;
  typedef BOOL (WINAPI *t_GetModuleHandleExA) (DWORD, LPCTSTR, HMODULE *);
  static t_GetModuleHandleExA p_GetModuleHandleExA = NULL;
  HMODULE hmodule = NULL;

  if (!address)
    return NULL;

  if (!beenhere)
    {
      p_GetModuleHandleExA =
	(t_GetModuleHandleExA) GetProcAddress (GetModuleHandle ("kernel32.dll"),
					       "GetModuleHandleExA");
      beenhere = TRUE;
    }

  if (p_GetModuleHandleExA == NULL ||
      !(*p_GetModuleHandleExA) (GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
				address, &hmodule))
    {
      MEMORY_BASIC_INFORMATION mbi;
      VirtualQuery (address, &mbi, sizeof (mbi));
      hmodule = (HMODULE) mbi.AllocationBase;
    }

  return hmodule;
}

static gchar *
get_module_share_dir (gconstpointer address)
{
  HMODULE hmodule;
  gchar *filename;
  gchar *retval;

  hmodule = get_module_for_address (address);
  if (hmodule == NULL)
    return NULL;

  filename = g_win32_get_package_installation_directory_of_module (hmodule);
  retval = g_build_filename (filename, "share", NULL);
  g_free (filename);

  return retval;
}

static const gchar * const *
g_win32_get_system_data_dirs_for_module_real (void (*address_of_function)(void))
{
  GArray *data_dirs;
  HMODULE hmodule;
  static GHashTable *per_module_data_dirs = NULL;
  gchar **retval;
  gchar *p;
  gchar *exe_root;

  hmodule = NULL;
  if (address_of_function)
    {
      G_LOCK (g_utils_global);
      hmodule = get_module_for_address (address_of_function);
      if (hmodule != NULL)
	{
	  if (per_module_data_dirs == NULL)
	    per_module_data_dirs = g_hash_table_new (NULL, NULL);
	  else
	    {
	      retval = g_hash_table_lookup (per_module_data_dirs, hmodule);
	      
	      if (retval != NULL)
		{
		  G_UNLOCK (g_utils_global);
		  return (const gchar * const *) retval;
		}
	    }
	}
    }

  data_dirs = g_array_new (TRUE, TRUE, sizeof (char *));

  /* Documents and Settings\All Users\Application Data */
  p = get_special_folder (CSIDL_COMMON_APPDATA);
  if (p)
    g_array_append_val (data_dirs, p);
  
  /* Documents and Settings\All Users\Documents */
  p = get_special_folder (CSIDL_COMMON_DOCUMENTS);
  if (p)
    g_array_append_val (data_dirs, p);
	
  /* Using the above subfolders of Documents and Settings perhaps
   * makes sense from a Windows perspective.
   *
   * But looking at the actual use cases of this function in GTK+
   * and GNOME software, what we really want is the "share"
   * subdirectory of the installation directory for the package
   * our caller is a part of.
   *
   * The address_of_function parameter, if non-NULL, points to a
   * function in the calling module. Use that to determine that
   * module's installation folder, and use its "share" subfolder.
   *
   * Additionally, also use the "share" subfolder of the installation
   * locations of GLib and the .exe file being run.
   *
   * To guard against none of the above being what is really wanted,
   * callers of this function should have Win32-specific code to look
   * up their installation folder themselves, and handle a subfolder
   * "share" of it in the same way as the folders returned from this
   * function.
   */

  p = get_module_share_dir (address_of_function);
  if (p)
    g_array_append_val (data_dirs, p);
    
  if (glib_dll != NULL)
    {
      gchar *glib_root = g_win32_get_package_installation_directory_of_module (glib_dll);
      p = g_build_filename (glib_root, "share", NULL);
      if (p)
	g_array_append_val (data_dirs, p);
      g_free (glib_root);
    }
  
  exe_root = g_win32_get_package_installation_directory_of_module (NULL);
  p = g_build_filename (exe_root, "share", NULL);
  if (p)
    g_array_append_val (data_dirs, p);
  g_free (exe_root);

  retval = (gchar **) g_array_free (data_dirs, FALSE);

  if (address_of_function)
    {
      if (hmodule != NULL)
	g_hash_table_insert (per_module_data_dirs, hmodule, retval);
      G_UNLOCK (g_utils_global);
    }

  return (const gchar * const *) retval;
}

const gchar * const *
g_win32_get_system_data_dirs_for_module (void (*address_of_function)(void))
{
  gboolean should_call_g_get_system_data_dirs;

  should_call_g_get_system_data_dirs = TRUE;
  /* These checks are the same as the ones that g_get_system_data_dirs() does.
   * Please keep them in sync.
   */
  G_LOCK (g_utils_global);

  if (!g_system_data_dirs)
    {
      const gchar *data_dirs = g_getenv ("XDG_DATA_DIRS");

      if (!data_dirs || !data_dirs[0])
        should_call_g_get_system_data_dirs = FALSE;
    }

  G_UNLOCK (g_utils_global);

  /* There is a subtle difference between g_win32_get_system_data_dirs_for_module (NULL),
   * which is what GLib code can normally call,
   * and g_win32_get_system_data_dirs_for_module (&_g_win32_get_system_data_dirs),
   * which is what the inline function used by non-GLib code calls.
   * The former gets prefix relative to currently-running executable,
   * the latter - relative to the module that calls _g_win32_get_system_data_dirs()
   * (disguised as g_get_system_data_dirs()), which could be an executable or
   * a DLL that is located somewhere else.
   * This is why that inline function in gutils.h exists, and why we can't just
   * call g_get_system_data_dirs() from there - because we need to get the address
   * local to the non-GLib caller-module.
   */

  /*
   * g_get_system_data_dirs() will fall back to calling
   * g_win32_get_system_data_dirs_for_module_real(NULL) if XDG_DATA_DIRS is NULL
   * or an empty string. The checks above ensure that we do not call it in such
   * cases and use the address_of_function that we've been given by the inline function.
   * The reason we're calling g_get_system_data_dirs /at all/ is to give
   * XDG_DATA_DIRS precedence (if it is set).
   */
  if (should_call_g_get_system_data_dirs)
    return g_get_system_data_dirs ();

  return g_win32_get_system_data_dirs_for_module_real (address_of_function);
}

#endif

/**
 * g_get_system_data_dirs:
 * 
 * Returns an ordered list of base directories in which to access 
 * system-wide application data.
 *
 * On UNIX platforms this is determined using the mechanisms described
 * in the
 * [XDG Base Directory Specification](http://www.freedesktop.org/Standards/basedir-spec)
 * In this case the list of directories retrieved will be `XDG_DATA_DIRS`.
 *
 * On Windows it follows XDG Base Directory Specification if `XDG_DATA_DIRS` is defined.
 * If `XDG_DATA_DIRS` is undefined,
 * the first elements in the list are the Application Data
 * and Documents folders for All Users. (These can be determined only
 * on Windows 2000 or later and are not present in the list on other
 * Windows versions.) See documentation for CSIDL_COMMON_APPDATA and
 * CSIDL_COMMON_DOCUMENTS.
 *
 * Then follows the "share" subfolder in the installation folder for
 * the package containing the DLL that calls this function, if it can
 * be determined.
 * 
 * Finally the list contains the "share" subfolder in the installation
 * folder for GLib, and in the installation folder for the package the
 * application's .exe file belongs to.
 *
 * The installation folders above are determined by looking up the
 * folder where the module (DLL or EXE) in question is located. If the
 * folder's name is "bin", its parent is used, otherwise the folder
 * itself.
 *
 * Note that on Windows the returned list can vary depending on where
 * this function is called.
 *
 * Returns: (array zero-terminated=1) (element-type filename) (transfer none):
 *     a %NULL-terminated array of strings owned by GLib that must not be
 *     modified or freed.
 * 
 * Since: 2.6
 **/
const gchar * const * 
g_get_system_data_dirs (void)
{
  gchar **data_dir_vector;

  /* These checks are the same as the ones that g_win32_get_system_data_dirs_for_module()
   * does. Please keep them in sync.
   */
  G_LOCK (g_utils_global);

  if (!g_system_data_dirs)
    {
      gchar *data_dirs = (gchar *) g_getenv ("XDG_DATA_DIRS");

#ifndef G_OS_WIN32
      if (!data_dirs || !data_dirs[0])
          data_dirs = "/usr/local/share/:/usr/share/";

      data_dir_vector = g_strsplit (data_dirs, G_SEARCHPATH_SEPARATOR_S, 0);
#else
      if (!data_dirs || !data_dirs[0])
        data_dir_vector = g_strdupv ((gchar **) g_win32_get_system_data_dirs_for_module_real (NULL));
      else
        data_dir_vector = g_strsplit (data_dirs, G_SEARCHPATH_SEPARATOR_S, 0);
#endif

      g_system_data_dirs = data_dir_vector;
    }
  else
    data_dir_vector = g_system_data_dirs;

  G_UNLOCK (g_utils_global);

  return (const gchar * const *) data_dir_vector;
}

/**
 * g_get_system_config_dirs:
 * 
 * Returns an ordered list of base directories in which to access 
 * system-wide configuration information.
 *
 * On UNIX platforms this is determined using the mechanisms described
 * in the
 * [XDG Base Directory Specification](http://www.freedesktop.org/Standards/basedir-spec).
 * In this case the list of directories retrieved will be `XDG_CONFIG_DIRS`.
 *
 * On Windows it follows XDG Base Directory Specification if `XDG_CONFIG_DIRS` is defined.
 * If `XDG_CONFIG_DIRS` is undefined, the directory that contains application
 * data for all users is used instead. A typical path is
 * `C:\Documents and Settings\All Users\Application Data`.
 * This folder is used for application data
 * that is not user specific. For example, an application can store
 * a spell-check dictionary, a database of clip art, or a log file in the
 * CSIDL_COMMON_APPDATA folder. This information will not roam and is available
 * to anyone using the computer.
 *
 * Returns: (array zero-terminated=1) (element-type filename) (transfer none):
 *     a %NULL-terminated array of strings owned by GLib that must not be
 *     modified or freed.
 * 
 * Since: 2.6
 **/
const gchar * const *
g_get_system_config_dirs (void)
{
  gchar **conf_dir_vector;

  G_LOCK (g_utils_global);

  if (!g_system_config_dirs)
    {
      const gchar *conf_dirs = g_getenv ("XDG_CONFIG_DIRS");
#ifdef G_OS_WIN32
      if (conf_dirs)
	{
	  conf_dir_vector = g_strsplit (conf_dirs, G_SEARCHPATH_SEPARATOR_S, 0);
	}
      else
	{
	  gchar *special_conf_dirs = get_special_folder (CSIDL_COMMON_APPDATA);

	  if (special_conf_dirs)
	    conf_dir_vector = g_strsplit (special_conf_dirs, G_SEARCHPATH_SEPARATOR_S, 0);
	  else
	    /* Return empty list */
	    conf_dir_vector = g_strsplit ("", G_SEARCHPATH_SEPARATOR_S, 0);

	  g_free (special_conf_dirs);
	}
#else
      if (!conf_dirs || !conf_dirs[0])
          conf_dirs = "/etc/xdg";

      conf_dir_vector = g_strsplit (conf_dirs, G_SEARCHPATH_SEPARATOR_S, 0);
#endif

      g_system_config_dirs = conf_dir_vector;
    }
  else
    conf_dir_vector = g_system_config_dirs;
  G_UNLOCK (g_utils_global);

  return (const gchar * const *) conf_dir_vector;
}

/**
 * g_nullify_pointer:
 * @nullify_location: (not nullable): the memory address of the pointer.
 *
 * Set the pointer at the specified location to %NULL.
 **/
void
g_nullify_pointer (gpointer *nullify_location)
{
  g_return_if_fail (nullify_location != NULL);

  *nullify_location = NULL;
}

#define KILOBYTE_FACTOR (G_GOFFSET_CONSTANT (1000))
#define MEGABYTE_FACTOR (KILOBYTE_FACTOR * KILOBYTE_FACTOR)
#define GIGABYTE_FACTOR (MEGABYTE_FACTOR * KILOBYTE_FACTOR)
#define TERABYTE_FACTOR (GIGABYTE_FACTOR * KILOBYTE_FACTOR)
#define PETABYTE_FACTOR (TERABYTE_FACTOR * KILOBYTE_FACTOR)
#define EXABYTE_FACTOR  (PETABYTE_FACTOR * KILOBYTE_FACTOR)

#define KIBIBYTE_FACTOR (G_GOFFSET_CONSTANT (1024))
#define MEBIBYTE_FACTOR (KIBIBYTE_FACTOR * KIBIBYTE_FACTOR)
#define GIBIBYTE_FACTOR (MEBIBYTE_FACTOR * KIBIBYTE_FACTOR)
#define TEBIBYTE_FACTOR (GIBIBYTE_FACTOR * KIBIBYTE_FACTOR)
#define PEBIBYTE_FACTOR (TEBIBYTE_FACTOR * KIBIBYTE_FACTOR)
#define EXBIBYTE_FACTOR (PEBIBYTE_FACTOR * KIBIBYTE_FACTOR)

/**
 * g_format_size:
 * @size: a size in bytes
 *
 * Formats a size (for example the size of a file) into a human readable
 * string.  Sizes are rounded to the nearest size prefix (kB, MB, GB)
 * and are displayed rounded to the nearest tenth. E.g. the file size
 * 3292528 bytes will be converted into the string "3.2 MB".
 *
 * The prefix units base is 1000 (i.e. 1 kB is 1000 bytes).
 *
 * This string should be freed with g_free() when not needed any longer.
 *
 * See g_format_size_full() for more options about how the size might be
 * formatted.
 *
 * Returns: a newly-allocated formatted string containing a human readable
 *     file size
 *
 * Since: 2.30
 */
gchar *
g_format_size (guint64 size)
{
  return g_format_size_full (size, G_FORMAT_SIZE_DEFAULT);
}

/**
 * GFormatSizeFlags:
 * @G_FORMAT_SIZE_DEFAULT: behave the same as g_format_size()
 * @G_FORMAT_SIZE_LONG_FORMAT: include the exact number of bytes as part
 *     of the returned string.  For example, "45.6 kB (45,612 bytes)".
 * @G_FORMAT_SIZE_IEC_UNITS: use IEC (base 1024) units with "KiB"-style
 *     suffixes. IEC units should only be used for reporting things with
 *     a strong "power of 2" basis, like RAM sizes or RAID stripe sizes.
 *     Network and storage sizes should be reported in the normal SI units.
 * @G_FORMAT_SIZE_BITS: set the size as a quantity in bits, rather than
 *     bytes, and return units in bits. For example, ‘Mb’ rather than ‘MB’.
 *
 * Flags to modify the format of the string returned by g_format_size_full().
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

/**
 * g_format_size_full:
 * @size: a size in bytes
 * @flags: #GFormatSizeFlags to modify the output
 *
 * Formats a size.
 *
 * This function is similar to g_format_size() but allows for flags
 * that modify the output. See #GFormatSizeFlags.
 *
 * Returns: a newly-allocated formatted string containing a human
 *     readable file size
 *
 * Since: 2.30
 */
gchar *
g_format_size_full (guint64          size,
                    GFormatSizeFlags flags)
{
  struct Format
  {
    guint64 factor;
    char string[9];
  };

  typedef enum
  {
    FORMAT_BYTES,
    FORMAT_BYTES_IEC,
    FORMAT_BITS,
    FORMAT_BITS_IEC
  } FormatIndex;

  const struct Format formats[4][6] = {
    {
      { KILOBYTE_FACTOR, N_("%.1f kB") },
      { MEGABYTE_FACTOR, N_("%.1f MB") },
      { GIGABYTE_FACTOR, N_("%.1f GB") },
      { TERABYTE_FACTOR, N_("%.1f TB") },
      { PETABYTE_FACTOR, N_("%.1f PB") },
      { EXABYTE_FACTOR,  N_("%.1f EB") }
    },
    {
      { KIBIBYTE_FACTOR, N_("%.1f KiB") },
      { MEBIBYTE_FACTOR, N_("%.1f MiB") },
      { GIBIBYTE_FACTOR, N_("%.1f GiB") },
      { TEBIBYTE_FACTOR, N_("%.1f TiB") },
      { PEBIBYTE_FACTOR, N_("%.1f PiB") },
      { EXBIBYTE_FACTOR, N_("%.1f EiB") }
    },
    {
      { KILOBYTE_FACTOR, N_("%.1f kb") },
      { MEGABYTE_FACTOR, N_("%.1f Mb") },
      { GIGABYTE_FACTOR, N_("%.1f Gb") },
      { TERABYTE_FACTOR, N_("%.1f Tb") },
      { PETABYTE_FACTOR, N_("%.1f Pb") },
      { EXABYTE_FACTOR,  N_("%.1f Eb") }
    },
    {
      { KIBIBYTE_FACTOR, N_("%.1f Kib") },
      { MEBIBYTE_FACTOR, N_("%.1f Mib") },
      { GIBIBYTE_FACTOR, N_("%.1f Gib") },
      { TEBIBYTE_FACTOR, N_("%.1f Tib") },
      { PEBIBYTE_FACTOR, N_("%.1f Pib") },
      { EXBIBYTE_FACTOR, N_("%.1f Eib") }
    }
  };

  GString *string;
  FormatIndex index;

  string = g_string_new (NULL);

  switch (flags & ~G_FORMAT_SIZE_LONG_FORMAT)
    {
    case G_FORMAT_SIZE_DEFAULT:
      index = FORMAT_BYTES;
      break;
    case (G_FORMAT_SIZE_DEFAULT | G_FORMAT_SIZE_IEC_UNITS):
      index = FORMAT_BYTES_IEC;
      break;
    case G_FORMAT_SIZE_BITS:
      index = FORMAT_BITS;
      break;
    case (G_FORMAT_SIZE_BITS | G_FORMAT_SIZE_IEC_UNITS):
      index = FORMAT_BITS_IEC;
      break;
    default:
      g_assert_not_reached ();
    }


  if (size < formats[index][0].factor)
    {
      const char * format;

      if (index == FORMAT_BYTES || index == FORMAT_BYTES_IEC)
        {
          format = g_dngettext (GETTEXT_PACKAGE, "%u byte", "%u bytes", (guint) size);
        }
      else
        {
          format = g_dngettext (GETTEXT_PACKAGE, "%u bit", "%u bits", (guint) size);
        }

      g_string_printf (string, format, (guint) size);

      flags &= ~G_FORMAT_SIZE_LONG_FORMAT;
    }
  else
    {
      const gsize n = G_N_ELEMENTS (formats[index]);
      gsize i;

      /*
       * Point the last format (the highest unit) by default
       * and then then scan all formats, starting with the 2nd one
       * because the 1st is already managed by with the plural form
       */
      const struct Format * f = &formats[index][n - 1];

      for (i = 1; i < n; i++)
        {
          if (size < formats[index][i].factor)
            {
              f = &formats[index][i - 1];
              break;
            }
        }

      g_string_printf (string, _(f->string), (gdouble) size / (gdouble) f->factor);
    }

  if (flags & G_FORMAT_SIZE_LONG_FORMAT)
    {
      /* First problem: we need to use the number of bytes to decide on
       * the plural form that is used for display, but the number of
       * bytes potentially exceeds the size of a guint (which is what
       * ngettext() takes).
       *
       * From a pragmatic standpoint, it seems that all known languages
       * base plural forms on one or both of the following:
       *
       *   - the lowest digits of the number
       *
       *   - if the number if greater than some small value
       *
       * Here's how we fake it:  Draw an arbitrary line at one thousand.
       * If the number is below that, then fine.  If it is above it,
       * then we take the modulus of the number by one thousand (in
       * order to keep the lowest digits) and add one thousand to that
       * (in order to ensure that 1001 is not treated the same as 1).
       */
      guint plural_form = size < 1000 ? size : size % 1000 + 1000;

      /* Second problem: we need to translate the string "%u byte/bit" and
       * "%u bytes/bits" for pluralisation, but the correct number format to
       * use for a gsize is different depending on which architecture
       * we're on.
       *
       * Solution: format the number separately and use "%s bytes/bits" on
       * all platforms.
       */
      const gchar *translated_format;
      gchar *formatted_number;

      if (index == FORMAT_BYTES || index == FORMAT_BYTES_IEC)
        {
          /* Translators: the %s in "%s bytes" will always be replaced by a number. */
          translated_format = g_dngettext (GETTEXT_PACKAGE, "%s byte", "%s bytes", plural_form);
        }
      else
        {
          /* Translators: the %s in "%s bits" will always be replaced by a number. */
          translated_format = g_dngettext (GETTEXT_PACKAGE, "%s bit", "%s bits", plural_form);
        }
      /* XXX: Windows doesn't support the "'" format modifier, so we
       * must not use it there.  Instead, just display the number
       * without separation.  Bug #655336 is open until a solution is
       * found.
       */
#ifndef G_OS_WIN32
      formatted_number = g_strdup_printf ("%'"G_GUINT64_FORMAT, size);
#else
      formatted_number = g_strdup_printf ("%"G_GUINT64_FORMAT, size);
#endif

      g_string_append (string, " (");
      g_string_append_printf (string, translated_format, formatted_number);
      g_free (formatted_number);
      g_string_append (string, ")");
    }

  return g_string_free (string, FALSE);
}

#pragma GCC diagnostic pop

/**
 * g_format_size_for_display:
 * @size: a size in bytes
 *
 * Formats a size (for example the size of a file) into a human
 * readable string. Sizes are rounded to the nearest size prefix
 * (KB, MB, GB) and are displayed rounded to the nearest tenth.
 * E.g. the file size 3292528 bytes will be converted into the
 * string "3.1 MB".
 *
 * The prefix units base is 1024 (i.e. 1 KB is 1024 bytes).
 *
 * This string should be freed with g_free() when not needed any longer.
 *
 * Returns: a newly-allocated formatted string containing a human
 *     readable file size
 *
 * Since: 2.16
 *
 * Deprecated:2.30: This function is broken due to its use of SI
 *     suffixes to denote IEC units. Use g_format_size() instead.
 */
gchar *
g_format_size_for_display (goffset size)
{
  if (size < (goffset) KIBIBYTE_FACTOR)
    return g_strdup_printf (g_dngettext(GETTEXT_PACKAGE, "%u byte", "%u bytes",(guint) size), (guint) size);
  else
    {
      gdouble displayed_size;

      if (size < (goffset) MEBIBYTE_FACTOR)
        {
          displayed_size = (gdouble) size / (gdouble) KIBIBYTE_FACTOR;
          /* Translators: this is from the deprecated function g_format_size_for_display() which uses 'KB' to
           * mean 1024 bytes.  I am aware that 'KB' is not correct, but it has been preserved for reasons of
           * compatibility.  Users will not see this string unless a program is using this deprecated function.
           * Please translate as literally as possible.
           */
          return g_strdup_printf (_("%.1f KB"), displayed_size);
        }
      else if (size < (goffset) GIBIBYTE_FACTOR)
        {
          displayed_size = (gdouble) size / (gdouble) MEBIBYTE_FACTOR;
          return g_strdup_printf (_("%.1f MB"), displayed_size);
        }
      else if (size < (goffset) TEBIBYTE_FACTOR)
        {
          displayed_size = (gdouble) size / (gdouble) GIBIBYTE_FACTOR;
          return g_strdup_printf (_("%.1f GB"), displayed_size);
        }
      else if (size < (goffset) PEBIBYTE_FACTOR)
        {
          displayed_size = (gdouble) size / (gdouble) TEBIBYTE_FACTOR;
          return g_strdup_printf (_("%.1f TB"), displayed_size);
        }
      else if (size < (goffset) EXBIBYTE_FACTOR)
        {
          displayed_size = (gdouble) size / (gdouble) PEBIBYTE_FACTOR;
          return g_strdup_printf (_("%.1f PB"), displayed_size);
        }
      else
        {
          displayed_size = (gdouble) size / (gdouble) EXBIBYTE_FACTOR;
          return g_strdup_printf (_("%.1f EB"), displayed_size);
        }
    }
}

#if defined (G_OS_WIN32) && !defined (_WIN64)

/* Binary compatibility versions. Not for newly compiled code. */

_GLIB_EXTERN const gchar *g_get_user_name_utf8        (void);
_GLIB_EXTERN const gchar *g_get_real_name_utf8        (void);
_GLIB_EXTERN const gchar *g_get_home_dir_utf8         (void);
_GLIB_EXTERN const gchar *g_get_tmp_dir_utf8          (void);
_GLIB_EXTERN gchar       *g_find_program_in_path_utf8 (const gchar *program);

gchar *
g_find_program_in_path_utf8 (const gchar *program)
{
  return g_find_program_in_path (program);
}

const gchar *g_get_user_name_utf8 (void) { return g_get_user_name (); }
const gchar *g_get_real_name_utf8 (void) { return g_get_real_name (); }
const gchar *g_get_home_dir_utf8 (void) { return g_get_home_dir (); }
const gchar *g_get_tmp_dir_utf8 (void) { return g_get_tmp_dir (); }

#endif

/* Private API:
 *
 * Returns %TRUE if the current process was executed as setuid (or an
 * equivalent __libc_enable_secure is available).  See:
 * http://osdir.com/ml/linux.lfs.hardened/2007-04/msg00032.html
 */ 
gboolean
g_check_setuid (void)
{
  /* TODO: get __libc_enable_secure exported from glibc.
   * See http://www.openwall.com/lists/owl-dev/2012/08/14/1
   */
#if 0 && defined(HAVE_LIBC_ENABLE_SECURE)
  {
    /* See glibc/include/unistd.h */
    extern int __libc_enable_secure;
    return __libc_enable_secure;
  }
#elif defined(HAVE_ISSETUGID) && !defined(__BIONIC__)
  /* BSD: http://www.freebsd.org/cgi/man.cgi?query=issetugid&sektion=2 */

  /* Android had it in older versions but the new 64 bit ABI does not
   * have it anymore, and some versions of the 32 bit ABI neither.
   * https://code.google.com/p/android-developer-preview/issues/detail?id=168
   */
  return issetugid ();
#elif defined(G_OS_UNIX)
  uid_t ruid, euid, suid; /* Real, effective and saved user ID's */
  gid_t rgid, egid, sgid; /* Real, effective and saved group ID's */

  static gsize check_setuid_initialised;
  static gboolean is_setuid;

  if (g_once_init_enter (&check_setuid_initialised))
    {
#ifdef HAVE_GETRESUID
      /* These aren't in the header files, so we prototype them here.
       */
      int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
      int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
      
      if (getresuid (&ruid, &euid, &suid) != 0 ||
          getresgid (&rgid, &egid, &sgid) != 0)
#endif /* HAVE_GETRESUID */
        {
          suid = ruid = getuid ();
          sgid = rgid = getgid ();
          euid = geteuid ();
          egid = getegid ();
        }

      is_setuid = (ruid != euid || ruid != suid ||
                   rgid != egid || rgid != sgid);

      g_once_init_leave (&check_setuid_initialised, 1);
    }
  return is_setuid;
#else
  return FALSE;
#endif
}

#ifdef G_OS_WIN32
/**
 * g_abort:
 *
 * A wrapper for the POSIX abort() function.
 *
 * On Windows it is a function that makes extra effort (including a call
 * to abort()) to ensure that a debugger-catchable exception is thrown
 * before the program terminates.
 *
 * See your C library manual for more details about abort().
 *
 * Since: 2.50
 */
void
g_abort (void)
{
  /* One call to break the debugger */
  DebugBreak ();
  /* One call in case CRT does get saner about abort() behaviour */
  abort ();
  /* And one call to bind them all and terminate the program for sure */
  ExitProcess (127);
}
#endif
