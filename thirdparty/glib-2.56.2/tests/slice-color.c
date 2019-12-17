/* GLIB sliced memory - fast threaded memory chunk allocator
 * Copyright (C) 2005 Tim Janik
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
#include <glib.h>
#include <string.h>

#define ALIGN(size, base)       ((base) * (gsize) (((size) + (base) - 1) / (base)))

static gdouble parse_memsize (const gchar *cstring);
static void    usage         (void);

static void
fill_memory (guint **mem,
             guint   n,
             guint   val)
{
  guint j, o = 0;
  for (j = 0; j < n; j++)
    mem[j][o] = val;
}

static guint64
access_memory3 (guint  **mema,
                guint  **memb,
                guint  **memd,
                guint    n,
                guint64  repeats)
{
  guint64 accu = 0, i, j;
  const guint o = 0;
  for (i = 0; i < repeats; i++)
    {
      for (j = 1; j < n; j += 2)
        memd[j][o] = mema[j][o] + memb[j][o];
    }
  for (i = 0; i < repeats; i++)
    for (j = 0; j < n; j++)
      accu += memd[j][o];
  return accu;
}

static void
touch_mem (guint64 block_size,
           guint64 n_blocks,
           guint64 repeats)
{
  guint64 j, accu, n = n_blocks;
  GTimer *timer;
  guint **memc;
  guint **memb;
  guint **mema = g_new (guint*, n);
  for (j = 0; j < n; j++)
    mema[j] = g_slice_alloc (block_size);
  memb = g_new (guint*, n);
  for (j = 0; j < n; j++)
    memb[j] = g_slice_alloc (block_size);
  memc = g_new (guint*, n);
  for (j = 0; j < n; j++)
    memc[j] = g_slice_alloc (block_size);

  timer = g_timer_new();
  fill_memory (mema, n, 2);
  fill_memory (memb, n, 3);
  fill_memory (memc, n, 4);
  access_memory3 (mema, memb, memc, n, 3);
  g_timer_start (timer);
  accu = access_memory3 (mema, memb, memc, n, repeats);
  g_timer_stop (timer);

  g_print ("Access-time = %fs\n", g_timer_elapsed (timer, NULL));
  g_assert (accu / repeats == (2 + 3) * n / 2 + 4 * n / 2);

  for (j = 0; j < n; j++)
    {
      g_slice_free1 (block_size, mema[j]);
      g_slice_free1 (block_size, memb[j]);
      g_slice_free1 (block_size, memc[j]);
    }
  g_timer_destroy (timer);
  g_free (mema);
  g_free (memb);
  g_free (memc);
}

static void
usage (void)
{
  g_print ("Usage: slice-color <block-size> [memory-size] [repeats] [colorization]\n");
}

int
main (int   argc,
      char *argv[])
{
  guint64 block_size = 512, area_size = 1024 * 1024, n_blocks, repeats = 1000000;

  if (argc > 1)
    block_size = parse_memsize (argv[1]);
  else
    {
      usage();
      block_size = 512;
    }
  if (argc > 2)
    area_size = parse_memsize (argv[2]);
  if (argc > 3)
    repeats = parse_memsize (argv[3]);
  if (argc > 4)
    g_slice_set_config (G_SLICE_CONFIG_COLOR_INCREMENT, parse_memsize (argv[4]));

  /* figure number of blocks from block and area size.
   * divide area by 3 because touch_mem() allocates 3 areas
   */
  n_blocks = area_size / 3 / ALIGN (block_size, sizeof (gsize) * 2);

  /* basic sanity checks */
  if (!block_size || !n_blocks || block_size >= area_size)
    {
      g_printerr ("Invalid arguments: block-size=%" G_GUINT64_FORMAT " memory-size=%" G_GUINT64_FORMAT "\n", block_size, area_size);
      usage();
      return 1;
    }

  g_printerr ("Will allocate and touch %" G_GUINT64_FORMAT " blocks of %" G_GUINT64_FORMAT " bytes (= %" G_GUINT64_FORMAT " bytes) %" G_GUINT64_FORMAT " times with color increment: 0x%08" G_GINT64_MODIFIER "x\n",
              n_blocks, block_size, n_blocks * block_size, repeats,
	      (guint64)g_slice_get_config (G_SLICE_CONFIG_COLOR_INCREMENT));

  touch_mem (block_size, n_blocks, repeats);
  
  return 0;
}

static gdouble
parse_memsize (const gchar *cstring)
{
  gchar *mem = g_strdup (cstring);
  gchar *string = g_strstrip (mem);
  guint l = strlen (string);
  gdouble f = 0;
  gchar *derr = NULL;
  gdouble msize;

  switch (l ? string[l - 1] : 0)
    {
    case 'k':   f = 1000;               break;
    case 'K':   f = 1024;               break;
    case 'm':   f = 1000000;            break;
    case 'M':   f = 1024 * 1024;        break;
    case 'g':   f = 1000000000;         break;
    case 'G':   f = 1024 * 1024 * 1024; break;
    }
  if (f)
    string[l - 1] = 0;
  msize = g_ascii_strtod (string, &derr);
  g_free (mem);
  if (derr && *derr)
    {
      g_printerr ("failed to parse number at: %s\n", derr);
      msize = 0;
    }
  if (f)
    msize *= f;
  return msize;
}
