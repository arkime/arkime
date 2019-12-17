#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024

static void
test_small_writes (void)
{
  GIOChannel *io;
  GIOStatus status = G_IO_STATUS_ERROR;
  guint cnt; 
  gchar tmp;
  GError *error = NULL;

  io = g_io_channel_new_file ("iochannel-test-outfile", "w", &error);
  if (error)
    {
      g_warning ("Unable to open file %s: %s", 
		 "iochannel-test-outfile", 
		 error->message);
      g_clear_error (&error);
      
      exit (1);
    }

  g_io_channel_set_encoding (io, NULL, NULL);
  g_io_channel_set_buffer_size (io, 1022);

  cnt = 2 * g_io_channel_get_buffer_size (io);
  tmp = 0;
 
  while (cnt)
    {
      status = g_io_channel_write_chars (io, &tmp, 1, NULL, NULL);
      if (status == G_IO_STATUS_ERROR)
	break;
      if (status == G_IO_STATUS_NORMAL)
	cnt--;
    }

  g_assert (status == G_IO_STATUS_NORMAL);

  g_io_channel_unref (io);
}


gint main (gint argc, gchar * argv[])
{
    GIOChannel *gio_r, *gio_w ;
    GError *gerr = NULL;
    GString *buffer;
    char *filename;
    gint rlength = 0;
    glong wlength = 0;
    gsize length_out;
    const gchar encoding[] = "EUC-JP";
    GIOStatus status;

    g_test_init (&argc, &argv, NULL);

    filename = g_test_build_filename (G_TEST_DIST, "iochannel-test-infile", NULL);

    setbuf (stdout, NULL); /* For debugging */

    gio_r = g_io_channel_new_file (filename, "r", &gerr);
    if (gerr)
      {
        g_warning ("Unable to open file %s: %s", filename, gerr->message);
        g_clear_error (&gerr);
        return 1;
      }
    gio_w = g_io_channel_new_file ("iochannel-test-outfile", "w", &gerr);
    if (gerr)
      {
        g_warning ("Unable to open file %s: %s", "iochannel-test-outfile", gerr->message);
        g_clear_error (&gerr);
        return 1;
      }

    g_io_channel_set_encoding (gio_r, encoding, &gerr);
    if (gerr)
      {
        g_warning ("%s", gerr->message);
        /* Keep going if this is just a case of iconv not supporting EUC-JP, see bug 428048 */
        if (gerr->code != G_CONVERT_ERROR_NO_CONVERSION)
          return 1;
        g_clear_error (&gerr);
      }
    
    g_io_channel_set_buffer_size (gio_r, BUFFER_SIZE);

    status = g_io_channel_set_flags (gio_r, G_IO_FLAG_NONBLOCK, &gerr);
    if (status == G_IO_STATUS_ERROR)
      {
        g_warning ("%s", gerr->message);
        g_clear_error (&gerr);
      }
    buffer = g_string_sized_new (BUFFER_SIZE);

    while (TRUE)
    {
        do
          status = g_io_channel_read_line_string (gio_r, buffer, NULL, &gerr);
        while (status == G_IO_STATUS_AGAIN);
        if (status != G_IO_STATUS_NORMAL)
          break;

        rlength += buffer->len;

        do
          status = g_io_channel_write_chars (gio_w, buffer->str, buffer->len,
            &length_out, &gerr);
        while (status == G_IO_STATUS_AGAIN);
        if (status != G_IO_STATUS_NORMAL)
          break;

        wlength += length_out;

        if (length_out < buffer->len)
          g_warning ("Only wrote part of the line.");

#ifdef VERBOSE
        g_print ("%s", buffer->str);
#endif
        g_string_truncate (buffer, 0);
    }

    switch (status)
      {
        case G_IO_STATUS_EOF:
          break;
        case G_IO_STATUS_ERROR:
          g_warning ("%s", gerr->message);
          g_clear_error (&gerr);
          break;
        default:
          g_warning ("Abnormal exit from write loop.");
          break;
      }

    do
      status = g_io_channel_flush (gio_w, &gerr);
    while (status == G_IO_STATUS_AGAIN);

    if (status == G_IO_STATUS_ERROR)
      {
        g_warning ("%s", gerr->message);
        g_clear_error (&gerr);
      }

#ifdef VERBOSE
    g_print ("read %d bytes, wrote %ld bytes\n", rlength, wlength);
#endif

    g_io_channel_unref(gio_r);
    g_io_channel_unref(gio_w);

    test_small_writes ();

    g_free (filename);
    g_string_free (buffer, TRUE);

    return 0;
}
