#include <windows.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef __CYGWIN__
/* For read() and write() */
#include <unistd.h>
/* Cygwin does not prototype __argc and __argv in stdlib.h */
extern int __argc;
extern char** __argv;
#endif

int _stdcall
WinMain (struct HINSTANCE__ *hInstance,
	struct HINSTANCE__ *hPrevInstance,
	char *lpszCmdLine,
	int   nCmdShow)
{
  char buf[100];

  if (__argc >= 2 && strcmp (__argv[1], "nop") == 0)
    {
      sprintf (buf, "spawn-test-win32-gui: argv[0]=\"%s\"", __argv[0]);
      MessageBox (NULL, buf, lpszCmdLine, MB_ICONINFORMATION|MB_SYSTEMMODAL);
    }
  else if (__argc <= 2)
    {
      MessageBox (NULL, "spawn-test-win32-gui: Will write to stdout",
		  lpszCmdLine, MB_ICONINFORMATION|MB_SYSTEMMODAL);
      
      printf ("This is stdout\n");
      fflush (stdout);
      
      MessageBox (NULL, "spawn-test-win32-gui: Will write to stderr",
		  lpszCmdLine, MB_ICONINFORMATION|MB_SYSTEMMODAL);
      
      fprintf (stderr, "This is stderr\n");
      fflush (stderr);
    }
  else if (__argc == 4 && strcmp (__argv[1], "pipes") == 0)
    {
      int infd = atoi (__argv[2]);
      int outfd = atoi (__argv[3]);
      int k, n;

      if (infd < 0 || outfd < 0)
	{
	  MessageBox (NULL, "spawn-test-win32-gui: illegal fds on command line",
		      lpszCmdLine, MB_ICONERROR|MB_SYSTEMMODAL);
	  exit (1);
	}

      MessageBox (NULL, "spawn-test-win32-gui: Will write to parent",
		  lpszCmdLine, MB_ICONINFORMATION|MB_SYSTEMMODAL);

      n = strlen ("Hello there");
      if (write (outfd, &n, sizeof (n)) == -1 ||
	  write (outfd, "Hello there", n) == -1)
	{
	  int errsv = errno;
	  sprintf (buf, "spawn-test-win32-gui: Write: %s", strerror (errsv));
	  MessageBox (NULL, buf, lpszCmdLine, MB_ICONERROR|MB_SYSTEMMODAL);
	  exit (1);
	}

      MessageBox (NULL, "spawn-test-win32-gui: Will read from parent",
		  lpszCmdLine, MB_ICONINFORMATION|MB_SYSTEMMODAL);

      if ((k = read (infd, &n, sizeof (n))) != sizeof (n))
	{
	  sprintf (buf, "spawn-test-win32-gui: Got only %d bytes, wanted %d",
		   k, sizeof (n));
	  MessageBox (NULL, buf, lpszCmdLine, MB_ICONERROR|MB_SYSTEMMODAL);
	  exit (1);
	}

      sprintf (buf, "spawn-test-win32-gui: Parent says %d bytes to read", n);
      MessageBox (NULL, buf, lpszCmdLine, MB_ICONINFORMATION|MB_SYSTEMMODAL);

      if ((k = read (infd, buf, n)) != n)
	{
	  int errsv = errno;
	  if (k == -1)
	    sprintf (buf, "spawn-test-win32-gui: Read: %s", strerror (errsv));
	  else
	    sprintf (buf, "spawn-test-win32-gui: Got only %d bytes", k);
	  MessageBox (NULL, buf, lpszCmdLine, MB_ICONERROR|MB_SYSTEMMODAL);
	  exit (1);
	}

      MessageBox (NULL, "spawn-test-win32-gui: Will write more to parent",
		  lpszCmdLine, MB_ICONINFORMATION|MB_SYSTEMMODAL);

      n = strlen ("See ya");
      if (write (outfd, &n, sizeof (n)) == -1 ||
	  write (outfd, "See ya", n) == -1)
	{
	  int errsv = errno;
	  sprintf (buf, "spawn-test-win32-gui: Write: %s", strerror (errsv));
	  MessageBox (NULL, buf, lpszCmdLine, MB_ICONERROR|MB_SYSTEMMODAL);
	  exit (1);
	}
    }

  Sleep (2000);
  
  MessageBox (NULL, "spawn-test-win32-gui: Done, exiting.",
	      lpszCmdLine, MB_ICONINFORMATION|MB_SYSTEMMODAL);

  return 0;
}
