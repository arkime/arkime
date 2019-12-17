/*
 * Copyright © 2015 Canonical Limited
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

#ifndef __G_CONTEXT_SPECIFIC_GROUP_H__
#define __G_CONTEXT_SPECIFIC_GROUP_H__

#include <glib-object.h>

typedef struct
{
  GHashTable *table;
  GMutex      lock;
  GCond       cond;
  gboolean    requested_state;
  GCallback   requested_func;
  gboolean    effective_state;
} GContextSpecificGroup;

gpointer
g_context_specific_group_get (GContextSpecificGroup *group,
                              GType                  type,
                              goffset                context_offset,
                              GCallback              start_func);

void
g_context_specific_group_remove (GContextSpecificGroup *group,
                                 GMainContext          *context,
                                 gpointer               instance,
                                 GCallback              stop_func);

void
g_context_specific_group_emit (GContextSpecificGroup *group,
                               guint                  signal_id);

#endif /* __G_CONTEXT_SPECIFIC_GROUP_H__ */
