/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 */

#ifndef __G_DUMMY_TLS_BACKEND_H__
#define __G_DUMMY_TLS_BACKEND_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_DUMMY_TLS_BACKEND         (_g_dummy_tls_backend_get_type ())
#define G_DUMMY_TLS_BACKEND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DUMMY_TLS_BACKEND, GDummyTlsBackend))
#define G_DUMMY_TLS_BACKEND_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DUMMY_TLS_BACKEND, GDummyTlsBackendClass))
#define G_IS_DUMMY_TLS_BACKEND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DUMMY_TLS_BACKEND))
#define G_IS_DUMMY_TLS_BACKEND_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DUMMY_TLS_BACKEND))
#define G_DUMMY_TLS_BACKEND_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DUMMY_TLS_BACKEND, GDummyTlsBackendClass))

typedef struct _GDummyTlsBackend       GDummyTlsBackend;
typedef struct _GDummyTlsBackendClass  GDummyTlsBackendClass;

struct _GDummyTlsBackendClass {
  GObjectClass parent_class;
};

GType _g_dummy_tls_backend_get_type       (void);

G_END_DECLS

#endif /* __G_DUMMY_TLS_BACKEND_H__ */
