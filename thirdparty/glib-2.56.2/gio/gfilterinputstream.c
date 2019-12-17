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
 * Author: Christian Kellner <gicmo@gnome.org> 
 */

#include "config.h"
#include "gfilterinputstream.h"
#include "ginputstream.h"
#include "glibintl.h"


/**
 * SECTION:gfilterinputstream
 * @short_description: Filter Input Stream
 * @include: gio/gio.h
 *
 * Base class for input stream implementations that perform some
 * kind of filtering operation on a base stream. Typical examples
 * of filtering operations are character set conversion, compression
 * and byte order flipping.
 **/

enum {
  PROP_0,
  PROP_BASE_STREAM,
  PROP_CLOSE_BASE
};

static void     g_filter_input_stream_set_property (GObject      *object,
                                                    guint         prop_id,
                                                    const GValue *value,
                                                    GParamSpec   *pspec);

static void     g_filter_input_stream_get_property (GObject      *object,
                                                    guint         prop_id,
                                                    GValue       *value,
                                                    GParamSpec   *pspec);
static void     g_filter_input_stream_finalize     (GObject *object);


static gssize   g_filter_input_stream_read         (GInputStream         *stream,
                                                    void                 *buffer,
                                                    gsize                 count,
                                                    GCancellable         *cancellable,
                                                    GError              **error);
static gssize   g_filter_input_stream_skip         (GInputStream         *stream,
                                                    gsize                 count,
                                                    GCancellable         *cancellable,
                                                    GError              **error);
static gboolean g_filter_input_stream_close        (GInputStream         *stream,
                                                    GCancellable         *cancellable,
                                                    GError              **error);

typedef struct
{
  gboolean close_base;
} GFilterInputStreamPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GFilterInputStream, g_filter_input_stream, G_TYPE_INPUT_STREAM)

static void
g_filter_input_stream_class_init (GFilterInputStreamClass *klass)
{
  GObjectClass *object_class;
  GInputStreamClass *istream_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = g_filter_input_stream_get_property;
  object_class->set_property = g_filter_input_stream_set_property;
  object_class->finalize     = g_filter_input_stream_finalize;

  istream_class = G_INPUT_STREAM_CLASS (klass);
  istream_class->read_fn  = g_filter_input_stream_read;
  istream_class->skip  = g_filter_input_stream_skip;
  istream_class->close_fn = g_filter_input_stream_close;

  g_object_class_install_property (object_class,
                                   PROP_BASE_STREAM,
                                   g_param_spec_object ("base-stream",
                                                         P_("The Filter Base Stream"),
                                                         P_("The underlying base stream on which the io ops will be done."),
                                                         G_TYPE_INPUT_STREAM,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | 
                                                         G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_CLOSE_BASE,
                                   g_param_spec_boolean ("close-base-stream",
                                                         P_("Close Base Stream"),
                                                         P_("If the base stream should be closed when the filter stream is closed."),
                                                         TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                         G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
}

static void
g_filter_input_stream_set_property (GObject         *object,
                                    guint            prop_id,
                                    const GValue    *value,
                                    GParamSpec      *pspec)
{
  GFilterInputStream *filter_stream;
  GObject *obj;

  filter_stream = G_FILTER_INPUT_STREAM (object);

  switch (prop_id) 
    {
    case PROP_BASE_STREAM:
      obj = g_value_dup_object (value);
      filter_stream->base_stream = G_INPUT_STREAM (obj); 
      break;

    case PROP_CLOSE_BASE:
      g_filter_input_stream_set_close_base_stream (filter_stream,
                                                   g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_filter_input_stream_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GFilterInputStream *filter_stream;
  GFilterInputStreamPrivate *priv;

  filter_stream = G_FILTER_INPUT_STREAM (object);
  priv = g_filter_input_stream_get_instance_private (filter_stream);

  switch (prop_id)
    {
    case PROP_BASE_STREAM:
      g_value_set_object (value, filter_stream->base_stream);
      break;

    case PROP_CLOSE_BASE:
      g_value_set_boolean (value, priv->close_base);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_filter_input_stream_finalize (GObject *object)
{
  GFilterInputStream *stream;

  stream = G_FILTER_INPUT_STREAM (object);

  g_object_unref (stream->base_stream);

  G_OBJECT_CLASS (g_filter_input_stream_parent_class)->finalize (object);
}

static void
g_filter_input_stream_init (GFilterInputStream *stream)
{
}

/**
 * g_filter_input_stream_get_base_stream:
 * @stream: a #GFilterInputStream.
 * 
 * Gets the base stream for the filter stream.
 *
 * Returns: (transfer none): a #GInputStream.
 **/
GInputStream *
g_filter_input_stream_get_base_stream (GFilterInputStream *stream)
{
  g_return_val_if_fail (G_IS_FILTER_INPUT_STREAM (stream), NULL);

  return stream->base_stream;
}

/**
 * g_filter_input_stream_get_close_base_stream:
 * @stream: a #GFilterInputStream.
 *
 * Returns whether the base stream will be closed when @stream is
 * closed.
 *
 * Returns: %TRUE if the base stream will be closed.
 **/
gboolean
g_filter_input_stream_get_close_base_stream (GFilterInputStream *stream)
{
  GFilterInputStreamPrivate *priv;

  g_return_val_if_fail (G_IS_FILTER_INPUT_STREAM (stream), FALSE);

  priv = g_filter_input_stream_get_instance_private (stream);

  return priv->close_base;
}

/**
 * g_filter_input_stream_set_close_base_stream:
 * @stream: a #GFilterInputStream.
 * @close_base: %TRUE to close the base stream.
 *
 * Sets whether the base stream will be closed when @stream is closed.
 **/
void
g_filter_input_stream_set_close_base_stream (GFilterInputStream *stream,
                                             gboolean            close_base)
{
  GFilterInputStreamPrivate *priv;

  g_return_if_fail (G_IS_FILTER_INPUT_STREAM (stream));

  close_base = !!close_base;
 
  priv = g_filter_input_stream_get_instance_private (stream);

  if (priv->close_base != close_base)
    {
      priv->close_base = close_base;
      g_object_notify (G_OBJECT (stream), "close-base-stream");
    }
}

static gssize
g_filter_input_stream_read (GInputStream  *stream,
                            void          *buffer,
                            gsize          count,
                            GCancellable  *cancellable,
                            GError       **error)
{
  GFilterInputStream *filter_stream;
  GInputStream       *base_stream;
  gssize              nread;

  filter_stream = G_FILTER_INPUT_STREAM (stream);
  base_stream = filter_stream->base_stream;

  nread = g_input_stream_read (base_stream,
                               buffer,
                               count,
                               cancellable,
                               error);

  return nread;
}

static gssize
g_filter_input_stream_skip (GInputStream  *stream,
                            gsize          count,
                            GCancellable  *cancellable,
                            GError       **error)
{
  GFilterInputStream *filter_stream;
  GInputStream       *base_stream;
  gssize              nskipped;

  filter_stream = G_FILTER_INPUT_STREAM (stream);
  base_stream = filter_stream->base_stream;

  nskipped = g_input_stream_skip (base_stream,
                                  count,
                                  cancellable,
                                  error);
  return nskipped;
}

static gboolean
g_filter_input_stream_close (GInputStream  *stream,
                             GCancellable  *cancellable,
                             GError       **error)
{
  GFilterInputStream *filter_stream = G_FILTER_INPUT_STREAM (stream);
  GFilterInputStreamPrivate *priv = g_filter_input_stream_get_instance_private (filter_stream);
  gboolean res = TRUE;

  if (priv->close_base)
    {
      res = g_input_stream_close (filter_stream->base_stream,
                                  cancellable,
                                  error);
    }

  return res;
}
