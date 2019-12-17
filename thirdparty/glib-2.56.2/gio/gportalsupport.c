/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2016 Red Hat, Inc.
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

#include "config.h"

#include "gportalsupport.h"

static gboolean flatpak_info_read;
static gboolean use_portal;
static gboolean network_available;

static void
read_flatpak_info (void)
{
  const gchar *path = "/.flatpak-info";

  if (flatpak_info_read)
    return;

  flatpak_info_read = TRUE;

  if (g_file_test (path, G_FILE_TEST_EXISTS))
    {
      GKeyFile *keyfile;

      use_portal = TRUE;
      network_available = FALSE;

      keyfile = g_key_file_new ();
      if (g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, NULL))
        {
          char **shared = NULL;

          shared = g_key_file_get_string_list (keyfile, "Context", "shared", NULL, NULL);
          if (shared)
            {
              network_available = g_strv_contains ((const char * const *)shared, "network");
              g_strfreev (shared);
            }
        }

      g_key_file_unref (keyfile);
    }
  else
    {
      const char *var;

      var = g_getenv ("GTK_USE_PORTAL");
      if (var && var[0] == '1')
        use_portal = TRUE;
      network_available = TRUE;
    }
}

gboolean
glib_should_use_portal (void)
{
  read_flatpak_info ();
  return use_portal;
}

gboolean
glib_network_available_in_sandbox (void)
{
  read_flatpak_info ();
  return network_available;
}

