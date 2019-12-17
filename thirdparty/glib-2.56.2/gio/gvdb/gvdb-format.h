/*
 * Copyright © 2010 Codethink Limited
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
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __gvdb_format_h__
#define __gvdb_format_h__

#include <glib.h>

typedef struct { guint16 value; } guint16_le;
typedef struct { guint32 value; } guint32_le;

struct gvdb_pointer {
  guint32_le start;
  guint32_le end;
};

struct gvdb_hash_header {
  guint32_le n_bloom_words;
  guint32_le n_buckets;
};

struct gvdb_hash_item {
  guint32_le hash_value;
  guint32_le parent;

  guint32_le key_start;
  guint16_le key_size;
  gchar type;
  gchar unused;

  union
  {
    struct gvdb_pointer pointer;
    gchar direct[8];
  } value;
};

struct gvdb_header {
  guint32 signature[2];
  guint32_le version;
  guint32_le options;

  struct gvdb_pointer root;
};

static inline guint32_le guint32_to_le (guint32 value) {
  guint32_le result = { GUINT32_TO_LE (value) };
  return result;
}

static inline guint32 guint32_from_le (guint32_le value) {
  return GUINT32_FROM_LE (value.value);
}

static inline guint16_le guint16_to_le (guint16 value) {
  guint16_le result = { GUINT16_TO_LE (value) };
  return result;
}

static inline guint16 guint16_from_le (guint16_le value) {
  return GUINT16_FROM_LE (value.value);
}

#define GVDB_SIGNATURE0 1918981703
#define GVDB_SIGNATURE1 1953390953
#define GVDB_SWAPPED_SIGNATURE0 GUINT32_SWAP_LE_BE (GVDB_SIGNATURE0)
#define GVDB_SWAPPED_SIGNATURE1 GUINT32_SWAP_LE_BE (GVDB_SIGNATURE1)

#endif /* __gvdb_format_h__ */
