/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2017 Collabora Inc.
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
 * Author: Xavier Claessens <xavier.claessens@collabora.com>
 */

#include "config.h"
#include "giomodule.h"
#include "giomodule-priv.h"

#include <string.h>

/**
 * _g_io_module_extract_name:
 * @filename: filename of a GIOModule
 *
 * Extract the plugin name from its filename. It removes optional "lib" or
 * "libgio" prefix, and removes everything after the first dot. For example:
 * "libgiognutls.so" -> "gnutls".
 *
 * Returns: (transfer full): the module's name
 */
gchar *
_g_io_module_extract_name (const char *filename)
{
  gchar *bname, *name;
  const gchar *dot;
  gsize prefix_len, len;
  gsize i;

  bname = g_path_get_basename (filename);
  for (i = 0; bname[i]; ++i)
    {
      if (bname[i] == '-')
        bname[i] = '_';
    }

  if (g_str_has_prefix (bname, "libgio"))
    prefix_len = 6;
  else if (g_str_has_prefix (bname, "lib"))
    prefix_len = 3;
  else
    prefix_len = 0; /* use whole name (minus suffix) as plugin name */

  dot = strchr (bname, '.');
  if (dot != NULL)
    len = dot - bname - prefix_len;
  else
    len = strlen (bname + prefix_len);

  name = g_strndup (bname + prefix_len, len);
  g_free (bname);

  return name;
}
