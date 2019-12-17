/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1998-1999, 2000-2001 Tim Janik and Red Hat, Inc.
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
#ifndef __G_TYPE_PRIVATE_H__
#define __G_TYPE_PRIVATE_H__

#if !defined (__GLIB_GOBJECT_H_INSIDE__) && !defined (GOBJECT_COMPILATION)
#error "Only <glib-object.h> can be included directly."
#endif

#include "gboxed.h"
#include "gclosure.h"

/*< private >
 * GOBJECT_IF_DEBUG:
 * @debug_type: Currently only OBJECTS and SIGNALS are supported.
 * @code_block: Custom debug code.
 *
 * A convenience macro for debugging GObject.
 * This macro is only used internally.
 */
#ifdef G_ENABLE_DEBUG
#define GOBJECT_IF_DEBUG(debug_type, code_block) \
G_STMT_START { \
    if (_g_type_debug_flags & G_TYPE_DEBUG_ ## debug_type) \
      { code_block; } \
} G_STMT_END
#else   /* !G_ENABLE_DEBUG */
#define GOBJECT_IF_DEBUG(debug_type, code_block)
#endif  /* G_ENABLE_DEBUG */

G_BEGIN_DECLS

extern GTypeDebugFlags _g_type_debug_flags;

typedef struct _GRealClosure  GRealClosure;
struct _GRealClosure
{
  GClosureMarshal meta_marshal;
  gpointer meta_marshal_data;
  GVaClosureMarshal va_meta_marshal;
  GVaClosureMarshal va_marshal;
  GClosure closure;
};

#define G_REAL_CLOSURE(_c) \
  ((GRealClosure *)G_STRUCT_MEMBER_P ((_c), -G_STRUCT_OFFSET (GRealClosure, closure)))

void    _g_value_c_init          (void); /* sync with gvalue.c */
void    _g_value_types_init      (void); /* sync with gvaluetypes.c */
void    _g_enum_types_init       (void); /* sync with genums.c */
void    _g_param_type_init       (void); /* sync with gparam.c */
void    _g_boxed_type_init       (void); /* sync with gboxed.c */
void    _g_object_type_init      (void); /* sync with gobject.c */
void    _g_param_spec_types_init (void); /* sync with gparamspecs.c */
void    _g_value_transforms_init (void); /* sync with gvaluetransform.c */
void    _g_signal_init           (void); /* sync with gsignal.c */

/* for gboxed.c */
gpointer        _g_type_boxed_copy      (GType          type,
                                         gpointer       value);
void            _g_type_boxed_free      (GType          type,
                                         gpointer       value);
void            _g_type_boxed_init      (GType          type,
                                         GBoxedCopyFunc copy_func,
                                         GBoxedFreeFunc free_func);

gboolean    _g_closure_is_void (GClosure       *closure,
				gpointer        instance);
gboolean    _g_closure_supports_invoke_va (GClosure       *closure);
void        _g_closure_set_va_marshal (GClosure       *closure,
				       GVaClosureMarshal marshal);
void        _g_closure_invoke_va (GClosure       *closure,
				  GValue /*out*/ *return_value,
				  gpointer        instance,
				  va_list         args,
				  int             n_params,
				  GType          *param_types);

/**
 * _G_DEFINE_TYPE_EXTENDED_WITH_PRELUDE:
 *
 * See also G_DEFINE_TYPE_EXTENDED().  This macro is generally only
 * necessary as a workaround for classes which have properties of
 * object types that may be initialized in distinct threads.  See:
 * https://bugzilla.gnome.org/show_bug.cgi?id=674885
 *
 * Currently private.
 */
#define _G_DEFINE_TYPE_EXTENDED_WITH_PRELUDE(TN, t_n, T_P, _f_, _P_, _C_)	    _G_DEFINE_TYPE_EXTENDED_BEGIN_PRE (TN, t_n, T_P) {_P_;} _G_DEFINE_TYPE_EXTENDED_BEGIN_REGISTER (TN, t_n, T_P, _f_){_C_;} _G_DEFINE_TYPE_EXTENDED_END()

G_END_DECLS

#endif /* __G_TYPE_PRIVATE_H__ */
