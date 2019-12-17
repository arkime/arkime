/* gspawn-win32-helper.c - Helper program for process launching on Win32.
 *
 *  Copyright 2000 Red Hat, Inc.
 *  Copyright 2000 Tor Lillqvist
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <fcntl.h>

/* For _CrtSetReportMode, we don't want Windows CRT (2005 and later)
 * to terminate the process if a bad file descriptor is passed into
 * _get_osfhandle().  This is necessary because we use _get_osfhandle()
 * to check the validity of the fd before we try to call close() on
 * it as attempting to close an invalid fd will cause the Windows CRT
 * to abort() this program internally.
 *
 * Please see http://msdn.microsoft.com/zh-tw/library/ks2530z6%28v=vs.80%29.aspx
 * for an explanation on this.
 */
#if (defined (_MSC_VER) && _MSC_VER >= 1400)
#include <crtdbg.h>
#endif

#undef G_LOG_DOMAIN
#include "glib.h"
#define GSPAWN_HELPER
#include "gspawn-win32.c"	/* For shared definitions */


static void
write_err_and_exit (gint    fd,
		    gintptr msg)
{
  gintptr en = errno;
  
  write (fd, &msg, sizeof(gintptr));
  write (fd, &en, sizeof(gintptr));
  
  _exit (1);
}

#ifdef __GNUC__
#  ifndef _stdcall
#    define _stdcall  __attribute__((stdcall))
#  endif
#endif

/* We build gspawn-win32-helper.exe as a Windows GUI application
 * to avoid any temporarily flashing console windows in case
 * the gspawn function is invoked by a GUI program. Thus, no main()
 * but a WinMain().
 */

/* Copy of protect_argv that handles wchar_t strings */

static gint
protect_wargv (gint       argc,
	       wchar_t  **wargv,
	       wchar_t ***new_wargv)
{
  gint i;
  
  *new_wargv = g_new (wchar_t *, argc+1);

  /* Quote each argv element if necessary, so that it will get
   * reconstructed correctly in the C runtime startup code.  Note that
   * the unquoting algorithm in the C runtime is really weird, and
   * rather different than what Unix shells do. See stdargv.c in the C
   * runtime sources (in the Platform SDK, in src/crt).
   *
   * Note that an new_wargv[0] constructed by this function should
   * *not* be passed as the filename argument to a _wspawn* or _wexec*
   * family function. That argument should be the real file name
   * without any quoting.
   */
  for (i = 0; i < argc; i++)
    {
      wchar_t *p = wargv[i];
      wchar_t *q;
      gint len = 0;
      gboolean need_dblquotes = FALSE;
      while (*p)
	{
	  if (*p == ' ' || *p == '\t')
	    need_dblquotes = TRUE;
	  else if (*p == '"')
	    len++;
	  else if (*p == '\\')
	    {
	      wchar_t *pp = p;
	      while (*pp && *pp == '\\')
		pp++;
	      if (*pp == '"')
		len++;
	    }
	  len++;
	  p++;
	}

      q = (*new_wargv)[i] = g_new (wchar_t, len + need_dblquotes*2 + 1);
      p = wargv[i];

      if (need_dblquotes)
	*q++ = '"';

      while (*p)
	{
	  if (*p == '"')
	    *q++ = '\\';
	  else if (*p == '\\')
	    {
	      wchar_t *pp = p;
	      while (*pp && *pp == '\\')
		pp++;
	      if (*pp == '"')
		*q++ = '\\';
	    }
	  *q++ = *p;
	  p++;
	}

      if (need_dblquotes)
	*q++ = '"';
      *q++ = '\0';
    }
  (*new_wargv)[argc] = NULL;

  return argc;
}

#if (defined (_MSC_VER) && _MSC_VER >= 1400)
/*
 * This is the (empty) invalid parameter handler
 * that is used for Visual C++ 2005 (and later) builds
 * so that we can use this instead of the system automatically
 * aborting the process.
 *
 * This is necessary as we use _get_oshandle() to check the validity
 * of the file descriptors as we close them, so when an invalid file
 * descriptor is passed into that function as we check on it, we get
 * -1 as the result, instead of the gspawn helper program aborting.
 *
 * Please see http://msdn.microsoft.com/zh-tw/library/ks2530z6%28v=vs.80%29.aspx
 * for an explanation on this.
 */
extern void
myInvalidParameterHandler(const wchar_t *expression,
                          const wchar_t *function,
                          const wchar_t *file,
                          unsigned int   line,
                          uintptr_t      pReserved);
#endif


#ifndef HELPER_CONSOLE
int _stdcall
WinMain (struct HINSTANCE__ *hInstance,
	 struct HINSTANCE__ *hPrevInstance,
	 char               *lpszCmdLine,
	 int                 nCmdShow)
#else
int
main (int ignored_argc, char **ignored_argv)
#endif
{
  int child_err_report_fd = -1;
  int helper_sync_fd = -1;
  int i;
  int fd;
  int mode;
  gintptr handle;
  int saved_errno;
  gintptr no_error = CHILD_NO_ERROR;
  gint argv_zero_offset = ARG_PROGRAM;
  wchar_t **new_wargv;
  int argc;
  char **argv;
  wchar_t **wargv;
  char c;

#if (defined (_MSC_VER) && _MSC_VER >= 1400)
  /* set up our empty invalid parameter handler */
  _invalid_parameter_handler oldHandler, newHandler;
  newHandler = myInvalidParameterHandler;
  oldHandler = _set_invalid_parameter_handler(newHandler);

  /* Disable the message box for assertions. */
  _CrtSetReportMode(_CRT_ASSERT, 0);
#endif

  /* Fetch the wide-char argument vector */
  wargv = CommandLineToArgvW (GetCommandLineW(), &argc);

  g_assert (argc >= ARG_COUNT);

  /* Convert unicode wargs to utf8 */
  argv = g_new(char *, argc + 1);
  for (i = 0; i < argc; i++)
    argv[i] = g_utf16_to_utf8(wargv[i], -1, NULL, NULL, NULL);
  argv[i] = NULL;

  /* argv[ARG_CHILD_ERR_REPORT] is the file descriptor number onto
   * which write error messages.
   */
  child_err_report_fd = atoi (argv[ARG_CHILD_ERR_REPORT]);

  /* Hack to implement G_SPAWN_FILE_AND_ARGV_ZERO. If
   * argv[ARG_CHILD_ERR_REPORT] is suffixed with a '#' it means we get
   * the program to run and its argv[0] separately.
   */
  if (argv[ARG_CHILD_ERR_REPORT][strlen (argv[ARG_CHILD_ERR_REPORT]) - 1] == '#')
    argv_zero_offset++;

  /* argv[ARG_HELPER_SYNC] is the file descriptor number we read a
   * byte that tells us it is OK to exit. We have to wait until the
   * parent allows us to exit, so that the parent has had time to
   * duplicate the process handle we sent it. Duplicating a handle
   * from another process works only if that other process exists.
   */
  helper_sync_fd = atoi (argv[ARG_HELPER_SYNC]);

  /* argv[ARG_STDIN..ARG_STDERR] are the file descriptor numbers that
   * should be dup2'd to 0, 1 and 2. '-' if the corresponding fd
   * should be left alone, and 'z' if it should be connected to the
   * bit bucket NUL:.
   */
  if (argv[ARG_STDIN][0] == '-')
    ; /* Nothing */
  else if (argv[ARG_STDIN][0] == 'z')
    {
      fd = open ("NUL:", O_RDONLY);
      if (fd != 0)
	{
	  dup2 (fd, 0);
	  close (fd);
	}
    }
  else
    {
      fd = atoi (argv[ARG_STDIN]);
      if (fd != 0)
	{
	  dup2 (fd, 0);
	  close (fd);
	}
    }

  if (argv[ARG_STDOUT][0] == '-')
    ; /* Nothing */
  else if (argv[ARG_STDOUT][0] == 'z')
    {
      fd = open ("NUL:", O_WRONLY);
      if (fd != 1)
	{
	  dup2 (fd, 1);
	  close (fd);
	}
    }
  else
    {
      fd = atoi (argv[ARG_STDOUT]);
      if (fd != 1)
	{
	  dup2 (fd, 1);
	  close (fd);
	}
    }

  if (argv[ARG_STDERR][0] == '-')
    ; /* Nothing */
  else if (argv[ARG_STDERR][0] == 'z')
    {
      fd = open ("NUL:", O_WRONLY);
      if (fd != 2)
	{
	  dup2 (fd, 2);
	  close (fd);
	}
    }
  else
    {
      fd = atoi (argv[ARG_STDERR]);
      if (fd != 2)
	{
	  dup2 (fd, 2);
	  close (fd);
	}
    }

  /* argv[ARG_WORKING_DIRECTORY] is the directory in which to run the
   * process.  If "-", don't change directory.
   */
  if (argv[ARG_WORKING_DIRECTORY][0] == '-' &&
      argv[ARG_WORKING_DIRECTORY][1] == 0)
    ; /* Nothing */
  else if (_wchdir (wargv[ARG_WORKING_DIRECTORY]) < 0)
    write_err_and_exit (child_err_report_fd, CHILD_CHDIR_FAILED);

  /* argv[ARG_CLOSE_DESCRIPTORS] is "y" if file descriptors from 3
   *  upwards should be closed
   */
  if (argv[ARG_CLOSE_DESCRIPTORS][0] == 'y')
    for (i = 3; i < 1000; i++)	/* FIXME real limit? */
      if (i != child_err_report_fd && i != helper_sync_fd)
        if (_get_osfhandle (i) != -1)
          close (i);

  /* We don't want our child to inherit the error report and
   * helper sync fds.
   */
  child_err_report_fd = dup_noninherited (child_err_report_fd, _O_WRONLY);
  helper_sync_fd = dup_noninherited (helper_sync_fd, _O_RDONLY);

  /* argv[ARG_WAIT] is "w" to wait for the program to exit */
  if (argv[ARG_WAIT][0] == 'w')
    mode = P_WAIT;
  else
    mode = P_NOWAIT;

  /* argv[ARG_USE_PATH] is "y" to use PATH, otherwise not */

  /* argv[ARG_PROGRAM] is executable file to run,
   * argv[argv_zero_offset]... is its argv. argv_zero_offset equals
   * ARG_PROGRAM unless G_SPAWN_FILE_AND_ARGV_ZERO was used, in which
   * case we have a separate executable name and argv[0].
   */

  /* For the program name passed to spawnv(), don't use the quoted
   * version.
   */
  protect_wargv (argc - argv_zero_offset, wargv + argv_zero_offset, &new_wargv);

  if (argv[ARG_USE_PATH][0] == 'y')
    handle = _wspawnvp (mode, wargv[ARG_PROGRAM], (const wchar_t **) new_wargv);
  else
    handle = _wspawnv (mode, wargv[ARG_PROGRAM], (const wchar_t **) new_wargv);

  saved_errno = errno;

  if (handle == -1 && saved_errno != 0)
    write_err_and_exit (child_err_report_fd, CHILD_SPAWN_FAILED);

  write (child_err_report_fd, &no_error, sizeof (no_error));
  write (child_err_report_fd, &handle, sizeof (handle));

  read (helper_sync_fd, &c, 1);

  LocalFree (wargv);
  g_strfreev (argv);

  return 0;
}
