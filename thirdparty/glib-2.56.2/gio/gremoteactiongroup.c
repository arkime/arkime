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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gsimpleaction.h"
#include "gactiongroup.h"
#include "gactionmap.h"
#include "gaction.h"

/**
 * SECTION:gremoteactiongroup
 * @title: GRemoteActionGroup
 * @short_description: A GActionGroup that interacts with other processes
 * @include: gio/gio.h
 *
 * The GRemoteActionGroup interface is implemented by #GActionGroup
 * instances that either transmit action invocations to other processes
 * or receive action invocations in the local process from other
 * processes.
 *
 * The interface has `_full` variants of the two
 * methods on #GActionGroup used to activate actions:
 * g_action_group_activate_action() and
 * g_action_group_change_action_state(). These variants allow a
 * "platform data" #GVariant to be specified: a dictionary providing
 * context for the action invocation (for example: timestamps, startup
 * notification IDs, etc).
 *
 * #GDBusActionGroup implements #GRemoteActionGroup.  This provides a
 * mechanism to send platform data for action invocations over D-Bus.
 *
 * Additionally, g_dbus_connection_export_action_group() will check if
 * the exported #GActionGroup implements #GRemoteActionGroup and use the
 * `_full` variants of the calls if available.  This
 * provides a mechanism by which to receive platform data for action
 * invocations that arrive by way of D-Bus.
 *
 * Since: 2.32
 **/

/**
 * GRemoteActionGroup:
 *
 * #GRemoteActionGroup is an opaque data structure and can only be accessed
 * using the following functions.
 **/

/**
 * GRemoteActionGroupInterface:
 * @activate_action_full: the virtual function pointer for g_remote_action_group_activate_action_full()
 * @change_action_state_full: the virtual function pointer for g_remote_action_group_change_action_state_full()
 *
 * The virtual function table for #GRemoteActionGroup.
 *
 * Since: 2.32
 **/

#include "config.h"

#include "gremoteactiongroup.h"

G_DEFINE_INTERFACE (GRemoteActionGroup, g_remote_action_group, G_TYPE_ACTION_GROUP)

static void
g_remote_action_group_default_init (GRemoteActionGroupInterface *iface)
{
}

/**
 * g_remote_action_group_activate_action_full:
 * @remote: a #GDBusActionGroup
 * @action_name: the name of the action to activate
 * @parameter: (nullable): the optional parameter to the activation
 * @platform_data: the platform data to send
 *
 * Activates the remote action.
 *
 * This is the same as g_action_group_activate_action() except that it
 * allows for provision of "platform data" to be sent along with the
 * activation request.  This typically contains details such as the user
 * interaction timestamp or startup notification information.
 *
 * @platform_data must be non-%NULL and must have the type
 * %G_VARIANT_TYPE_VARDICT.  If it is floating, it will be consumed.
 *
 * Since: 2.32
 **/
void
g_remote_action_group_activate_action_full (GRemoteActionGroup *remote,
                                            const gchar        *action_name,
                                            GVariant           *parameter,
                                            GVariant           *platform_data)
{
  G_REMOTE_ACTION_GROUP_GET_IFACE (remote)
    ->activate_action_full (remote, action_name, parameter, platform_data);
}

/**
 * g_remote_action_group_change_action_state_full:
 * @remote: a #GRemoteActionGroup
 * @action_name: the name of the action to change the state of
 * @value: the new requested value for the state
 * @platform_data: the platform data to send
 *
 * Changes the state of a remote action.
 *
 * This is the same as g_action_group_change_action_state() except that
 * it allows for provision of "platform data" to be sent along with the
 * state change request.  This typically contains details such as the
 * user interaction timestamp or startup notification information.
 *
 * @platform_data must be non-%NULL and must have the type
 * %G_VARIANT_TYPE_VARDICT.  If it is floating, it will be consumed.
 *
 * Since: 2.32
 **/
void
g_remote_action_group_change_action_state_full (GRemoteActionGroup *remote,
                                                const gchar        *action_name,
                                                GVariant           *value,
                                                GVariant           *platform_data)
{
  G_REMOTE_ACTION_GROUP_GET_IFACE (remote)
    ->change_action_state_full (remote, action_name, value, platform_data);
}
