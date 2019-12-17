/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2010 Christian Kellner
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
 * Author: Christian Kellner <gicmo@gnome.org>
 */

#include "config.h"
#include "gfiledescriptorbased.h"
#include "glibintl.h"


/**
 * SECTION:gfiledescriptorbased
 * @short_description: Interface for file descriptor based IO
 * @include: gio/gfiledescriptorbased.h
 * @see_also: #GInputStream, #GOutputStream
 *
 * #GFileDescriptorBased is implemented by streams (implementations of
 * #GInputStream or #GOutputStream) that are based on file descriptors.
 *
 * Note that `<gio/gfiledescriptorbased.h>` belongs to the UNIX-specific
 * GIO interfaces, thus you have to use the `gio-unix-2.0.pc` pkg-config
 * file when using it.
 *
 * Since: 2.24
 *
 **/

typedef GFileDescriptorBasedIface GFileDescriptorBasedInterface;
G_DEFINE_INTERFACE (GFileDescriptorBased, g_file_descriptor_based, G_TYPE_OBJECT)

static void
g_file_descriptor_based_default_init (GFileDescriptorBasedInterface *iface)
{
}

/**
 * g_file_descriptor_based_get_fd:
 * @fd_based: a #GFileDescriptorBased.
 *
 * Gets the underlying file descriptor.
 *
 * Returns: The file descriptor
 *
 * Since: 2.24
 **/
int
g_file_descriptor_based_get_fd (GFileDescriptorBased *fd_based)
{
  GFileDescriptorBasedIface *iface;

  g_return_val_if_fail (G_IS_FILE_DESCRIPTOR_BASED (fd_based), 0);

  iface = G_FILE_DESCRIPTOR_BASED_GET_IFACE (fd_based);

  return (* iface->get_fd) (fd_based);
}
