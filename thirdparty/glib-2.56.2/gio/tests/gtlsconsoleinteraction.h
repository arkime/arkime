/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2011 Collabora, Ltd.
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

#ifndef __G_TLS_CONSOLE_INTERACTION_H__
#define __G_TLS_CONSOLE_INTERACTION_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define G_TYPE_TLS_CONSOLE_INTERACTION         (g_tls_console_interaction_get_type ())
#define G_TLS_CONSOLE_INTERACTION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_TLS_CONSOLE_INTERACTION, GTlsConsoleInteraction))
#define G_TLS_CONSOLE_INTERACTION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_TLS_CONSOLE_INTERACTION, GTlsConsoleInteractionClass))
#define G_IS_TLS_CONSOLE_INTERACTION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_TLS_CONSOLE_INTERACTION))
#define G_IS_TLS_CONSOLE_INTERACTION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_TLS_CONSOLE_INTERACTION))
#define G_TLS_CONSOLE_INTERACTION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_TLS_CONSOLE_INTERACTION, GTlsConsoleInteractionClass))

typedef struct _GTlsConsoleInteraction        GTlsConsoleInteraction;
typedef struct _GTlsConsoleInteractionClass   GTlsConsoleInteractionClass;

struct _GTlsConsoleInteraction
{
  GTlsInteraction parent_instance;
};

struct _GTlsConsoleInteractionClass
{
  GTlsInteractionClass parent_class;
};

GType                  g_tls_console_interaction_get_type    (void) G_GNUC_CONST;

GTlsInteraction *      g_tls_console_interaction_new         (void);

G_END_DECLS

#endif /* __G_TLS_CONSOLE_INTERACTION_H__ */
