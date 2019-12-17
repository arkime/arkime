/* GMODULE - GLIB wrapper code for dynamic module loading
 * Copyright (C) 1998, 2000 Tim Janik
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */
#include "config.h"

#include <dlfcn.h>

/* Perl includes <nlist.h> and <link.h> instead of <dlfcn.h> on some systmes? */


/* dlerror() is not implemented on all systems
 */
#ifndef	G_MODULE_HAVE_DLERROR
#  ifdef __NetBSD__
#    define dlerror()	g_strerror (errno)
#  else /* !__NetBSD__ */
/* could we rely on errno's state here? */
#    define dlerror()	"unknown dl-error"
#  endif /* !__NetBSD__ */
#endif	/* G_MODULE_HAVE_DLERROR */

/* some flags are missing on some systems, so we provide
 * harmless defaults.
 * The Perl sources say, RTLD_LAZY needs to be defined as (1),
 * at least for Solaris 1.
 *
 * Mandatory:
 * RTLD_LAZY   - resolve undefined symbols as code from the dynamic library
 *		 is executed.
 * RTLD_NOW    - resolve all undefined symbols before dlopen returns, and fail
 *		 if this cannot be done.
 * Optionally:
 * RTLD_GLOBAL - the external symbols defined in the library will be made
 *		 available to subsequently loaded libraries.
 */
#ifndef	HAVE_RTLD_LAZY
#define	RTLD_LAZY	1
#endif	/* RTLD_LAZY */
#ifndef	HAVE_RTLD_NOW
#define	RTLD_NOW	0
#endif	/* RTLD_NOW */
/* some systems (OSF1 V5.0) have broken RTLD_GLOBAL linkage */
#ifdef G_MODULE_BROKEN_RTLD_GLOBAL
#undef	RTLD_GLOBAL
#undef	HAVE_RTLD_GLOBAL
#endif /* G_MODULE_BROKEN_RTLD_GLOBAL */
#ifndef	HAVE_RTLD_GLOBAL
#define	RTLD_GLOBAL	0
#endif	/* RTLD_GLOBAL */


/* --- functions --- */
static gchar*
fetch_dlerror (gboolean replace_null)
{
  gchar *msg = dlerror ();

  /* make sure we always return an error message != NULL, if
   * expected to do so. */

  if (!msg && replace_null)
    return "unknown dl-error";

  return msg;
}

static gpointer
_g_module_open (const gchar *file_name,
		gboolean     bind_lazy,
		gboolean     bind_local)
{
  gpointer handle;
  
  handle = dlopen (file_name,
		   (bind_local ? 0 : RTLD_GLOBAL) | (bind_lazy ? RTLD_LAZY : RTLD_NOW));
  if (!handle)
    g_module_set_error (fetch_dlerror (TRUE));
  
  return handle;
}

static gpointer
_g_module_self (void)
{
  gpointer handle;
  
  /* to query symbols from the program itself, special link options
   * are required on some systems.
   */

  /* On Android 32 bit (i.e. not __LP64__), dlopen(NULL)
   * does not work reliable and generally no symbols are found
   * at all. RTLD_DEFAULT works though.
   * On Android 64 bit, dlopen(NULL) seems to work but dlsym(handle)
   * always returns 'undefined symbol'. Only if RTLD_DEFAULT or 
   * NULL is given, dlsym returns an appropriate pointer.
   */
#if defined(__BIONIC__)
  handle = RTLD_DEFAULT;
#else
  handle = dlopen (NULL, RTLD_GLOBAL | RTLD_LAZY);
#endif
  if (!handle)
    g_module_set_error (fetch_dlerror (TRUE));
  
  return handle;
}

static void
_g_module_close (gpointer handle,
		 gboolean is_unref)
{
  /* are there any systems out there that have dlopen()/dlclose()
   * without a reference count implementation?
   *
   * See above for the Android special case
   */
#if defined(__BIONIC__)
  is_unref = (handle != RTLD_DEFAULT);
#else
  is_unref |= 1;
#endif

  if (is_unref)
    {
      if (dlclose (handle) != 0)
	g_module_set_error (fetch_dlerror (TRUE));
    }
}

static gpointer
_g_module_symbol (gpointer     handle,
		  const gchar *symbol_name)
{
  gpointer p;
  gchar *msg;

  fetch_dlerror (FALSE);
  p = dlsym (handle, symbol_name);
  msg = fetch_dlerror (FALSE);
  if (msg)
    g_module_set_error (msg);
  
  return p;
}

static gchar*
_g_module_build_path (const gchar *directory,
		      const gchar *module_name)
{
  if (directory && *directory) {
    if (strncmp (module_name, "lib", 3) == 0)
      return g_strconcat (directory, "/", module_name, NULL);
    else
      return g_strconcat (directory, "/lib", module_name, "." G_MODULE_SUFFIX, NULL);
  } else if (strncmp (module_name, "lib", 3) == 0)
    return g_strdup (module_name);
  else
    return g_strconcat ("lib", module_name, "." G_MODULE_SUFFIX, NULL);
}
