/* GIO - GLib Input, Output and Streaming Library
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
#include "giomodule.h"
#include "giomodule-priv.h"

#include <gstdio.h>
#include <errno.h>
#include <locale.h>

static gboolean
is_valid_module_name (const gchar *basename)
{
#if !defined(G_OS_WIN32) && !defined(G_WITH_CYGWIN)
  return
    g_str_has_prefix (basename, "lib") &&
    g_str_has_suffix (basename, ".so");
#else
  return g_str_has_suffix (basename, ".dll");
#endif
}

static void
query_dir (const char *dirname)
{
  GString *data;
  GDir *dir;
  GList *list = NULL, *iterator = NULL;
  const char *name;
  char *cachename;
  char **(* query)  (void);
  GError *error;
  int i;

  if (!g_module_supported ())
    return;

  error = NULL;
  dir = g_dir_open (dirname, 0, &error);
  if (!dir)
    {
      g_printerr ("Unable to open directory %s: %s\n", dirname, error->message);
      g_error_free (error);
      return;
    }

  data = g_string_new ("");

  while ((name = g_dir_read_name (dir)))
    list = g_list_prepend (list, g_strdup (name));

  list = g_list_sort (list, (GCompareFunc) g_strcmp0);
  for (iterator = list; iterator; iterator = iterator->next)
    {
      GModule *module;
      gchar     *path;
      char **extension_points;

      name = iterator->data;
      if (!is_valid_module_name (name))
	continue;

      path = g_build_filename (dirname, name, NULL);
      module = g_module_open (path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
      g_free (path);

      if (module)
	{
	  gchar *modulename;
	  gchar *symname;

	  modulename = _g_io_module_extract_name (name);
	  symname = g_strconcat ("g_io_", modulename, "_query", NULL);
	  g_module_symbol (module, symname, (gpointer) &query);
	  g_free (symname);
	  g_free (modulename);

	  if (!query)
	    {
	      /* Fallback to old name */
	      g_module_symbol (module, "g_io_module_query", (gpointer) &query);
	    }

	  if (query)
	    {
	      extension_points = query ();

	      if (extension_points)
		{
		  g_string_append_printf (data, "%s: ", name);

		  for (i = 0; extension_points[i] != NULL; i++)
		    g_string_append_printf (data, "%s%s", i == 0 ? "" : ",", extension_points[i]);

		  g_string_append (data, "\n");
		  g_strfreev (extension_points);
		}
	    }

	  g_module_close (module);
	}
    }

  g_dir_close (dir);
  g_list_free_full (list, g_free);

  cachename = g_build_filename (dirname, "giomodule.cache", NULL);

  if (data->len > 0)
    {
      error = NULL;

      if (!g_file_set_contents (cachename, data->str, data->len, &error))
        {
          g_printerr ("Unable to create %s: %s\n", cachename, error->message);
          g_error_free (error);
        }
    }
  else
    {
      if (g_unlink (cachename) != 0 && errno != ENOENT)
        {
          int errsv = errno;
          g_printerr ("Unable to unlink %s: %s\n", cachename, g_strerror (errsv));
        }
    }

  g_free (cachename);
  g_string_free (data, TRUE);
}

int
main (gint   argc,
      gchar *argv[])
{
  int i;

  if (argc == 1)
    {
      g_print ("Usage: gio-querymodules <directory1> [<directory2> ...]\n");
      g_print ("Will update giomodule.cache in the listed directories\n");
      return 1;
    }

  setlocale (LC_ALL, "");

  /* Be defensive and ensure we're linked to GObject */
  g_type_ensure (G_TYPE_OBJECT);

  for (i = 1; i < argc; i++)
    query_dir (argv[i]);

  return 0;
}
