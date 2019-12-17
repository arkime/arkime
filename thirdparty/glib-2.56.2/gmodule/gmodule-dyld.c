/* GMODULE - GLIB wrapper code for dynamic module loading
 * Copyright (C) 1998, 2000 Tim Janik
 *
 * dyld (Darwin) GMODULE implementation
 * Copyright (C) 2001 Dan Winship
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"

#include <mach-o/dyld.h>

static gpointer self_module = GINT_TO_POINTER (1);

static gpointer
_g_module_open (const gchar *file_name,
		gboolean     bind_lazy,
		gboolean     bind_local)
{
  NSObjectFileImage image;
  NSObjectFileImageReturnCode ret;
  NSModule module;
  unsigned long options;
  char *msg;

  ret = NSCreateObjectFileImageFromFile (file_name, &image);
  if (ret != NSObjectFileImageSuccess)
    {
      switch (ret)
	{
	case NSObjectFileImageInappropriateFile:
	case NSObjectFileImageFormat:
	  msg = g_strdup_printf ("%s is not a loadable module", file_name);
	  break;

	case NSObjectFileImageArch:
	  msg = g_strdup_printf ("%s is not built for this architecture",
				 file_name);
	  break;

	case NSObjectFileImageAccess:
	  if (access (file_name, F_OK) == 0)
	    msg = g_strdup_printf ("%s: permission denied", file_name);
	  else
	    msg = g_strdup_printf ("%s: no such file or directory", file_name);
	  break;

	default:
	  msg = g_strdup_printf ("unknown error for %s", file_name);
	  break;
	}

      g_module_set_error (msg);
      g_free (msg);
      return NULL;
    }

  options = NSLINKMODULE_OPTION_RETURN_ON_ERROR;
  if (bind_local)
    options |= NSLINKMODULE_OPTION_PRIVATE;
  if (!bind_lazy)
    options |= NSLINKMODULE_OPTION_BINDNOW;
  module = NSLinkModule (image, file_name, options);
  NSDestroyObjectFileImage (image);
  if (!module)
    {
      NSLinkEditErrors c;
      int error_number;
      const char *file, *error;

      NSLinkEditError (&c, &error_number, &file, &error);
      msg = g_strdup_printf ("could not link %s: %s", file_name, error);
      g_module_set_error (msg);
      g_free (msg);
      return NULL;
    }

  return module;
}

static gpointer
_g_module_self (void)
{
  return &self_module;
}

static void
_g_module_close (gpointer handle,
		 gboolean is_unref)
{
  if (handle == &self_module)
    return;

  if (!NSUnLinkModule (handle, 0))
    g_module_set_error ("could not unlink module");
}

static gpointer
_g_module_symbol (gpointer     handle,
		  const gchar *symbol_name)
{
  NSSymbol sym;
  char *msg;

  if (handle == &self_module)
    {
      if (NSIsSymbolNameDefined (symbol_name))
	sym = NSLookupAndBindSymbol (symbol_name);
      else
	sym = NULL;
    }
  else
    sym = NSLookupSymbolInModule (handle, symbol_name);

  if (!sym)
    {
      msg = g_strdup_printf ("no such symbol %s", symbol_name);
      g_module_set_error (msg);
      g_free (msg);
      return NULL;
    }

  return NSAddressOfSymbol (sym);
}

static gchar*
_g_module_build_path (const gchar *directory,
		      const gchar *module_name)
{
  if (directory && *directory)
    {
      if (strncmp (module_name, "lib", 3) == 0)
	return g_strconcat (directory, "/", module_name, NULL);
      else
	return g_strconcat (directory, "/lib", module_name, "." G_MODULE_SUFFIX, NULL);
    }
  else if (strncmp (module_name, "lib", 3) == 0)
    return g_strdup (module_name);
  else
    return g_strconcat ("lib", module_name, "." G_MODULE_SUFFIX, NULL);
}
