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
#include "gconverter.h"
#include "glibintl.h"


/**
 * SECTION:gconverter
 * @short_description: Data conversion interface
 * @include: gio/gio.h
 * @see_also: #GInputStream, #GOutputStream
 *
 * #GConverter is implemented by objects that convert
 * binary data in various ways. The conversion can be
 * stateful and may fail at any place.
 *
 * Some example conversions are: character set conversion,
 * compression, decompression and regular expression
 * replace.
 *
 * Since: 2.24
 **/


typedef GConverterIface GConverterInterface;
G_DEFINE_INTERFACE (GConverter, g_converter, G_TYPE_OBJECT)

static void
g_converter_default_init (GConverterInterface *iface)
{
}

/**
 * g_converter_convert:
 * @converter: a #GConverter.
 * @inbuf: (array length=inbuf_size) (element-type guint8): the buffer
 *         containing the data to convert.
 * @inbuf_size: the number of bytes in @inbuf
 * @outbuf: (element-type guint8) (array length=outbuf_size): a buffer to write
 *    converted data in.
 * @outbuf_size: the number of bytes in @outbuf, must be at least one
 * @flags: a #GConverterFlags controlling the conversion details
 * @bytes_read: (out): will be set to the number of bytes read from @inbuf on success
 * @bytes_written: (out): will be set to the number of bytes written to @outbuf on success
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * This is the main operation used when converting data. It is to be called
 * multiple times in a loop, and each time it will do some work, i.e.
 * producing some output (in @outbuf) or consuming some input (from @inbuf) or
 * both. If its not possible to do any work an error is returned.
 *
 * Note that a single call may not consume all input (or any input at all).
 * Also a call may produce output even if given no input, due to state stored
 * in the converter producing output.
 *
 * If any data was either produced or consumed, and then an error happens, then
 * only the successful conversion is reported and the error is returned on the
 * next call.
 *
 * A full conversion loop involves calling this method repeatedly, each time
 * giving it new input and space output space. When there is no more input
 * data after the data in @inbuf, the flag %G_CONVERTER_INPUT_AT_END must be set.
 * The loop will be (unless some error happens) returning %G_CONVERTER_CONVERTED
 * each time until all data is consumed and all output is produced, then
 * %G_CONVERTER_FINISHED is returned instead. Note, that %G_CONVERTER_FINISHED
 * may be returned even if %G_CONVERTER_INPUT_AT_END is not set, for instance
 * in a decompression converter where the end of data is detectable from the
 * data (and there might even be other data after the end of the compressed data).
 *
 * When some data has successfully been converted @bytes_read and is set to
 * the number of bytes read from @inbuf, and @bytes_written is set to indicate
 * how many bytes was written to @outbuf. If there are more data to output
 * or consume (i.e. unless the %G_CONVERTER_INPUT_AT_END is specified) then
 * %G_CONVERTER_CONVERTED is returned, and if no more data is to be output
 * then %G_CONVERTER_FINISHED is returned.
 *
 * On error %G_CONVERTER_ERROR is returned and @error is set accordingly.
 * Some errors need special handling:
 *
 * %G_IO_ERROR_NO_SPACE is returned if there is not enough space
 * to write the resulting converted data, the application should
 * call the function again with a larger @outbuf to continue.
 *
 * %G_IO_ERROR_PARTIAL_INPUT is returned if there is not enough
 * input to fully determine what the conversion should produce,
 * and the %G_CONVERTER_INPUT_AT_END flag is not set. This happens for
 * example with an incomplete multibyte sequence when converting text,
 * or when a regexp matches up to the end of the input (and may match
 * further input). It may also happen when @inbuf_size is zero and
 * there is no more data to produce.
 *
 * When this happens the application should read more input and then
 * call the function again. If further input shows that there is no
 * more data call the function again with the same data but with
 * the %G_CONVERTER_INPUT_AT_END flag set. This may cause the conversion
 * to finish as e.g. in the regexp match case (or, to fail again with
 * %G_IO_ERROR_PARTIAL_INPUT in e.g. a charset conversion where the
 * input is actually partial).
 *
 * After g_converter_convert() has returned %G_CONVERTER_FINISHED the
 * converter object is in an invalid state where its not allowed
 * to call g_converter_convert() anymore. At this time you can only
 * free the object or call g_converter_reset() to reset it to the
 * initial state.
 *
 * If the flag %G_CONVERTER_FLUSH is set then conversion is modified
 * to try to write out all internal state to the output. The application
 * has to call the function multiple times with the flag set, and when
 * the available input has been consumed and all internal state has
 * been produced then %G_CONVERTER_FLUSHED (or %G_CONVERTER_FINISHED if
 * really at the end) is returned instead of %G_CONVERTER_CONVERTED.
 * This is somewhat similar to what happens at the end of the input stream,
 * but done in the middle of the data.
 *
 * This has different meanings for different conversions. For instance
 * in a compression converter it would mean that we flush all the
 * compression state into output such that if you uncompress the
 * compressed data you get back all the input data. Doing this may
 * make the final file larger due to padding though. Another example
 * is a regexp conversion, where if you at the end of the flushed data
 * have a match, but there is also a potential longer match. In the
 * non-flushed case we would ask for more input, but when flushing we
 * treat this as the end of input and do the match.
 *
 * Flushing is not always possible (like if a charset converter flushes
 * at a partial multibyte sequence). Converters are supposed to try
 * to produce as much output as possible and then return an error
 * (typically %G_IO_ERROR_PARTIAL_INPUT).
 *
 * Returns: a #GConverterResult, %G_CONVERTER_ERROR on error.
 *
 * Since: 2.24
 **/
GConverterResult
g_converter_convert (GConverter *converter,
		     const void *inbuf,
		     gsize       inbuf_size,
		     void       *outbuf,
		     gsize       outbuf_size,
		     GConverterFlags flags,
		     gsize      *bytes_read,
		     gsize      *bytes_written,
		     GError    **error)
{
  GConverterIface *iface;

  g_return_val_if_fail (G_IS_CONVERTER (converter), G_CONVERTER_ERROR);
  g_return_val_if_fail (outbuf_size > 0, G_CONVERTER_ERROR);

  *bytes_read = 0;
  *bytes_written = 0;

  iface = G_CONVERTER_GET_IFACE (converter);

  return (* iface->convert) (converter,
			     inbuf, inbuf_size,
			     outbuf, outbuf_size,
			     flags,
			     bytes_read, bytes_written, error);
}

/**
 * g_converter_reset:
 * @converter: a #GConverter.
 *
 * Resets all internal state in the converter, making it behave
 * as if it was just created. If the converter has any internal
 * state that would produce output then that output is lost.
 *
 * Since: 2.24
 **/
void
g_converter_reset (GConverter *converter)
{
  GConverterIface *iface;

  g_return_if_fail (G_IS_CONVERTER (converter));

  iface = G_CONVERTER_GET_IFACE (converter);

  (* iface->reset) (converter);
}
