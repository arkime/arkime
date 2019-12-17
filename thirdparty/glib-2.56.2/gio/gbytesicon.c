/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2013 Canonical Limited
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

#include "gbytesicon.h"
#include "gbytes.h"
#include "gicon.h"
#include "glibintl.h"
#include "gloadableicon.h"
#include "gmemoryinputstream.h"
#include "gtask.h"
#include "gioerror.h"


/**
 * SECTION:gbytesicon
 * @short_description: An icon stored in memory as a GBytes
 * @include: gio/gio.h
 * @see_also: #GIcon, #GLoadableIcon, #GBytes
 *
 * #GBytesIcon specifies an image held in memory in a common format (usually
 * png) to be used as icon.
 *
 * Since: 2.38
 **/

typedef GObjectClass GBytesIconClass;

struct _GBytesIcon
{
  GObject parent_instance;

  GBytes *bytes;
};

enum
{
  PROP_0,
  PROP_BYTES
};

static void g_bytes_icon_icon_iface_init          (GIconIface          *iface);
static void g_bytes_icon_loadable_icon_iface_init (GLoadableIconIface  *iface);
G_DEFINE_TYPE_WITH_CODE (GBytesIcon, g_bytes_icon, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ICON, g_bytes_icon_icon_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LOADABLE_ICON, g_bytes_icon_loadable_icon_iface_init))

static void
g_bytes_icon_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GBytesIcon *icon = G_BYTES_ICON (object);

  switch (prop_id)
    {
      case PROP_BYTES:
        g_value_set_boxed (value, icon->bytes);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_bytes_icon_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GBytesIcon *icon = G_BYTES_ICON (object);

  switch (prop_id)
    {
      case PROP_BYTES:
        icon->bytes = g_value_dup_boxed (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_bytes_icon_finalize (GObject *object)
{
  GBytesIcon *icon;

  icon = G_BYTES_ICON (object);

  g_bytes_unref (icon->bytes);

  G_OBJECT_CLASS (g_bytes_icon_parent_class)->finalize (object);
}

static void
g_bytes_icon_class_init (GBytesIconClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = g_bytes_icon_get_property;
  gobject_class->set_property = g_bytes_icon_set_property;
  gobject_class->finalize = g_bytes_icon_finalize;

  /**
   * GBytesIcon:bytes:
   *
   * The bytes containing the icon.
   */
  g_object_class_install_property (gobject_class, PROP_BYTES,
                                   g_param_spec_boxed ("bytes",
                                                       P_("bytes"),
                                                       P_("The bytes containing the icon"),
                                                       G_TYPE_BYTES,
                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
g_bytes_icon_init (GBytesIcon *bytes)
{
}

/**
 * g_bytes_icon_new:
 * @bytes: a #GBytes.
 *
 * Creates a new icon for a bytes.
 *
 * Returns: (transfer full) (type GBytesIcon): a #GIcon for the given
 *   @bytes, or %NULL on error.
 *
 * Since: 2.38
 **/
GIcon *
g_bytes_icon_new (GBytes *bytes)
{
  g_return_val_if_fail (bytes != NULL, NULL);

  return g_object_new (G_TYPE_BYTES_ICON, "bytes", bytes, NULL);
}

/**
 * g_bytes_icon_get_bytes:
 * @icon: a #GIcon.
 *
 * Gets the #GBytes associated with the given @icon.
 *
 * Returns: (transfer none): a #GBytes, or %NULL.
 *
 * Since: 2.38
 **/
GBytes *
g_bytes_icon_get_bytes (GBytesIcon *icon)
{
  g_return_val_if_fail (G_IS_BYTES_ICON (icon), NULL);

  return icon->bytes;
}

static guint
g_bytes_icon_hash (GIcon *icon)
{
  GBytesIcon *bytes_icon = G_BYTES_ICON (icon);

  return g_bytes_hash (bytes_icon->bytes);
}

static gboolean
g_bytes_icon_equal (GIcon *icon1,
                    GIcon *icon2)
{
  GBytesIcon *bytes1 = G_BYTES_ICON (icon1);
  GBytesIcon *bytes2 = G_BYTES_ICON (icon2);

  return g_bytes_equal (bytes1->bytes, bytes2->bytes);
}

static GVariant *
g_bytes_icon_serialize (GIcon *icon)
{
  GBytesIcon *bytes_icon = G_BYTES_ICON (icon);

  return g_variant_new ("(sv)", "bytes",
                        g_variant_new_from_bytes (G_VARIANT_TYPE_BYTESTRING, bytes_icon->bytes, TRUE));
}

static void
g_bytes_icon_icon_iface_init (GIconIface *iface)
{
  iface->hash = g_bytes_icon_hash;
  iface->equal = g_bytes_icon_equal;
  iface->serialize = g_bytes_icon_serialize;
}

static GInputStream *
g_bytes_icon_load (GLoadableIcon  *icon,
                   int            size,
                   char          **type,
                   GCancellable   *cancellable,
                   GError        **error)
{
  GBytesIcon *bytes_icon = G_BYTES_ICON (icon);

  if (type)
    *type = NULL;

  return g_memory_input_stream_new_from_bytes (bytes_icon->bytes);
}

static void
g_bytes_icon_load_async (GLoadableIcon       *icon,
                         int                  size,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GBytesIcon *bytes_icon = G_BYTES_ICON (icon);
  GTask *task;

  task = g_task_new (icon, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_bytes_icon_load_async);
  g_task_return_pointer (task, g_memory_input_stream_new_from_bytes (bytes_icon->bytes), g_object_unref);
  g_object_unref (task);
}

static GInputStream *
g_bytes_icon_load_finish (GLoadableIcon  *icon,
                          GAsyncResult   *res,
                          char          **type,
                          GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (res, icon), NULL);

  if (type)
    *type = NULL;

  return g_task_propagate_pointer (G_TASK (res), error);
}

static void
g_bytes_icon_loadable_icon_iface_init (GLoadableIconIface *iface)
{
  iface->load = g_bytes_icon_load;
  iface->load_async = g_bytes_icon_load_async;
  iface->load_finish = g_bytes_icon_load_finish;
}
