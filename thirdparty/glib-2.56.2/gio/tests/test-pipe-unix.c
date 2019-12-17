/* Unix pipe-to-self. This is a utility module for tests, not a test.
 *
 * Copyright © 2008-2010 Red Hat, Inc.
 * Copyright © 2011 Nokia Corporation
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
 *
 * Author: Simon McVittie <simon.mcvittie@collabora.co.uk>
 */

#include <errno.h>
#include <unistd.h>

#include <gio/gio.h>

#include "test-io-stream.h"
#include "test-pipe-unix.h"

#ifdef G_OS_UNIX
#   include <gio/gunixinputstream.h>
#   include <gio/gunixoutputstream.h>
#else
#   error This module only exists on Unix
#endif

/**
 * test_pipe:
 * @is: (out) (optional): used to return a #GInputStream
 * @os: (out) (optional): used to return a #GOutputStream
 * @error: used to raise an error
 *
 * Return a "pipe to self" connecting @is to @os. This can be used
 * as a unidirectional pipe to or from a child process, for instance.
 *
 * See test_bidi_pipe() if you want to emulate a bidirectional pipe
 * via a pair of unidirectional pipes.
 *
 * Returns: %TRUE on success
 */
gboolean
test_pipe (GInputStream  **is,
           GOutputStream **os,
           GError        **error)
{
  int pipefd[2];
  int ret;

  ret = pipe (pipefd);

  if (ret != 0)
    {
      int e = errno;

      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (e),
                   "%s", g_strerror (e));
      return FALSE;
    }

  if (is != NULL)
    *is = g_unix_input_stream_new (pipefd[0], TRUE);
  else
    close (pipefd[0]);

  if (os != NULL)
    *os = g_unix_output_stream_new (pipefd[1], TRUE);
  else
    close (pipefd[1]);

  return TRUE;
}

/**
 * test_bidi_pipe:
 * @left: (out) (optional): used to return one #GIOStream
 * @right: (out) (optional): used to return the other #GIOStream
 * @error: used to raise an error
 *
 * Return two #GIOStreams connected to each other with pipes.
 * The "left" input stream is connected by a unidirectional pipe
 * to the "right" output stream, and vice versa. This can be used
 * as a bidirectional pipe to a child process, for instance.
 *
 * See test_pipe() if you only need a one-way pipe.
 *
 * Returns: %TRUE on success
 */
gboolean
test_bidi_pipe (GIOStream **left,
                GIOStream **right,
                GError    **error)
{
  GInputStream *left_in = NULL;
  GOutputStream *left_out = NULL;
  GInputStream *right_in = NULL;
  GOutputStream *right_out = NULL;
  gboolean ret = FALSE;

  if (!test_pipe (&left_in, &right_out, error))
    goto out;

  if (!test_pipe (&right_in, &left_out, error))
    goto out;

  if (left != NULL)
    *left = test_io_stream_new (left_in, left_out);

  if (right != NULL)
    *right = test_io_stream_new (right_in, right_out);

  ret = TRUE;

out:
  g_clear_object (&left_in);
  g_clear_object (&left_out);
  g_clear_object (&right_in);
  g_clear_object (&right_out);
  return ret;
}
