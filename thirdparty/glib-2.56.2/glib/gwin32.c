/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1998  Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-1999  Tor Lillqvist
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

#include "glibconfig.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>

#define STRICT			/* Strict typing, please */
#include <windows.h>
#undef STRICT
#ifndef G_WITH_CYGWIN
#include <direct.h>
#endif
#include <errno.h>
#include <ctype.h>
#if defined(_MSC_VER) || defined(__DMC__)
#  include <io.h>
#endif /* _MSC_VER || __DMC__ */

#define MODERN_API_FAMILY 2

#if WINAPI_FAMILY == MODERN_API_FAMILY
/* This is for modern UI Builds, where we can't use LoadLibraryW()/GetProcAddress() */
/* ntddk.h is found in the WDK, and MinGW */
#include <ntddk.h>

#ifdef _MSC_VER
#pragma comment (lib, "ntoskrnl.lib")
#endif
#elif defined (__MINGW32__)
/* mingw-w64, not MinGW, has winternl.h */
#include <ntdef.h>
#else
#include <winternl.h>
#endif

#include "glib.h"
#include "gthreadprivate.h"

#ifdef G_WITH_CYGWIN
#include <sys/cygwin.h>
#endif

#ifndef G_WITH_CYGWIN

gint
g_win32_ftruncate (gint  fd,
		   guint size)
{
  return _chsize (fd, size);
}

#endif

/**
 * g_win32_getlocale:
 *
 * The setlocale() function in the Microsoft C library uses locale
 * names of the form "English_United States.1252" etc. We want the
 * UNIXish standard form "en_US", "zh_TW" etc. This function gets the
 * current thread locale from Windows - without any encoding info -
 * and returns it as a string of the above form for use in forming
 * file names etc. The returned string should be deallocated with
 * g_free().
 *
 * Returns: newly-allocated locale name.
 **/

#ifndef SUBLANG_SERBIAN_LATIN_BA
#define SUBLANG_SERBIAN_LATIN_BA 0x06
#endif

gchar *
g_win32_getlocale (void)
{
  LCID lcid;
  LANGID langid;
  gchar *ev;
  gint primary, sub;
  char iso639[10];
  char iso3166[10];
  const gchar *script = NULL;

  /* Let the user override the system settings through environment
   * variables, as on POSIX systems. Note that in GTK+ applications
   * since GTK+ 2.10.7 setting either LC_ALL or LANG also sets the
   * Win32 locale and C library locale through code in gtkmain.c.
   */
  if (((ev = getenv ("LC_ALL")) != NULL && ev[0] != '\0')
      || ((ev = getenv ("LC_MESSAGES")) != NULL && ev[0] != '\0')
      || ((ev = getenv ("LANG")) != NULL && ev[0] != '\0'))
    return g_strdup (ev);

  lcid = GetThreadLocale ();

  if (!GetLocaleInfo (lcid, LOCALE_SISO639LANGNAME, iso639, sizeof (iso639)) ||
      !GetLocaleInfo (lcid, LOCALE_SISO3166CTRYNAME, iso3166, sizeof (iso3166)))
    return g_strdup ("C");
  
  /* Strip off the sorting rules, keep only the language part.  */
  langid = LANGIDFROMLCID (lcid);

  /* Split into language and territory part.  */
  primary = PRIMARYLANGID (langid);
  sub = SUBLANGID (langid);

  /* Handle special cases */
  switch (primary)
    {
    case LANG_AZERI:
      switch (sub)
	{
	case SUBLANG_AZERI_LATIN:
	  script = "@Latn";
	  break;
	case SUBLANG_AZERI_CYRILLIC:
	  script = "@Cyrl";
	  break;
	}
      break;
    case LANG_SERBIAN:		/* LANG_CROATIAN == LANG_SERBIAN */
      switch (sub)
	{
	case SUBLANG_SERBIAN_LATIN:
	case 0x06: /* Serbian (Latin) - Bosnia and Herzegovina */
	  script = "@Latn";
	  break;
	}
      break;
    case LANG_UZBEK:
      switch (sub)
	{
	case SUBLANG_UZBEK_LATIN:
	  script = "@Latn";
	  break;
	case SUBLANG_UZBEK_CYRILLIC:
	  script = "@Cyrl";
	  break;
	}
      break;
    }
  return g_strconcat (iso639, "_", iso3166, script, NULL);
}

/**
 * g_win32_error_message:
 * @error: error code.
 *
 * Translate a Win32 error code (as returned by GetLastError() or
 * WSAGetLastError()) into the corresponding message. The message is
 * either language neutral, or in the thread's language, or the user's
 * language, the system's language, or US English (see docs for
 * FormatMessage()). The returned string is in UTF-8. It should be
 * deallocated with g_free().
 *
 * Returns: newly-allocated error message
 **/
gchar *
g_win32_error_message (gint error)
{
  gchar *retval;
  wchar_t *msg = NULL;
  size_t nchars;

  FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER
		  |FORMAT_MESSAGE_IGNORE_INSERTS
		  |FORMAT_MESSAGE_FROM_SYSTEM,
		  NULL, error, 0,
		  (LPWSTR) &msg, 0, NULL);
  if (msg != NULL)
    {
      nchars = wcslen (msg);

      if (nchars >= 2 && msg[nchars-1] == L'\n' && msg[nchars-2] == L'\r')
        msg[nchars-2] = L'\0';

      retval = g_utf16_to_utf8 (msg, -1, NULL, NULL, NULL);

      LocalFree (msg);
    }
  else
    retval = g_strdup ("");

  return retval;
}

/**
 * g_win32_get_package_installation_directory_of_module:
 * @hmodule: (nullable): The Win32 handle for a DLL loaded into the current process, or %NULL
 *
 * This function tries to determine the installation directory of a
 * software package based on the location of a DLL of the software
 * package.
 *
 * @hmodule should be the handle of a loaded DLL or %NULL. The
 * function looks up the directory that DLL was loaded from. If
 * @hmodule is NULL, the directory the main executable of the current
 * process is looked up. If that directory's last component is "bin"
 * or "lib", its parent directory is returned, otherwise the directory
 * itself.
 *
 * It thus makes sense to pass only the handle to a "public" DLL of a
 * software package to this function, as such DLLs typically are known
 * to be installed in a "bin" or occasionally "lib" subfolder of the
 * installation folder. DLLs that are of the dynamically loaded module
 * or plugin variety are often located in more private locations
 * deeper down in the tree, from which it is impossible for GLib to
 * deduce the root of the package installation.
 *
 * The typical use case for this function is to have a DllMain() that
 * saves the handle for the DLL. Then when code in the DLL needs to
 * construct names of files in the installation tree it calls this
 * function passing the DLL handle.
 *
 * Returns: a string containing the guessed installation directory for
 * the software package @hmodule is from. The string is in the GLib
 * file name encoding, i.e. UTF-8. The return value should be freed
 * with g_free() when not needed any longer. If the function fails
 * %NULL is returned.
 *
 * Since: 2.16
 */
gchar *
g_win32_get_package_installation_directory_of_module (gpointer hmodule)
{
  gchar *filename;
  gchar *retval;
  gchar *p;
  wchar_t wc_fn[MAX_PATH];

  /* NOTE: it relies that GetModuleFileNameW returns only canonical paths */
  if (!GetModuleFileNameW (hmodule, wc_fn, MAX_PATH))
    return NULL;

  filename = g_utf16_to_utf8 (wc_fn, -1, NULL, NULL, NULL);

  if ((p = strrchr (filename, G_DIR_SEPARATOR)) != NULL)
    *p = '\0';

  retval = g_strdup (filename);

  do
    {
      p = strrchr (retval, G_DIR_SEPARATOR);
      if (p == NULL)
        break;

      *p = '\0';

      if (g_ascii_strcasecmp (p + 1, "bin") == 0 ||
          g_ascii_strcasecmp (p + 1, "lib") == 0)
        break;
    }
  while (p != NULL);

  if (p == NULL)
    {
      g_free (retval);
      retval = filename;
    }
  else
    g_free (filename);

#ifdef G_WITH_CYGWIN
  /* In Cygwin we need to have POSIX paths */
  {
    gchar tmp[MAX_PATH];

    cygwin_conv_to_posix_path (retval, tmp);
    g_free (retval);
    retval = g_strdup (tmp);
  }
#endif

  return retval;
}

static gchar *
get_package_directory_from_module (const gchar *module_name)
{
  static GHashTable *module_dirs = NULL;
  G_LOCK_DEFINE_STATIC (module_dirs);
  HMODULE hmodule = NULL;
  gchar *fn;

  G_LOCK (module_dirs);

  if (module_dirs == NULL)
    module_dirs = g_hash_table_new (g_str_hash, g_str_equal);
  
  fn = g_hash_table_lookup (module_dirs, module_name ? module_name : "");
      
  if (fn)
    {
      G_UNLOCK (module_dirs);
      return g_strdup (fn);
    }

  if (module_name)
    {
      wchar_t *wc_module_name = g_utf8_to_utf16 (module_name, -1, NULL, NULL, NULL);
      hmodule = GetModuleHandleW (wc_module_name);
      g_free (wc_module_name);

      if (!hmodule)
	{
	  G_UNLOCK (module_dirs);
	  return NULL;
	}
    }

  fn = g_win32_get_package_installation_directory_of_module (hmodule);

  if (fn == NULL)
    {
      G_UNLOCK (module_dirs);
      return NULL;
    }
  
  g_hash_table_insert (module_dirs, module_name ? g_strdup (module_name) : "", fn);

  G_UNLOCK (module_dirs);

  return g_strdup (fn);
}

/**
 * g_win32_get_package_installation_directory:
 * @package: (nullable): You should pass %NULL for this.
 * @dll_name: (nullable): The name of a DLL that a package provides in UTF-8, or %NULL.
 *
 * Try to determine the installation directory for a software package.
 *
 * This function is deprecated. Use
 * g_win32_get_package_installation_directory_of_module() instead.
 *
 * The use of @package is deprecated. You should always pass %NULL. A
 * warning is printed if non-NULL is passed as @package.
 *
 * The original intended use of @package was for a short identifier of
 * the package, typically the same identifier as used for
 * `GETTEXT_PACKAGE` in software configured using GNU
 * autotools. The function first looks in the Windows Registry for the
 * value `#InstallationDirectory` in the key
 * `#HKLM\Software\@package`, and if that value
 * exists and is a string, returns that.
 *
 * It is strongly recommended that packagers of GLib-using libraries
 * for Windows do not store installation paths in the Registry to be
 * used by this function as that interfers with having several
 * parallel installations of the library. Enabling multiple
 * installations of different versions of some GLib-using library, or
 * GLib itself, is desirable for various reasons.
 *
 * For this reason it is recommeded to always pass %NULL as
 * @package to this function, to avoid the temptation to use the
 * Registry. In version 2.20 of GLib the @package parameter
 * will be ignored and this function won't look in the Registry at all.
 *
 * If @package is %NULL, or the above value isn't found in the
 * Registry, but @dll_name is non-%NULL, it should name a DLL loaded
 * into the current process. Typically that would be the name of the
 * DLL calling this function, looking for its installation
 * directory. The function then asks Windows what directory that DLL
 * was loaded from. If that directory's last component is "bin" or
 * "lib", the parent directory is returned, otherwise the directory
 * itself. If that DLL isn't loaded, the function proceeds as if
 * @dll_name was %NULL.
 *
 * If both @package and @dll_name are %NULL, the directory from where
 * the main executable of the process was loaded is used instead in
 * the same way as above.
 *
 * Returns: a string containing the installation directory for
 * @package. The string is in the GLib file name encoding,
 * i.e. UTF-8. The return value should be freed with g_free() when not
 * needed any longer. If the function fails %NULL is returned.
 *
 * Deprecated: 2.18: Pass the HMODULE of a DLL or EXE to
 * g_win32_get_package_installation_directory_of_module() instead.
 **/

gchar *
g_win32_get_package_installation_directory (const gchar *package,
                                            const gchar *dll_name)
{
  gchar *result = NULL;

  if (package != NULL)
      g_warning ("Passing a non-NULL package to g_win32_get_package_installation_directory() is deprecated and it is ignored.");

  if (dll_name != NULL)
    result = get_package_directory_from_module (dll_name);

  if (result == NULL)
    result = get_package_directory_from_module (NULL);

  return result;
}

/**
 * g_win32_get_package_installation_subdirectory:
 * @package: (nullable): You should pass %NULL for this.
 * @dll_name: (nullable): The name of a DLL that a package provides, in UTF-8, or %NULL.
 * @subdir: A subdirectory of the package installation directory, also in UTF-8
 *
 * This function is deprecated. Use
 * g_win32_get_package_installation_directory_of_module() and
 * g_build_filename() instead.
 *
 * Returns a newly-allocated string containing the path of the
 * subdirectory @subdir in the return value from calling
 * g_win32_get_package_installation_directory() with the @package and
 * @dll_name parameters. See the documentation for
 * g_win32_get_package_installation_directory() for more details. In
 * particular, note that it is deprecated to pass anything except NULL
 * as @package.
 *
 * Returns: a string containing the complete path to @subdir inside
 * the installation directory of @package. The returned string is in
 * the GLib file name encoding, i.e. UTF-8. The return value should be
 * freed with g_free() when no longer needed. If something goes wrong,
 * %NULL is returned.
 *
 * Deprecated: 2.18: Pass the HMODULE of a DLL or EXE to
 * g_win32_get_package_installation_directory_of_module() instead, and
 * then construct a subdirectory pathname with g_build_filename().
 **/

gchar *
g_win32_get_package_installation_subdirectory (const gchar *package,
                                               const gchar *dll_name,
                                               const gchar *subdir)
{
  gchar *prefix;
  gchar *dirname;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  prefix = g_win32_get_package_installation_directory (package, dll_name);
G_GNUC_END_IGNORE_DEPRECATIONS

  dirname = g_build_filename (prefix, subdir, NULL);
  g_free (prefix);

  return dirname;
}

/**
 * g_win32_check_windows_version:
 * @major: major version of Windows
 * @minor: minor version of Windows
 * @spver: Windows Service Pack Level, 0 if none
 * @os_type: Type of Windows OS
 *
 * Returns whether the version of the Windows operating system the
 * code is running on is at least the specified major, minor and
 * service pack versions.  See MSDN documentation for the Operating
 * System Version.  Software that needs even more detailed version and
 * feature information should use the Win32 API VerifyVersionInfo()
 * directly.
 *
 * Successive calls of this function can be used for enabling or
 * disabling features at run-time for a range of Windows versions,
 * as per the VerifyVersionInfo() API documentation.
 *
 * Returns: %TRUE if the Windows Version is the same or greater than
 *          the specified major, minor and service pack versions, and
 *          whether the running Windows is a workstation or server edition
 *          of Windows, if specifically specified.
 *
 * Since: 2.44
 **/
gboolean
g_win32_check_windows_version (const gint major,
                               const gint minor,
                               const gint spver,
                               const GWin32OSType os_type)
{
  OSVERSIONINFOEXW osverinfo;
  gboolean is_ver_checked = FALSE;
  gboolean is_type_checked = FALSE;

#if WINAPI_FAMILY != MODERN_API_FAMILY
  /* For non-modern UI Apps, use the LoadLibraryW()/GetProcAddress() thing */
  typedef NTSTATUS (WINAPI fRtlGetVersion) (PRTL_OSVERSIONINFOEXW);

  fRtlGetVersion *RtlGetVersion;
  HMODULE hmodule;
#endif
  /* We Only Support Checking for XP or later */
  g_return_val_if_fail (major >= 5 && (major <=6 || major == 10), FALSE);
  g_return_val_if_fail ((major >= 5 && minor >= 1) || major >= 6, FALSE);

  /* Check for Service Pack Version >= 0 */
  g_return_val_if_fail (spver >= 0, FALSE);

#if WINAPI_FAMILY != MODERN_API_FAMILY
  hmodule = LoadLibraryW (L"ntdll.dll");
  g_return_val_if_fail (hmodule != NULL, FALSE);

  RtlGetVersion = (fRtlGetVersion *) GetProcAddress (hmodule, "RtlGetVersion");
  g_return_val_if_fail (RtlGetVersion != NULL, FALSE);
#endif

  memset (&osverinfo, 0, sizeof (OSVERSIONINFOEXW));
  osverinfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEXW);
  RtlGetVersion (&osverinfo);

  /* check the OS and Service Pack Versions */
  if (osverinfo.dwMajorVersion > major)
    is_ver_checked = TRUE;
  else if (osverinfo.dwMajorVersion == major)
    {
      if (osverinfo.dwMinorVersion > minor)
        is_ver_checked = TRUE;
      else if (osverinfo.dwMinorVersion == minor)
        if (osverinfo.wServicePackMajor >= spver)
          is_ver_checked = TRUE;
    }

  /* Check OS Type */
  if (is_ver_checked)
    {
      switch (os_type)
        {
          case G_WIN32_OS_ANY:
            is_type_checked = TRUE;
            break;
          case G_WIN32_OS_WORKSTATION:
            if (osverinfo.wProductType == VER_NT_WORKSTATION)
              is_type_checked = TRUE;
            break;
          case G_WIN32_OS_SERVER:
            if (osverinfo.wProductType == VER_NT_SERVER ||
                osverinfo.wProductType == VER_NT_DOMAIN_CONTROLLER)
              is_type_checked = TRUE;
            break;
          default:
            /* shouldn't get here normally */
            g_warning ("Invalid os_type specified");
            break;
        }
    }

#if WINAPI_FAMILY != MODERN_API_FAMILY
  FreeLibrary (hmodule);
#endif

  return is_ver_checked && is_type_checked;
}

/**
 * g_win32_get_windows_version:
 *
 * This function is deprecated. Use
 * g_win32_check_windows_version() instead.
 *
 * Returns version information for the Windows operating system the
 * code is running on. See MSDN documentation for the GetVersion()
 * function. To summarize, the most significant bit is one on Win9x,
 * and zero on NT-based systems. Since version 2.14, GLib works only
 * on NT-based systems, so checking whether your are running on Win9x
 * in your own software is moot. The least significant byte is 4 on
 * Windows NT 4, and 5 on Windows XP. Software that needs really
 * detailed version and feature information should use Win32 API like
 * GetVersionEx() and VerifyVersionInfo().
 *
 * Returns: The version information.
 *
 * Deprecated: 2.44: Be aware that for Windows 8.1 and Windows Server
 * 2012 R2 and later, this will return 62 unless the application is
 * manifested for Windows 8.1/Windows Server 2012 R2, for example.
 * MSDN stated that GetVersion(), which is used here, is subject to
 * further change or removal after Windows 8.1.
 **/
guint
g_win32_get_windows_version (void)
{
  static gsize windows_version;

  if (g_once_init_enter (&windows_version))
    g_once_init_leave (&windows_version, GetVersion ());

  return windows_version;
}

/*
 * Doesn't use gettext (and gconv), preventing recursive calls when
 * g_win32_locale_filename_from_utf8() is called during
 * gettext initialization.
 */
static gchar *
special_wchar_to_locale_enoding (wchar_t *wstring)
{
  int sizeof_output;
  int wctmb_result;
  char *result;
  BOOL not_representable = FALSE;

  sizeof_output = WideCharToMultiByte (CP_ACP,
                                       WC_NO_BEST_FIT_CHARS,
                                       wstring, -1,
                                       NULL, 0,
                                       NULL,
                                       &not_representable);

  if (not_representable ||
      sizeof_output == 0 ||
      sizeof_output > MAX_PATH)
    return NULL;

  result = g_malloc0 (sizeof_output + 1);

  wctmb_result = WideCharToMultiByte (CP_ACP,
                                      WC_NO_BEST_FIT_CHARS,
                                      wstring, -1,
                                      result, sizeof_output + 1,
                                      NULL,
                                      &not_representable);

  if (wctmb_result == sizeof_output &&
      not_representable == FALSE)
    return result;

  g_free (result);

  return NULL;
}

/**
 * g_win32_locale_filename_from_utf8:
 * @utf8filename: a UTF-8 encoded filename.
 *
 * Converts a filename from UTF-8 to the system codepage.
 *
 * On NT-based Windows, on NTFS file systems, file names are in
 * Unicode. It is quite possible that Unicode file names contain
 * characters not representable in the system codepage. (For instance,
 * Greek or Cyrillic characters on Western European or US Windows
 * installations, or various less common CJK characters on CJK Windows
 * installations.)
 *
 * In such a case, and if the filename refers to an existing file, and
 * the file system stores alternate short (8.3) names for directory
 * entries, the short form of the filename is returned. Note that the
 * "short" name might in fact be longer than the Unicode name if the
 * Unicode name has very short pathname components containing
 * non-ASCII characters. If no system codepage name for the file is
 * possible, %NULL is returned.
 *
 * The return value is dynamically allocated and should be freed with
 * g_free() when no longer needed.
 *
 * Returns: The converted filename, or %NULL on conversion
 * failure and lack of short names.
 *
 * Since: 2.8
 */
gchar *
g_win32_locale_filename_from_utf8 (const gchar *utf8filename)
{
  gchar *retval;
  wchar_t *wname;

  wname = g_utf8_to_utf16 (utf8filename, -1, NULL, NULL, NULL);

  if (wname == NULL)
    return NULL;

  retval = special_wchar_to_locale_enoding (wname);

  if (retval == NULL)
    {
      /* Conversion failed, so check if there is a 8.3 version, and use that. */
      wchar_t wshortname[MAX_PATH + 1];

      if (GetShortPathNameW (wname, wshortname, G_N_ELEMENTS (wshortname)))
        retval = special_wchar_to_locale_enoding (wshortname);
    }

  g_free (wname);

  return retval;
}

/**
 * g_win32_get_command_line:
 *
 * Gets the command line arguments, on Windows, in the GLib filename
 * encoding (ie: UTF-8).
 *
 * Normally, on Windows, the command line arguments are passed to main()
 * in the system codepage encoding.  This prevents passing filenames as
 * arguments if the filenames contain characters that fall outside of
 * this codepage.  If such filenames are passed, then substitutions
 * will occur (such as replacing some characters with '?').
 *
 * GLib's policy of using UTF-8 as a filename encoding on Windows was
 * designed to localise the pain of dealing with filenames outside of
 * the system codepage to one area: dealing with commandline arguments
 * in main().
 *
 * As such, most GLib programs should ignore the value of argv passed to
 * their main() function and call g_win32_get_command_line() instead.
 * This will get the "full Unicode" commandline arguments using
 * GetCommandLineW() and convert it to the GLib filename encoding (which
 * is UTF-8 on Windows).
 *
 * The strings returned by this function are suitable for use with
 * functions such as g_open() and g_file_new_for_commandline_arg() but
 * are not suitable for use with g_option_context_parse(), which assumes
 * that its input will be in the system codepage.  The return value is
 * suitable for use with g_option_context_parse_strv(), however, which
 * is a better match anyway because it won't leak memory.
 *
 * Unlike argv, the returned value is a normal strv and can (and should)
 * be freed with g_strfreev() when no longer needed.
 *
 * Returns: (transfer full): the commandline arguments in the GLib
 *   filename encoding (ie: UTF-8)
 *
 * Since: 2.40
 **/
gchar **
g_win32_get_command_line (void)
{
  gchar **result;
  LPWSTR *args;
  gint i, n;

  args = CommandLineToArgvW (GetCommandLineW(), &n);

  result = g_new (gchar *, n + 1);
  for (i = 0; i < n; i++)
    result[i] = g_utf16_to_utf8 (args[i], -1, NULL, NULL, NULL);
  result[i] = NULL;

  LocalFree (args);
  return result;
}

#ifdef G_OS_WIN32

/* Binary compatibility versions. Not for newly compiled code. */

_GLIB_EXTERN gchar *g_win32_get_package_installation_directory_utf8    (const gchar *package,
                                                                        const gchar *dll_name);

_GLIB_EXTERN gchar *g_win32_get_package_installation_subdirectory_utf8 (const gchar *package,
                                                                        const gchar *dll_name,
                                                                        const gchar *subdir);

gchar *
g_win32_get_package_installation_directory_utf8 (const gchar *package,
                                                 const gchar *dll_name)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return g_win32_get_package_installation_directory (package, dll_name);
G_GNUC_END_IGNORE_DEPRECATIONS
}

gchar *
g_win32_get_package_installation_subdirectory_utf8 (const gchar *package,
                                                    const gchar *dll_name,
                                                    const gchar *subdir)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return g_win32_get_package_installation_subdirectory (package,
                                                        dll_name,
                                                        subdir);
G_GNUC_END_IGNORE_DEPRECATIONS
}

#endif
