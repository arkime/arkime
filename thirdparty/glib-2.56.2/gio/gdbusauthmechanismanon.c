/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include "gdbusauthmechanismanon.h"
#include "gdbuserror.h"
#include "gioenumtypes.h"

#include "glibintl.h"

struct _GDBusAuthMechanismAnonPrivate
{
  gboolean is_client;
  gboolean is_server;
  GDBusAuthMechanismState state;
};

static gint                     mechanism_get_priority              (void);
static const gchar             *mechanism_get_name                  (void);

static gboolean                 mechanism_is_supported              (GDBusAuthMechanism   *mechanism);
static gchar                   *mechanism_encode_data               (GDBusAuthMechanism   *mechanism,
                                                                     const gchar          *data,
                                                                     gsize                 data_len,
                                                                     gsize                *out_data_len);
static gchar                   *mechanism_decode_data               (GDBusAuthMechanism   *mechanism,
                                                                     const gchar          *data,
                                                                     gsize                 data_len,
                                                                     gsize                *out_data_len);
static GDBusAuthMechanismState  mechanism_server_get_state          (GDBusAuthMechanism   *mechanism);
static void                     mechanism_server_initiate           (GDBusAuthMechanism   *mechanism,
                                                                     const gchar          *initial_response,
                                                                     gsize                 initial_response_len);
static void                     mechanism_server_data_receive       (GDBusAuthMechanism   *mechanism,
                                                                     const gchar          *data,
                                                                     gsize                 data_len);
static gchar                   *mechanism_server_data_send          (GDBusAuthMechanism   *mechanism,
                                                                     gsize                *out_data_len);
static gchar                   *mechanism_server_get_reject_reason  (GDBusAuthMechanism   *mechanism);
static void                     mechanism_server_shutdown           (GDBusAuthMechanism   *mechanism);
static GDBusAuthMechanismState  mechanism_client_get_state          (GDBusAuthMechanism   *mechanism);
static gchar                   *mechanism_client_initiate           (GDBusAuthMechanism   *mechanism,
                                                                     gsize                *out_initial_response_len);
static void                     mechanism_client_data_receive       (GDBusAuthMechanism   *mechanism,
                                                                     const gchar          *data,
                                                                     gsize                 data_len);
static gchar                   *mechanism_client_data_send          (GDBusAuthMechanism   *mechanism,
                                                                     gsize                *out_data_len);
static void                     mechanism_client_shutdown           (GDBusAuthMechanism   *mechanism);

/* ---------------------------------------------------------------------------------------------------- */

G_DEFINE_TYPE_WITH_PRIVATE (GDBusAuthMechanismAnon, _g_dbus_auth_mechanism_anon, G_TYPE_DBUS_AUTH_MECHANISM)

/* ---------------------------------------------------------------------------------------------------- */

static void
_g_dbus_auth_mechanism_anon_finalize (GObject *object)
{
  //GDBusAuthMechanismAnon *mechanism = G_DBUS_AUTH_MECHANISM_ANON (object);

  if (G_OBJECT_CLASS (_g_dbus_auth_mechanism_anon_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (_g_dbus_auth_mechanism_anon_parent_class)->finalize (object);
}

static void
_g_dbus_auth_mechanism_anon_class_init (GDBusAuthMechanismAnonClass *klass)
{
  GObjectClass *gobject_class;
  GDBusAuthMechanismClass *mechanism_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = _g_dbus_auth_mechanism_anon_finalize;

  mechanism_class = G_DBUS_AUTH_MECHANISM_CLASS (klass);
  mechanism_class->get_priority              = mechanism_get_priority;
  mechanism_class->get_name                  = mechanism_get_name;
  mechanism_class->is_supported              = mechanism_is_supported;
  mechanism_class->encode_data               = mechanism_encode_data;
  mechanism_class->decode_data               = mechanism_decode_data;
  mechanism_class->server_get_state          = mechanism_server_get_state;
  mechanism_class->server_initiate           = mechanism_server_initiate;
  mechanism_class->server_data_receive       = mechanism_server_data_receive;
  mechanism_class->server_data_send          = mechanism_server_data_send;
  mechanism_class->server_get_reject_reason  = mechanism_server_get_reject_reason;
  mechanism_class->server_shutdown           = mechanism_server_shutdown;
  mechanism_class->client_get_state          = mechanism_client_get_state;
  mechanism_class->client_initiate           = mechanism_client_initiate;
  mechanism_class->client_data_receive       = mechanism_client_data_receive;
  mechanism_class->client_data_send          = mechanism_client_data_send;
  mechanism_class->client_shutdown           = mechanism_client_shutdown;
}

static void
_g_dbus_auth_mechanism_anon_init (GDBusAuthMechanismAnon *mechanism)
{
  mechanism->priv = _g_dbus_auth_mechanism_anon_get_instance_private (mechanism);
}

/* ---------------------------------------------------------------------------------------------------- */


static gint
mechanism_get_priority (void)
{
  /* We prefer ANONYMOUS to most other mechanism (such as DBUS_COOKIE_SHA1) but not to EXTERNAL */
  return 50;
}


static const gchar *
mechanism_get_name (void)
{
  return "ANONYMOUS";
}

static gboolean
mechanism_is_supported (GDBusAuthMechanism *mechanism)
{
  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM_ANON (mechanism), FALSE);
  return TRUE;
}

static gchar *
mechanism_encode_data (GDBusAuthMechanism   *mechanism,
                       const gchar          *data,
                       gsize                 data_len,
                       gsize                *out_data_len)
{
  return NULL;
}


static gchar *
mechanism_decode_data (GDBusAuthMechanism   *mechanism,
                       const gchar          *data,
                       gsize                 data_len,
                       gsize                *out_data_len)
{
  return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusAuthMechanismState
mechanism_server_get_state (GDBusAuthMechanism   *mechanism)
{
  GDBusAuthMechanismAnon *m = G_DBUS_AUTH_MECHANISM_ANON (mechanism);

  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM_ANON (mechanism), G_DBUS_AUTH_MECHANISM_STATE_INVALID);
  g_return_val_if_fail (m->priv->is_server && !m->priv->is_client, G_DBUS_AUTH_MECHANISM_STATE_INVALID);

  return m->priv->state;
}

static void
mechanism_server_initiate (GDBusAuthMechanism   *mechanism,
                           const gchar          *initial_response,
                           gsize                 initial_response_len)
{
  GDBusAuthMechanismAnon *m = G_DBUS_AUTH_MECHANISM_ANON (mechanism);

  g_return_if_fail (G_IS_DBUS_AUTH_MECHANISM_ANON (mechanism));
  g_return_if_fail (!m->priv->is_server && !m->priv->is_client);

  //g_debug ("ANONYMOUS: initial_response was '%s'", initial_response);

  m->priv->is_server = TRUE;
  m->priv->state = G_DBUS_AUTH_MECHANISM_STATE_ACCEPTED;
}

static void
mechanism_server_data_receive (GDBusAuthMechanism   *mechanism,
                               const gchar          *data,
                               gsize                 data_len)
{
  GDBusAuthMechanismAnon *m = G_DBUS_AUTH_MECHANISM_ANON (mechanism);

  g_return_if_fail (G_IS_DBUS_AUTH_MECHANISM_ANON (mechanism));
  g_return_if_fail (m->priv->is_server && !m->priv->is_client);
  g_return_if_fail (m->priv->state == G_DBUS_AUTH_MECHANISM_STATE_WAITING_FOR_DATA);

  /* can never end up here because we are never in the WAITING_FOR_DATA state */
  g_assert_not_reached ();
}

static gchar *
mechanism_server_data_send (GDBusAuthMechanism   *mechanism,
                            gsize                *out_data_len)
{
  GDBusAuthMechanismAnon *m = G_DBUS_AUTH_MECHANISM_ANON (mechanism);

  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM_ANON (mechanism), NULL);
  g_return_val_if_fail (m->priv->is_server && !m->priv->is_client, NULL);
  g_return_val_if_fail (m->priv->state == G_DBUS_AUTH_MECHANISM_STATE_HAVE_DATA_TO_SEND, NULL);

  /* can never end up here because we are never in the HAVE_DATA_TO_SEND state */
  g_assert_not_reached ();

  return NULL;
}

static gchar *
mechanism_server_get_reject_reason (GDBusAuthMechanism   *mechanism)
{
  GDBusAuthMechanismAnon *m = G_DBUS_AUTH_MECHANISM_ANON (mechanism);

  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM_ANON (mechanism), NULL);
  g_return_val_if_fail (m->priv->is_server && !m->priv->is_client, NULL);
  g_return_val_if_fail (m->priv->state == G_DBUS_AUTH_MECHANISM_STATE_REJECTED, NULL);

  /* can never end up here because we are never in the REJECTED state */
  g_assert_not_reached ();

  return NULL;
}

static void
mechanism_server_shutdown (GDBusAuthMechanism   *mechanism)
{
  GDBusAuthMechanismAnon *m = G_DBUS_AUTH_MECHANISM_ANON (mechanism);

  g_return_if_fail (G_IS_DBUS_AUTH_MECHANISM_ANON (mechanism));
  g_return_if_fail (m->priv->is_server && !m->priv->is_client);

  m->priv->is_server = FALSE;
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusAuthMechanismState
mechanism_client_get_state (GDBusAuthMechanism   *mechanism)
{
  GDBusAuthMechanismAnon *m = G_DBUS_AUTH_MECHANISM_ANON (mechanism);

  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM_ANON (mechanism), G_DBUS_AUTH_MECHANISM_STATE_INVALID);
  g_return_val_if_fail (m->priv->is_client && !m->priv->is_server, G_DBUS_AUTH_MECHANISM_STATE_INVALID);

  return m->priv->state;
}

static gchar *
mechanism_client_initiate (GDBusAuthMechanism   *mechanism,
                           gsize                *out_initial_response_len)
{
  GDBusAuthMechanismAnon *m = G_DBUS_AUTH_MECHANISM_ANON (mechanism);

  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM_ANON (mechanism), NULL);
  g_return_val_if_fail (!m->priv->is_server && !m->priv->is_client, NULL);

  m->priv->is_client = TRUE;
  m->priv->state = G_DBUS_AUTH_MECHANISM_STATE_ACCEPTED;

  *out_initial_response_len = -1;

  /* just return our library name and version */
  return g_strdup ("GDBus 0.1");
}

static void
mechanism_client_data_receive (GDBusAuthMechanism   *mechanism,
                               const gchar          *data,
                               gsize                 data_len)
{
  GDBusAuthMechanismAnon *m = G_DBUS_AUTH_MECHANISM_ANON (mechanism);

  g_return_if_fail (G_IS_DBUS_AUTH_MECHANISM_ANON (mechanism));
  g_return_if_fail (m->priv->is_client && !m->priv->is_server);
  g_return_if_fail (m->priv->state == G_DBUS_AUTH_MECHANISM_STATE_WAITING_FOR_DATA);

  /* can never end up here because we are never in the WAITING_FOR_DATA state */
  g_assert_not_reached ();
}

static gchar *
mechanism_client_data_send (GDBusAuthMechanism   *mechanism,
                            gsize                *out_data_len)
{
  GDBusAuthMechanismAnon *m = G_DBUS_AUTH_MECHANISM_ANON (mechanism);

  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM_ANON (mechanism), NULL);
  g_return_val_if_fail (m->priv->is_client && !m->priv->is_server, NULL);
  g_return_val_if_fail (m->priv->state == G_DBUS_AUTH_MECHANISM_STATE_HAVE_DATA_TO_SEND, NULL);

  /* can never end up here because we are never in the HAVE_DATA_TO_SEND state */
  g_assert_not_reached ();

  return NULL;
}

static void
mechanism_client_shutdown (GDBusAuthMechanism   *mechanism)
{
  GDBusAuthMechanismAnon *m = G_DBUS_AUTH_MECHANISM_ANON (mechanism);

  g_return_if_fail (G_IS_DBUS_AUTH_MECHANISM_ANON (mechanism));
  g_return_if_fail (m->priv->is_client && !m->priv->is_server);

  m->priv->is_client = FALSE;
}

/* ---------------------------------------------------------------------------------------------------- */
