/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2012 Colin Walters <walters@verbum.org>
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

#ifndef __G_SUBPROCESS_CONTEXT_PRIVATE_H__
#define __G_SUBPROCESS_CONTEXT_PRIVATE_H__

#include "gsubprocesslauncher.h"

G_BEGIN_DECLS

struct _GSubprocessLauncher
{
  GObject parent;

  GSubprocessFlags flags;
  gboolean path_from_envp;
  char **envp;
  char *cwd;

#ifdef G_OS_UNIX
  gint stdin_fd;
  gchar *stdin_path;

  gint stdout_fd;
  gchar *stdout_path;

  gint stderr_fd;
  gchar *stderr_path;

  GArray *basic_fd_assignments;
  GArray *needdup_fd_assignments;

  GSpawnChildSetupFunc child_setup_func;
  gpointer child_setup_user_data;
  GDestroyNotify child_setup_destroy_notify;
#endif
};

void g_subprocess_set_launcher (GSubprocess         *subprocess,
                                GSubprocessLauncher *launcher);

G_END_DECLS

#endif
