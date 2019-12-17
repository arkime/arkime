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
#include "gfileenumerator.h"
#include "gfile.h"
#include "gioscheduler.h"
#include "gasyncresult.h"
#include "gasynchelper.h"
#include "gioerror.h"
#include "glibintl.h"

struct _GFileEnumeratorPrivate {
  /* TODO: Should be public for subclasses? */
  GFile *container;
  guint closed : 1;
  guint pending : 1;
  GAsyncReadyCallback outstanding_callback;
  GError *outstanding_error;
};

/**
 * SECTION:gfileenumerator
 * @short_description: Enumerated Files Routines
 * @include: gio/gio.h
 * 
 * #GFileEnumerator allows you to operate on a set of #GFiles, 
 * returning a #GFileInfo structure for each file enumerated (e.g. 
 * g_file_enumerate_children() will return a #GFileEnumerator for each 
 * of the children within a directory).
 *
 * To get the next file's information from a #GFileEnumerator, use 
 * g_file_enumerator_next_file() or its asynchronous version, 
 * g_file_enumerator_next_files_async(). Note that the asynchronous 
 * version will return a list of #GFileInfos, whereas the 
 * synchronous will only return the next file in the enumerator.
 *
 * The ordering of returned files is unspecified for non-Unix
 * platforms; for more information, see g_dir_read_name().  On Unix,
 * when operating on local files, returned files will be sorted by
 * inode number.  Effectively you can assume that the ordering of
 * returned files will be stable between successive calls (and
 * applications) assuming the directory is unchanged.
 *
 * If your application needs a specific ordering, such as by name or
 * modification time, you will have to implement that in your
 * application code.
 *
 * To close a #GFileEnumerator, use g_file_enumerator_close(), or 
 * its asynchronous version, g_file_enumerator_close_async(). Once 
 * a #GFileEnumerator is closed, no further actions may be performed 
 * on it, and it should be freed with g_object_unref().
 * 
 **/ 

G_DEFINE_TYPE_WITH_PRIVATE (GFileEnumerator, g_file_enumerator, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONTAINER
};

static void     g_file_enumerator_real_next_files_async  (GFileEnumerator      *enumerator,
							  int                   num_files,
							  int                   io_priority,
							  GCancellable         *cancellable,
							  GAsyncReadyCallback   callback,
							  gpointer              user_data);
static GList *  g_file_enumerator_real_next_files_finish (GFileEnumerator      *enumerator,
							  GAsyncResult         *res,
							  GError              **error);
static void     g_file_enumerator_real_close_async       (GFileEnumerator      *enumerator,
							  int                   io_priority,
							  GCancellable         *cancellable,
							  GAsyncReadyCallback   callback,
							  gpointer              user_data);
static gboolean g_file_enumerator_real_close_finish      (GFileEnumerator      *enumerator,
							  GAsyncResult         *res,
							  GError              **error);

static void
g_file_enumerator_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GFileEnumerator *enumerator;
  
  enumerator = G_FILE_ENUMERATOR (object);
  
  switch (property_id) {
  case PROP_CONTAINER:
    enumerator->priv->container = g_value_dup_object (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
g_file_enumerator_dispose (GObject *object)
{
  GFileEnumerator *enumerator;

  enumerator = G_FILE_ENUMERATOR (object);
  
  if (enumerator->priv->container) {
    g_object_unref (enumerator->priv->container);
    enumerator->priv->container = NULL;
  }

  G_OBJECT_CLASS (g_file_enumerator_parent_class)->dispose (object);
}

static void
g_file_enumerator_finalize (GObject *object)
{
  GFileEnumerator *enumerator;

  enumerator = G_FILE_ENUMERATOR (object);
  
  if (!enumerator->priv->closed)
    g_file_enumerator_close (enumerator, NULL, NULL);

  G_OBJECT_CLASS (g_file_enumerator_parent_class)->finalize (object);
}

static void
g_file_enumerator_class_init (GFileEnumeratorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = g_file_enumerator_set_property;
  gobject_class->dispose = g_file_enumerator_dispose;
  gobject_class->finalize = g_file_enumerator_finalize;

  klass->next_files_async = g_file_enumerator_real_next_files_async;
  klass->next_files_finish = g_file_enumerator_real_next_files_finish;
  klass->close_async = g_file_enumerator_real_close_async;
  klass->close_finish = g_file_enumerator_real_close_finish;

  g_object_class_install_property
    (gobject_class, PROP_CONTAINER,
     g_param_spec_object ("container", P_("Container"),
                          P_("The container that is being enumerated"),
                          G_TYPE_FILE,
                          G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
}

static void
g_file_enumerator_init (GFileEnumerator *enumerator)
{
  enumerator->priv = g_file_enumerator_get_instance_private (enumerator);
}

/**
 * g_file_enumerator_next_file:
 * @enumerator: a #GFileEnumerator.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Returns information for the next file in the enumerated object.
 * Will block until the information is available. The #GFileInfo 
 * returned from this function will contain attributes that match the 
 * attribute string that was passed when the #GFileEnumerator was created.
 *
 * See the documentation of #GFileEnumerator for information about the
 * order of returned files.
 *
 * On error, returns %NULL and sets @error to the error. If the
 * enumerator is at the end, %NULL will be returned and @error will
 * be unset.
 *
 * Returns: (nullable) (transfer full): A #GFileInfo or %NULL on error
 *    or end of enumerator.  Free the returned object with
 *    g_object_unref() when no longer needed.
 **/
GFileInfo *
g_file_enumerator_next_file (GFileEnumerator *enumerator,
			     GCancellable *cancellable,
			     GError **error)
{
  GFileEnumeratorClass *class;
  GFileInfo *info;
  
  g_return_val_if_fail (G_IS_FILE_ENUMERATOR (enumerator), NULL);
  g_return_val_if_fail (enumerator != NULL, NULL);
  
  if (enumerator->priv->closed)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CLOSED,
                           _("Enumerator is closed"));
      return NULL;
    }

  if (enumerator->priv->pending)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PENDING,
                           _("File enumerator has outstanding operation"));
      return NULL;
    }

  if (enumerator->priv->outstanding_error)
    {
      g_propagate_error (error, enumerator->priv->outstanding_error);
      enumerator->priv->outstanding_error = NULL;
      return NULL;
    }
  
  class = G_FILE_ENUMERATOR_GET_CLASS (enumerator);

  if (cancellable)
    g_cancellable_push_current (cancellable);
  
  enumerator->priv->pending = TRUE;
  info = (* class->next_file) (enumerator, cancellable, error);
  enumerator->priv->pending = FALSE;

  if (cancellable)
    g_cancellable_pop_current (cancellable);
  
  return info;
}
  
/**
 * g_file_enumerator_close:
 * @enumerator: a #GFileEnumerator.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Releases all resources used by this enumerator, making the
 * enumerator return %G_IO_ERROR_CLOSED on all calls.
 *
 * This will be automatically called when the last reference
 * is dropped, but you might want to call this function to make 
 * sure resources are released as early as possible.
 *
 * Returns: #TRUE on success or #FALSE on error.
 **/
gboolean
g_file_enumerator_close (GFileEnumerator  *enumerator,
			 GCancellable     *cancellable,
			 GError          **error)
{
  GFileEnumeratorClass *class;

  g_return_val_if_fail (G_IS_FILE_ENUMERATOR (enumerator), FALSE);
  g_return_val_if_fail (enumerator != NULL, FALSE);
  
  class = G_FILE_ENUMERATOR_GET_CLASS (enumerator);

  if (enumerator->priv->closed)
    return TRUE;
  
  if (enumerator->priv->pending)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PENDING,
                           _("File enumerator has outstanding operation"));
      return FALSE;
    }

  if (cancellable)
    g_cancellable_push_current (cancellable);
  
  enumerator->priv->pending = TRUE;
  (* class->close_fn) (enumerator, cancellable, error);
  enumerator->priv->pending = FALSE;
  enumerator->priv->closed = TRUE;

  if (cancellable)
    g_cancellable_pop_current (cancellable);
  
  return TRUE;
}

static void
next_async_callback_wrapper (GObject      *source_object,
			     GAsyncResult *res,
			     gpointer      user_data)
{
  GFileEnumerator *enumerator = G_FILE_ENUMERATOR (source_object);

  enumerator->priv->pending = FALSE;
  if (enumerator->priv->outstanding_callback)
    (*enumerator->priv->outstanding_callback) (source_object, res, user_data);
  g_object_unref (enumerator);
}

/**
 * g_file_enumerator_next_files_async:
 * @enumerator: a #GFileEnumerator.
 * @num_files: the number of file info objects to request
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Request information for a number of files from the enumerator asynchronously.
 * When all i/o for the operation is finished the @callback will be called with
 * the requested information. 
 *
 * See the documentation of #GFileEnumerator for information about the
 * order of returned files.
 *
 * The callback can be called with less than @num_files files in case of error
 * or at the end of the enumerator. In case of a partial error the callback will
 * be called with any succeeding items and no error, and on the next request the
 * error will be reported. If a request is cancelled the callback will be called
 * with %G_IO_ERROR_CANCELLED.
 *
 * During an async request no other sync and async calls are allowed, and will
 * result in %G_IO_ERROR_PENDING errors. 
 *
 * Any outstanding i/o request with higher priority (lower numerical value) will
 * be executed before an outstanding request with lower priority. Default
 * priority is %G_PRIORITY_DEFAULT.
 **/
void
g_file_enumerator_next_files_async (GFileEnumerator     *enumerator,
				    int                  num_files,
				    int                  io_priority,
				    GCancellable        *cancellable,
				    GAsyncReadyCallback  callback,
				    gpointer             user_data)
{
  GFileEnumeratorClass *class;

  g_return_if_fail (G_IS_FILE_ENUMERATOR (enumerator));
  g_return_if_fail (enumerator != NULL);
  g_return_if_fail (num_files >= 0);

  if (num_files == 0)
    {
      GTask *task;

      task = g_task_new (enumerator, cancellable, callback, user_data);
      g_task_set_source_tag (task, g_file_enumerator_next_files_async);
      g_task_return_pointer (task, NULL, NULL);
      g_object_unref (task);
      return;
    }
  
  if (enumerator->priv->closed)
    {
      g_task_report_new_error (enumerator, callback, user_data,
                               g_file_enumerator_next_files_async,
                               G_IO_ERROR, G_IO_ERROR_CLOSED,
                               _("File enumerator is already closed"));
      return;
    }
  
  if (enumerator->priv->pending)
    {
      g_task_report_new_error (enumerator, callback, user_data,
                               g_file_enumerator_next_files_async,
                               G_IO_ERROR, G_IO_ERROR_PENDING,
                               _("File enumerator has outstanding operation"));
      return;
    }

  class = G_FILE_ENUMERATOR_GET_CLASS (enumerator);
  
  enumerator->priv->pending = TRUE;
  enumerator->priv->outstanding_callback = callback;
  g_object_ref (enumerator);
  (* class->next_files_async) (enumerator, num_files, io_priority, cancellable, 
			       next_async_callback_wrapper, user_data);
}

/**
 * g_file_enumerator_next_files_finish:
 * @enumerator: a #GFileEnumerator.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 * 
 * Finishes the asynchronous operation started with g_file_enumerator_next_files_async().
 * 
 * Returns: (transfer full) (element-type Gio.FileInfo): a #GList of #GFileInfos. You must free the list with 
 *     g_list_free() and unref the infos with g_object_unref() when you're 
 *     done with them.
 **/
GList *
g_file_enumerator_next_files_finish (GFileEnumerator  *enumerator,
				     GAsyncResult     *result,
				     GError          **error)
{
  GFileEnumeratorClass *class;
  
  g_return_val_if_fail (G_IS_FILE_ENUMERATOR (enumerator), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
  
  if (g_async_result_legacy_propagate_error (result, error))
    return NULL;
  else if (g_async_result_is_tagged (result, g_file_enumerator_next_files_async))
    return g_task_propagate_pointer (G_TASK (result), error);
  
  class = G_FILE_ENUMERATOR_GET_CLASS (enumerator);
  return class->next_files_finish (enumerator, result, error);
}

static void
close_async_callback_wrapper (GObject      *source_object,
			      GAsyncResult *res,
			      gpointer      user_data)
{
  GFileEnumerator *enumerator = G_FILE_ENUMERATOR (source_object);
  
  enumerator->priv->pending = FALSE;
  enumerator->priv->closed = TRUE;
  if (enumerator->priv->outstanding_callback)
    (*enumerator->priv->outstanding_callback) (source_object, res, user_data);
  g_object_unref (enumerator);
}

/**
 * g_file_enumerator_close_async:
 * @enumerator: a #GFileEnumerator.
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously closes the file enumerator. 
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned in 
 * g_file_enumerator_close_finish(). 
 **/
void
g_file_enumerator_close_async (GFileEnumerator     *enumerator,
			       int                  io_priority,
			       GCancellable        *cancellable,
			       GAsyncReadyCallback  callback,
			       gpointer             user_data)
{
  GFileEnumeratorClass *class;

  g_return_if_fail (G_IS_FILE_ENUMERATOR (enumerator));

  if (enumerator->priv->closed)
    {
      g_task_report_new_error (enumerator, callback, user_data,
                               g_file_enumerator_close_async,
                               G_IO_ERROR, G_IO_ERROR_CLOSED,
                               _("File enumerator is already closed"));
      return;
    }
  
  if (enumerator->priv->pending)
    {
      g_task_report_new_error (enumerator, callback, user_data,
                               g_file_enumerator_close_async,
                               G_IO_ERROR, G_IO_ERROR_PENDING,
                               _("File enumerator has outstanding operation"));
      return;
    }

  class = G_FILE_ENUMERATOR_GET_CLASS (enumerator);
  
  enumerator->priv->pending = TRUE;
  enumerator->priv->outstanding_callback = callback;
  g_object_ref (enumerator);
  (* class->close_async) (enumerator, io_priority, cancellable,
			  close_async_callback_wrapper, user_data);
}

/**
 * g_file_enumerator_close_finish:
 * @enumerator: a #GFileEnumerator.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 * 
 * Finishes closing a file enumerator, started from g_file_enumerator_close_async().
 * 
 * If the file enumerator was already closed when g_file_enumerator_close_async() 
 * was called, then this function will report %G_IO_ERROR_CLOSED in @error, and 
 * return %FALSE. If the file enumerator had pending operation when the close 
 * operation was started, then this function will report %G_IO_ERROR_PENDING, and
 * return %FALSE.  If @cancellable was not %NULL, then the operation may have been 
 * cancelled by triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be set, and %FALSE will be 
 * returned. 
 * 
 * Returns: %TRUE if the close operation has finished successfully.
 **/
gboolean
g_file_enumerator_close_finish (GFileEnumerator  *enumerator,
				GAsyncResult     *result,
				GError          **error)
{
  GFileEnumeratorClass *class;

  g_return_val_if_fail (G_IS_FILE_ENUMERATOR (enumerator), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
  
  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_file_enumerator_close_async))
    return g_task_propagate_boolean (G_TASK (result), error);

  class = G_FILE_ENUMERATOR_GET_CLASS (enumerator);
  return class->close_finish (enumerator, result, error);
}

/**
 * g_file_enumerator_is_closed:
 * @enumerator: a #GFileEnumerator.
 *
 * Checks if the file enumerator has been closed.
 * 
 * Returns: %TRUE if the @enumerator is closed.
 **/
gboolean
g_file_enumerator_is_closed (GFileEnumerator *enumerator)
{
  g_return_val_if_fail (G_IS_FILE_ENUMERATOR (enumerator), TRUE);
  
  return enumerator->priv->closed;
}

/**
 * g_file_enumerator_has_pending:
 * @enumerator: a #GFileEnumerator.
 * 
 * Checks if the file enumerator has pending operations.
 *
 * Returns: %TRUE if the @enumerator has pending operations.
 **/
gboolean
g_file_enumerator_has_pending (GFileEnumerator *enumerator)
{
  g_return_val_if_fail (G_IS_FILE_ENUMERATOR (enumerator), TRUE);
  
  return enumerator->priv->pending;
}

/**
 * g_file_enumerator_set_pending:
 * @enumerator: a #GFileEnumerator.
 * @pending: a boolean value.
 * 
 * Sets the file enumerator as having pending operations.
 **/
void
g_file_enumerator_set_pending (GFileEnumerator *enumerator,
			       gboolean         pending)
{
  g_return_if_fail (G_IS_FILE_ENUMERATOR (enumerator));
  
  enumerator->priv->pending = pending;
}

/**
 * g_file_enumerator_iterate:
 * @direnum: an open #GFileEnumerator
 * @out_info: (out) (transfer none) (optional): Output location for the next #GFileInfo, or %NULL
 * @out_child: (out) (transfer none) (optional): Output location for the next #GFile, or %NULL
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * This is a version of g_file_enumerator_next_file() that's easier to
 * use correctly from C programs.  With g_file_enumerator_next_file(),
 * the gboolean return value signifies "end of iteration or error", which
 * requires allocation of a temporary #GError.
 *
 * In contrast, with this function, a %FALSE return from
 * g_file_enumerator_iterate() *always* means
 * "error".  End of iteration is signaled by @out_info or @out_child being %NULL.
 *
 * Another crucial difference is that the references for @out_info and
 * @out_child are owned by @direnum (they are cached as hidden
 * properties).  You must not unref them in your own code.  This makes
 * memory management significantly easier for C code in combination
 * with loops.
 *
 * Finally, this function optionally allows retrieving a #GFile as
 * well.
 *
 * You must specify at least one of @out_info or @out_child.
 *
 * The code pattern for correctly using g_file_enumerator_iterate() from C
 * is:
 *
 * |[
 * direnum = g_file_enumerate_children (file, ...);
 * while (TRUE)
 *   {
 *     GFileInfo *info;
 *     if (!g_file_enumerator_iterate (direnum, &info, NULL, cancellable, error))
 *       goto out;
 *     if (!info)
 *       break;
 *     ... do stuff with "info"; do not unref it! ...
 *   }
 * 
 * out:
 *   g_object_unref (direnum); // Note: frees the last @info
 * ]|
 *
 *
 * Since: 2.44
 */
gboolean
g_file_enumerator_iterate (GFileEnumerator  *direnum,
                           GFileInfo       **out_info,
                           GFile           **out_child,
                           GCancellable     *cancellable,
                           GError          **error)
{
  gboolean ret = FALSE;
  GError *temp_error = NULL;
  GFileInfo *ret_info = NULL;

  static GQuark cached_info_quark;
  static GQuark cached_child_quark;
  static gsize quarks_initialized;

  g_return_val_if_fail (direnum != NULL, FALSE);
  g_return_val_if_fail (out_info != NULL || out_child != NULL, FALSE);

  if (g_once_init_enter (&quarks_initialized))
    {
      cached_info_quark = g_quark_from_static_string ("g-cached-info");
      cached_child_quark = g_quark_from_static_string ("g-cached-child");
      g_once_init_leave (&quarks_initialized, 1);
    }

  ret_info = g_file_enumerator_next_file (direnum, cancellable, &temp_error);
  if (temp_error != NULL)
    {
      g_propagate_error (error, temp_error);
      goto out;
    }

  if (ret_info)
    { 
      if (out_child != NULL)
        {
          const char *name = g_file_info_get_name (ret_info);

          if (G_UNLIKELY (name == NULL))
            g_warning ("g_file_enumerator_iterate() created without standard::name");
          else
            {
              *out_child = g_file_get_child (g_file_enumerator_get_container (direnum), name);
              g_object_set_qdata_full ((GObject*)direnum, cached_child_quark, *out_child, (GDestroyNotify)g_object_unref);
            }
        }
      if (out_info != NULL)
        {
          g_object_set_qdata_full ((GObject*)direnum, cached_info_quark, ret_info, (GDestroyNotify)g_object_unref);
          *out_info = ret_info;
        }
      else
        g_object_unref (ret_info);
    }
  else
    {
      if (out_info)
        *out_info = NULL;
      if (out_child)
        *out_child = NULL;
    }

  ret = TRUE;
 out:
  return ret;
}

/**
 * g_file_enumerator_get_container:
 * @enumerator: a #GFileEnumerator
 *
 * Get the #GFile container which is being enumerated.
 *
 * Returns: (transfer none): the #GFile which is being enumerated.
 *
 * Since: 2.18
 */
GFile *
g_file_enumerator_get_container (GFileEnumerator *enumerator)
{
  g_return_val_if_fail (G_IS_FILE_ENUMERATOR (enumerator), NULL);

  return enumerator->priv->container;
}

/**
 * g_file_enumerator_get_child:
 * @enumerator: a #GFileEnumerator
 * @info: a #GFileInfo gotten from g_file_enumerator_next_file()
 *   or the async equivalents.
 *
 * Return a new #GFile which refers to the file named by @info in the source
 * directory of @enumerator.  This function is primarily intended to be used
 * inside loops with g_file_enumerator_next_file().
 *
 * This is a convenience method that's equivalent to:
 * |[<!-- language="C" -->
 *   gchar *name = g_file_info_get_name (info);
 *   GFile *child = g_file_get_child (g_file_enumerator_get_container (enumr),
 *                                    name);
 * ]|
 *
 * Returns: (transfer full): a #GFile for the #GFileInfo passed it.
 *
 * Since: 2.36
 */
GFile *
g_file_enumerator_get_child (GFileEnumerator *enumerator,
                             GFileInfo       *info)
{
  g_return_val_if_fail (G_IS_FILE_ENUMERATOR (enumerator), NULL);

  return g_file_get_child (enumerator->priv->container,
                           g_file_info_get_name (info));
}

static void
next_async_op_free (GList *files)
{
  g_list_free_full (files, g_object_unref);
}

static void
next_files_thread (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  GFileEnumerator *enumerator = source_object;
  int num_files = GPOINTER_TO_INT (task_data);
  GFileEnumeratorClass *class;
  GList *files = NULL;
  GError *error = NULL;
  GFileInfo *info;
  int i;

  class = G_FILE_ENUMERATOR_GET_CLASS (enumerator);

  for (i = 0; i < num_files; i++)
    {
      if (g_cancellable_set_error_if_cancelled (cancellable, &error))
	info = NULL;
      else
	info = class->next_file (enumerator, cancellable, &error);
      
      if (info == NULL)
	{
	  /* If we get an error after first file, return that on next operation */
	  if (error != NULL && i > 0)
	    {
	      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		g_error_free (error); /* Never propagate cancel errors to other call */
	      else
		enumerator->priv->outstanding_error = error;
	      error = NULL;
	    }
	      
	  break;
	}
      else
	files = g_list_prepend (files, info);
    }

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, files, (GDestroyNotify)next_async_op_free);
}

static void
g_file_enumerator_real_next_files_async (GFileEnumerator     *enumerator,
					 int                  num_files,
					 int                  io_priority,
					 GCancellable        *cancellable,
					 GAsyncReadyCallback  callback,
					 gpointer             user_data)
{
  GTask *task;

  task = g_task_new (enumerator, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_file_enumerator_real_next_files_async);
  g_task_set_task_data (task, GINT_TO_POINTER (num_files), NULL);
  g_task_set_priority (task, io_priority);

  g_task_run_in_thread (task, next_files_thread);
  g_object_unref (task);
}

static GList *
g_file_enumerator_real_next_files_finish (GFileEnumerator                *enumerator,
					  GAsyncResult                   *result,
					  GError                        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, enumerator), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
close_async_thread (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  GFileEnumerator *enumerator = source_object;
  GFileEnumeratorClass *class;
  GError *error = NULL;
  gboolean result;

  class = G_FILE_ENUMERATOR_GET_CLASS (enumerator);
  result = class->close_fn (enumerator, cancellable, &error);
  if (result)
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);
}

static void
g_file_enumerator_real_close_async (GFileEnumerator     *enumerator,
				    int                  io_priority,
				    GCancellable        *cancellable,
				    GAsyncReadyCallback  callback,
				    gpointer             user_data)
{
  GTask *task;

  task = g_task_new (enumerator, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_file_enumerator_real_close_async);
  g_task_set_priority (task, io_priority);
  
  g_task_run_in_thread (task, close_async_thread);
  g_object_unref (task);
}

static gboolean
g_file_enumerator_real_close_finish (GFileEnumerator  *enumerator,
                                     GAsyncResult     *result,
                                     GError          **error)
{
  g_return_val_if_fail (g_task_is_valid (result, enumerator), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

