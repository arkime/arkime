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

#include "gioscheduler.h"
#include "gcancellable.h"
#include "gtask.h"

/**
 * SECTION:gioscheduler
 * @short_description: I/O Scheduler
 * @include: gio/gio.h
 * 
 * As of GLib 2.36, #GIOScheduler is deprecated in favor of
 * #GThreadPool and #GTask.
 *
 * Schedules asynchronous I/O operations. #GIOScheduler integrates 
 * into the main event loop (#GMainLoop) and uses threads.
 */

struct _GIOSchedulerJob {
  GList *active_link;
  GTask *task;

  GIOSchedulerJobFunc job_func;
  gpointer data;
  GDestroyNotify destroy_notify;

  GCancellable *cancellable;
  gulong cancellable_id;
  GMainContext *context;
};

G_LOCK_DEFINE_STATIC(active_jobs);
static GList *active_jobs = NULL;

static void
g_io_job_free (GIOSchedulerJob *job)
{
  if (job->destroy_notify)
    job->destroy_notify (job->data);

  G_LOCK (active_jobs);
  active_jobs = g_list_delete_link (active_jobs, job->active_link);
  G_UNLOCK (active_jobs);

  if (job->cancellable)
    g_object_unref (job->cancellable);
  g_main_context_unref (job->context);
  g_slice_free (GIOSchedulerJob, job);
}

static void
io_job_thread (GTask         *task,
               gpointer       source_object,
               gpointer       task_data,
               GCancellable  *cancellable)
{
  GIOSchedulerJob *job = task_data;
  gboolean result;

  if (job->cancellable)
    g_cancellable_push_current (job->cancellable);

  do 
    {
      result = job->job_func (job, job->cancellable, job->data);
    }
  while (result);

  if (job->cancellable)
    g_cancellable_pop_current (job->cancellable);
}

/**
 * g_io_scheduler_push_job:
 * @job_func: a #GIOSchedulerJobFunc.
 * @user_data: data to pass to @job_func
 * @notify: (nullable): a #GDestroyNotify for @user_data, or %NULL
 * @io_priority: the [I/O priority][io-priority]
 * of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 *
 * Schedules the I/O job to run in another thread.
 *
 * @notify will be called on @user_data after @job_func has returned,
 * regardless whether the job was cancelled or has run to completion.
 * 
 * If @cancellable is not %NULL, it can be used to cancel the I/O job
 * by calling g_cancellable_cancel() or by calling 
 * g_io_scheduler_cancel_all_jobs().
 *
 * Deprecated: use #GThreadPool or g_task_run_in_thread()
 **/
void
g_io_scheduler_push_job (GIOSchedulerJobFunc  job_func,
			 gpointer             user_data,
			 GDestroyNotify       notify,
			 gint                 io_priority,
			 GCancellable        *cancellable)
{
  GIOSchedulerJob *job;
  GTask *task;

  g_return_if_fail (job_func != NULL);

  job = g_slice_new0 (GIOSchedulerJob);
  job->job_func = job_func;
  job->data = user_data;
  job->destroy_notify = notify;

  if (cancellable)
    job->cancellable = g_object_ref (cancellable);

  job->context = g_main_context_ref_thread_default ();

  G_LOCK (active_jobs);
  active_jobs = g_list_prepend (active_jobs, job);
  job->active_link = active_jobs;
  G_UNLOCK (active_jobs);

  task = g_task_new (NULL, cancellable, NULL, NULL);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_task_set_source_tag (task, g_io_scheduler_push_job);
G_GNUC_END_IGNORE_DEPRECATIONS
  g_task_set_task_data (task, job, (GDestroyNotify)g_io_job_free);
  g_task_set_priority (task, io_priority);
  g_task_run_in_thread (task, io_job_thread);
  g_object_unref (task);
}

/**
 * g_io_scheduler_cancel_all_jobs:
 * 
 * Cancels all cancellable I/O jobs. 
 *
 * A job is cancellable if a #GCancellable was passed into
 * g_io_scheduler_push_job().
 *
 * Deprecated: You should never call this function, since you don't
 * know how other libraries in your program might be making use of
 * gioscheduler.
 **/
void
g_io_scheduler_cancel_all_jobs (void)
{
  GList *cancellable_list, *l;
  
  G_LOCK (active_jobs);
  cancellable_list = NULL;
  for (l = active_jobs; l != NULL; l = l->next)
    {
      GIOSchedulerJob *job = l->data;
      if (job->cancellable)
	cancellable_list = g_list_prepend (cancellable_list,
					   g_object_ref (job->cancellable));
    }
  G_UNLOCK (active_jobs);

  for (l = cancellable_list; l != NULL; l = l->next)
    {
      GCancellable *c = l->data;
      g_cancellable_cancel (c);
      g_object_unref (c);
    }
  g_list_free (cancellable_list);
}

typedef struct {
  GSourceFunc func;
  gboolean ret_val;
  gpointer data;
  GDestroyNotify notify;

  GMutex ack_lock;
  GCond ack_condition;
  gboolean ack;
} MainLoopProxy;

static gboolean
mainloop_proxy_func (gpointer data)
{
  MainLoopProxy *proxy = data;

  proxy->ret_val = proxy->func (proxy->data);

  if (proxy->notify)
    proxy->notify (proxy->data);

  g_mutex_lock (&proxy->ack_lock);
  proxy->ack = TRUE;
  g_cond_signal (&proxy->ack_condition);
  g_mutex_unlock (&proxy->ack_lock);

  return FALSE;
}

static void
mainloop_proxy_free (MainLoopProxy *proxy)
{
  g_mutex_clear (&proxy->ack_lock);
  g_cond_clear (&proxy->ack_condition);
  g_free (proxy);
}

/**
 * g_io_scheduler_job_send_to_mainloop:
 * @job: a #GIOSchedulerJob
 * @func: a #GSourceFunc callback that will be called in the original thread
 * @user_data: data to pass to @func
 * @notify: (nullable): a #GDestroyNotify for @user_data, or %NULL
 * 
 * Used from an I/O job to send a callback to be run in the thread
 * that the job was started from, waiting for the result (and thus
 * blocking the I/O job).
 *
 * Returns: The return value of @func
 *
 * Deprecated: Use g_main_context_invoke().
 **/
gboolean
g_io_scheduler_job_send_to_mainloop (GIOSchedulerJob *job,
				     GSourceFunc      func,
				     gpointer         user_data,
				     GDestroyNotify   notify)
{
  GSource *source;
  MainLoopProxy *proxy;
  gboolean ret_val;

  g_return_val_if_fail (job != NULL, FALSE);
  g_return_val_if_fail (func != NULL, FALSE);

  proxy = g_new0 (MainLoopProxy, 1);
  proxy->func = func;
  proxy->data = user_data;
  proxy->notify = notify;
  g_mutex_init (&proxy->ack_lock);
  g_cond_init (&proxy->ack_condition);
  g_mutex_lock (&proxy->ack_lock);

  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_callback (source, mainloop_proxy_func, proxy,
			 NULL);
  g_source_set_name (source, "[gio] mainloop_proxy_func");

  g_source_attach (source, job->context);
  g_source_unref (source);

  while (!proxy->ack)
    g_cond_wait (&proxy->ack_condition, &proxy->ack_lock);
  g_mutex_unlock (&proxy->ack_lock);

  ret_val = proxy->ret_val;
  mainloop_proxy_free (proxy);
  
  return ret_val;
}

/**
 * g_io_scheduler_job_send_to_mainloop_async:
 * @job: a #GIOSchedulerJob
 * @func: a #GSourceFunc callback that will be called in the original thread
 * @user_data: data to pass to @func
 * @notify: (nullable): a #GDestroyNotify for @user_data, or %NULL
 * 
 * Used from an I/O job to send a callback to be run asynchronously in
 * the thread that the job was started from. The callback will be run
 * when the main loop is available, but at that time the I/O job might
 * have finished. The return value from the callback is ignored.
 *
 * Note that if you are passing the @user_data from g_io_scheduler_push_job()
 * on to this function you have to ensure that it is not freed before
 * @func is called, either by passing %NULL as @notify to 
 * g_io_scheduler_push_job() or by using refcounting for @user_data.
 *
 * Deprecated: Use g_main_context_invoke().
 **/
void
g_io_scheduler_job_send_to_mainloop_async (GIOSchedulerJob *job,
					   GSourceFunc      func,
					   gpointer         user_data,
					   GDestroyNotify   notify)
{
  GSource *source;
  MainLoopProxy *proxy;

  g_return_if_fail (job != NULL);
  g_return_if_fail (func != NULL);

  proxy = g_new0 (MainLoopProxy, 1);
  proxy->func = func;
  proxy->data = user_data;
  proxy->notify = notify;
  g_mutex_init (&proxy->ack_lock);
  g_cond_init (&proxy->ack_condition);

  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_callback (source, mainloop_proxy_func, proxy,
			 (GDestroyNotify)mainloop_proxy_free);
  g_source_set_name (source, "[gio] mainloop_proxy_func");

  g_source_attach (source, job->context);
  g_source_unref (source);
}
