/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998 Tim Janik
 *
 * gquark.c: Functions for dealing with quarks and interned strings
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

/*
 * MT safe
 */

#include "config.h"

#include <string.h>

#include "gslice.h"
#include "ghash.h"
#include "gquark.h"
#include "gstrfuncs.h"
#include "gthread.h"
#include "gtestutils.h"
#include "glib_trace.h"
#include "glib-init.h"

#define QUARK_BLOCK_SIZE         2048
#define QUARK_STRING_BLOCK_SIZE (4096 - sizeof (gsize))

static inline GQuark  quark_new (gchar *string);

G_LOCK_DEFINE_STATIC (quark_global);
static GHashTable    *quark_ht = NULL;
static gchar        **quarks = NULL;
static gint           quark_seq_id = 0;
static gchar         *quark_block = NULL;
static gint           quark_block_offset = 0;

void
g_quark_init (void)
{
  g_assert (quark_seq_id == 0);
  quark_ht = g_hash_table_new (g_str_hash, g_str_equal);
  quarks = g_new (gchar*, QUARK_BLOCK_SIZE);
  quarks[0] = NULL;
  quark_seq_id = 1;
}

/**
 * SECTION:quarks
 * @title: Quarks
 * @short_description: a 2-way association between a string and a
 *     unique integer identifier
 *
 * Quarks are associations between strings and integer identifiers.
 * Given either the string or the #GQuark identifier it is possible to
 * retrieve the other.
 *
 * Quarks are used for both [datasets][glib-Datasets] and
 * [keyed data lists][glib-Keyed-Data-Lists].
 *
 * To create a new quark from a string, use g_quark_from_string() or
 * g_quark_from_static_string().
 *
 * To find the string corresponding to a given #GQuark, use
 * g_quark_to_string().
 *
 * To find the #GQuark corresponding to a given string, use
 * g_quark_try_string().
 *
 * Another use for the string pool maintained for the quark functions
 * is string interning, using g_intern_string() or
 * g_intern_static_string(). An interned string is a canonical
 * representation for a string. One important advantage of interned
 * strings is that they can be compared for equality by a simple
 * pointer comparison, rather than using strcmp().
 */

/**
 * GQuark:
 *
 * A GQuark is a non-zero integer which uniquely identifies a
 * particular string. A GQuark value of zero is associated to %NULL.
 */

/**
 * G_DEFINE_QUARK:
 * @QN: the name to return a #GQuark for
 * @q_n: prefix for the function name
 *
 * A convenience macro which defines a function returning the
 * #GQuark for the name @QN. The function will be named
 * @q_n_quark().
 *
 * Note that the quark name will be stringified automatically
 * in the macro, so you shouldn't use double quotes.
 *
 * Since: 2.34
 */

/**
 * g_quark_try_string:
 * @string: (nullable): a string
 *
 * Gets the #GQuark associated with the given string, or 0 if string is
 * %NULL or it has no associated #GQuark.
 *
 * If you want the GQuark to be created if it doesn't already exist,
 * use g_quark_from_string() or g_quark_from_static_string().
 *
 * Returns: the #GQuark associated with the string, or 0 if @string is
 *     %NULL or there is no #GQuark associated with it
 */
GQuark
g_quark_try_string (const gchar *string)
{
  GQuark quark = 0;

  if (string == NULL)
    return 0;

  G_LOCK (quark_global);
  quark = GPOINTER_TO_UINT (g_hash_table_lookup (quark_ht, string));
  G_UNLOCK (quark_global);

  return quark;
}

/* HOLDS: quark_global_lock */
static char *
quark_strdup (const gchar *string)
{
  gchar *copy;
  gsize len;

  len = strlen (string) + 1;

  /* For strings longer than half the block size, fall back
     to strdup so that we fill our blocks at least 50%. */
  if (len > QUARK_STRING_BLOCK_SIZE / 2)
    return g_strdup (string);

  if (quark_block == NULL ||
      QUARK_STRING_BLOCK_SIZE - quark_block_offset < len)
    {
      quark_block = g_malloc (QUARK_STRING_BLOCK_SIZE);
      quark_block_offset = 0;
    }

  copy = quark_block + quark_block_offset;
  memcpy (copy, string, len);
  quark_block_offset += len;

  return copy;
}

/* HOLDS: quark_global_lock */
static inline GQuark
quark_from_string (const gchar *string,
                   gboolean     duplicate)
{
  GQuark quark = 0;

  quark = GPOINTER_TO_UINT (g_hash_table_lookup (quark_ht, string));

  if (!quark)
    {
      quark = quark_new (duplicate ? quark_strdup (string) : (gchar *)string);
      TRACE(GLIB_QUARK_NEW(string, quark));
    }

  return quark;
}

static inline GQuark
quark_from_string_locked (const gchar   *string,
                          gboolean       duplicate)
{
  GQuark quark = 0;

  if (!string)
    return 0;

  G_LOCK (quark_global);
  quark = quark_from_string (string, duplicate);
  G_UNLOCK (quark_global);

  return quark;
}

/**
 * g_quark_from_string:
 * @string: (nullable): a string
 *
 * Gets the #GQuark identifying the given string. If the string does
 * not currently have an associated #GQuark, a new #GQuark is created,
 * using a copy of the string.
 *
 * Returns: the #GQuark identifying the string, or 0 if @string is %NULL
 */
GQuark
g_quark_from_string (const gchar *string)
{
  return quark_from_string_locked (string, TRUE);
}

/**
 * g_quark_from_static_string:
 * @string: (nullable): a string
 *
 * Gets the #GQuark identifying the given (static) string. If the
 * string does not currently have an associated #GQuark, a new #GQuark
 * is created, linked to the given string.
 *
 * Note that this function is identical to g_quark_from_string() except
 * that if a new #GQuark is created the string itself is used rather
 * than a copy. This saves memory, but can only be used if the string
 * will continue to exist until the program terminates. It can be used
 * with statically allocated strings in the main program, but not with
 * statically allocated memory in dynamically loaded modules, if you
 * expect to ever unload the module again (e.g. do not use this
 * function in GTK+ theme engines).
 *
 * Returns: the #GQuark identifying the string, or 0 if @string is %NULL
 */
GQuark
g_quark_from_static_string (const gchar *string)
{
  return quark_from_string_locked (string, FALSE);
}

/**
 * g_quark_to_string:
 * @quark: a #GQuark.
 *
 * Gets the string associated with the given #GQuark.
 *
 * Returns: the string associated with the #GQuark
 */
const gchar *
g_quark_to_string (GQuark quark)
{
  gchar* result = NULL;
  gchar **strings;
  gint seq_id;

  seq_id = g_atomic_int_get (&quark_seq_id);
  strings = g_atomic_pointer_get (&quarks);

  if (quark < seq_id)
    result = strings[quark];

  return result;
}

/* HOLDS: g_quark_global_lock */
static inline GQuark
quark_new (gchar *string)
{
  GQuark quark;
  gchar **quarks_new;

  if (quark_seq_id % QUARK_BLOCK_SIZE == 0)
    {
      quarks_new = g_new (gchar*, quark_seq_id + QUARK_BLOCK_SIZE);
      if (quark_seq_id != 0)
        memcpy (quarks_new, quarks, sizeof (char *) * quark_seq_id);
      memset (quarks_new + quark_seq_id, 0, sizeof (char *) * QUARK_BLOCK_SIZE);
      /* This leaks the old quarks array. Its unfortunate, but it allows
       * us to do lockless lookup of the arrays, and there shouldn't be that
       * many quarks in an app
       */
      g_atomic_pointer_set (&quarks, quarks_new);
    }

  quark = quark_seq_id;
  g_atomic_pointer_set (&quarks[quark], string);
  g_hash_table_insert (quark_ht, string, GUINT_TO_POINTER (quark));
  g_atomic_int_inc (&quark_seq_id);

  return quark;
}

static inline const gchar *
quark_intern_string_locked (const gchar   *string,
                            gboolean       duplicate)
{
  const gchar *result;
  GQuark quark;

  if (!string)
    return NULL;

  G_LOCK (quark_global);
  quark = quark_from_string (string, duplicate);
  result = quarks[quark];
  G_UNLOCK (quark_global);

  return result;
}

/**
 * g_intern_string:
 * @string: (nullable): a string
 *
 * Returns a canonical representation for @string. Interned strings
 * can be compared for equality by comparing the pointers, instead of
 * using strcmp().
 *
 * Returns: a canonical representation for the string
 *
 * Since: 2.10
 */
const gchar *
g_intern_string (const gchar *string)
{
  return quark_intern_string_locked (string, TRUE);
}

/**
 * g_intern_static_string:
 * @string: (nullable): a static string
 *
 * Returns a canonical representation for @string. Interned strings
 * can be compared for equality by comparing the pointers, instead of
 * using strcmp(). g_intern_static_string() does not copy the string,
 * therefore @string must not be freed or modified.
 *
 * Returns: a canonical representation for the string
 *
 * Since: 2.10
 */
const gchar *
g_intern_static_string (const gchar *string)
{
  return quark_intern_string_locked (string, FALSE);
}
