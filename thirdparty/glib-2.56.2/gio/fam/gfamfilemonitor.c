/*
 * Copyright © 2015 Canonical Limited
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
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include <gio/glocalfilemonitor.h>
#include <gio/giomodule.h>
#include "glib-private.h"
#include <glib-unix.h>
#include <fam.h>

static GMutex         fam_lock;
static gboolean       fam_initialised;
static FAMConnection  fam_connection;
static GSource       *fam_source;

#define G_TYPE_FAM_FILE_MONITOR      (g_fam_file_monitor_get_type ())
#define G_FAM_FILE_MONITOR(inst)     (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                      G_TYPE_FAM_FILE_MONITOR, GFamFileMonitor))

typedef GLocalFileMonitorClass GFamFileMonitorClass;

typedef struct
{
  GLocalFileMonitor parent_instance;

  FAMRequest request;
} GFamFileMonitor;

static GType g_fam_file_monitor_get_type (void);
G_DEFINE_DYNAMIC_TYPE (GFamFileMonitor, g_fam_file_monitor, G_TYPE_LOCAL_FILE_MONITOR)

static gboolean
g_fam_file_monitor_callback (gint         fd,
                             GIOCondition condition,
                             gpointer     user_data)
{
  gint64 now = g_source_get_time (fam_source);

  g_mutex_lock (&fam_lock);

  while (FAMPending (&fam_connection))
    {
      const gchar *child;
      FAMEvent ev;

      if (FAMNextEvent (&fam_connection, &ev) != 1)
        {
          /* The daemon died.  We're in a really bad situation now
           * because we potentially have a bunch of request structures
           * outstanding which no longer make any sense to anyone.
           *
           * The best thing that we can do is do nothing.  Notification
           * won't work anymore for this process.
           */
          g_mutex_unlock (&fam_lock);

          g_warning ("Lost connection to FAM (file monitoring) service.  Expect no further file monitor events.");

          return FALSE;
        }

      /* We expect ev.filename to be a relative path for children in a
       * monitored directory, and an absolute path for a monitored file
       * or the directory itself.
       */
      if (ev.filename[0] != '/')
        child = ev.filename;
      else
        child = NULL;

      switch (ev.code)
        {
        case FAMAcknowledge:
          g_source_unref (ev.userdata);
          break;

        case FAMChanged:
          g_file_monitor_source_handle_event (ev.userdata, G_FILE_MONITOR_EVENT_CHANGED, child, NULL, NULL, now);
          break;

        case FAMDeleted:
          g_file_monitor_source_handle_event (ev.userdata, G_FILE_MONITOR_EVENT_DELETED, child, NULL, NULL, now);
          break;

        case FAMCreated:
          g_file_monitor_source_handle_event (ev.userdata, G_FILE_MONITOR_EVENT_CREATED, child, NULL, NULL, now);
          break;

        default:
          /* unknown type */
          break;
        }
    }

  g_mutex_unlock (&fam_lock);

  return TRUE;
}

static gboolean
g_fam_file_monitor_is_supported (void)
{
  g_mutex_lock (&fam_lock);

  if (!fam_initialised)
    {
      fam_initialised = FAMOpen2 (&fam_connection, "GLib GIO") == 0;

      if (fam_initialised)
        {
#ifdef HAVE_FAM_NO_EXISTS
          /* This is a gamin extension that avoids sending all the
           * Exists event for dir monitors
           */
          FAMNoExists (&fam_connection);
#endif

          fam_source = g_unix_fd_source_new (FAMCONNECTION_GETFD (&fam_connection), G_IO_IN);
          g_source_set_callback (fam_source, (GSourceFunc) g_fam_file_monitor_callback, NULL, NULL);
          g_source_attach (fam_source, GLIB_PRIVATE_CALL(g_get_worker_context) ());
        }
    }

  g_mutex_unlock (&fam_lock);

  return fam_initialised;
}

static gboolean
g_fam_file_monitor_cancel (GFileMonitor *monitor)
{
  GFamFileMonitor *gffm = G_FAM_FILE_MONITOR (monitor);

  g_mutex_lock (&fam_lock);

  g_assert (fam_initialised);

  FAMCancelMonitor (&fam_connection, &gffm->request);

  g_mutex_unlock (&fam_lock);

  return TRUE;
}

static void
g_fam_file_monitor_start (GLocalFileMonitor  *local_monitor,
                          const gchar        *dirname,
                          const gchar        *basename,
                          const gchar        *filename,
                          GFileMonitorSource *source)
{
  GFamFileMonitor *gffm = G_FAM_FILE_MONITOR (local_monitor);

  g_mutex_lock (&fam_lock);

  g_assert (fam_initialised);

  g_source_ref ((GSource *) source);

  if (dirname)
    FAMMonitorDirectory (&fam_connection, dirname, &gffm->request, source);
  else
    FAMMonitorFile (&fam_connection, filename, &gffm->request, source);

  g_mutex_unlock (&fam_lock);
}

static void
g_fam_file_monitor_init (GFamFileMonitor* monitor)
{
}

static void
g_fam_file_monitor_class_init (GFamFileMonitorClass *class)
{
  GFileMonitorClass *file_monitor_class = G_FILE_MONITOR_CLASS (class);

  class->is_supported = g_fam_file_monitor_is_supported;
  class->start = g_fam_file_monitor_start;
  file_monitor_class->cancel = g_fam_file_monitor_cancel;
}

static void
g_fam_file_monitor_class_finalize (GFamFileMonitorClass *class)
{
}

void
g_io_module_load (GIOModule *module)
{
  g_type_module_use (G_TYPE_MODULE (module));

  g_fam_file_monitor_register_type (G_TYPE_MODULE (module));

  g_io_extension_point_implement (G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME,
                                 G_TYPE_FAM_FILE_MONITOR, "fam", 10);

  g_io_extension_point_implement (G_NFS_FILE_MONITOR_EXTENSION_POINT_NAME,
                                 G_TYPE_FAM_FILE_MONITOR, "fam", 10);
}

void
g_io_module_unload (GIOModule *module)
{
  g_assert_not_reached ();
}

char **
g_io_module_query (void)
{
  char *eps[] = {
    G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME,
    G_NFS_FILE_MONITOR_EXTENSION_POINT_NAME,
    NULL
  };

  return g_strdupv (eps);
}
