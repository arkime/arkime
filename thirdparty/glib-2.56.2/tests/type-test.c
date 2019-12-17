/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <stdio.h>
#include <string.h>
#include "glib.h"



int
main (int   argc,
      char *argv[])
{
  gchar *string;
  gushort gus;
  guint gui;
  gulong gul;
  gsize gsz;
  gshort gs;
  gint gi;
  glong gl;
  gint16 gi16t1;
  gint16 gi16t2;
  gint32 gi32t1;
  gint32 gi32t2;
  guint16 gu16t1 = 0x44afU, gu16t2 = 0xaf44U;
  guint32 gu32t1 = 0x02a7f109U, gu32t2 = 0x09f1a702U;
  guint64 gu64t1 = G_GINT64_CONSTANT(0x1d636b02300a7aa7U),
	  gu64t2 = G_GINT64_CONSTANT(0xa77a0a30026b631dU);
  gint64 gi64t1;
  gint64 gi64t2;
  gssize gsst1;
  gssize gsst2;
  gsize  gst1;
  gsize  gst2;

  /* type sizes */
  g_assert (sizeof (gint8) == 1);
  g_assert (sizeof (gint16) == 2);
  g_assert (sizeof (gint32) == 4);
  g_assert (sizeof (gint64) == 8);

  g_assert (GUINT16_SWAP_LE_BE (gu16t1) == gu16t2);
  g_assert (GUINT32_SWAP_LE_BE (gu32t1) == gu32t2);
  g_assert (GUINT64_SWAP_LE_BE (gu64t1) == gu64t2);

  /* Test the G_(MIN|MAX|MAXU)(SHORT|INT|LONG) macros */

  gus = G_MAXUSHORT;
  gus++;
  g_assert (gus == 0);

  gui = G_MAXUINT;
  gui++;
  g_assert (gui == 0);

  gul = G_MAXULONG;
  gul++;
  g_assert (gul == 0);

  gsz = G_MAXSIZE;
  gsz++;
  
  g_assert (gsz == 0);

  gs = G_MAXSHORT;
  gs = (gshort) (1 + (gushort) gs);
  g_assert (gs == G_MINSHORT);

  gi = G_MAXINT;
  gi = (gint) (1 + (guint) gi);
  g_assert (gi == G_MININT);

  gl = G_MAXLONG;
  gl = (glong) (1 + (gulong) gl);
  g_assert (gl == G_MINLONG);

  /* Test the G_G(U)?INT(16|32|64)_FORMAT macros */

  gi16t1 = -0x3AFA;
  gu16t1 = 0xFAFA;
  gi32t1 = -0x3AFAFAFA;
  gu32t1 = 0xFAFAFAFA; 

#define FORMAT "%" G_GINT16_FORMAT " %" G_GINT32_FORMAT \
               " %" G_GUINT16_FORMAT " %" G_GUINT32_FORMAT "\n"
  string = g_strdup_printf (FORMAT, gi16t1, gi32t1, gu16t1, gu32t1);
  sscanf (string, FORMAT, &gi16t2, &gi32t2, &gu16t2, &gu32t2);
  g_free (string);
  g_assert (gi16t1 == gi16t2);
  g_assert (gi32t1 == gi32t2);
  g_assert (gu16t1 == gu16t2);
  g_assert (gu32t1 == gu32t2);

  gi64t1 = G_GINT64_CONSTANT (-0x3AFAFAFAFAFAFAFA);
  gu64t1 = G_GINT64_CONSTANT (0xFAFAFAFAFAFAFAFA); 

#define FORMAT64 "%" G_GINT64_FORMAT " %" G_GUINT64_FORMAT "\n"
  string = g_strdup_printf (FORMAT64, gi64t1, gu64t1);
  sscanf (string, FORMAT64, &gi64t2, &gu64t2);
  g_free (string);
  g_assert (gi64t1 == gi64t2);
  g_assert (gu64t1 == gu64t2);

  gsst1 = -0x3AFAFAFA;
  gst1 = 0xFAFAFAFA; 

#define FORMATSIZE "%" G_GSSIZE_FORMAT " %" G_GSIZE_FORMAT "\n"
  string = g_strdup_printf (FORMATSIZE, gsst1, gst1);
  sscanf (string, FORMATSIZE, &gsst2, &gst2);
  g_free (string);
  g_assert (gsst1 == gsst2);
  g_assert (gst1 == gst2);
  
  return 0;
}
