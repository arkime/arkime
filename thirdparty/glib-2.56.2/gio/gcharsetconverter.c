/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2009 Red Hat, Inc.
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

#include "gcharsetconverter.h"

#include <errno.h>

#include "ginitable.h"
#include "gioerror.h"
#include "glibintl.h"


enum {
  PROP_0,
  PROP_FROM_CHARSET,
  PROP_TO_CHARSET,
  PROP_USE_FALLBACK
};

/**
 * SECTION:gcharsetconverter
 * @short_description: Convert between charsets
 * @include: gio/gio.h
 *
 * #GCharsetConverter is an implementation of #GConverter based on
 * GIConv.
 */

static void g_charset_converter_iface_init          (GConverterIface *iface);
static void g_charset_converter_initable_iface_init (GInitableIface  *iface);

/**
 * GCharsetConverter:
 *
 * Conversions between character sets.
 */
struct _GCharsetConverter
{
  GObject parent_instance;

  char *from;
  char *to;
  GIConv iconv;
  gboolean use_fallback;
  guint n_fallback_errors;
};

G_DEFINE_TYPE_WITH_CODE (GCharsetConverter, g_charset_converter, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER,
						g_charset_converter_iface_init);
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						g_charset_converter_initable_iface_init))

static void
g_charset_converter_finalize (GObject *object)
{
  GCharsetConverter *conv;

  conv = G_CHARSET_CONVERTER (object);

  g_free (conv->from);
  g_free (conv->to);
  if (conv->iconv)
    g_iconv_close (conv->iconv);

  G_OBJECT_CLASS (g_charset_converter_parent_class)->finalize (object);
}

static void
g_charset_converter_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GCharsetConverter *conv;

  conv = G_CHARSET_CONVERTER (object);

  switch (prop_id)
    {
    case PROP_TO_CHARSET:
      g_free (conv->to);
      conv->to = g_value_dup_string (value);
      break;

    case PROP_FROM_CHARSET:
      g_free (conv->from);
      conv->from = g_value_dup_string (value);
      break;

    case PROP_USE_FALLBACK:
      conv->use_fallback = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_charset_converter_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  GCharsetConverter *conv;

  conv = G_CHARSET_CONVERTER (object);

  switch (prop_id)
    {
    case PROP_TO_CHARSET:
      g_value_set_string (value, conv->to);
      break;

    case PROP_FROM_CHARSET:
      g_value_set_string (value, conv->from);
      break;

    case PROP_USE_FALLBACK:
      g_value_set_boolean (value, conv->use_fallback);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_charset_converter_class_init (GCharsetConverterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_charset_converter_finalize;
  gobject_class->get_property = g_charset_converter_get_property;
  gobject_class->set_property = g_charset_converter_set_property;

  g_object_class_install_property (gobject_class,
				   PROP_TO_CHARSET,
				   g_param_spec_string ("to-charset",
							P_("To Charset"),
							P_("The character encoding to convert to"),
							NULL,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
				   PROP_FROM_CHARSET,
				   g_param_spec_string ("from-charset",
							P_("From Charset"),
							P_("The character encoding to convert from"),
							NULL,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
				   PROP_USE_FALLBACK,
				   g_param_spec_boolean ("use-fallback",
							 P_("Fallback enabled"),
							 P_("Use fallback (of form \\<hexval>) for invalid bytes"),
							 FALSE,
							 G_PARAM_READWRITE |
							 G_PARAM_CONSTRUCT |
							 G_PARAM_STATIC_STRINGS));
}

static void
g_charset_converter_init (GCharsetConverter *local)
{
}


/**
 * g_charset_converter_new:
 * @to_charset: destination charset
 * @from_charset: source charset
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Creates a new #GCharsetConverter.
 *
 * Returns: a new #GCharsetConverter or %NULL on error.
 *
 * Since: 2.24
 **/
GCharsetConverter *
g_charset_converter_new (const gchar *to_charset,
			 const gchar *from_charset,
			 GError      **error)
{
  GCharsetConverter *conv;

  conv = g_initable_new (G_TYPE_CHARSET_CONVERTER,
			 NULL, error,
			 "to-charset", to_charset,
			 "from-charset", from_charset,
			 NULL);

  return conv;
}

static void
g_charset_converter_reset (GConverter *converter)
{
  GCharsetConverter *conv = G_CHARSET_CONVERTER (converter);

  if (conv->iconv == NULL)
    {
      g_warning ("Invalid object, not initialized");
      return;
    }

  g_iconv (conv->iconv, NULL, NULL, NULL, NULL);
  conv->n_fallback_errors = 0;
}

static GConverterResult
g_charset_converter_convert (GConverter       *converter,
			     const void       *inbuf,
			     gsize             inbuf_size,
			     void             *outbuf,
			     gsize             outbuf_size,
			     GConverterFlags   flags,
			     gsize            *bytes_read,
			     gsize            *bytes_written,
			     GError          **error)
{
  GCharsetConverter  *conv;
  gsize res;
  GConverterResult ret;
  gchar *inbufp, *outbufp;
  gsize in_left, out_left;
  int errsv;
  gboolean reset;

  conv = G_CHARSET_CONVERTER (converter);

  if (conv->iconv == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
			   _("Invalid object, not initialized"));
      return G_CONVERTER_ERROR;
    }

  inbufp = (char *)inbuf;
  outbufp = (char *)outbuf;
  in_left = inbuf_size;
  out_left = outbuf_size;
  reset = FALSE;

  /* if there is not input try to flush the data */
  if (inbuf_size == 0)
    {
      if (flags & G_CONVERTER_INPUT_AT_END ||
          flags & G_CONVERTER_FLUSH)
        {
          reset = TRUE;
        }
      else
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT,
                               _("Incomplete multibyte sequence in input"));
          return G_CONVERTER_ERROR;
        }
    }

  if (reset)
    /* call g_iconv with NULL inbuf to cleanup shift state */
    res = g_iconv (conv->iconv,
                   NULL, &in_left,
                   &outbufp, &out_left);
  else
    res = g_iconv (conv->iconv,
                   &inbufp, &in_left,
                   &outbufp, &out_left);

  *bytes_read = inbufp - (char *)inbuf;
  *bytes_written = outbufp - (char *)outbuf;

  /* Don't report error if we converted anything */
  if (res == (gsize) -1 && *bytes_read == 0)
    {
      errsv = errno;

      switch (errsv)
	{
	case EINVAL:
	  /* Incomplete input text */
	  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT,
			       _("Incomplete multibyte sequence in input"));
	  break;

	case E2BIG:
	  /* Not enough destination space */
	  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
			       _("Not enough space in destination"));
	  break;

	case EILSEQ:
	  /* Invalid code sequence */
	  if (conv->use_fallback)
	    {
	      if (outbuf_size < 3)
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
				     _("Not enough space in destination"));
	      else
		{
		  const char hex[] = "0123456789ABCDEF";
		  guint8 v = *(guint8 *)inbuf;
		  guint8 *out = (guint8 *)outbuf;
		  out[0] = '\\';
		  out[1] = hex[(v & 0xf0) >> 4];
		  out[2] = hex[(v & 0x0f) >> 0];
		  *bytes_read = 1;
		  *bytes_written = 3;
		  in_left--;
		  conv->n_fallback_errors++;
		  goto ok;
		}
	    }
	  else
	    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
				 _("Invalid byte sequence in conversion input"));
	  break;

	default:
	  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		       _("Error during conversion: %s"),
		       g_strerror (errsv));
	  break;
	}
      ret = G_CONVERTER_ERROR;
    }
  else
    {
    ok:
      ret = G_CONVERTER_CONVERTED;

      if (reset &&
	  (flags & G_CONVERTER_INPUT_AT_END))
        ret = G_CONVERTER_FINISHED;
      else if (reset &&
	       (flags & G_CONVERTER_FLUSH))
        ret = G_CONVERTER_FLUSHED;
    }

  return ret;
}

/**
 * g_charset_converter_set_use_fallback:
 * @converter: a #GCharsetConverter
 * @use_fallback: %TRUE to use fallbacks
 *
 * Sets the #GCharsetConverter:use-fallback property.
 *
 * Since: 2.24
 */
void
g_charset_converter_set_use_fallback (GCharsetConverter *converter,
				      gboolean           use_fallback)
{
  use_fallback = !!use_fallback;

  if (converter->use_fallback != use_fallback)
    {
      converter->use_fallback = use_fallback;
      g_object_notify (G_OBJECT (converter), "use-fallback");
    }
}

/**
 * g_charset_converter_get_use_fallback:
 * @converter: a #GCharsetConverter
 *
 * Gets the #GCharsetConverter:use-fallback property.
 *
 * Returns: %TRUE if fallbacks are used by @converter
 *
 * Since: 2.24
 */
gboolean
g_charset_converter_get_use_fallback (GCharsetConverter *converter)
{
  return converter->use_fallback;
}

/**
 * g_charset_converter_get_num_fallbacks:
 * @converter: a #GCharsetConverter
 *
 * Gets the number of fallbacks that @converter has applied so far.
 *
 * Returns: the number of fallbacks that @converter has applied
 *
 * Since: 2.24
 */
guint
g_charset_converter_get_num_fallbacks (GCharsetConverter *converter)
{
  return converter->n_fallback_errors;
}

static void
g_charset_converter_iface_init (GConverterIface *iface)
{
  iface->convert = g_charset_converter_convert;
  iface->reset = g_charset_converter_reset;
}

static gboolean
g_charset_converter_initable_init (GInitable     *initable,
				   GCancellable  *cancellable,
				   GError       **error)
{
  GCharsetConverter  *conv;
  int errsv;

  g_return_val_if_fail (G_IS_CHARSET_CONVERTER (initable), FALSE);

  conv = G_CHARSET_CONVERTER (initable);

  if (cancellable != NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			   _("Cancellable initialization not supported"));
      return FALSE;
    }

  conv->iconv = g_iconv_open (conv->to, conv->from);
  errsv = errno;

  if (conv->iconv == (GIConv)-1)
    {
      if (errsv == EINVAL)
	g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
		     _("Conversion from character set “%s” to “%s” is not supported"),
		     conv->from, conv->to);
      else
	g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		     _("Could not open converter from “%s” to “%s”"),
		     conv->from, conv->to);
      return FALSE;
    }

  return TRUE;
}

static void
g_charset_converter_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_charset_converter_initable_init;
}
