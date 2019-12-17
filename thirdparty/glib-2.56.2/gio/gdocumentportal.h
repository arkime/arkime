/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2016 Endless Mobile, Inc.
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

#ifndef __G_DOCUMENT_PORTAL_H__

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

char * g_document_portal_add_document (GFile   *file,
                                       GError **error);

GList * g_document_portal_add_documents (GList       *uris,
                                         const char  *app_id,
                                         GError     **error);

G_END_DECLS

#endif
