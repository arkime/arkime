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
#include "gicon.h"
#include "gloadableicon.h"
#include "gasyncresult.h"
#include "gtask.h"
#include "glibintl.h"


/**
 * SECTION:gloadableicon
 * @short_description: Loadable Icons
 * @include: gio/gio.h
 * @see_also: #GIcon, #GThemedIcon
 * 
 * Extends the #GIcon interface and adds the ability to 
 * load icons from streams.
 **/

static void          g_loadable_icon_real_load_async  (GLoadableIcon        *icon,
						       int                   size,
						       GCancellable         *cancellable,
						       GAsyncReadyCallback   callback,
						       gpointer              user_data);
static GInputStream *g_loadable_icon_real_load_finish (GLoadableIcon        *icon,
						       GAsyncResult         *res,
						       char                **type,
						       GError              **error);

typedef GLoadableIconIface GLoadableIconInterface;
G_DEFINE_INTERFACE(GLoadableIcon, g_loadable_icon, G_TYPE_ICON)

static void
g_loadable_icon_default_init (GLoadableIconIface *iface)
{
  iface->load_async = g_loadable_icon_real_load_async;
  iface->load_finish = g_loadable_icon_real_load_finish;
}

/**
 * g_loadable_icon_load:
 * @icon: a #GLoadableIcon.
 * @size: an integer.
 * @type: (out) (optional): a location to store the type of the loaded
 * icon, %NULL to ignore.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to
 * ignore.
 * @error: a #GError location to store the error occurring, or %NULL
 * to ignore.
 * 
 * Loads a loadable icon. For the asynchronous version of this function, 
 * see g_loadable_icon_load_async().
 * 
 * Returns: (transfer full): a #GInputStream to read the icon from.
 **/
GInputStream *
g_loadable_icon_load (GLoadableIcon  *icon,
		      int             size,
		      char          **type,
		      GCancellable   *cancellable,
		      GError        **error)
{
  GLoadableIconIface *iface;

  g_return_val_if_fail (G_IS_LOADABLE_ICON (icon), NULL);

  iface = G_LOADABLE_ICON_GET_IFACE (icon);

  return (* iface->load) (icon, size, type, cancellable, error);
}

/**
 * g_loadable_icon_load_async:
 * @icon: a #GLoadableIcon.
 * @size: an integer.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore. 
 * @callback: (scope async): a #GAsyncReadyCallback to call when the
 *            request is satisfied
 * @user_data: (closure): the data to pass to callback function
 * 
 * Loads an icon asynchronously. To finish this function, see 
 * g_loadable_icon_load_finish(). For the synchronous, blocking 
 * version of this function, see g_loadable_icon_load().
 **/
void
g_loadable_icon_load_async (GLoadableIcon       *icon,
                            int                  size,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GLoadableIconIface *iface;
  
  g_return_if_fail (G_IS_LOADABLE_ICON (icon));

  iface = G_LOADABLE_ICON_GET_IFACE (icon);

  (* iface->load_async) (icon, size, cancellable, callback, user_data);
}

/**
 * g_loadable_icon_load_finish:
 * @icon: a #GLoadableIcon.
 * @res: a #GAsyncResult.
 * @type: (out) (optional): a location to store the type of the loaded
 *        icon, %NULL to ignore.
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 * 
 * Finishes an asynchronous icon load started in g_loadable_icon_load_async().
 * 
 * Returns: (transfer full): a #GInputStream to read the icon from.
 **/
GInputStream *
g_loadable_icon_load_finish (GLoadableIcon  *icon,
			     GAsyncResult   *res,
			     char          **type,
			     GError        **error)
{
  GLoadableIconIface *iface;
  
  g_return_val_if_fail (G_IS_LOADABLE_ICON (icon), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (g_async_result_legacy_propagate_error (res, error))
    return NULL;

  iface = G_LOADABLE_ICON_GET_IFACE (icon);

  return (* iface->load_finish) (icon, res, type, error);
}

/********************************************
 *   Default implementation of async load   *
 ********************************************/

typedef struct {
  int size;
  char *type;
} LoadData;

static void
load_data_free (LoadData *data)
{
  g_free (data->type);
  g_free (data);
}

static void
load_async_thread (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  GLoadableIcon *icon = source_object;
  LoadData *data = task_data;
  GLoadableIconIface *iface;
  GInputStream *stream;
  GError *error = NULL;

  iface = G_LOADABLE_ICON_GET_IFACE (icon);
  stream = iface->load (icon, data->size, &data->type,
                        cancellable, &error);

  if (stream)
    g_task_return_pointer (task, stream, g_object_unref);
  else
    g_task_return_error (task, error);
}



static void
g_loadable_icon_real_load_async (GLoadableIcon       *icon,
				 int                  size,
				 GCancellable        *cancellable,
				 GAsyncReadyCallback  callback,
				 gpointer             user_data)
{
  GTask *task;
  LoadData *data;

  task = g_task_new (icon, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_loadable_icon_real_load_async);
  data = g_new0 (LoadData, 1);
  g_task_set_task_data (task, data, (GDestroyNotify) load_data_free);
  g_task_run_in_thread (task, load_async_thread);
  g_object_unref (task);
}

static GInputStream *
g_loadable_icon_real_load_finish (GLoadableIcon        *icon,
				  GAsyncResult         *res,
				  char                **type,
				  GError              **error)
{
  GTask *task;
  LoadData *data;
  GInputStream *stream;

  g_return_val_if_fail (g_task_is_valid (res, icon), NULL);

  task = G_TASK (res);
  data = g_task_get_task_data (task);

  stream = g_task_propagate_pointer (task, error);
  if (stream && type)
    {
      *type = data->type;
      data->type = NULL;
    }

  return stream;
}
