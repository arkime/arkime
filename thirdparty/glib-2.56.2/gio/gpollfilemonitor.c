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
#include <string.h>

#include "gpollfilemonitor.h"
#include "gfile.h"
#include "gfilemonitor.h"
#include "gfileinfo.h"


static gboolean g_poll_file_monitor_cancel (GFileMonitor* monitor);
static void schedule_poll_timeout (GPollFileMonitor* poll_monitor);

struct _GPollFileMonitor
{
  GFileMonitor parent_instance;
  GFile *file;
  GFileInfo *last_info;
  GSource *timeout;
};

#define POLL_TIME_SECS 5

#define g_poll_file_monitor_get_type _g_poll_file_monitor_get_type
G_DEFINE_TYPE (GPollFileMonitor, g_poll_file_monitor, G_TYPE_FILE_MONITOR)

static void
g_poll_file_monitor_finalize (GObject* object)
{
  GPollFileMonitor* poll_monitor;
  
  poll_monitor = G_POLL_FILE_MONITOR (object);

  g_poll_file_monitor_cancel (G_FILE_MONITOR (poll_monitor));
  g_object_unref (poll_monitor->file);
  g_clear_object (&poll_monitor->last_info);

  G_OBJECT_CLASS (g_poll_file_monitor_parent_class)->finalize (object);
}


static void
g_poll_file_monitor_class_init (GPollFileMonitorClass* klass)
{
  GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
  GFileMonitorClass *file_monitor_class = G_FILE_MONITOR_CLASS (klass);
  
  gobject_class->finalize = g_poll_file_monitor_finalize;

  file_monitor_class->cancel = g_poll_file_monitor_cancel;
}

static void
g_poll_file_monitor_init (GPollFileMonitor* poll_monitor)
{
}

static int
calc_event_type (GFileInfo *last,
		 GFileInfo *new)
{
  if (last == NULL && new == NULL)
    return -1;

  if (last == NULL && new != NULL)
    return G_FILE_MONITOR_EVENT_CREATED;
  
  if (last != NULL && new == NULL)
    return G_FILE_MONITOR_EVENT_DELETED;

  if (g_strcmp0 (g_file_info_get_etag (last), g_file_info_get_etag (new)))
    return G_FILE_MONITOR_EVENT_CHANGED;
  
  if (g_file_info_get_size (last) != g_file_info_get_size (new))
    return G_FILE_MONITOR_EVENT_CHANGED;

  return -1;
}

static void
got_new_info (GObject      *source_object,
              GAsyncResult *res,
              gpointer      user_data)
{
  GPollFileMonitor* poll_monitor = user_data;
  GFileInfo *info;
  int event;

  info = g_file_query_info_finish (poll_monitor->file, res, NULL);

  if (!g_file_monitor_is_cancelled (G_FILE_MONITOR (poll_monitor)))
    {
      event = calc_event_type (poll_monitor->last_info, info);

      if (event != -1)
	{
	  g_file_monitor_emit_event (G_FILE_MONITOR (poll_monitor),
				     poll_monitor->file,
				     NULL, event);
	  /* We're polling so slowly anyway, so always emit the done hint */
	  if (!g_file_monitor_is_cancelled (G_FILE_MONITOR (poll_monitor)) &&
             (event == G_FILE_MONITOR_EVENT_CHANGED || event == G_FILE_MONITOR_EVENT_CREATED))
	    g_file_monitor_emit_event (G_FILE_MONITOR (poll_monitor),
				       poll_monitor->file,
				       NULL, G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT);
	}
      
      if (poll_monitor->last_info)
	{
	  g_object_unref (poll_monitor->last_info);
	  poll_monitor->last_info = NULL;
	}
      
      if (info)
	poll_monitor->last_info = g_object_ref (info);
      
      schedule_poll_timeout (poll_monitor);
    }

  if (info)
    g_object_unref (info);
  
  g_object_unref (poll_monitor);
}

static gboolean
poll_file_timeout (gpointer data)
{
  GPollFileMonitor* poll_monitor = data;

  g_source_unref (poll_monitor->timeout);
  poll_monitor->timeout = NULL;

  g_file_query_info_async (poll_monitor->file, G_FILE_ATTRIBUTE_ETAG_VALUE "," G_FILE_ATTRIBUTE_STANDARD_SIZE,
			 0, 0, NULL, got_new_info, g_object_ref (poll_monitor));
  
  return G_SOURCE_REMOVE;
}

static void
schedule_poll_timeout (GPollFileMonitor* poll_monitor)
{
  poll_monitor->timeout = g_timeout_source_new_seconds (POLL_TIME_SECS);
  g_source_set_callback (poll_monitor->timeout, poll_file_timeout, poll_monitor, NULL);
  g_source_attach (poll_monitor->timeout, g_main_context_get_thread_default ());
}

static void
got_initial_info (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  GPollFileMonitor* poll_monitor = user_data;
  GFileInfo *info;

  info = g_file_query_info_finish (poll_monitor->file, res, NULL);

  poll_monitor->last_info = info;

  if (!g_file_monitor_is_cancelled (G_FILE_MONITOR (poll_monitor)))
    schedule_poll_timeout (poll_monitor);
  
  g_object_unref (poll_monitor);
}

/**
 * _g_poll_file_monitor_new:
 * @file: a #GFile.
 * 
 * Polls @file for changes.
 * 
 * Returns: a new #GFileMonitor for the given #GFile. 
 **/
GFileMonitor*
_g_poll_file_monitor_new (GFile *file)
{
  GPollFileMonitor* poll_monitor;
  
  poll_monitor = g_object_new (G_TYPE_POLL_FILE_MONITOR, NULL);

  poll_monitor->file = g_object_ref (file);

  g_file_query_info_async (file, G_FILE_ATTRIBUTE_ETAG_VALUE "," G_FILE_ATTRIBUTE_STANDARD_SIZE,
			   0, 0, NULL, got_initial_info, g_object_ref (poll_monitor));
  
  return G_FILE_MONITOR (poll_monitor);
}

static gboolean
g_poll_file_monitor_cancel (GFileMonitor* monitor)
{
  GPollFileMonitor *poll_monitor = G_POLL_FILE_MONITOR (monitor);
  
  if (poll_monitor->timeout)
    {
      g_source_destroy (poll_monitor->timeout);
      g_source_unref (poll_monitor->timeout);
      poll_monitor->timeout = NULL;
    }
  
  return TRUE;
}
