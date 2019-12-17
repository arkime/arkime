/*******************************************************************************
  Copyright (c) 2011, 2012 Dmitry Matveev <me@dmitrymatveev.co.uk>

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

#ifndef __G_KQUEUE_FILE_MONITOR_H__
#define __G_KQUEUE_FILE_MONITOR_H__

#include <glib-object.h>
#include <string.h>
#include <gio/gfilemonitor.h>
#include <gio/glocalfilemonitor.h>
#include <gio/giomodule.h>

G_BEGIN_DECLS

#define G_TYPE_KQUEUE_FILE_MONITOR        (g_kqueue_file_monitor_get_type ())
#define G_KQUEUE_FILE_MONITOR(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_KQUEUE_FILE_MONITOR, GKqueueFileMonitor))
#define G_KQUEUE_FILE_MONITOR_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), G_TYPE_KQUEUE_FILE_MONITOR, GKqueueFileMonitorClass))
#define G_IS_KQUEUE_FILE_MONITOR(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_KQUEUE_FILE_MONITOR))
#define G_IS_KQUEUE_FILE_MONITOR_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_KQUEUE_FILE_MONITOR))

typedef struct _GKqueueFileMonitor      GKqueueFileMonitor;
typedef struct _GKqueueFileMonitorClass GKqueueFileMonitorClass;

struct _GKqueueFileMonitorClass {
  GLocalFileMonitorClass parent_class;
};

GType g_kqueue_file_monitor_get_type (void);

G_END_DECLS

#endif /* __G_KQUEUE_FILE_MONITOR_H__ */
