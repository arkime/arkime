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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __G_ASYNCQUEUEPRIVATE_H__
#define __G_ASYNCQUEUEPRIVATE_H__

#include "gasyncqueue.h"

G_BEGIN_DECLS

GMutex *_g_async_queue_get_mutex (GAsyncQueue *queue);

G_END_DECLS

#endif /* __G_ASYNCQUEUEPRIVATE_H__ */
