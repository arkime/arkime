/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2009 Benjamin Otte <otte@gnome.org>
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
#ifndef __G_ATOMIC_ARRAY_H__
#define __G_ATOMIC_ARRAY_H__

#if !defined (__GLIB_GOBJECT_H_INSIDE__) && !defined (GOBJECT_COMPILATION)
#error "Only <glib-object.h> can be included directly."
#endif

#include <glib/glib.h>

G_BEGIN_DECLS

#define G_ATOMIC_ARRAY_DATA_SIZE(mem) (*((gsize *) (mem) - 1))

typedef struct _GAtomicArray GAtomicArray;
struct _GAtomicArray {
  volatile gpointer data;               /* elements - atomic */
};

void     _g_atomic_array_init   (GAtomicArray *array);
gpointer _g_atomic_array_copy   (GAtomicArray *array,
				 gsize         header_size,
				 gsize         additional_element_size);
void     _g_atomic_array_update (GAtomicArray *array,
				 gpointer      new_data);

#define  G_ATOMIC_ARRAY_GET_LOCKED(_array, _type) ((_type *)((_array)->data))

#define G_ATOMIC_ARRAY_DO_TRANSACTION(_array, _type, _C_) G_STMT_START {	\
    volatile gpointer *_datap  = &(_array)->data;				\
    _type *transaction_data, *__check;						\
										\
    __check = g_atomic_pointer_get (_datap);					\
    do {									\
      transaction_data = __check;						\
      {_C_;}									\
      __check = g_atomic_pointer_get (_datap);					\
    } while (transaction_data != __check);					\
  } G_STMT_END

G_END_DECLS

#endif	/* __G_ATOMIC_ARRAY_H__ */
