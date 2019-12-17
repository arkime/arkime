/* gmain-internal.h - GLib-internal mainloop API
 * Copyright (C) 2011 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __G_MAIN_INTERNAL_H__
#define __G_MAIN_INTERNAL_H__

#if !defined (GLIB_COMPILATION)
#error "This is a private header"
#endif

#include "gmain.h"

G_BEGIN_DECLS

GSource *_g_main_create_unix_signal_watch (int signum);

G_END_DECLS

#endif /* __G_MAIN_H__ */
