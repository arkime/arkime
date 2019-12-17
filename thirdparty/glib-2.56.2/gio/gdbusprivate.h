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

#ifndef __G_DBUS_PRIVATE_H__
#define __G_DBUS_PRIVATE_H__

#if !defined (GIO_COMPILATION)
#error "gdbusprivate.h is a private header file."
#endif

#include <gio/giotypes.h>

G_BEGIN_DECLS

/* ---------------------------------------------------------------------------------------------------- */

typedef struct GDBusWorker GDBusWorker;

typedef void (*GDBusWorkerMessageReceivedCallback) (GDBusWorker   *worker,
                                                    GDBusMessage  *message,
                                                    gpointer       user_data);

typedef GDBusMessage *(*GDBusWorkerMessageAboutToBeSentCallback) (GDBusWorker   *worker,
                                                                  GDBusMessage  *message,
                                                                  gpointer       user_data);

typedef void (*GDBusWorkerDisconnectedCallback)    (GDBusWorker   *worker,
                                                    gboolean       remote_peer_vanished,
                                                    GError        *error,
                                                    gpointer       user_data);

/* This function may be called from any thread - callbacks will be in the shared private message thread
 * and must not block.
 */
GDBusWorker *_g_dbus_worker_new          (GIOStream                          *stream,
                                          GDBusCapabilityFlags                capabilities,
                                          gboolean                            initially_frozen,
                                          GDBusWorkerMessageReceivedCallback  message_received_callback,
                                          GDBusWorkerMessageAboutToBeSentCallback message_about_to_be_sent_callback,
                                          GDBusWorkerDisconnectedCallback     disconnected_callback,
                                          gpointer                            user_data);

/* can be called from any thread - steals blob */
void         _g_dbus_worker_send_message (GDBusWorker    *worker,
                                          GDBusMessage   *message,
                                          gchar          *blob,
                                          gsize           blob_len);

/* can be called from any thread */
void         _g_dbus_worker_stop         (GDBusWorker    *worker);

/* can be called from any thread */
void         _g_dbus_worker_unfreeze     (GDBusWorker    *worker);

/* can be called from any thread (except the worker thread) */
gboolean     _g_dbus_worker_flush_sync   (GDBusWorker    *worker,
                                          GCancellable   *cancellable,
                                          GError        **error);

/* can be called from any thread */
void         _g_dbus_worker_close        (GDBusWorker         *worker,
                                          GTask               *task);

/* ---------------------------------------------------------------------------------------------------- */

void _g_dbus_initialize (void);
gboolean _g_dbus_debug_authentication (void);
gboolean _g_dbus_debug_transport (void);
gboolean _g_dbus_debug_message (void);
gboolean _g_dbus_debug_payload (void);
gboolean _g_dbus_debug_call    (void);
gboolean _g_dbus_debug_signal  (void);
gboolean _g_dbus_debug_incoming (void);
gboolean _g_dbus_debug_return (void);
gboolean _g_dbus_debug_emission (void);
gboolean _g_dbus_debug_address (void);

void     _g_dbus_debug_print_lock (void);
void     _g_dbus_debug_print_unlock (void);

gboolean _g_dbus_address_parse_entry (const gchar  *address_entry,
                                      gchar       **out_transport_name,
                                      GHashTable  **out_key_value_pairs,
                                      GError      **error);

GVariantType * _g_dbus_compute_complete_signature (GDBusArgInfo **args);

gchar *_g_dbus_hexdump (const gchar *data, gsize len, guint indent);

/* ---------------------------------------------------------------------------------------------------- */

#ifdef G_OS_WIN32
gchar *_g_dbus_win32_get_user_sid (void);
#endif

gchar *_g_dbus_get_machine_id (GError **error);

gchar *_g_dbus_enum_to_string (GType enum_type, gint value);

/* ---------------------------------------------------------------------------------------------------- */

GDBusMethodInvocation *_g_dbus_method_invocation_new (const gchar             *sender,
                                                      const gchar             *object_path,
                                                      const gchar             *interface_name,
                                                      const gchar             *method_name,
                                                      const GDBusMethodInfo   *method_info,
                                                      const GDBusPropertyInfo *property_info,
                                                      GDBusConnection         *connection,
                                                      GDBusMessage            *message,
                                                      GVariant                *parameters,
                                                      gpointer                 user_data);

/* ---------------------------------------------------------------------------------------------------- */

gboolean _g_signal_accumulator_false_handled (GSignalInvocationHint *ihint,
                                              GValue                *return_accu,
                                              const GValue          *handler_return,
                                              gpointer               dummy);

gboolean _g_dbus_object_skeleton_has_authorize_method_handlers (GDBusObjectSkeleton *object);

void _g_dbus_object_proxy_add_interface (GDBusObjectProxy *proxy,
                                         GDBusProxy       *interface_proxy);
void _g_dbus_object_proxy_remove_interface (GDBusObjectProxy *proxy,
                                            const gchar      *interface_name);

/* Implemented in gdbusconnection.c */
GDBusConnection *_g_bus_get_singleton_if_exists (GBusType bus_type);

G_END_DECLS

#endif /* __G_DBUS_PRIVATE_H__ */
