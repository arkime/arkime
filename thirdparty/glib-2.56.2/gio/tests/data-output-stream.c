/* GLib testing framework examples and tests
 * Copyright (C) 2008 Red Hat, Inc.
 * Authors: Tomas Bzatek <tbzatek@redhat.com>
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <glib/glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES		0xFFF	
#define MAX_LINES_BUFF 		0xFFFFFF
#define MAX_BYTES_BINARY	0x100	

static void
test_basic (void)
{
  GOutputStream *stream;
  GOutputStream *base_stream;
  gpointer data;
  gint val;

  data = g_malloc0 (MAX_LINES_BUFF);
  
  /* initialize objects */
  base_stream = g_memory_output_stream_new (data, MAX_LINES_BUFF, NULL, NULL);
  stream = G_OUTPUT_STREAM (g_data_output_stream_new (base_stream));

  g_object_get (stream, "byte-order", &val, NULL);
  g_assert_cmpint (val, ==, G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN);
  g_object_set (stream, "byte-order", G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN, NULL);
  g_assert_cmpint (g_data_output_stream_get_byte_order (G_DATA_OUTPUT_STREAM (stream)), ==, G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

  g_object_unref (stream);
  g_object_unref (base_stream);
  g_free (data);
}

static void
test_read_lines (GDataStreamNewlineType newline_type)
{
  GOutputStream *stream;
  GOutputStream *base_stream;
  GError *error = NULL;
  gpointer data;
  char *lines;
  int size;
  int i;

#define TEST_STRING	"some_text"
  
  const char* endl[4] = {"\n", "\r", "\r\n", "\n"};
  
  
  data = g_malloc0 (MAX_LINES_BUFF);
  lines = g_malloc0 ((strlen (TEST_STRING) + strlen (endl[newline_type])) * MAX_LINES + 1);
  
  /* initialize objects */
  base_stream = g_memory_output_stream_new (data, MAX_LINES_BUFF, NULL, NULL);
  stream = G_OUTPUT_STREAM (g_data_output_stream_new (base_stream));

  
  /*  fill data */
  for (i = 0; i < MAX_LINES; i++)
    {
      gboolean res;
      char *s = g_strconcat (TEST_STRING, endl[newline_type], NULL);
      res = g_data_output_stream_put_string (G_DATA_OUTPUT_STREAM (stream), s, NULL, &error);
      g_stpcpy ((char*)(lines + i*strlen(s)), s);
      g_assert_no_error (error);
      g_assert (res == TRUE);
      g_free (s);
    }

  /*  Byte order testing */
  g_data_output_stream_set_byte_order (G_DATA_OUTPUT_STREAM (stream), G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN);
  g_assert_cmpint (g_data_output_stream_get_byte_order (G_DATA_OUTPUT_STREAM (stream)), ==, G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN);
  g_data_output_stream_set_byte_order (G_DATA_OUTPUT_STREAM (stream), G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);
  g_assert_cmpint (g_data_output_stream_get_byte_order (G_DATA_OUTPUT_STREAM (stream)), ==, G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);
  
  /*  compare data */
  size = strlen (data);
  g_assert_cmpint (size, <, MAX_LINES_BUFF);
  g_assert_cmpstr ((char*)data, ==, lines);
  
  g_object_unref (base_stream);
  g_object_unref (stream);
  g_free (data);
  g_free (lines);
}

static void
test_read_lines_LF (void)
{
  test_read_lines (G_DATA_STREAM_NEWLINE_TYPE_LF);
}

static void
test_read_lines_CR (void)
{
  test_read_lines (G_DATA_STREAM_NEWLINE_TYPE_CR);
}

static void
test_read_lines_CR_LF (void)
{
  test_read_lines (G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
}

enum TestDataType {
  TEST_DATA_BYTE = 0,
  TEST_DATA_INT16,
  TEST_DATA_UINT16,
  TEST_DATA_INT32,
  TEST_DATA_UINT32,
  TEST_DATA_INT64,
  TEST_DATA_UINT64
};

static void
test_data_array (guchar *buffer, gsize len,
		 enum TestDataType data_type, GDataStreamByteOrder byte_order)
{
  GOutputStream *stream;
  GOutputStream *base_stream;
  guchar *stream_data;
  
  GError *error = NULL;
  guint pos;
  GDataStreamByteOrder native;
  gboolean swap;
  gboolean res;
  
  /*  create objects */
  stream_data = g_malloc0 (len);
  base_stream = g_memory_output_stream_new (stream_data, len, NULL, NULL);
  stream = G_OUTPUT_STREAM (g_data_output_stream_new (base_stream));
  g_data_output_stream_set_byte_order (G_DATA_OUTPUT_STREAM (stream), byte_order);
  
  /*  Set flag to swap bytes if needed */
  native = (G_BYTE_ORDER == G_BIG_ENDIAN) ? G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN : G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN;
  swap = (byte_order != G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN) && (byte_order != native);

  /* set len to length of buffer cast to actual type */
  switch (data_type)
    {
    case TEST_DATA_BYTE:
      break;
    case TEST_DATA_INT16:
    case TEST_DATA_UINT16:
      g_assert_cmpint (len % 2, ==, 0);
    case TEST_DATA_INT32:
    case TEST_DATA_UINT32:
      g_assert_cmpint (len % 4, ==, 0);
    case TEST_DATA_INT64:
    case TEST_DATA_UINT64:
      g_assert_cmpint (len % 8, ==, 0);
      len /= 8;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  /*  Write data to the file */
  for (pos = 0; pos < len; pos++)
    {
      switch (data_type)
	{
	case TEST_DATA_BYTE:
	  res = g_data_output_stream_put_byte (G_DATA_OUTPUT_STREAM (stream), buffer[pos], NULL, &error);
	  break;
	case TEST_DATA_INT16:
	  res = g_data_output_stream_put_int16 (G_DATA_OUTPUT_STREAM (stream), ((gint16 *) buffer)[pos], NULL, &error);
	  break;
	case TEST_DATA_UINT16:
	  res = g_data_output_stream_put_uint16 (G_DATA_OUTPUT_STREAM (stream), ((guint16 *) buffer)[pos], NULL, &error);
	  break;
	case TEST_DATA_INT32:
	  res = g_data_output_stream_put_int32 (G_DATA_OUTPUT_STREAM (stream), ((gint32 *) buffer)[pos], NULL, &error);
	  break;
	case TEST_DATA_UINT32:
	  res = g_data_output_stream_put_uint32 (G_DATA_OUTPUT_STREAM (stream), ((guint32 *) buffer)[pos], NULL, &error);
	  break;
	case TEST_DATA_INT64:
	  res = g_data_output_stream_put_int64 (G_DATA_OUTPUT_STREAM (stream), ((gint64 *) buffer)[pos], NULL, &error);
	  break;
	case TEST_DATA_UINT64:
	  res = g_data_output_stream_put_uint64 (G_DATA_OUTPUT_STREAM (stream), ((guint64 *) buffer)[pos], NULL, &error);
	  break;
        default:
          g_assert_not_reached ();
          break;
	}
      g_assert_no_error (error);
      g_assert_cmpint (res, ==, TRUE);
    }
  
  /*  Compare data back */
  for (pos = 0; pos < len; pos++)
    {
      switch (data_type)
        {
        case TEST_DATA_BYTE:
          /* swapping unnecessary */
          g_assert_cmpint (buffer[pos], ==, stream_data[pos]);
          break;
        case TEST_DATA_UINT16:
          if (swap)
            g_assert_cmpint (GUINT16_SWAP_LE_BE (((guint16 *) buffer)[pos]), ==, ((guint16 *) stream_data)[pos]);
          else
            g_assert_cmpint (((guint16 *) buffer)[pos], ==, ((guint16 *) stream_data)[pos]);
          break;
        case TEST_DATA_INT16:
          if (swap)
            g_assert_cmpint ((gint16) GUINT16_SWAP_LE_BE (((gint16 *) buffer)[pos]), ==, ((gint16 *) stream_data)[pos]);
          else
            g_assert_cmpint (((gint16 *) buffer)[pos], ==, ((gint16 *) stream_data)[pos]);
          break;
        case TEST_DATA_UINT32:
          if (swap)
            g_assert_cmpint (GUINT32_SWAP_LE_BE (((guint32 *) buffer)[pos]), ==, ((guint32 *) stream_data)[pos]);
          else
            g_assert_cmpint (((guint32 *) buffer)[pos], ==, ((guint32 *) stream_data)[pos]);
          break;
        case TEST_DATA_INT32:
          if (swap)
            g_assert_cmpint ((gint32) GUINT32_SWAP_LE_BE (((gint32 *) buffer)[pos]), ==, ((gint32 *) stream_data)[pos]);
          else
            g_assert_cmpint (((gint32 *) buffer)[pos], ==, ((gint32 *) stream_data)[pos]);
          break;
        case TEST_DATA_UINT64:
          if (swap)
            g_assert_cmpint (GUINT64_SWAP_LE_BE (((guint64 *) buffer)[pos]), ==, ((guint64 *) stream_data)[pos]);
          else
            g_assert_cmpint (((guint64 *) buffer)[pos], ==, ((guint64 *) stream_data)[pos]);
          break;
        case TEST_DATA_INT64:
          if (swap)
            g_assert_cmpint ((gint64) GUINT64_SWAP_LE_BE (((gint64 *) buffer)[pos]), ==, ((gint64 *) stream_data)[pos]);
          else
            g_assert_cmpint (((gint64 *) buffer)[pos], ==, ((gint64 *) stream_data)[pos]);
          break;
        default:
            g_assert_not_reached ();
          break;
        }
    }
  
  g_object_unref (base_stream);
  g_object_unref (stream);
  g_free (stream_data);
}

static void
test_read_int (void)
{
  GRand *randomizer;
  gpointer buffer;
  int i;
  
  randomizer = g_rand_new ();
  buffer = g_malloc0(MAX_BYTES_BINARY);
  
  /*  Fill in some random data */
  for (i = 0; i < MAX_BYTES_BINARY; i++)
    {
      guchar x = 0;
      while (! x)  x = (guchar)g_rand_int (randomizer);
      *(guchar*)((guchar*)buffer + sizeof (guchar) * i) = x; 
    }

  for (i = 0; i < 3; i++)
    {
      int j;
      for (j = 0; j <= TEST_DATA_UINT64; j++)
	test_data_array (buffer, MAX_BYTES_BINARY, j, i);
    }
  
  g_rand_free (randomizer);
  g_free (buffer);
}

static void
test_seek (void)
{
  GDataOutputStream *stream;
  GMemoryOutputStream *base_stream;
  GSeekable *seekable;
  GError *error;
  guchar *stream_data;
  gsize len;
  gboolean res;

  len = 8;
  
  /*  create objects */
  stream_data = g_malloc0 (len);
  base_stream = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new (stream_data, len, NULL, NULL));
  stream = g_data_output_stream_new (G_OUTPUT_STREAM (base_stream));
  g_data_output_stream_set_byte_order (stream, G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN);
  seekable = G_SEEKABLE (stream);
  g_assert (!g_seekable_can_truncate (seekable));
  error = NULL;
  
  /* Write */
  g_assert_cmpint (g_seekable_tell (seekable), ==, 0);
  res = g_data_output_stream_put_uint16 (stream, 0x0123, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_data_output_stream_put_uint16 (stream, 0x4567, NULL, NULL);
  g_assert_cmpint (g_seekable_tell (seekable), ==, 4);
  g_assert_cmpint (stream_data[0], ==, 0x01);
  g_assert_cmpint (stream_data[1], ==, 0x23);
  g_assert_cmpint (stream_data[2], ==, 0x45);
  g_assert_cmpint (stream_data[3], ==, 0x67);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 4);

  /* Forward relative seek */
  res = g_seekable_seek (seekable, 2, G_SEEK_CUR, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_seekable_tell (seekable), ==, 6);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 4);
  res = g_data_output_stream_put_uint16 (stream, 0x89AB, NULL, &error);
  g_assert (res);
  g_assert_cmpint (g_seekable_tell (seekable), ==, 8);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 8);
  g_assert_cmpint (stream_data[0], ==, 0x01);
  g_assert_cmpint (stream_data[1], ==, 0x23);
  g_assert_cmpint (stream_data[2], ==, 0x45);
  g_assert_cmpint (stream_data[3], ==, 0x67);
  g_assert_cmpint (stream_data[4], ==, 0x00);
  g_assert_cmpint (stream_data[5], ==, 0x00);
  g_assert_cmpint (stream_data[6], ==, 0x89);
  g_assert_cmpint (stream_data[7], ==, 0xAB);

  /* Backward relative seek */
  res = g_seekable_seek (seekable, -3, G_SEEK_CUR, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_seekable_tell (seekable), ==, 5);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 8);
  res = g_data_output_stream_put_uint16 (stream, 0xCDEF, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_seekable_tell (seekable), ==, 7);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 8);
  g_assert_cmpint (stream_data[0], ==, 0x01);
  g_assert_cmpint (stream_data[1], ==, 0x23);
  g_assert_cmpint (stream_data[2], ==, 0x45);
  g_assert_cmpint (stream_data[3], ==, 0x67);
  g_assert_cmpint (stream_data[4], ==, 0x00);
  g_assert_cmpint (stream_data[5], ==, 0xCD);
  g_assert_cmpint (stream_data[6], ==, 0xEF);
  g_assert_cmpint (stream_data[7], ==, 0xAB);

  /* From start */
  res = g_seekable_seek (seekable, 4, G_SEEK_SET, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_seekable_tell (seekable), ==, 4);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 8);
  res = g_data_output_stream_put_uint16 (stream, 0xFEDC, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_seekable_tell (seekable), ==, 6);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 8);
  g_assert_cmpint (stream_data[0], ==, 0x01);
  g_assert_cmpint (stream_data[1], ==, 0x23);
  g_assert_cmpint (stream_data[2], ==, 0x45);
  g_assert_cmpint (stream_data[3], ==, 0x67);
  g_assert_cmpint (stream_data[4], ==, 0xFE);
  g_assert_cmpint (stream_data[5], ==, 0xDC);
  g_assert_cmpint (stream_data[6], ==, 0xEF);
  g_assert_cmpint (stream_data[7], ==, 0xAB);

  /* From end */
  res = g_seekable_seek (seekable, -4, G_SEEK_END, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_seekable_tell (seekable), ==, 4);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 8);
  res = g_data_output_stream_put_uint16 (stream, 0xBA87, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_seekable_tell (seekable), ==, 6);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 8);
  g_assert_cmpint (stream_data[0], ==, 0x01);
  g_assert_cmpint (stream_data[1], ==, 0x23);
  g_assert_cmpint (stream_data[2], ==, 0x45);
  g_assert_cmpint (stream_data[3], ==, 0x67);
  g_assert_cmpint (stream_data[4], ==, 0xBA);
  g_assert_cmpint (stream_data[5], ==, 0x87);
  g_assert_cmpint (stream_data[6], ==, 0xEF);
  g_assert_cmpint (stream_data[7], ==, 0xAB);

  g_object_unref (stream);
  g_object_unref (base_stream);
  g_free (stream_data);
}

static void
test_truncate (void)
{
  GDataOutputStream *stream;
  GMemoryOutputStream *base_stream;
  GSeekable *seekable;
  GError *error;
  guchar *stream_data;
  gsize len;
  gboolean res;

  len = 8;

  /* Create objects */
  stream_data = g_malloc0 (len);
  base_stream = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new (stream_data, len, g_realloc, g_free));
  stream = g_data_output_stream_new (G_OUTPUT_STREAM (base_stream));
  g_data_output_stream_set_byte_order (stream, G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN);
  seekable = G_SEEKABLE (stream);
  error = NULL;
  g_assert (g_seekable_can_truncate (seekable));
  
  /* Write */
  g_assert_cmpint (g_memory_output_stream_get_size (base_stream), ==, len);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 0);
  res = g_data_output_stream_put_uint16 (stream, 0x0123, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  res = g_data_output_stream_put_uint16 (stream, 0x4567, NULL, NULL);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_memory_output_stream_get_size (base_stream), ==, len);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 4);
  stream_data = g_memory_output_stream_get_data (base_stream);
  g_assert_cmpint (stream_data[0], ==, 0x01);
  g_assert_cmpint (stream_data[1], ==, 0x23);
  g_assert_cmpint (stream_data[2], ==, 0x45);
  g_assert_cmpint (stream_data[3], ==, 0x67);

  /* Truncate at position */
  res = g_seekable_truncate (seekable, 4, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_memory_output_stream_get_size (base_stream), ==, 4);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 4);
  stream_data = g_memory_output_stream_get_data (base_stream);
  g_assert_cmpint (stream_data[0], ==, 0x01);
  g_assert_cmpint (stream_data[1], ==, 0x23);
  g_assert_cmpint (stream_data[2], ==, 0x45);
  g_assert_cmpint (stream_data[3], ==, 0x67);

  /* Truncate beyond position */
  res = g_seekable_truncate (seekable, 6, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_memory_output_stream_get_size (base_stream), ==, 6);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 6);
  stream_data = g_memory_output_stream_get_data (base_stream);
  g_assert_cmpint (stream_data[0], ==, 0x01);
  g_assert_cmpint (stream_data[1], ==, 0x23);
  g_assert_cmpint (stream_data[2], ==, 0x45);
  g_assert_cmpint (stream_data[3], ==, 0x67);

  /* Truncate before position */
  res = g_seekable_truncate (seekable, 2, NULL, &error);
  g_assert_no_error (error);
  g_assert (res);
  g_assert_cmpint (g_memory_output_stream_get_size (base_stream), ==, 2);
  g_assert_cmpint (g_memory_output_stream_get_data_size (base_stream), ==, 2);
  stream_data = g_memory_output_stream_get_data (base_stream);
  g_assert_cmpint (stream_data[0], ==, 0x01);
  g_assert_cmpint (stream_data[1], ==, 0x23);

  g_object_unref (stream);
  g_object_unref (base_stream);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/data-output-stream/basic", test_basic);
  g_test_add_func ("/data-output-stream/write-lines-LF", test_read_lines_LF);
  g_test_add_func ("/data-output-stream/write-lines-CR", test_read_lines_CR);
  g_test_add_func ("/data-output-stream/write-lines-CR-LF", test_read_lines_CR_LF);
  g_test_add_func ("/data-output-stream/write-int", test_read_int);
  g_test_add_func ("/data-output-stream/seek", test_seek);
  g_test_add_func ("/data-output-stream/truncate", test_truncate);

  return g_test_run();
}
