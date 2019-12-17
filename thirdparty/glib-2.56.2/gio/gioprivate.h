/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2013 Collabora Ltd.
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

#ifndef __G_IO_PRIVATE_H__
#define __G_IO_PRIVATE_H__

#include "ginputstream.h"
#include "goutputstream.h"
#include "gsocketconnection.h"
#include "gsocketaddress.h"

G_BEGIN_DECLS

gboolean g_input_stream_async_read_is_via_threads (GInputStream *stream);
gboolean g_input_stream_async_close_is_via_threads (GInputStream *stream);
gboolean g_output_stream_async_write_is_via_threads (GOutputStream *stream);
gboolean g_output_stream_async_close_is_via_threads (GOutputStream *stream);

void g_socket_connection_set_cached_remote_address (GSocketConnection *connection,
                                                    GSocketAddress    *address);


G_END_DECLS

#endif /* __G_IO_PRIVATE__ */
