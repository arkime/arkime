/*******************************************************************************
  Copyright (c) 2012 Dmitry Matveev <me@dmitrymatveev.co.uk>
  Copyright (c) 2012 Antoine Jacoutot <ajacoutot@openbsd.org>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*******************************************************************************/

#include <fcntl.h>
#include <glib.h>
#include <gio/gio.h>
#include "kqueue-exclusions.h"

static gboolean ke_debug_enabled = FALSE;
#define KE_W if (ke_debug_enabled) g_warning

/*
 * _ke_is_excluded:
 * @full_path - a path to file to check.
 *
 * Returns: TRUE if the file should be excluded from the kqueue-powered
 *      monitoring, FALSE otherwise.
 **/
gboolean
_ke_is_excluded (const char *full_path)
{
#if defined (O_EVTONLY)
  return FALSE;
#else
  GFile *f = NULL;
  GMount *mount = NULL;

  f = g_file_new_for_path (full_path);

  if (f != NULL) {
    mount = g_file_find_enclosing_mount (f, NULL, NULL);
    g_object_unref (f);
  }

  if ((mount != NULL && (g_mount_can_unmount (mount))) || g_str_has_prefix (full_path, "/mnt/"))
  {
    KE_W ("Excluding %s from kernel notification, falling back to poll", full_path);
    if (mount)
      g_object_unref (mount);
    return TRUE;
  }
  else
    return FALSE;
#endif
}
