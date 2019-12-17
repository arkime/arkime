/* deftype.c
 * Copyright (C) 2006 Behdad Esfahbod
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
#include <glib-object.h>

/* see http://bugzilla.gnome.org/show_bug.cgi?id=337128 for the purpose of this test */

#define MY_G_IMPLEMENT_INTERFACE(TYPE_IFACE, iface_init)       { \
  const GInterfaceInfo g_implement_interface_info = { \
      (GInterfaceInitFunc) iface_init, \
      NULL, \
      NULL \
    }; \
  g_type_add_interface_static (g_define_type_id, TYPE_IFACE, &g_implement_interface_info); \
}

#define MY_DEFINE_TYPE(TN, t_n, T_P) \
	G_DEFINE_TYPE_WITH_CODE (TN, t_n, T_P, \
				 MY_G_IMPLEMENT_INTERFACE (G_TYPE_INTERFACE, NULL))

typedef struct _TypeName {
  GObject parent_instance;
  const char *name;
} TypeName;

typedef struct _TypeNameClass {
  GObjectClass parent_parent;
} TypeNameClass;

GType           type_name_get_type          (void);

MY_DEFINE_TYPE (TypeName, type_name, G_TYPE_OBJECT)

static void     type_name_init              (TypeName      *self)
{
}

static void     type_name_class_init        (TypeNameClass *klass)
{
}

int
main (void)
{
  return 0;
}
