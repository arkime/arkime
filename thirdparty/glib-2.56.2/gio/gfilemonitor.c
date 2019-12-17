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

#include "gfilemonitor.h"
#include "gioenumtypes.h"
#include "gfile.h"
#include "gvfs.h"
#include "glibintl.h"

/**
 * SECTION:gfilemonitor
 * @short_description: File Monitor
 * @include: gio/gio.h
 *
 * Monitors a file or directory for changes.
 *
 * To obtain a #GFileMonitor for a file or directory, use
 * g_file_monitor(), g_file_monitor_file(), or
 * g_file_monitor_directory().
 *
 * To get informed about changes to the file or directory you are
 * monitoring, connect to the #GFileMonitor::changed signal. The
 * signal will be emitted in the
 * [thread-default main context][g-main-context-push-thread-default]
 * of the thread that the monitor was created in
 * (though if the global default main context is blocked, this may
 * cause notifications to be blocked even if the thread-default
 * context is still running).
 **/

#define DEFAULT_RATE_LIMIT_MSECS 800

struct _GFileMonitorPrivate
{
  gboolean cancelled;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GFileMonitor, g_file_monitor, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_RATE_LIMIT,
  PROP_CANCELLED
};

static guint g_file_monitor_changed_signal;

static void
g_file_monitor_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  //GFileMonitor *monitor;

  //monitor = G_FILE_MONITOR (object);

  switch (prop_id)
    {
    case PROP_RATE_LIMIT:
      /* not supported by default */
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_file_monitor_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_RATE_LIMIT:
      /* we expect this to be overridden... */
      g_value_set_int (value, DEFAULT_RATE_LIMIT_MSECS);
      break;

    case PROP_CANCELLED:
      //g_mutex_lock (&fms->lock);
      g_value_set_boolean (value, FALSE);//fms->cancelled);
      //g_mutex_unlock (&fms->lock);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_file_monitor_dispose (GObject *object)
{
  GFileMonitor *monitor = G_FILE_MONITOR (object);

  /* Make sure we cancel on last unref */
  g_file_monitor_cancel (monitor);

  G_OBJECT_CLASS (g_file_monitor_parent_class)->dispose (object);
}

static void
g_file_monitor_init (GFileMonitor *monitor)
{
  monitor->priv = g_file_monitor_get_instance_private (monitor);
}

static void
g_file_monitor_class_init (GFileMonitorClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = g_file_monitor_dispose;
  object_class->get_property = g_file_monitor_get_property;
  object_class->set_property = g_file_monitor_set_property;

  /**
   * GFileMonitor::changed:
   * @monitor: a #GFileMonitor.
   * @file: a #GFile.
   * @other_file: (nullable): a #GFile or #NULL.
   * @event_type: a #GFileMonitorEvent.
   *
   * Emitted when @file has been changed.
   *
   * If using %G_FILE_MONITOR_WATCH_MOVES on a directory monitor, and
   * the information is available (and if supported by the backend),
   * @event_type may be %G_FILE_MONITOR_EVENT_RENAMED,
   * %G_FILE_MONITOR_EVENT_MOVED_IN or %G_FILE_MONITOR_EVENT_MOVED_OUT.
   *
   * In all cases @file will be a child of the monitored directory.  For
   * renames, @file will be the old name and @other_file is the new
   * name.  For "moved in" events, @file is the name of the file that
   * appeared and @other_file is the old name that it was moved from (in
   * another directory).  For "moved out" events, @file is the name of
   * the file that used to be in this directory and @other_file is the
   * name of the file at its new location.
   *
   * It makes sense to treat %G_FILE_MONITOR_EVENT_MOVED_IN as
   * equivalent to %G_FILE_MONITOR_EVENT_CREATED and
   * %G_FILE_MONITOR_EVENT_MOVED_OUT as equivalent to
   * %G_FILE_MONITOR_EVENT_DELETED, with extra information.
   * %G_FILE_MONITOR_EVENT_RENAMED is equivalent to a delete/create
   * pair.  This is exactly how the events will be reported in the case
   * that the %G_FILE_MONITOR_WATCH_MOVES flag is not in use.
   *
   * If using the deprecated flag %G_FILE_MONITOR_SEND_MOVED flag and @event_type is
   * #G_FILE_MONITOR_EVENT_MOVED, @file will be set to a #GFile containing the
   * old path, and @other_file will be set to a #GFile containing the new path.
   *
   * In all the other cases, @other_file will be set to #NULL.
   **/
  g_file_monitor_changed_signal = g_signal_new (I_("changed"),
                                                G_TYPE_FILE_MONITOR,
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (GFileMonitorClass, changed),
                                                NULL, NULL,
                                                NULL,
                                                G_TYPE_NONE, 3,
                                                G_TYPE_FILE, G_TYPE_FILE, G_TYPE_FILE_MONITOR_EVENT);

  g_object_class_install_property (object_class, PROP_RATE_LIMIT,
                                   g_param_spec_int ("rate-limit",
                                                     P_("Rate limit"),
                                                     P_("The limit of the monitor to watch for changes, in milliseconds"),
                                                     0, G_MAXINT, DEFAULT_RATE_LIMIT_MSECS, G_PARAM_READWRITE |
                                                     G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_CANCELLED,
                                   g_param_spec_boolean ("cancelled",
                                                         P_("Cancelled"),
                                                         P_("Whether the monitor has been cancelled"),
                                                         FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

/**
 * g_file_monitor_is_cancelled:
 * @monitor: a #GFileMonitor
 * 
 * Returns whether the monitor is canceled.
 *
 * Returns: %TRUE if monitor is canceled. %FALSE otherwise.
 **/
gboolean
g_file_monitor_is_cancelled (GFileMonitor *monitor)
{
  gboolean res;

  g_return_val_if_fail (G_IS_FILE_MONITOR (monitor), FALSE);

  res = monitor->priv->cancelled;

  return res;
}

/**
 * g_file_monitor_cancel:
 * @monitor: a #GFileMonitor.
 *
 * Cancels a file monitor.
 *
 * Returns: always %TRUE
 **/
gboolean
g_file_monitor_cancel (GFileMonitor *monitor)
{
  g_return_val_if_fail (G_IS_FILE_MONITOR (monitor), FALSE);

  if (!monitor->priv->cancelled)
    {
      G_FILE_MONITOR_GET_CLASS (monitor)->cancel (monitor);

      monitor->priv->cancelled = TRUE;
      g_object_notify (G_OBJECT (monitor), "cancelled");
    }

  return TRUE;
}

/**
 * g_file_monitor_set_rate_limit:
 * @monitor: a #GFileMonitor.
 * @limit_msecs: a non-negative integer with the limit in milliseconds
 *     to poll for changes
 *
 * Sets the rate limit to which the @monitor will report
 * consecutive change events to the same file.
 */
void
g_file_monitor_set_rate_limit (GFileMonitor *monitor,
                               gint          limit_msecs)
{
  g_object_set (monitor, "rate-limit", limit_msecs, NULL);
}

/**
 * g_file_monitor_emit_event:
 * @monitor: a #GFileMonitor.
 * @child: a #GFile.
 * @other_file: a #GFile.
 * @event_type: a set of #GFileMonitorEvent flags.
 *
 * Emits the #GFileMonitor::changed signal if a change
 * has taken place. Should be called from file monitor
 * implementations only.
 *
 * Implementations are responsible to call this method from the
 * [thread-default main context][g-main-context-push-thread-default] of the
 * thread that the monitor was created in.
 **/
void
g_file_monitor_emit_event (GFileMonitor      *monitor,
                           GFile             *child,
                           GFile             *other_file,
                           GFileMonitorEvent  event_type)
{
  g_return_if_fail (G_IS_FILE_MONITOR (monitor));
  g_return_if_fail (G_IS_FILE (child));
  g_return_if_fail (!other_file || G_IS_FILE (other_file));

  if (monitor->priv->cancelled)
    return;

  g_signal_emit (monitor, g_file_monitor_changed_signal, 0, child, other_file, event_type);
}
