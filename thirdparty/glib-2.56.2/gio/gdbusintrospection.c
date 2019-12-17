/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "gdbusintrospection.h"

#include "glibintl.h"

/**
 * SECTION:gdbusintrospection
 * @title: D-Bus Introspection Data
 * @short_description: Node and interface description data structures
 * @include: gio/gio.h
 *
 * Various data structures and convenience routines to parse and
 * generate D-Bus introspection XML. Introspection information is
 * used when registering objects with g_dbus_connection_register_object().
 *
 * The format of D-Bus introspection XML is specified in the
 * [D-Bus specification](http://dbus.freedesktop.org/doc/dbus-specification.html#introspection-format)
 */

/* ---------------------------------------------------------------------------------------------------- */

#define _MY_DEFINE_BOXED_TYPE(TypeName, type_name) \
  G_DEFINE_BOXED_TYPE (TypeName, type_name, type_name##_ref, type_name##_unref)

_MY_DEFINE_BOXED_TYPE (GDBusNodeInfo,       g_dbus_node_info)
_MY_DEFINE_BOXED_TYPE (GDBusInterfaceInfo,  g_dbus_interface_info)
_MY_DEFINE_BOXED_TYPE (GDBusMethodInfo,     g_dbus_method_info)
_MY_DEFINE_BOXED_TYPE (GDBusSignalInfo,     g_dbus_signal_info)
_MY_DEFINE_BOXED_TYPE (GDBusPropertyInfo,   g_dbus_property_info)
_MY_DEFINE_BOXED_TYPE (GDBusArgInfo,        g_dbus_arg_info)
_MY_DEFINE_BOXED_TYPE (GDBusAnnotationInfo, g_dbus_annotation_info)

#undef _MY_DEFINE_BOXED_TYPE

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  /* stuff we are currently collecting */
  GPtrArray *args;
  GPtrArray *out_args;
  GPtrArray *methods;
  GPtrArray *signals;
  GPtrArray *properties;
  GPtrArray *interfaces;
  GPtrArray *nodes;
  GPtrArray *annotations;

  /* A list of GPtrArray's containing annotations */
  GSList *annotations_stack;

  /* A list of GPtrArray's containing interfaces */
  GSList *interfaces_stack;

  /* A list of GPtrArray's containing nodes */
  GSList *nodes_stack;

  /* Whether the direction was "in" for last parsed arg */
  gboolean last_arg_was_in;

  /* Number of args currently being collected; used for assigning
   * names to args without a "name" attribute
   */
  guint num_args;

} ParseData;

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_node_info_ref:
 * @info: A #GDBusNodeInfo
 *
 * If @info is statically allocated does nothing. Otherwise increases
 * the reference count.
 *
 * Returns: The same @info.
 *
 * Since: 2.26
 */
GDBusNodeInfo *
g_dbus_node_info_ref (GDBusNodeInfo *info)
{
  if (info->ref_count == -1)
    return info;
  g_atomic_int_inc (&info->ref_count);
  return info;
}

/**
 * g_dbus_interface_info_ref:
 * @info: A #GDBusInterfaceInfo
 *
 * If @info is statically allocated does nothing. Otherwise increases
 * the reference count.
 *
 * Returns: The same @info.
 *
 * Since: 2.26
 */
GDBusInterfaceInfo *
g_dbus_interface_info_ref (GDBusInterfaceInfo *info)
{
  if (info->ref_count == -1)
    return info;
  g_atomic_int_inc (&info->ref_count);
  return info;
}

/**
 * g_dbus_method_info_ref:
 * @info: A #GDBusMethodInfo
 *
 * If @info is statically allocated does nothing. Otherwise increases
 * the reference count.
 *
 * Returns: The same @info.
 *
 * Since: 2.26
 */
GDBusMethodInfo *
g_dbus_method_info_ref (GDBusMethodInfo *info)
{
  if (info->ref_count == -1)
    return info;
  g_atomic_int_inc (&info->ref_count);
  return info;
}

/**
 * g_dbus_signal_info_ref:
 * @info: A #GDBusSignalInfo
 *
 * If @info is statically allocated does nothing. Otherwise increases
 * the reference count.
 *
 * Returns: The same @info.
 *
 * Since: 2.26
 */
GDBusSignalInfo *
g_dbus_signal_info_ref (GDBusSignalInfo *info)
{
  if (info->ref_count == -1)
    return info;
  g_atomic_int_inc (&info->ref_count);
  return info;
}

/**
 * g_dbus_property_info_ref:
 * @info: A #GDBusPropertyInfo
 *
 * If @info is statically allocated does nothing. Otherwise increases
 * the reference count.
 *
 * Returns: The same @info.
 *
 * Since: 2.26
 */
GDBusPropertyInfo *
g_dbus_property_info_ref (GDBusPropertyInfo *info)
{
  if (info->ref_count == -1)
    return info;
  g_atomic_int_inc (&info->ref_count);
  return info;
}

/**
 * g_dbus_arg_info_ref:
 * @info: A #GDBusArgInfo
 *
 * If @info is statically allocated does nothing. Otherwise increases
 * the reference count.
 *
 * Returns: The same @info.
 *
 * Since: 2.26
 */
GDBusArgInfo *
g_dbus_arg_info_ref (GDBusArgInfo *info)
{
  if (info->ref_count == -1)
    return info;
  g_atomic_int_inc (&info->ref_count);
  return info;
}

/**
 * g_dbus_annotation_info_ref:
 * @info: A #GDBusNodeInfo
 *
 * If @info is statically allocated does nothing. Otherwise increases
 * the reference count.
 *
 * Returns: The same @info.
 *
 * Since: 2.26
 */
GDBusAnnotationInfo *
g_dbus_annotation_info_ref (GDBusAnnotationInfo *info)
{
  if (info->ref_count == -1)
    return info;
  g_atomic_int_inc (&info->ref_count);
  return info;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
free_null_terminated_array (gpointer array, GDestroyNotify unref_func)
{
  guint n;
  gpointer *p = array;
  if (p == NULL)
    return;
  for (n = 0; p[n] != NULL; n++)
    unref_func (p[n]);
  g_free (p);
}

/**
 * g_dbus_annotation_info_unref:
 * @info: A #GDBusAnnotationInfo.
 *
 * If @info is statically allocated, does nothing. Otherwise decreases
 * the reference count of @info. When its reference count drops to 0,
 * the memory used is freed.
 *
 * Since: 2.26
 */
void
g_dbus_annotation_info_unref (GDBusAnnotationInfo *info)
{
  if (info->ref_count == -1)
    return;
  if (g_atomic_int_dec_and_test (&info->ref_count))
    {
      g_free (info->key);
      g_free (info->value);
      free_null_terminated_array (info->annotations, (GDestroyNotify) g_dbus_annotation_info_unref);
      g_free (info);
    }
}

/**
 * g_dbus_arg_info_unref:
 * @info: A #GDBusArgInfo.
 *
 * If @info is statically allocated, does nothing. Otherwise decreases
 * the reference count of @info. When its reference count drops to 0,
 * the memory used is freed.
 *
 * Since: 2.26
 */
void
g_dbus_arg_info_unref (GDBusArgInfo *info)
{
  if (info->ref_count == -1)
    return;
  if (g_atomic_int_dec_and_test (&info->ref_count))
    {
      g_free (info->name);
      g_free (info->signature);
      free_null_terminated_array (info->annotations, (GDestroyNotify) g_dbus_annotation_info_unref);
      g_free (info);
    }
}

/**
 * g_dbus_method_info_unref:
 * @info: A #GDBusMethodInfo.
 *
 * If @info is statically allocated, does nothing. Otherwise decreases
 * the reference count of @info. When its reference count drops to 0,
 * the memory used is freed.
 *
 * Since: 2.26
 */
void
g_dbus_method_info_unref (GDBusMethodInfo *info)
{
  if (info->ref_count == -1)
    return;
  if (g_atomic_int_dec_and_test (&info->ref_count))
    {
      g_free (info->name);
      free_null_terminated_array (info->in_args, (GDestroyNotify) g_dbus_arg_info_unref);
      free_null_terminated_array (info->out_args, (GDestroyNotify) g_dbus_arg_info_unref);
      free_null_terminated_array (info->annotations, (GDestroyNotify) g_dbus_annotation_info_unref);
      g_free (info);
    }
}

/**
 * g_dbus_signal_info_unref:
 * @info: A #GDBusSignalInfo.
 *
 * If @info is statically allocated, does nothing. Otherwise decreases
 * the reference count of @info. When its reference count drops to 0,
 * the memory used is freed.
 *
 * Since: 2.26
 */
void
g_dbus_signal_info_unref (GDBusSignalInfo *info)
{
  if (info->ref_count == -1)
    return;
  if (g_atomic_int_dec_and_test (&info->ref_count))
    {
      g_free (info->name);
      free_null_terminated_array (info->args, (GDestroyNotify) g_dbus_arg_info_unref);
      free_null_terminated_array (info->annotations, (GDestroyNotify) g_dbus_annotation_info_unref);
      g_free (info);
    }
}

/**
 * g_dbus_property_info_unref:
 * @info: A #GDBusPropertyInfo.
 *
 * If @info is statically allocated, does nothing. Otherwise decreases
 * the reference count of @info. When its reference count drops to 0,
 * the memory used is freed.
 *
 * Since: 2.26
 */
void
g_dbus_property_info_unref (GDBusPropertyInfo *info)
{
  if (info->ref_count == -1)
    return;
  if (g_atomic_int_dec_and_test (&info->ref_count))
    {
      g_free (info->name);
      g_free (info->signature);
      free_null_terminated_array (info->annotations, (GDestroyNotify) g_dbus_annotation_info_unref);
      g_free (info);
    }
}

/**
 * g_dbus_interface_info_unref:
 * @info: A #GDBusInterfaceInfo.
 *
 * If @info is statically allocated, does nothing. Otherwise decreases
 * the reference count of @info. When its reference count drops to 0,
 * the memory used is freed.
 *
 * Since: 2.26
 */
void
g_dbus_interface_info_unref (GDBusInterfaceInfo *info)
{
  if (info->ref_count == -1)
    return;
  if (g_atomic_int_dec_and_test (&info->ref_count))
    {
      g_free (info->name);
      free_null_terminated_array (info->methods, (GDestroyNotify) g_dbus_method_info_unref);
      free_null_terminated_array (info->signals, (GDestroyNotify) g_dbus_signal_info_unref);
      free_null_terminated_array (info->properties, (GDestroyNotify) g_dbus_property_info_unref);
      free_null_terminated_array (info->annotations, (GDestroyNotify) g_dbus_annotation_info_unref);
      g_free (info);
    }
}

/**
 * g_dbus_node_info_unref:
 * @info: A #GDBusNodeInfo.
 *
 * If @info is statically allocated, does nothing. Otherwise decreases
 * the reference count of @info. When its reference count drops to 0,
 * the memory used is freed.
 *
 * Since: 2.26
 */
void
g_dbus_node_info_unref (GDBusNodeInfo *info)
{
  if (info->ref_count == -1)
    return;
  if (g_atomic_int_dec_and_test (&info->ref_count))
    {
      g_free (info->path);
      free_null_terminated_array (info->interfaces, (GDestroyNotify) g_dbus_interface_info_unref);
      free_null_terminated_array (info->nodes, (GDestroyNotify) g_dbus_node_info_unref);
      free_null_terminated_array (info->annotations, (GDestroyNotify) g_dbus_annotation_info_unref);
      g_free (info);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
g_dbus_annotation_info_set (ParseData            *data,
                            GDBusAnnotationInfo  *info,
                            const gchar          *key,
                            const gchar          *value,
                            GDBusAnnotationInfo **embedded_annotations)
{
  info->ref_count = 1;

  if (key != NULL)
    info->key = g_strdup (key);

  if (value != NULL)
    info->value = g_strdup (value);

  if (embedded_annotations != NULL)
    info->annotations = embedded_annotations;
}

static void
g_dbus_arg_info_set (ParseData            *data,
                     GDBusArgInfo         *info,
                     const gchar          *name,
                     const gchar          *signature,
                     GDBusAnnotationInfo **annotations)
{
  info->ref_count = 1;

  /* name may be NULL - TODO: compute name? */
  if (name != NULL)
    info->name = g_strdup (name);

  if (signature != NULL)
    info->signature = g_strdup (signature);

  if (annotations != NULL)
    info->annotations = annotations;
}

static void
g_dbus_method_info_set (ParseData            *data,
                        GDBusMethodInfo      *info,
                        const gchar          *name,
                        GDBusArgInfo        **in_args,
                        GDBusArgInfo        **out_args,
                        GDBusAnnotationInfo **annotations)
{
  info->ref_count = 1;

  if (name != NULL)
    info->name = g_strdup (name);

  if (in_args != NULL)
    info->in_args = in_args;

  if (out_args != NULL)
    info->out_args = out_args;

  if (annotations != NULL)
    info->annotations = annotations;
}

static void
g_dbus_signal_info_set (ParseData            *data,
                        GDBusSignalInfo      *info,
                        const gchar          *name,
                        GDBusArgInfo        **args,
                        GDBusAnnotationInfo **annotations)
{
  info->ref_count = 1;

  if (name != NULL)
    info->name = g_strdup (name);

  if (args != NULL)
    info->args = args;

  if (annotations != NULL)
    info->annotations = annotations;
}

static void
g_dbus_property_info_set (ParseData               *data,
                          GDBusPropertyInfo       *info,
                          const gchar             *name,
                          const gchar             *signature,
                          GDBusPropertyInfoFlags   flags,
                          GDBusAnnotationInfo    **annotations)
{
  info->ref_count = 1;

  if (name != NULL)
    info->name = g_strdup (name);

  if (flags != G_DBUS_PROPERTY_INFO_FLAGS_NONE)
    info->flags = flags;

  if (signature != NULL)
    info->signature = g_strdup (signature);

  if (annotations != NULL)
    info->annotations = annotations;
}

static void
g_dbus_interface_info_set (ParseData            *data,
                           GDBusInterfaceInfo   *info,
                           const gchar          *name,
                           GDBusMethodInfo     **methods,
                           GDBusSignalInfo     **signals,
                           GDBusPropertyInfo   **properties,
                           GDBusAnnotationInfo **annotations)
{
  info->ref_count = 1;

  if (name != NULL)
    info->name = g_strdup (name);

  if (methods != NULL)
    info->methods = methods;

  if (signals != NULL)
    info->signals = signals;

  if (properties != NULL)
    info->properties = properties;

  if (annotations != NULL)
    info->annotations = annotations;
}

static void
g_dbus_node_info_set (ParseData            *data,
                      GDBusNodeInfo        *info,
                      const gchar          *path,
                      GDBusInterfaceInfo  **interfaces,
                      GDBusNodeInfo       **nodes,
                      GDBusAnnotationInfo **annotations)
{
  info->ref_count = 1;

  if (path != NULL)
    {
      info->path = g_strdup (path);
      /* TODO: relative / absolute path snafu */
    }

  if (interfaces != NULL)
    info->interfaces = interfaces;

  if (nodes != NULL)
    info->nodes = nodes;

  if (annotations != NULL)
    info->annotations = annotations;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
g_dbus_annotation_info_generate_xml (GDBusAnnotationInfo *info,
                                     guint                indent,
                                     GString             *string_builder)
{
  gchar *tmp;
  guint n;

  tmp = g_markup_printf_escaped ("%*s<annotation name=\"%s\" value=\"%s\"",
                                 indent, "",
                                 info->key,
                                 info->value);
  g_string_append (string_builder, tmp);
  g_free (tmp);

  if (info->annotations == NULL)
    {
      g_string_append (string_builder, "/>\n");
    }
  else
    {
      g_string_append (string_builder, ">\n");

      for (n = 0; info->annotations != NULL && info->annotations[n] != NULL; n++)
        g_dbus_annotation_info_generate_xml (info->annotations[n],
                                             indent + 2,
                                             string_builder);

      g_string_append_printf (string_builder, "%*s</annotation>\n",
                              indent, "");
    }

}

static void
g_dbus_arg_info_generate_xml (GDBusArgInfo *info,
                              guint         indent,
                              const gchar  *extra_attributes,
                              GString      *string_builder)
{
  guint n;

  g_string_append_printf (string_builder, "%*s<arg type=\"%s\"",
                          indent, "",
                          info->signature);

  if (info->name != NULL)
    g_string_append_printf (string_builder, " name=\"%s\"", info->name);

  if (extra_attributes != NULL)
    g_string_append_printf (string_builder, " %s", extra_attributes);

  if (info->annotations == NULL)
    {
      g_string_append (string_builder, "/>\n");
    }
  else
    {
      g_string_append (string_builder, ">\n");

      for (n = 0; info->annotations != NULL && info->annotations[n] != NULL; n++)
        g_dbus_annotation_info_generate_xml (info->annotations[n],
                                             indent + 2,
                                             string_builder);

      g_string_append_printf (string_builder, "%*s</arg>\n", indent, "");
    }

}

static void
g_dbus_method_info_generate_xml (GDBusMethodInfo *info,
                                 guint            indent,
                                 GString         *string_builder)
{
  guint n;

  g_string_append_printf (string_builder, "%*s<method name=\"%s\"",
                          indent, "",
                          info->name);

  if (info->annotations == NULL && info->in_args == NULL && info->out_args == NULL)
    {
      g_string_append (string_builder, "/>\n");
    }
  else
    {
      g_string_append (string_builder, ">\n");

      for (n = 0; info->annotations != NULL && info->annotations[n] != NULL; n++)
        g_dbus_annotation_info_generate_xml (info->annotations[n],
                                             indent + 2,
                                             string_builder);

      for (n = 0; info->in_args != NULL && info->in_args[n] != NULL; n++)
        g_dbus_arg_info_generate_xml (info->in_args[n],
                                      indent + 2,
                                      "direction=\"in\"",
                                      string_builder);

      for (n = 0; info->out_args != NULL && info->out_args[n] != NULL; n++)
        g_dbus_arg_info_generate_xml (info->out_args[n],
                                      indent + 2,
                                      "direction=\"out\"",
                                      string_builder);

      g_string_append_printf (string_builder, "%*s</method>\n", indent, "");
    }
}

static void
g_dbus_signal_info_generate_xml (GDBusSignalInfo *info,
                                 guint            indent,
                                 GString         *string_builder)
{
  guint n;

  g_string_append_printf (string_builder, "%*s<signal name=\"%s\"",
                          indent, "",
                          info->name);

  if (info->annotations == NULL && info->args == NULL)
    {
      g_string_append (string_builder, "/>\n");
    }
  else
    {
      g_string_append (string_builder, ">\n");

      for (n = 0; info->annotations != NULL && info->annotations[n] != NULL; n++)
        g_dbus_annotation_info_generate_xml (info->annotations[n],
                                             indent + 2,
                                             string_builder);

      for (n = 0; info->args != NULL && info->args[n] != NULL; n++)
        g_dbus_arg_info_generate_xml (info->args[n],
                                      indent + 2,
                                      NULL,
                                      string_builder);

      g_string_append_printf (string_builder, "%*s</signal>\n", indent, "");
    }
}

static void
g_dbus_property_info_generate_xml (GDBusPropertyInfo *info,
                                   guint              indent,
                                   GString           *string_builder)
{
  guint n;
  const gchar *access_string;

  if ((info->flags & G_DBUS_PROPERTY_INFO_FLAGS_READABLE) &&
      (info->flags & G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE))
    {
      access_string = "readwrite";
    }
  else if (info->flags & G_DBUS_PROPERTY_INFO_FLAGS_READABLE)
    {
      access_string = "read";
    }
  else if (info->flags & G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE)
    {
      access_string = "write";
    }
  else
    {
      g_assert_not_reached ();
    }

  g_string_append_printf (string_builder, "%*s<property type=\"%s\" name=\"%s\" access=\"%s\"",
                          indent, "",
                          info->signature,
                          info->name,
                          access_string);

  if (info->annotations == NULL)
    {
      g_string_append (string_builder, "/>\n");
    }
  else
    {
      g_string_append (string_builder, ">\n");

      for (n = 0; info->annotations != NULL && info->annotations[n] != NULL; n++)
        g_dbus_annotation_info_generate_xml (info->annotations[n],
                                               indent + 2,
                                               string_builder);

      g_string_append_printf (string_builder, "%*s</property>\n", indent, "");
    }

}

/**
 * g_dbus_interface_info_generate_xml:
 * @info: A #GDBusNodeInfo
 * @indent: Indentation level.
 * @string_builder: A #GString to to append XML data to.
 *
 * Appends an XML representation of @info (and its children) to @string_builder.
 *
 * This function is typically used for generating introspection XML
 * documents at run-time for handling the
 * `org.freedesktop.DBus.Introspectable.Introspect`
 * method.
 *
 * Since: 2.26
 */
void
g_dbus_interface_info_generate_xml (GDBusInterfaceInfo *info,
                                    guint               indent,
                                    GString            *string_builder)
{
  guint n;

  g_string_append_printf (string_builder, "%*s<interface name=\"%s\">\n",
                          indent, "",
                          info->name);

  for (n = 0; info->annotations != NULL && info->annotations[n] != NULL; n++)
    g_dbus_annotation_info_generate_xml (info->annotations[n],
                                         indent + 2,
                                         string_builder);

  for (n = 0; info->methods != NULL && info->methods[n] != NULL; n++)
    g_dbus_method_info_generate_xml (info->methods[n],
                                     indent + 2,
                                     string_builder);

  for (n = 0; info->signals != NULL && info->signals[n] != NULL; n++)
    g_dbus_signal_info_generate_xml (info->signals[n],
                                     indent + 2,
                                     string_builder);

  for (n = 0; info->properties != NULL && info->properties[n] != NULL; n++)
    g_dbus_property_info_generate_xml (info->properties[n],
                                       indent + 2,
                                       string_builder);

  g_string_append_printf (string_builder, "%*s</interface>\n", indent, "");
}

/**
 * g_dbus_node_info_generate_xml:
 * @info: A #GDBusNodeInfo.
 * @indent: Indentation level.
 * @string_builder: A #GString to to append XML data to.
 *
 * Appends an XML representation of @info (and its children) to @string_builder.
 *
 * This function is typically used for generating introspection XML documents at run-time for
 * handling the `org.freedesktop.DBus.Introspectable.Introspect`  method.
 *
 * Since: 2.26
 */
void
g_dbus_node_info_generate_xml (GDBusNodeInfo *info,
                               guint          indent,
                               GString       *string_builder)
{
  guint n;

  g_string_append_printf (string_builder, "%*s<node", indent, "");
  if (info->path != NULL)
    g_string_append_printf (string_builder, " name=\"%s\"", info->path);

  if (info->interfaces == NULL && info->nodes == NULL && info->annotations == NULL)
    {
      g_string_append (string_builder, "/>\n");
    }
  else
    {
      g_string_append (string_builder, ">\n");

      for (n = 0; info->annotations != NULL && info->annotations[n] != NULL; n++)
        g_dbus_annotation_info_generate_xml (info->annotations[n],
                                             indent + 2,
                                             string_builder);

      for (n = 0; info->interfaces != NULL && info->interfaces[n] != NULL; n++)
        g_dbus_interface_info_generate_xml (info->interfaces[n],
                                            indent + 2,
                                            string_builder);

      for (n = 0; info->nodes != NULL && info->nodes[n] != NULL; n++)
        g_dbus_node_info_generate_xml (info->nodes[n],
                                       indent + 2,
                                       string_builder);

      g_string_append_printf (string_builder, "%*s</node>\n", indent, "");
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusAnnotationInfo **
parse_data_steal_annotations (ParseData *data,
                              guint     *out_num_elements)
{
  GDBusAnnotationInfo **ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->annotations->len;
  if (data->annotations == NULL)
    ret = NULL;
  else
    {
      g_ptr_array_add (data->annotations, NULL);
      ret = (GDBusAnnotationInfo **) g_ptr_array_free (data->annotations, FALSE);
    }
  data->annotations = g_ptr_array_new ();
  return ret;
}

static GDBusArgInfo **
parse_data_steal_args (ParseData *data,
                       guint     *out_num_elements)
{
  GDBusArgInfo **ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->args->len;
  if (data->args == NULL)
    ret = NULL;
  else
    {
      g_ptr_array_add (data->args, NULL);
      ret = (GDBusArgInfo **) g_ptr_array_free (data->args, FALSE);
    }
  data->args = g_ptr_array_new ();
  return ret;
}

static GDBusArgInfo **
parse_data_steal_out_args (ParseData *data,
                           guint     *out_num_elements)
{
  GDBusArgInfo **ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->out_args->len;
  if (data->out_args == NULL)
    ret = NULL;
  else
    {
      g_ptr_array_add (data->out_args, NULL);
      ret = (GDBusArgInfo **) g_ptr_array_free (data->out_args, FALSE);
    }
  data->out_args = g_ptr_array_new ();
  return ret;
}

static GDBusMethodInfo **
parse_data_steal_methods (ParseData *data,
                          guint     *out_num_elements)
{
  GDBusMethodInfo **ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->methods->len;
  if (data->methods == NULL)
    ret = NULL;
  else
    {
      g_ptr_array_add (data->methods, NULL);
      ret = (GDBusMethodInfo **) g_ptr_array_free (data->methods, FALSE);
    }
  data->methods = g_ptr_array_new ();
  return ret;
}

static GDBusSignalInfo **
parse_data_steal_signals (ParseData *data,
                          guint     *out_num_elements)
{
  GDBusSignalInfo **ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->signals->len;
  if (data->signals == NULL)
    ret = NULL;
  else
    {
      g_ptr_array_add (data->signals, NULL);
      ret = (GDBusSignalInfo **) g_ptr_array_free (data->signals, FALSE);
    }
  data->signals = g_ptr_array_new ();
  return ret;
}

static GDBusPropertyInfo **
parse_data_steal_properties (ParseData *data,
                             guint     *out_num_elements)
{
  GDBusPropertyInfo **ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->properties->len;
  if (data->properties == NULL)
    ret = NULL;
  else
    {
      g_ptr_array_add (data->properties, NULL);
      ret = (GDBusPropertyInfo **) g_ptr_array_free (data->properties, FALSE);
    }
  data->properties = g_ptr_array_new ();
  return ret;
}

static GDBusInterfaceInfo **
parse_data_steal_interfaces (ParseData *data,
                             guint     *out_num_elements)
{
  GDBusInterfaceInfo **ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->interfaces->len;
  if (data->interfaces == NULL)
    ret = NULL;
  else
    {
      g_ptr_array_add (data->interfaces, NULL);
      ret = (GDBusInterfaceInfo **) g_ptr_array_free (data->interfaces, FALSE);
    }
  data->interfaces = g_ptr_array_new ();
  return ret;
}

static GDBusNodeInfo **
parse_data_steal_nodes (ParseData *data,
                        guint     *out_num_elements)
{
  GDBusNodeInfo **ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->nodes->len;
  if (data->nodes == NULL)
    ret = NULL;
  else
    {
      g_ptr_array_add (data->nodes, NULL);
      ret = (GDBusNodeInfo **) g_ptr_array_free (data->nodes, FALSE);
    }
  data->nodes = g_ptr_array_new ();
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
parse_data_free_annotations (ParseData *data)
{
  if (data->annotations == NULL)
    return;
  g_ptr_array_foreach (data->annotations, (GFunc) g_dbus_annotation_info_unref, NULL);
  g_ptr_array_free (data->annotations, TRUE);
  data->annotations = NULL;
}

static void
parse_data_free_args (ParseData *data)
{
  if (data->args == NULL)
    return;
  g_ptr_array_foreach (data->args, (GFunc) g_dbus_arg_info_unref, NULL);
  g_ptr_array_free (data->args, TRUE);
  data->args = NULL;
}

static void
parse_data_free_out_args (ParseData *data)
{
  if (data->out_args == NULL)
    return;
  g_ptr_array_foreach (data->out_args, (GFunc) g_dbus_arg_info_unref, NULL);
  g_ptr_array_free (data->out_args, TRUE);
  data->out_args = NULL;
}

static void
parse_data_free_methods (ParseData *data)
{
  if (data->methods == NULL)
    return;
  g_ptr_array_foreach (data->methods, (GFunc) g_dbus_method_info_unref, NULL);
  g_ptr_array_free (data->methods, TRUE);
  data->methods = NULL;
}

static void
parse_data_free_signals (ParseData *data)
{
  if (data->signals == NULL)
    return;
  g_ptr_array_foreach (data->signals, (GFunc) g_dbus_signal_info_unref, NULL);
  g_ptr_array_free (data->signals, TRUE);
  data->signals = NULL;
}

static void
parse_data_free_properties (ParseData *data)
{
  if (data->properties == NULL)
    return;
  g_ptr_array_foreach (data->properties, (GFunc) g_dbus_property_info_unref, NULL);
  g_ptr_array_free (data->properties, TRUE);
  data->properties = NULL;
}

static void
parse_data_free_interfaces (ParseData *data)
{
  if (data->interfaces == NULL)
    return;
  g_ptr_array_foreach (data->interfaces, (GFunc) g_dbus_interface_info_unref, NULL);
  g_ptr_array_free (data->interfaces, TRUE);
  data->interfaces = NULL;
}

static void
parse_data_free_nodes (ParseData *data)
{
  if (data->nodes == NULL)
    return;
  g_ptr_array_foreach (data->nodes, (GFunc) g_dbus_node_info_unref, NULL);
  g_ptr_array_free (data->nodes, TRUE);
  data->nodes = NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusAnnotationInfo *
parse_data_get_annotation (ParseData *data,
                           gboolean   create_new)
{
  if (create_new)
    g_ptr_array_add (data->annotations, g_new0 (GDBusAnnotationInfo, 1));
  return data->annotations->pdata[data->annotations->len - 1];
}

static GDBusArgInfo *
parse_data_get_arg (ParseData *data,
                    gboolean   create_new)
{
  if (create_new)
    g_ptr_array_add (data->args, g_new0 (GDBusArgInfo, 1));
  return data->args->pdata[data->args->len - 1];
}

static GDBusArgInfo *
parse_data_get_out_arg (ParseData *data,
                        gboolean   create_new)
{
  if (create_new)
    g_ptr_array_add (data->out_args, g_new0 (GDBusArgInfo, 1));
  return data->out_args->pdata[data->out_args->len - 1];
}

static GDBusMethodInfo *
parse_data_get_method (ParseData *data,
                       gboolean   create_new)
{
  if (create_new)
    g_ptr_array_add (data->methods, g_new0 (GDBusMethodInfo, 1));
  return data->methods->pdata[data->methods->len - 1];
}

static GDBusSignalInfo *
parse_data_get_signal (ParseData *data,
                       gboolean   create_new)
{
  if (create_new)
    g_ptr_array_add (data->signals, g_new0 (GDBusSignalInfo, 1));
  return data->signals->pdata[data->signals->len - 1];
}

static GDBusPropertyInfo *
parse_data_get_property (ParseData *data,
                         gboolean   create_new)
{
  if (create_new)
    g_ptr_array_add (data->properties, g_new0 (GDBusPropertyInfo, 1));
  return data->properties->pdata[data->properties->len - 1];
}

static GDBusInterfaceInfo *
parse_data_get_interface (ParseData *data,
                          gboolean   create_new)
{
  if (create_new)
    g_ptr_array_add (data->interfaces, g_new0 (GDBusInterfaceInfo, 1));
  return data->interfaces->pdata[data->interfaces->len - 1];
}

static GDBusNodeInfo *
parse_data_get_node (ParseData *data,
                     gboolean   create_new)
{
  if (create_new)
    g_ptr_array_add (data->nodes, g_new0 (GDBusNodeInfo, 1));
  return data->nodes->pdata[data->nodes->len - 1];
}

/* ---------------------------------------------------------------------------------------------------- */

static ParseData *
parse_data_new (void)
{
  ParseData *data;

  data = g_new0 (ParseData, 1);

  /* initialize arrays */
  parse_data_steal_annotations (data, NULL);
  parse_data_steal_args (data, NULL);
  parse_data_steal_out_args (data, NULL);
  parse_data_steal_methods (data, NULL);
  parse_data_steal_signals (data, NULL);
  parse_data_steal_properties (data, NULL);
  parse_data_steal_interfaces (data, NULL);
  parse_data_steal_nodes (data, NULL);

  return data;
}

static void
parse_data_free (ParseData *data)
{
  GSList *l;

  /* free stack of annotation arrays */
  for (l = data->annotations_stack; l != NULL; l = l->next)
    {
      GPtrArray *annotations = l->data;
      g_ptr_array_foreach (annotations, (GFunc) g_dbus_annotation_info_unref, NULL);
      g_ptr_array_free (annotations, TRUE);
    }
  g_slist_free (data->annotations_stack);

  /* free stack of interface arrays */
  for (l = data->interfaces_stack; l != NULL; l = l->next)
    {
      GPtrArray *interfaces = l->data;
      g_ptr_array_foreach (interfaces, (GFunc) g_dbus_interface_info_unref, NULL);
      g_ptr_array_free (interfaces, TRUE);
    }
  g_slist_free (data->interfaces_stack);

  /* free stack of node arrays */
  for (l = data->nodes_stack; l != NULL; l = l->next)
    {
      GPtrArray *nodes = l->data;
      g_ptr_array_foreach (nodes, (GFunc) g_dbus_node_info_unref, NULL);
      g_ptr_array_free (nodes, TRUE);
    }
  g_slist_free (data->nodes_stack);

  /* free arrays (data->annotations, data->interfaces and data->nodes have been freed above) */
  parse_data_free_args (data);
  parse_data_free_out_args (data);
  parse_data_free_methods (data);
  parse_data_free_signals (data);
  parse_data_free_properties (data);
  parse_data_free_interfaces (data);
  parse_data_free_annotations (data);
  parse_data_free_nodes (data);

  g_free (data);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
parser_start_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **attribute_names,
                      const gchar         **attribute_values,
                      gpointer              user_data,
                      GError              **error)
{
  ParseData *data = user_data;
  GSList *stack;
  const gchar *name;
  const gchar *type;
  const gchar *access;
  const gchar *direction;
  const gchar *value;

  name = NULL;
  type = NULL;
  access = NULL;
  direction = NULL;
  value = NULL;

  stack = (GSList *) g_markup_parse_context_get_element_stack (context);

  /* ---------------------------------------------------------------------------------------------------- */
  if (strcmp (element_name, "node") == 0)
    {
      if (!(g_slist_length (stack) >= 1 || strcmp (stack->next->data, "node") != 0))
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<node> elements can only be top-level or embedded in other <node> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "name", &name,
                                        /* some hand-written introspection XML documents use this */
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "xmlns:doc", NULL,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      g_dbus_node_info_set (data,
                            parse_data_get_node (data, TRUE),
                            name,
                            NULL,
                            NULL,
                            NULL);

      /* push the currently retrieved interfaces and nodes on the stack and prepare new arrays */
      data->interfaces_stack = g_slist_prepend (data->interfaces_stack, data->interfaces);
      data->interfaces = NULL;
      parse_data_steal_interfaces (data, NULL);

      data->nodes_stack = g_slist_prepend (data->nodes_stack, data->nodes);
      data->nodes = NULL;
      parse_data_steal_nodes (data, NULL);

    }
  /* ---------------------------------------------------------------------------------------------------- */
  else if (strcmp (element_name, "interface") == 0)
    {
      if (g_slist_length (stack) < 2 || strcmp (stack->next->data, "node") != 0)
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<interface> elements can only be embedded in <node> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        /* seen in the wild */
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "version", NULL,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      g_dbus_interface_info_set (data,
                                 parse_data_get_interface (data, TRUE),
                                 name,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);

    }
  /* ---------------------------------------------------------------------------------------------------- */
  else if (strcmp (element_name, "method") == 0)
    {
      if (g_slist_length (stack) < 2 || strcmp (stack->next->data, "interface") != 0)
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<method> elements can only be embedded in <interface> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        /* seen in the wild */
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "version", NULL,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      g_dbus_method_info_set (data,
                              parse_data_get_method (data, TRUE),
                              name,
                              NULL,
                              NULL,
                              NULL);

      data->num_args = 0;

    }
  /* ---------------------------------------------------------------------------------------------------- */
  else if (strcmp (element_name, "signal") == 0)
    {
      if (g_slist_length (stack) < 2 || strcmp (stack->next->data, "interface") != 0)
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<signal> elements can only be embedded in <interface> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      g_dbus_signal_info_set (data,
                              parse_data_get_signal (data, TRUE),
                              name,
                              NULL,
                              NULL);

      data->num_args = 0;

    }
  /* ---------------------------------------------------------------------------------------------------- */
  else if (strcmp (element_name, "property") == 0)
    {
      GDBusPropertyInfoFlags flags;

      if (g_slist_length (stack) < 2 || strcmp (stack->next->data, "interface") != 0)
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<property> elements can only be embedded in <interface> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "type", &type,
                                        G_MARKUP_COLLECT_STRING, "access", &access,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      if (strcmp (access, "read") == 0)
        flags = G_DBUS_PROPERTY_INFO_FLAGS_READABLE;
      else if (strcmp (access, "write") == 0)
        flags = G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE;
      else if (strcmp (access, "readwrite") == 0)
        flags = G_DBUS_PROPERTY_INFO_FLAGS_READABLE | G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE;
      else
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Unknown value '%s' of access attribute for element <property>",
                       access);
          goto out;
        }

      g_dbus_property_info_set (data,
                                parse_data_get_property (data, TRUE),
                                name,
                                type,
                                flags,
                                NULL);

    }
  /* ---------------------------------------------------------------------------------------------------- */
  else if (strcmp (element_name, "arg") == 0)
    {
      gboolean is_in;
      gchar *name_to_use;

      if (g_slist_length (stack) < 2 ||
          (strcmp (stack->next->data, "method") != 0 &&
           strcmp (stack->next->data, "signal") != 0))
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<arg> elements can only be embedded in <method> or <signal> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "name", &name,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "direction", &direction,
                                        G_MARKUP_COLLECT_STRING, "type", &type,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      if (strcmp (stack->next->data, "method") == 0)
        is_in = TRUE;
      else
        is_in = FALSE;
      if (direction != NULL)
        {
          if (strcmp (direction, "in") == 0)
            is_in = TRUE;
          else if (strcmp (direction, "out") == 0)
            is_in = FALSE;
          else
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Unknown value '%s' of direction attribute",
                           direction);
              goto out;
            }
        }

      if (is_in && strcmp (stack->next->data, "signal") == 0)
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "Only direction 'out' is allowed for <arg> elements embedded in <signal>");
          goto out;
        }

      if (name == NULL)
        name_to_use = g_strdup_printf ("arg_%d", data->num_args);
      else
        name_to_use = g_strdup (name);
      data->num_args++;

      if (is_in)
        {
          g_dbus_arg_info_set (data,
                               parse_data_get_arg (data, TRUE),
                               name_to_use,
                               type,
                               NULL);
          data->last_arg_was_in = TRUE;
        }
      else
        {
          g_dbus_arg_info_set (data,
                               parse_data_get_out_arg (data, TRUE),
                               name_to_use,
                               type,
                               NULL);
          data->last_arg_was_in = FALSE;

        }

      g_free (name_to_use);
    }
  /* ---------------------------------------------------------------------------------------------------- */
  else if (strcmp (element_name, "annotation") == 0)
    {
      if (g_slist_length (stack) < 2 ||
          (strcmp (stack->next->data, "node") != 0 &&
           strcmp (stack->next->data, "interface") != 0 &&
           strcmp (stack->next->data, "signal") != 0 &&
           strcmp (stack->next->data, "method") != 0 &&
           strcmp (stack->next->data, "property") != 0 &&
           strcmp (stack->next->data, "arg") != 0 &&
           strcmp (stack->next->data, "annotation") != 0))
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<annotation> elements can only be embedded in <node>, <interface>, <signal>, <method>, <property>, <arg> or <annotation> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "value", &value,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      g_dbus_annotation_info_set (data,
                                  parse_data_get_annotation (data, TRUE),
                                  name,
                                  value,
                                  NULL);
    }
  /* ---------------------------------------------------------------------------------------------------- */
  else
    {
      /* don't bail on unknown elements; just ignore them */
    }
  /* ---------------------------------------------------------------------------------------------------- */

  /* push the currently retrieved annotations on the stack and prepare a new one */
  data->annotations_stack = g_slist_prepend (data->annotations_stack, data->annotations);
  data->annotations = NULL;
  parse_data_steal_annotations (data, NULL);

 out:
  ;
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusAnnotationInfo **
steal_annotations (ParseData *data)
{
  return parse_data_steal_annotations (data, NULL);
}


static void
parser_end_element (GMarkupParseContext  *context,
                    const gchar          *element_name,
                    gpointer              user_data,
                    GError              **error)
{
  ParseData *data = user_data;
  gboolean have_popped_annotations;

  have_popped_annotations = FALSE;

  if (strcmp (element_name, "node") == 0)
    {
      guint num_nodes;
      guint num_interfaces;
      GDBusNodeInfo **nodes;
      GDBusInterfaceInfo **interfaces;

      nodes = parse_data_steal_nodes (data, &num_nodes);
      interfaces = parse_data_steal_interfaces (data, &num_interfaces);

      /* destroy the nodes, interfaces for scope we're exiting and and pop the nodes, interfaces from the
       * scope we're reentering
       */
      parse_data_free_interfaces (data);
      data->interfaces = (GPtrArray *) data->interfaces_stack->data;
      data->interfaces_stack = g_slist_remove (data->interfaces_stack, data->interfaces_stack->data);

      parse_data_free_nodes (data);
      data->nodes = (GPtrArray *) data->nodes_stack->data;
      data->nodes_stack = g_slist_remove (data->nodes_stack, data->nodes_stack->data);

      g_dbus_node_info_set (data,
                            parse_data_get_node (data, FALSE),
                            NULL,
                            interfaces,
                            nodes,
                            steal_annotations (data));

    }
  else if (strcmp (element_name, "interface") == 0)
    {
      guint num_methods;
      guint num_signals;
      guint num_properties;
      GDBusMethodInfo **methods;
      GDBusSignalInfo **signals;
      GDBusPropertyInfo **properties;

      methods    = parse_data_steal_methods    (data, &num_methods);
      signals    = parse_data_steal_signals    (data, &num_signals);
      properties = parse_data_steal_properties (data, &num_properties);

      g_dbus_interface_info_set (data,
                                 parse_data_get_interface (data, FALSE),
                                 NULL,
                                 methods,
                                 signals,
                                 properties,
                                 steal_annotations (data));

    }
  else if (strcmp (element_name, "method") == 0)
    {
      guint in_num_args;
      guint out_num_args;
      GDBusArgInfo **in_args;
      GDBusArgInfo **out_args;

      in_args  = parse_data_steal_args     (data, &in_num_args);
      out_args = parse_data_steal_out_args (data, &out_num_args);

      g_dbus_method_info_set (data,
                              parse_data_get_method (data, FALSE),
                              NULL,
                              in_args,
                              out_args,
                              steal_annotations (data));
    }
  else if (strcmp (element_name, "signal") == 0)
    {
      guint num_args;
      GDBusArgInfo **args;

      args = parse_data_steal_out_args (data, &num_args);

      g_dbus_signal_info_set (data,
                              parse_data_get_signal (data, FALSE),
                              NULL,
                              args,
                              steal_annotations (data));
    }
  else if (strcmp (element_name, "property") == 0)
    {
      g_dbus_property_info_set (data,
                                parse_data_get_property (data, FALSE),
                                NULL,
                                NULL,
                                G_DBUS_PROPERTY_INFO_FLAGS_NONE,
                                steal_annotations (data));
    }
  else if (strcmp (element_name, "arg") == 0)
    {
      g_dbus_arg_info_set (data,
                           data->last_arg_was_in ? parse_data_get_arg (data, FALSE) : parse_data_get_out_arg (data, FALSE),
                           NULL,
                           NULL,
                           steal_annotations (data));
    }
  else if (strcmp (element_name, "annotation") == 0)
    {
      GDBusAnnotationInfo **embedded_annotations;

      embedded_annotations = steal_annotations (data);

      /* destroy the annotations for scope we're exiting and and pop the annotations from the scope we're reentering */
      parse_data_free_annotations (data);
      data->annotations = (GPtrArray *) data->annotations_stack->data;
      data->annotations_stack = g_slist_remove (data->annotations_stack, data->annotations_stack->data);

      have_popped_annotations = TRUE;

      g_dbus_annotation_info_set (data,
                                  parse_data_get_annotation (data, FALSE),
                                  NULL,
                                  NULL,
                                  embedded_annotations);
    }
  else
    {
      /* don't bail on unknown elements; just ignore them */
    }

  if (!have_popped_annotations)
    {
      /* destroy the annotations for scope we're exiting and and pop the annotations from the scope we're reentering */
      parse_data_free_annotations (data);
      data->annotations = (GPtrArray *) data->annotations_stack->data;
      data->annotations_stack = g_slist_remove (data->annotations_stack, data->annotations_stack->data);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
parser_error (GMarkupParseContext *context,
              GError              *error,
              gpointer             user_data)
{
  gint line_number;
  gint char_number;

  g_markup_parse_context_get_position (context, &line_number, &char_number);

  g_prefix_error (&error, "%d:%d: ",
                  line_number,
                  char_number);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_node_info_new_for_xml:
 * @xml_data: Valid D-Bus introspection XML.
 * @error: Return location for error.
 *
 * Parses @xml_data and returns a #GDBusNodeInfo representing the data.
 *
 * The introspection XML must contain exactly one top-level
 * <node> element.
 *
 * Note that this routine is using a
 * [GMarkup][glib-Simple-XML-Subset-Parser.description]-based
 * parser that only accepts a subset of valid XML documents.
 *
 * Returns: A #GDBusNodeInfo structure or %NULL if @error is set. Free
 * with g_dbus_node_info_unref().
 *
 * Since: 2.26
 */
GDBusNodeInfo *
g_dbus_node_info_new_for_xml (const gchar  *xml_data,
                              GError      **error)
{
  GDBusNodeInfo *ret;
  GMarkupParseContext *context;
  GMarkupParser *parser;
  guint num_nodes;
  ParseData *data;
  GDBusNodeInfo **ughret;

  ret = NULL;
  parser = NULL;
  context = NULL;

  parser = g_new0 (GMarkupParser, 1);
  parser->start_element = parser_start_element;
  parser->end_element   = parser_end_element;
  parser->error         = parser_error;

  data = parse_data_new ();
  context = g_markup_parse_context_new (parser,
                                        G_MARKUP_IGNORE_QUALIFIED,
                                        data,
                                        (GDestroyNotify) parse_data_free);

  if (!g_markup_parse_context_parse (context,
                                     xml_data,
                                     strlen (xml_data),
                                     error))
    goto out;

  if (!g_markup_parse_context_end_parse (context, error))
    goto out;

  ughret = parse_data_steal_nodes (data, &num_nodes);

  if (num_nodes != 1)
    {
      guint n;

      g_set_error (error,
                   G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   "Expected a single node in introspection XML, found %d",
                   num_nodes);

      /* clean up */
      for (n = 0; n < num_nodes; n++)
        {
          g_dbus_node_info_unref (ughret[n]);
          ughret[n] = NULL;
        }
    }

  ret = ughret[0];
  g_free (ughret);

 out:
  g_free (parser);
  if (context != NULL)
    g_markup_parse_context_free (context);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_annotation_info_lookup:
 * @annotations: (array zero-terminated=1) (nullable): A %NULL-terminated array of annotations or %NULL.
 * @name: The name of the annotation to look up.
 *
 * Looks up the value of an annotation.
 *
 * The cost of this function is O(n) in number of annotations.
 *
 * Returns: The value or %NULL if not found. Do not free, it is owned by @annotations.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_annotation_info_lookup (GDBusAnnotationInfo **annotations,
                               const gchar          *name)
{
  guint n;
  const gchar *ret;

  ret = NULL;
  for (n = 0; annotations != NULL && annotations[n] != NULL; n++)
    {
      if (g_strcmp0 (annotations[n]->key, name) == 0)
        {
          ret = annotations[n]->value;
          goto out;
        }
    }

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

G_LOCK_DEFINE_STATIC (info_cache_lock);

typedef struct
{
  gint use_count;

  /* gchar* -> GDBusMethodInfo* */
  GHashTable *method_name_to_data;

  /* gchar* -> GDBusMethodInfo* */
  GHashTable *signal_name_to_data;

  /* gchar* -> GDBusMethodInfo* */
  GHashTable *property_name_to_data;
} InfoCacheEntry;

static void
info_cache_free (InfoCacheEntry *cache)
{
  g_assert (cache->use_count == 0);
  g_hash_table_unref (cache->method_name_to_data);
  g_hash_table_unref (cache->signal_name_to_data);
  g_hash_table_unref (cache->property_name_to_data);
  g_slice_free (InfoCacheEntry, cache);
}

/* maps from GDBusInterfaceInfo* to InfoCacheEntry* */
static GHashTable *info_cache = NULL;

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_interface_info_lookup_method:
 * @info: A #GDBusInterfaceInfo.
 * @name: A D-Bus method name (typically in CamelCase)
 *
 * Looks up information about a method.
 *
 * The cost of this function is O(n) in number of methods unless
 * g_dbus_interface_info_cache_build() has been used on @info.
 *
 * Returns: (transfer none): A #GDBusMethodInfo or %NULL if not found. Do not free, it is owned by @info.
 *
 * Since: 2.26
 */
GDBusMethodInfo *
g_dbus_interface_info_lookup_method (GDBusInterfaceInfo *info,
                                     const gchar        *name)
{
  guint n;
  GDBusMethodInfo *result;

  G_LOCK (info_cache_lock);
  if (G_LIKELY (info_cache != NULL))
    {
      InfoCacheEntry *cache;
      cache = g_hash_table_lookup (info_cache, info);
      if (G_LIKELY (cache != NULL))
        {
          result = g_hash_table_lookup (cache->method_name_to_data, name);
          G_UNLOCK (info_cache_lock);
          goto out;
        }
    }
  G_UNLOCK (info_cache_lock);

  for (n = 0; info->methods != NULL && info->methods[n] != NULL; n++)
    {
      GDBusMethodInfo *i = info->methods[n];

      if (g_strcmp0 (i->name, name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_interface_info_lookup_signal:
 * @info: A #GDBusInterfaceInfo.
 * @name: A D-Bus signal name (typically in CamelCase)
 *
 * Looks up information about a signal.
 *
 * The cost of this function is O(n) in number of signals unless
 * g_dbus_interface_info_cache_build() has been used on @info.
 *
 * Returns: (transfer none): A #GDBusSignalInfo or %NULL if not found. Do not free, it is owned by @info.
 *
 * Since: 2.26
 */
GDBusSignalInfo *
g_dbus_interface_info_lookup_signal (GDBusInterfaceInfo *info,
                                     const gchar        *name)
{
  guint n;
  GDBusSignalInfo *result;

  G_LOCK (info_cache_lock);
  if (G_LIKELY (info_cache != NULL))
    {
      InfoCacheEntry *cache;
      cache = g_hash_table_lookup (info_cache, info);
      if (G_LIKELY (cache != NULL))
        {
          result = g_hash_table_lookup (cache->signal_name_to_data, name);
          G_UNLOCK (info_cache_lock);
          goto out;
        }
    }
  G_UNLOCK (info_cache_lock);

  for (n = 0; info->signals != NULL && info->signals[n] != NULL; n++)
    {
      GDBusSignalInfo *i = info->signals[n];

      if (g_strcmp0 (i->name, name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_interface_info_lookup_property:
 * @info: A #GDBusInterfaceInfo.
 * @name: A D-Bus property name (typically in CamelCase).
 *
 * Looks up information about a property.
 *
 * The cost of this function is O(n) in number of properties unless
 * g_dbus_interface_info_cache_build() has been used on @info.
 *
 * Returns: (transfer none): A #GDBusPropertyInfo or %NULL if not found. Do not free, it is owned by @info.
 *
 * Since: 2.26
 */
GDBusPropertyInfo *
g_dbus_interface_info_lookup_property (GDBusInterfaceInfo *info,
                                       const gchar        *name)
{
  guint n;
  GDBusPropertyInfo *result;

  G_LOCK (info_cache_lock);
  if (G_LIKELY (info_cache != NULL))
    {
      InfoCacheEntry *cache;
      cache = g_hash_table_lookup (info_cache, info);
      if (G_LIKELY (cache != NULL))
        {
          result = g_hash_table_lookup (cache->property_name_to_data, name);
          G_UNLOCK (info_cache_lock);
          goto out;
        }
    }
  G_UNLOCK (info_cache_lock);

  for (n = 0; info->properties != NULL && info->properties[n] != NULL; n++)
    {
      GDBusPropertyInfo *i = info->properties[n];

      if (g_strcmp0 (i->name, name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_interface_info_cache_build:
 * @info: A #GDBusInterfaceInfo.
 *
 * Builds a lookup-cache to speed up
 * g_dbus_interface_info_lookup_method(),
 * g_dbus_interface_info_lookup_signal() and
 * g_dbus_interface_info_lookup_property().
 *
 * If this has already been called with @info, the existing cache is
 * used and its use count is increased.
 *
 * Note that @info cannot be modified until
 * g_dbus_interface_info_cache_release() is called.
 *
 * Since: 2.30
 */
void
g_dbus_interface_info_cache_build (GDBusInterfaceInfo *info)
{
  InfoCacheEntry *cache;
  guint n;

  G_LOCK (info_cache_lock);
  if (info_cache == NULL)
    info_cache = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) info_cache_free);
  cache = g_hash_table_lookup (info_cache, info);
  if (cache != NULL)
    {
      cache->use_count += 1;
      goto out;
    }
  cache = g_slice_new0 (InfoCacheEntry);
  cache->use_count = 1;
  cache->method_name_to_data = g_hash_table_new (g_str_hash, g_str_equal);
  cache->signal_name_to_data = g_hash_table_new (g_str_hash, g_str_equal);
  cache->property_name_to_data = g_hash_table_new (g_str_hash, g_str_equal);
  for (n = 0; info->methods != NULL && info->methods[n] != NULL; n++)
    g_hash_table_insert (cache->method_name_to_data, info->methods[n]->name, info->methods[n]);
  for (n = 0; info->signals != NULL && info->signals[n] != NULL; n++)
    g_hash_table_insert (cache->signal_name_to_data, info->signals[n]->name, info->signals[n]);
  for (n = 0; info->properties != NULL && info->properties[n] != NULL; n++)
    g_hash_table_insert (cache->property_name_to_data, info->properties[n]->name, info->properties[n]);
  g_hash_table_insert (info_cache, info, cache);
 out:
  G_UNLOCK (info_cache_lock);
}

/**
 * g_dbus_interface_info_cache_release:
 * @info: A GDBusInterfaceInfo
 *
 * Decrements the usage count for the cache for @info built by
 * g_dbus_interface_info_cache_build() (if any) and frees the
 * resources used by the cache if the usage count drops to zero.
 *
 * Since: 2.30
 */
void
g_dbus_interface_info_cache_release (GDBusInterfaceInfo *info)
{
  InfoCacheEntry *cache;

  G_LOCK (info_cache_lock);
  if (G_UNLIKELY (info_cache == NULL))
    {
      g_warning ("%s called for interface %s but there is no cache", info->name, G_STRFUNC);
      goto out;
    }

  cache = g_hash_table_lookup (info_cache, info);
  if (G_UNLIKELY (cache == NULL))
    {
      g_warning ("%s called for interface %s but there is no cache entry", info->name, G_STRFUNC);
      goto out;
    }
  cache->use_count -= 1;
  if (cache->use_count == 0)
    {
      g_hash_table_remove (info_cache, info);
      /* could nuke info_cache itself if empty */
    }
 out:
  G_UNLOCK (info_cache_lock);
}


/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_node_info_lookup_interface:
 * @info: A #GDBusNodeInfo.
 * @name: A D-Bus interface name.
 *
 * Looks up information about an interface.
 *
 * The cost of this function is O(n) in number of interfaces.
 *
 * Returns: (transfer none): A #GDBusInterfaceInfo or %NULL if not found. Do not free, it is owned by @info.
 *
 * Since: 2.26
 */
GDBusInterfaceInfo *
g_dbus_node_info_lookup_interface (GDBusNodeInfo *info,
                                   const gchar   *name)
{
  guint n;
  GDBusInterfaceInfo *result;

  for (n = 0; info->interfaces != NULL && info->interfaces[n] != NULL; n++)
    {
      GDBusInterfaceInfo *i = info->interfaces[n];

      if (g_strcmp0 (i->name, name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}
