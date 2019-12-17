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
 * MT safe
 */

/* because we are compatible with archive format only since AIX 4.3 */

#define __AR_BIG__

#include "config.h"

#include <ar.h>
#include <stdlib.h>

#include <dlfcn.h>

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

static gchar* _g_module_get_member(const gchar* file_name)
{
  gchar* member = NULL;
  struct fl_hdr file_header;
  struct ar_hdr ar_header;
  long first_member;
  long name_len;
  int fd;

  fd = open(file_name, O_RDONLY);
  if (fd == -1)
    return NULL;

  if (read(fd, (void*)&file_header, FL_HSZ) != FL_HSZ)
    goto exit;

  if (strncmp(file_header.fl_magic, AIAMAGBIG, SAIAMAG) != 0)
    goto exit;

  /* read first archive file member header */

  first_member = atol(file_header.fl_fstmoff);

  if (lseek(fd, first_member, SEEK_SET) != first_member)
    goto exit;

  if (read(fd, (void*)&ar_header, AR_HSZ - 2) != AR_HSZ - 2)
    goto exit;

  /* read member name */

  name_len = atol(ar_header.ar_namlen);

  member = g_malloc(name_len+1);
  if (!member)
    goto exit;

  if (read(fd, (void*)member, name_len) != name_len)
    {
      g_free(member);
      member = NULL;
      goto exit;
    }

  member[name_len] = 0;

exit:
  close(fd);

  return member;
}

static gpointer
_g_module_open (const gchar *file_name,
		gboolean     bind_lazy,
		gboolean     bind_local)
{
  gpointer handle;
  gchar* member;
  gchar* full_name;

  /* extract name of first member of archive */

  member = _g_module_get_member (file_name);
  if (member != NULL)
    {
      full_name = g_strconcat (file_name, "(", member, ")", NULL);
      g_free (member);
    }
  else
    full_name = g_strdup (file_name);
  
  handle = dlopen (full_name, 
		   (bind_local ? RTLD_LOCAL : RTLD_GLOBAL) | RTLD_MEMBER | (bind_lazy ? RTLD_LAZY : RTLD_NOW));

  g_free (full_name);

  if (!handle)
    g_module_set_error (fetch_dlerror (TRUE));
  
  return handle;
}

static gpointer
_g_module_self (void)
{
  gpointer handle;

  handle = dlopen (NULL, RTLD_GLOBAL | RTLD_LAZY);
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
   */
  is_unref |= 1;
  
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
  
  p = dlsym (handle, symbol_name);
  if (!p)
    g_module_set_error (fetch_dlerror (FALSE));
  
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
