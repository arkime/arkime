/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2010 Collabora, Ltd
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include "gtlsfiledatabase.h"

#include "ginitable.h"
#include "gtlsbackend.h"
#include "gtlsdatabase.h"
#include "glibintl.h"

/**
 * SECTION:gtlsfiledatabase
 * @short_description: TLS file based database type
 * @include: gio/gio.h
 *
 * #GTlsFileDatabase is implemented by #GTlsDatabase objects which load
 * their certificate information from a file. It is an interface which
 * TLS library specific subtypes implement.
 *
 * Since: 2.30
 */

/**
 * GTlsFileDatabase:
 *
 * Implemented by a #GTlsDatabase which allows you to load certificates
 * from a file.
 *
 * Since: 2.30
 */
G_DEFINE_INTERFACE (GTlsFileDatabase, g_tls_file_database, G_TYPE_TLS_DATABASE)

static void
g_tls_file_database_default_init (GTlsFileDatabaseInterface *iface)
{
  /**
   * GTlsFileDatabase:anchors:
   *
   * The path to a file containing PEM encoded certificate authority
   * root anchors. The certificates in this file will be treated as
   * root authorities for the purpose of verifying other certificates
   * via the g_tls_database_verify_chain() operation.
   *
   * Since: 2.30
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_string ("anchors",
                                                           P_("Anchors"),
                                                           P_("The certificate authority anchor file"),
                                                           NULL,
                                                           G_PARAM_READWRITE |
                                                           G_PARAM_CONSTRUCT |
                                                           G_PARAM_STATIC_STRINGS));
}

/**
 * g_tls_file_database_new:
 * @anchors: (type filename): filename of anchor certificate authorities.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Creates a new #GTlsFileDatabase which uses anchor certificate authorities
 * in @anchors to verify certificate chains.
 *
 * The certificates in @anchors must be PEM encoded.
 *
 * Returns: (transfer full) (type GTlsFileDatabase): the new
 * #GTlsFileDatabase, or %NULL on error
 *
 * Since: 2.30
 */
GTlsDatabase*
g_tls_file_database_new (const gchar     *anchors,
                         GError         **error)
{
  GObject *database;
  GTlsBackend *backend;

  backend = g_tls_backend_get_default ();
  database = g_initable_new (g_tls_backend_get_file_database_type (backend),
                             NULL, error,
                             "anchors", anchors,
                             NULL);
  return G_TLS_DATABASE (database);
}
