/* glib-private.h - GLib-internal private API, shared between glib, gobject, gio
 * Copyright (C) 2011 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GLIB_PRIVATE_H__
#define __GLIB_PRIVATE_H__

#include <glib.h>
#include "gwakeup.h"
#include "gstdioprivate.h"

#if defined(__GNUC__)
# define _g_alignof(type) (__alignof__ (type))
#else
# define _g_alignof(type) (G_STRUCT_OFFSET (struct { char a; type b; }, b))
#endif

GMainContext *          g_get_worker_context            (void);
gboolean                g_check_setuid                  (void);
GMainContext *          g_main_context_new_with_next_id (guint next_id);

#ifdef G_OS_WIN32
gchar *_glib_get_dll_directory (void);
GLIB_AVAILABLE_IN_ALL
gchar *_glib_get_locale_dir    (void);
#endif

GDir * g_dir_open_with_errno (const gchar *path, guint flags);
GDir * g_dir_new_from_dirp (gpointer dirp);

#define GLIB_PRIVATE_CALL(symbol) (glib__private__()->symbol)

typedef struct {
  /* See gwakeup.c */
  GWakeup *             (* g_wakeup_new)                (void);
  void                  (* g_wakeup_free)               (GWakeup *wakeup);
  void                  (* g_wakeup_get_pollfd)         (GWakeup *wakeup,
                                                        GPollFD *poll_fd);
  void                  (* g_wakeup_signal)             (GWakeup *wakeup);
  void                  (* g_wakeup_acknowledge)        (GWakeup *wakeup);

  /* See gmain.c */
  GMainContext *        (* g_get_worker_context)        (void);

  gboolean              (* g_check_setuid)              (void);
  GMainContext *        (* g_main_context_new_with_next_id) (guint next_id);

  GDir *                (* g_dir_open_with_errno)       (const gchar *path,
                                                         guint        flags);
  GDir *                (* g_dir_new_from_dirp)         (gpointer dirp);

  /* See glib-init.c */
  void                  (* glib_init)                   (void);

  /* See gstdio.c */
#ifdef G_OS_WIN32
  int                   (* g_win32_stat_utf8)           (const gchar       *filename,
                                                         GWin32PrivateStat *buf);

  int                   (* g_win32_lstat_utf8)          (const gchar       *filename,
                                                         GWin32PrivateStat *buf);

  int                   (* g_win32_readlink_utf8)       (const gchar *filename,
                                                         gchar       *buf,
                                                         gsize        buf_size);

  int                   (* g_win32_fstat)               (int                fd,
                                                         GWin32PrivateStat *buf);
#endif


  /* Add other private functions here, initialize them in glib-private.c */
} GLibPrivateVTable;

GLIB_AVAILABLE_IN_ALL
GLibPrivateVTable *glib__private__ (void);

#endif /* __GLIB_PRIVATE_H__ */
