/* GIO - GLib Input, IO and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include "glibintl.h"
#include "gioerror.h"
#include "gcancellable.h"
#include "glocalfileiostream.h"
#include "glocalfileinputstream.h"
#include "glocalfileinfo.h"

#ifdef G_OS_UNIX
#include "gfiledescriptorbased.h"
#endif


#define g_local_file_io_stream_get_type _g_local_file_io_stream_get_type
G_DEFINE_TYPE (GLocalFileIOStream, g_local_file_io_stream, G_TYPE_FILE_IO_STREAM)

static void
g_local_file_io_stream_finalize (GObject *object)
{
  GLocalFileIOStream *file;

  file = G_LOCAL_FILE_IO_STREAM (object);

  g_object_unref (file->input_stream);
  g_object_unref (file->output_stream);

  G_OBJECT_CLASS (g_local_file_io_stream_parent_class)->finalize (object);
}

GFileIOStream *
_g_local_file_io_stream_new (GLocalFileOutputStream *output_stream)
{
  GLocalFileIOStream *stream;
  int fd;

  stream = g_object_new (G_TYPE_LOCAL_FILE_IO_STREAM, NULL);
  stream->output_stream = g_object_ref (G_OUTPUT_STREAM (output_stream));
  _g_local_file_output_stream_set_do_close (output_stream, FALSE);
  fd = _g_local_file_output_stream_get_fd (output_stream);
  stream->input_stream = (GInputStream *)_g_local_file_input_stream_new (fd);

  _g_local_file_input_stream_set_do_close (G_LOCAL_FILE_INPUT_STREAM (stream->input_stream),
					   FALSE);

  return G_FILE_IO_STREAM (stream);
}

static GInputStream *
g_local_file_io_stream_get_input_stream (GIOStream *stream)
{
  return G_LOCAL_FILE_IO_STREAM (stream)->input_stream;
}

static GOutputStream *
g_local_file_io_stream_get_output_stream (GIOStream *stream)
{
  return G_LOCAL_FILE_IO_STREAM (stream)->output_stream;
}


static gboolean
g_local_file_io_stream_close (GIOStream  *stream,
			      GCancellable   *cancellable,
			      GError        **error)
{
  GLocalFileIOStream *file = G_LOCAL_FILE_IO_STREAM (stream);

  /* There are shortcutted and can't fail */
  g_output_stream_close (file->output_stream, cancellable, NULL);
  g_input_stream_close (file->input_stream, cancellable, NULL);

  return
    _g_local_file_output_stream_really_close (G_LOCAL_FILE_OUTPUT_STREAM (file->output_stream),
					      cancellable, error);
}

static void
g_local_file_io_stream_class_init (GLocalFileIOStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GIOStreamClass *stream_class = G_IO_STREAM_CLASS (klass);

  gobject_class->finalize = g_local_file_io_stream_finalize;

  stream_class->get_input_stream = g_local_file_io_stream_get_input_stream;
  stream_class->get_output_stream = g_local_file_io_stream_get_output_stream;
  stream_class->close_fn = g_local_file_io_stream_close;
}

static void
g_local_file_io_stream_init (GLocalFileIOStream *stream)
{
}
