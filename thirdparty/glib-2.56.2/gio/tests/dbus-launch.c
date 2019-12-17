/*
 * Mock version of dbus-launch, for gdbus-unix-addresses test
 *
 * Copyright © 2015 Collabora Ltd.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>

#ifndef G_OS_UNIX
#error This is a Unix-specific test helper
#endif

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define ME "GDBus mock version of dbus-launch"

static void
write_all (const void *ptr,
           size_t      len)
{
  const char *p = ptr;

  while (len > 0)
    {
      gssize done = write (STDOUT_FILENO, p, len);
      int errsv = errno;

      if (done == 0)
        {
          g_error ("%s: write: EOF", ME);
        }
      else if (done < 0)
        {
          if (errsv == EINTR)
            continue;

          g_error ("%s: write: %s", ME, g_strerror (errsv));
        }
      else
        {
          if (len < (size_t) done)
            g_error ("%s: wrote too many bytes?", ME);

          len -= done;
          p += done;
        }
    }
}

int
main (int   argc,
      char *argv[])
{
  pid_t pid = 0x2323;
  long window_id = 0x42424242;
  const char *addr = "hello:this=address-is-from-the,mock=dbus-launch";

  write_all (addr, strlen (addr) + 1);
  write_all (&pid, sizeof (pid));
  write_all (&window_id, sizeof (window_id));
  return 0;
}
