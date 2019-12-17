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

#include "gdbusauthmechanism.h"
#include "gcredentials.h"
#include "gdbuserror.h"
#include "gioenumtypes.h"
#include "giostream.h"

#include "glibintl.h"

/* ---------------------------------------------------------------------------------------------------- */

struct _GDBusAuthMechanismPrivate
{
  GIOStream *stream;
  GCredentials *credentials;
};

enum
{
  PROP_0,
  PROP_STREAM,
  PROP_CREDENTIALS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GDBusAuthMechanism, _g_dbus_auth_mechanism, G_TYPE_OBJECT)

/* ---------------------------------------------------------------------------------------------------- */

static void
_g_dbus_auth_mechanism_finalize (GObject *object)
{
  GDBusAuthMechanism *mechanism = G_DBUS_AUTH_MECHANISM (object);

  if (mechanism->priv->stream != NULL)
    g_object_unref (mechanism->priv->stream);
  if (mechanism->priv->credentials != NULL)
    g_object_unref (mechanism->priv->credentials);

  G_OBJECT_CLASS (_g_dbus_auth_mechanism_parent_class)->finalize (object);
}

static void
_g_dbus_auth_mechanism_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GDBusAuthMechanism *mechanism = G_DBUS_AUTH_MECHANISM (object);

  switch (prop_id)
    {
    case PROP_STREAM:
      g_value_set_object (value, mechanism->priv->stream);
      break;

    case PROP_CREDENTIALS:
      g_value_set_object (value, mechanism->priv->credentials);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_g_dbus_auth_mechanism_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GDBusAuthMechanism *mechanism = G_DBUS_AUTH_MECHANISM (object);

  switch (prop_id)
    {
    case PROP_STREAM:
      mechanism->priv->stream = g_value_dup_object (value);
      break;

    case PROP_CREDENTIALS:
      mechanism->priv->credentials = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_g_dbus_auth_mechanism_class_init (GDBusAuthMechanismClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = _g_dbus_auth_mechanism_get_property;
  gobject_class->set_property = _g_dbus_auth_mechanism_set_property;
  gobject_class->finalize     = _g_dbus_auth_mechanism_finalize;

  g_object_class_install_property (gobject_class,
                                   PROP_STREAM,
                                   g_param_spec_object ("stream",
                                                        P_("IO Stream"),
                                                        P_("The underlying GIOStream used for I/O"),
                                                        G_TYPE_IO_STREAM,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusAuthMechanism:credentials:
   *
   * If authenticating as a server, this property contains the
   * received credentials, if any.
   *
   * If authenticating as a client, the property contains the
   * credentials that were sent, if any.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CREDENTIALS,
                                   g_param_spec_object ("credentials",
                                                        P_("Credentials"),
                                                        P_("The credentials of the remote peer"),
                                                        G_TYPE_CREDENTIALS,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));
}

static void
_g_dbus_auth_mechanism_init (GDBusAuthMechanism *mechanism)
{
  mechanism->priv = _g_dbus_auth_mechanism_get_instance_private (mechanism);
}

/* ---------------------------------------------------------------------------------------------------- */

GIOStream *
_g_dbus_auth_mechanism_get_stream (GDBusAuthMechanism *mechanism)
{
  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism), NULL);
  return mechanism->priv->stream;
}

GCredentials *
_g_dbus_auth_mechanism_get_credentials (GDBusAuthMechanism *mechanism)
{
  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism), NULL);
  return mechanism->priv->credentials;
}

/* ---------------------------------------------------------------------------------------------------- */

const gchar *
_g_dbus_auth_mechanism_get_name (GType mechanism_type)
{
  const gchar *name;
  GDBusAuthMechanismClass *klass;

  g_return_val_if_fail (g_type_is_a (mechanism_type, G_TYPE_DBUS_AUTH_MECHANISM), NULL);

  klass = g_type_class_ref (mechanism_type);
  g_assert (klass != NULL);
  name = klass->get_name ();
  //g_type_class_unref (klass);

  return name;
}

gint
_g_dbus_auth_mechanism_get_priority (GType mechanism_type)
{
  gint priority;
  GDBusAuthMechanismClass *klass;

  g_return_val_if_fail (g_type_is_a (mechanism_type, G_TYPE_DBUS_AUTH_MECHANISM), 0);

  klass = g_type_class_ref (mechanism_type);
  g_assert (klass != NULL);
  priority = klass->get_priority ();
  //g_type_class_unref (klass);

  return priority;
}

/* ---------------------------------------------------------------------------------------------------- */

gboolean
_g_dbus_auth_mechanism_is_supported (GDBusAuthMechanism *mechanism)
{
  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism), FALSE);
  return G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->is_supported (mechanism);
}

gchar *
_g_dbus_auth_mechanism_encode_data (GDBusAuthMechanism *mechanism,
                                    const gchar        *data,
                                    gsize               data_len,
                                    gsize              *out_data_len)
{
  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism), NULL);
  return G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->encode_data (mechanism, data, data_len, out_data_len);
}


gchar *
_g_dbus_auth_mechanism_decode_data (GDBusAuthMechanism *mechanism,
                                    const gchar        *data,
                                    gsize               data_len,
                                    gsize              *out_data_len)
{
  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism), NULL);
  return G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->decode_data (mechanism, data, data_len, out_data_len);
}

/* ---------------------------------------------------------------------------------------------------- */

GDBusAuthMechanismState
_g_dbus_auth_mechanism_server_get_state (GDBusAuthMechanism *mechanism)
{
  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism), G_DBUS_AUTH_MECHANISM_STATE_INVALID);
  return G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->server_get_state (mechanism);
}

void
_g_dbus_auth_mechanism_server_initiate (GDBusAuthMechanism *mechanism,
                                        const gchar        *initial_response,
                                        gsize               initial_response_len)
{
  g_return_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism));
  G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->server_initiate (mechanism, initial_response, initial_response_len);
}

void
_g_dbus_auth_mechanism_server_data_receive (GDBusAuthMechanism *mechanism,
                                            const gchar        *data,
                                            gsize               data_len)
{
  g_return_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism));
  G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->server_data_receive (mechanism, data, data_len);
}

gchar *
_g_dbus_auth_mechanism_server_data_send (GDBusAuthMechanism *mechanism,
                                         gsize              *out_data_len)
{
  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism), NULL);
  return G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->server_data_send (mechanism, out_data_len);
}

gchar *
_g_dbus_auth_mechanism_server_get_reject_reason (GDBusAuthMechanism *mechanism)
{
  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism), NULL);
  return G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->server_get_reject_reason (mechanism);
}

void
_g_dbus_auth_mechanism_server_shutdown (GDBusAuthMechanism *mechanism)
{
  g_return_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism));
  G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->server_shutdown (mechanism);
}

/* ---------------------------------------------------------------------------------------------------- */

GDBusAuthMechanismState
_g_dbus_auth_mechanism_client_get_state (GDBusAuthMechanism *mechanism)
{
  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism), G_DBUS_AUTH_MECHANISM_STATE_INVALID);
  return G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->client_get_state (mechanism);
}

gchar *
_g_dbus_auth_mechanism_client_initiate (GDBusAuthMechanism *mechanism,
                                        gsize              *out_initial_response_len)
{
  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism), NULL);
  return G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->client_initiate (mechanism,
                                                                       out_initial_response_len);
}

void
_g_dbus_auth_mechanism_client_data_receive (GDBusAuthMechanism *mechanism,
                                            const gchar        *data,
                                            gsize               data_len)
{
  g_return_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism));
  G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->client_data_receive (mechanism, data, data_len);
}

gchar *
_g_dbus_auth_mechanism_client_data_send (GDBusAuthMechanism *mechanism,
                                         gsize              *out_data_len)
{
  g_return_val_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism), NULL);
  return G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->client_data_send (mechanism, out_data_len);
}

void
_g_dbus_auth_mechanism_client_shutdown (GDBusAuthMechanism *mechanism)
{
  g_return_if_fail (G_IS_DBUS_AUTH_MECHANISM (mechanism));
  G_DBUS_AUTH_MECHANISM_GET_CLASS (mechanism)->client_shutdown (mechanism);
}

/* ---------------------------------------------------------------------------------------------------- */
