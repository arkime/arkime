
#include "config.h"

#include "gobject.h"
#include "genums.h"
#include "gboxed.h"
#include "gvaluetypes.h"

/**
 * g_cclosure_marshal_VOID__VOID:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with no arguments.
 */
/**
 * g_cclosure_marshal_VOID__BOOLEAN:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single
 * boolean argument.
 */
/**
 * g_cclosure_marshal_VOID__CHAR:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single
 * character argument.
 */
/**
 * g_cclosure_marshal_VOID__UCHAR:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single
 * unsigned character argument.
 */
/**
 * g_cclosure_marshal_VOID__INT:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single
 * integer argument.
 */
/**
 * g_cclosure_marshal_VOID__UINT:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with with a single
 * unsigned integer argument.
 */
/**
 * g_cclosure_marshal_VOID__LONG:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with with a single
 * long integer argument.
 */
/**
 * g_cclosure_marshal_VOID__ULONG:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single
 * unsigned long integer argument.
 */
/**
 * g_cclosure_marshal_VOID__ENUM:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single
 * argument with an enumerated type.
 */
/**
 * g_cclosure_marshal_VOID__FLAGS:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single
 * argument with a flags types.
 */
/**
 * g_cclosure_marshal_VOID__FLOAT:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with one
 * single-precision floating point argument.
 */
/**
 * g_cclosure_marshal_VOID__DOUBLE:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with one
 * double-precision floating point argument.
 */
/**
 * g_cclosure_marshal_VOID__STRING:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single string
 * argument.
 */
/**
 * g_cclosure_marshal_VOID__PARAM:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single
 * argument of type #GParamSpec.
 */
/**
 * g_cclosure_marshal_VOID__BOXED:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single
 * argument which is any boxed pointer type.
 */
/**
 * g_cclosure_marshal_VOID__POINTER:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single raw
 * pointer argument type.
 *
 * If it is possible, it is better to use one of the more specific
 * functions such as g_cclosure_marshal_VOID__OBJECT() or
 * g_cclosure_marshal_VOID__OBJECT().
 */
/**
 * g_cclosure_marshal_VOID__OBJECT:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single
 * #GObject argument.
 */
/**
 * g_cclosure_marshal_VOID__VARIANT:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a single
 * #GVariant argument.
 */
/**
 * g_cclosure_marshal_STRING__OBJECT_POINTER:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with handlers that
 * take a #GObject and a pointer and produce a string.  It is highly
 * unlikely that your signal handler fits this description.
 */
/**
 * g_cclosure_marshal_VOID__UINT_POINTER:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with a unsigned int
 * and a pointer as arguments.
 */
/**
 * g_cclosure_marshal_BOOLEAN__FLAGS:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with handlers that
 * take a flags type as an argument and return a boolean.  If you have
 * such a signal, you will probably also need to use an accumulator,
 * such as g_signal_accumulator_true_handled().
 */
/**
 * g_cclosure_marshal_BOOL__FLAGS:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * An old alias for g_cclosure_marshal_BOOLEAN__FLAGS().
 */
/**
 * g_cclosure_marshal_BOOLEAN__BOXED_BOXED:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A #GClosureMarshal function for use with signals with handlers that
 * take two boxed pointers as arguments and return a boolean.  If you
 * have such a signal, you will probably also need to use an
 * accumulator, such as g_signal_accumulator_true_handled().
 */
/**
 * g_cclosure_marshal_BOOL__BOXED_BOXED:
 * @closure: A #GClosure.
 * @return_value: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * An old alias for g_cclosure_marshal_BOOLEAN__BOXED_BOXED().
 */
/**
 * g_cclosure_marshal_VOID__VOIDv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__VOID().
 */

/**
 * g_cclosure_marshal_VOID__BOOLEANv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__BOOLEAN().
 */
/**
 * g_cclosure_marshal_VOID__CHARv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__CHAR().
 */
/**
 * g_cclosure_marshal_VOID__UCHARv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__UCHAR().
 */
/**
 * g_cclosure_marshal_VOID__INTv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__INT().
 */
/**
 * g_cclosure_marshal_VOID__UINTv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__UINT().
 */
/**
 * g_cclosure_marshal_VOID__LONGv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__LONG().
 */
/**
 * g_cclosure_marshal_VOID__ULONGv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__ULONG().
 */
/**
 * g_cclosure_marshal_VOID__ENUMv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__ENUM().
 */
/**
 * g_cclosure_marshal_VOID__FLAGSv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__FLAGS().
 */
/**
 * g_cclosure_marshal_VOID__FLOATv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__FLOAT().
 */
/**
 * g_cclosure_marshal_VOID__DOUBLEv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__DOUBLE().
 */
/**
 * g_cclosure_marshal_VOID__STRINGv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__STRING().
 */
/**
 * g_cclosure_marshal_VOID__PARAMv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__PARAM().
 */
/**
 * g_cclosure_marshal_VOID__BOXEDv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__BOXED().
 */
/**
 * g_cclosure_marshal_VOID__POINTERv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__POINTER().
 */
/**
 * g_cclosure_marshal_VOID__OBJECTv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__OBJECT().
 */
/**
 * g_cclosure_marshal_VOID__VARIANTv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__VARIANT().
 */
/**
 * g_cclosure_marshal_STRING__OBJECT_POINTERv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_STRING__OBJECT_POINTER().
 */
/**
 * g_cclosure_marshal_VOID__UINT_POINTERv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_VOID__UINT_POINTER().
 */
/**
 * g_cclosure_marshal_BOOLEAN__FLAGSv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_BOOLEAN__FLAGS().
 */
/**
 * g_cclosure_marshal_BOOLEAN__BOXED_BOXEDv:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is invoked.
 * @args: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args.
 *
 * The #GVaClosureMarshal equivalent to g_cclosure_marshal_BOOLEAN__BOXED_BOXED().
 */

#ifdef G_ENABLE_DEBUG
#define g_marshal_value_peek_boolean(v)  g_value_get_boolean (v)
#define g_marshal_value_peek_char(v)     g_value_get_schar (v)
#define g_marshal_value_peek_uchar(v)    g_value_get_uchar (v)
#define g_marshal_value_peek_int(v)      g_value_get_int (v)
#define g_marshal_value_peek_uint(v)     g_value_get_uint (v)
#define g_marshal_value_peek_long(v)     g_value_get_long (v)
#define g_marshal_value_peek_ulong(v)    g_value_get_ulong (v)
#define g_marshal_value_peek_int64(v)    g_value_get_int64 (v)
#define g_marshal_value_peek_uint64(v)   g_value_get_uint64 (v)
#define g_marshal_value_peek_enum(v)     g_value_get_enum (v)
#define g_marshal_value_peek_flags(v)    g_value_get_flags (v)
#define g_marshal_value_peek_float(v)    g_value_get_float (v)
#define g_marshal_value_peek_double(v)   g_value_get_double (v)
#define g_marshal_value_peek_string(v)   (char*) g_value_get_string (v)
#define g_marshal_value_peek_param(v)    g_value_get_param (v)
#define g_marshal_value_peek_boxed(v)    g_value_get_boxed (v)
#define g_marshal_value_peek_pointer(v)  g_value_get_pointer (v)
#define g_marshal_value_peek_object(v)   g_value_get_object (v)
#define g_marshal_value_peek_variant(v)  g_value_get_variant (v)
#else /* !G_ENABLE_DEBUG */
/* WARNING: This code accesses GValues directly, which is UNSUPPORTED API.
 *          Do not access GValues directly in your code. Instead, use the
 *          g_value_get_*() functions
 */
#define g_marshal_value_peek_boolean(v)  (v)->data[0].v_int
#define g_marshal_value_peek_char(v)     (v)->data[0].v_int
#define g_marshal_value_peek_uchar(v)    (v)->data[0].v_uint
#define g_marshal_value_peek_int(v)      (v)->data[0].v_int
#define g_marshal_value_peek_uint(v)     (v)->data[0].v_uint
#define g_marshal_value_peek_long(v)     (v)->data[0].v_long
#define g_marshal_value_peek_ulong(v)    (v)->data[0].v_ulong
#define g_marshal_value_peek_int64(v)    (v)->data[0].v_int64
#define g_marshal_value_peek_uint64(v)   (v)->data[0].v_uint64
#define g_marshal_value_peek_enum(v)     (v)->data[0].v_long
#define g_marshal_value_peek_flags(v)    (v)->data[0].v_ulong
#define g_marshal_value_peek_float(v)    (v)->data[0].v_float
#define g_marshal_value_peek_double(v)   (v)->data[0].v_double
#define g_marshal_value_peek_string(v)   (v)->data[0].v_pointer
#define g_marshal_value_peek_param(v)    (v)->data[0].v_pointer
#define g_marshal_value_peek_boxed(v)    (v)->data[0].v_pointer
#define g_marshal_value_peek_pointer(v)  (v)->data[0].v_pointer
#define g_marshal_value_peek_object(v)   (v)->data[0].v_pointer
#define g_marshal_value_peek_variant(v)  (v)->data[0].v_pointer
#endif /* !G_ENABLE_DEBUG */


/* VOID:VOID (./gmarshal.list:6) */
void
g_cclosure_marshal_VOID__VOID (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__VOID) (gpointer     data1,
                                           gpointer     data2);
  GMarshalFunc_VOID__VOID callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 1);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__VOID) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            data2);
}
void
g_cclosure_marshal_VOID__VOIDv (GClosure     *closure,
                                GValue       *return_value,
                                gpointer      instance,
                                va_list       args,
                                gpointer      marshal_data,
                                int           n_params,
                                GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__VOID) (gpointer     instance,
                                           gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__VOID callback;

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__VOID) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            data2);
}


/* VOID:BOOLEAN (./gmarshal.list:7) */
void
g_cclosure_marshal_VOID__BOOLEAN (GClosure     *closure,
                                  GValue       *return_value G_GNUC_UNUSED,
                                  guint         n_param_values,
                                  const GValue *param_values,
                                  gpointer      invocation_hint G_GNUC_UNUSED,
                                  gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__BOOLEAN) (gpointer     data1,
                                              gboolean     arg_1,
                                              gpointer     data2);
  GMarshalFunc_VOID__BOOLEAN callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__BOOLEAN) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_boolean (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__BOOLEANv (GClosure     *closure,
                                   GValue       *return_value,
                                   gpointer      instance,
                                   va_list       args,
                                   gpointer      marshal_data,
                                   int           n_params,
                                   GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__BOOLEAN) (gpointer     instance,
                                              gboolean     arg_0,
                                              gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__BOOLEAN callback;
  gboolean arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (gboolean) va_arg (args_copy, gboolean);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__BOOLEAN) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
}


/* VOID:CHAR (./gmarshal.list:8) */
void
g_cclosure_marshal_VOID__CHAR (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__CHAR) (gpointer     data1,
                                           gchar        arg_1,
                                           gpointer     data2);
  GMarshalFunc_VOID__CHAR callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__CHAR) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_char (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__CHARv (GClosure     *closure,
                                GValue       *return_value,
                                gpointer      instance,
                                va_list       args,
                                gpointer      marshal_data,
                                int           n_params,
                                GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__CHAR) (gpointer     instance,
                                           gchar        arg_0,
                                           gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__CHAR callback;
  gchar arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (gchar) va_arg (args_copy, gint);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__CHAR) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
}


/* VOID:UCHAR (./gmarshal.list:9) */
void
g_cclosure_marshal_VOID__UCHAR (GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__UCHAR) (gpointer     data1,
                                            guchar       arg_1,
                                            gpointer     data2);
  GMarshalFunc_VOID__UCHAR callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__UCHAR) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_uchar (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__UCHARv (GClosure     *closure,
                                 GValue       *return_value,
                                 gpointer      instance,
                                 va_list       args,
                                 gpointer      marshal_data,
                                 int           n_params,
                                 GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__UCHAR) (gpointer     instance,
                                            guchar       arg_0,
                                            gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__UCHAR callback;
  guchar arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (guchar) va_arg (args_copy, guint);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__UCHAR) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
}


/* VOID:INT (./gmarshal.list:10) */
void
g_cclosure_marshal_VOID__INT (GClosure     *closure,
                              GValue       *return_value G_GNUC_UNUSED,
                              guint         n_param_values,
                              const GValue *param_values,
                              gpointer      invocation_hint G_GNUC_UNUSED,
                              gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__INT) (gpointer     data1,
                                          gint         arg_1,
                                          gpointer     data2);
  GMarshalFunc_VOID__INT callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__INT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_int (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__INTv (GClosure     *closure,
                               GValue       *return_value,
                               gpointer      instance,
                               va_list       args,
                               gpointer      marshal_data,
                               int           n_params,
                               GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__INT) (gpointer     instance,
                                          gint         arg_0,
                                          gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__INT callback;
  gint arg0;
  va_list args_copy;

 G_VA_COPY (args_copy, args);
  arg0 = (gint) va_arg (args_copy, gint);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__INT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
}


/* VOID:UINT (./gmarshal.list:11) */
void
g_cclosure_marshal_VOID__UINT (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__UINT) (gpointer     data1,
                                           guint        arg_1,
                                           gpointer     data2);
  GMarshalFunc_VOID__UINT callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__UINT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_uint (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__UINTv (GClosure     *closure,
                                GValue       *return_value,
                                gpointer      instance,
                                va_list       args,
                                gpointer      marshal_data,
                                int           n_params,
                                GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__UINT) (gpointer     instance,
                                           guint        arg_0,
                                           gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__UINT callback;
  guint arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (guint) va_arg (args_copy, guint);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__UINT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
}


/* VOID:LONG (./gmarshal.list:12) */
void
g_cclosure_marshal_VOID__LONG (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__LONG) (gpointer     data1,
                                           glong        arg_1,
                                           gpointer     data2);
  GMarshalFunc_VOID__LONG callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__LONG) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_long (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__LONGv (GClosure     *closure,
                                GValue       *return_value,
                                gpointer      instance,
                                va_list       args,
                                gpointer      marshal_data,
                                int           n_params,
                                GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__LONG) (gpointer     instance,
                                           glong        arg_0,
                                           gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__LONG callback;
  glong arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (glong) va_arg (args_copy, glong);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__LONG) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
}


/* VOID:ULONG (./gmarshal.list:13) */
void
g_cclosure_marshal_VOID__ULONG (GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__ULONG) (gpointer     data1,
                                            gulong       arg_1,
                                            gpointer     data2);
  GMarshalFunc_VOID__ULONG callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__ULONG) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_ulong (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__ULONGv (GClosure     *closure,
                                 GValue       *return_value,
                                 gpointer      instance,
                                 va_list       args,
                                 gpointer      marshal_data,
                                 int           n_params,
                                 GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__ULONG) (gpointer     instance,
                                            gulong       arg_0,
                                            gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__ULONG callback;
  gulong arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (gulong) va_arg (args_copy, gulong);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__ULONG) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
}


/* VOID:ENUM (./gmarshal.list:14) */
void
g_cclosure_marshal_VOID__ENUM (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__ENUM) (gpointer     data1,
                                           gint         arg_1,
                                           gpointer     data2);
  GMarshalFunc_VOID__ENUM callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__ENUM) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_enum (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__ENUMv (GClosure     *closure,
                                GValue       *return_value,
                                gpointer      instance,
                                va_list       args,
                                gpointer      marshal_data,
                                int           n_params,
                                GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__ENUM) (gpointer     instance,
                                           gint         arg_0,
                                           gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__ENUM callback;
  gint arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (gint) va_arg (args_copy, gint);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__ENUM) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
}


/* VOID:FLAGS (./gmarshal.list:15) */
void
g_cclosure_marshal_VOID__FLAGS (GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__FLAGS) (gpointer     data1,
                                            guint        arg_1,
                                            gpointer     data2);
  GMarshalFunc_VOID__FLAGS callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__FLAGS) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_flags (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__FLAGSv (GClosure     *closure,
                                 GValue       *return_value,
                                 gpointer      instance,
                                 va_list       args,
                                 gpointer      marshal_data,
                                 int           n_params,
                                 GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__FLAGS) (gpointer     instance,
                                            guint        arg_0,
                                            gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__FLAGS callback;
  guint arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (guint) va_arg (args_copy, guint);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__FLAGS) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
}


/* VOID:FLOAT (./gmarshal.list:16) */
void
g_cclosure_marshal_VOID__FLOAT (GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__FLOAT) (gpointer     data1,
                                            gfloat       arg_1,
                                            gpointer     data2);
  GMarshalFunc_VOID__FLOAT callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__FLOAT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_float (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__FLOATv (GClosure     *closure,
                                 GValue       *return_value,
                                 gpointer      instance,
                                 va_list       args,
                                 gpointer      marshal_data,
                                 int           n_params,
                                 GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__FLOAT) (gpointer     instance,
                                            gfloat       arg_0,
                                            gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__FLOAT callback;
  gfloat arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (gfloat) va_arg (args_copy, gdouble);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__FLOAT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
}


/* VOID:DOUBLE (./gmarshal.list:17) */
void
g_cclosure_marshal_VOID__DOUBLE (GClosure     *closure,
                                 GValue       *return_value G_GNUC_UNUSED,
                                 guint         n_param_values,
                                 const GValue *param_values,
                                 gpointer      invocation_hint G_GNUC_UNUSED,
                                 gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__DOUBLE) (gpointer     data1,
                                             gdouble      arg_1,
                                             gpointer     data2);
  GMarshalFunc_VOID__DOUBLE callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__DOUBLE) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_double (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__DOUBLEv (GClosure     *closure,
                                  GValue       *return_value,
                                  gpointer      instance,
                                  va_list       args,
                                  gpointer      marshal_data,
                                  int           n_params,
                                  GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__DOUBLE) (gpointer     instance,
                                             gdouble      arg_0,
                                             gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__DOUBLE callback;
  gdouble arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (gdouble) va_arg (args_copy, gdouble);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__DOUBLE) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
}


/* VOID:STRING (./gmarshal.list:18) */
void
g_cclosure_marshal_VOID__STRING (GClosure     *closure,
                                 GValue       *return_value G_GNUC_UNUSED,
                                 guint         n_param_values,
                                 const GValue *param_values,
                                 gpointer      invocation_hint G_GNUC_UNUSED,
                                 gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__STRING) (gpointer     data1,
                                             gpointer     arg_1,
                                             gpointer     data2);
  GMarshalFunc_VOID__STRING callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__STRING) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_string (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__STRINGv (GClosure     *closure,
                                  GValue       *return_value,
                                  gpointer      instance,
                                  va_list       args,
                                  gpointer      marshal_data,
                                  int           n_params,
                                  GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__STRING) (gpointer     instance,
                                             gpointer     arg_0,
                                             gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__STRING callback;
  gpointer arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (gpointer) va_arg (args_copy, gpointer);
  if ((param_types[0] & G_SIGNAL_TYPE_STATIC_SCOPE) == 0 && arg0 != NULL)
    arg0 = g_strdup (arg0);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__STRING) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
  if ((param_types[0] & G_SIGNAL_TYPE_STATIC_SCOPE) == 0 && arg0 != NULL)
    g_free (arg0);
}


/* VOID:PARAM (./gmarshal.list:19) */
void
g_cclosure_marshal_VOID__PARAM (GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__PARAM) (gpointer     data1,
                                            gpointer     arg_1,
                                            gpointer     data2);
  GMarshalFunc_VOID__PARAM callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__PARAM) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_param (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__PARAMv (GClosure     *closure,
                                 GValue       *return_value,
                                 gpointer      instance,
                                 va_list       args,
                                 gpointer      marshal_data,
                                 int           n_params,
                                 GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__PARAM) (gpointer     instance,
                                            gpointer     arg_0,
                                            gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__PARAM callback;
  gpointer arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (gpointer) va_arg (args_copy, gpointer);
  if ((param_types[0] & G_SIGNAL_TYPE_STATIC_SCOPE) == 0 && arg0 != NULL)
    arg0 = g_param_spec_ref (arg0);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__PARAM) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
  if ((param_types[0] & G_SIGNAL_TYPE_STATIC_SCOPE) == 0 && arg0 != NULL)
    g_param_spec_unref (arg0);
}


/* VOID:BOXED (./gmarshal.list:20) */
void
g_cclosure_marshal_VOID__BOXED (GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__BOXED) (gpointer     data1,
                                            gpointer     arg_1,
                                            gpointer     data2);
  GMarshalFunc_VOID__BOXED callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__BOXED) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_boxed (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__BOXEDv (GClosure     *closure,
                                 GValue       *return_value,
                                 gpointer      instance,
                                 va_list       args,
                                 gpointer      marshal_data,
                                 int           n_params,
                                 GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__BOXED) (gpointer     instance,
                                            gpointer     arg_0,
                                            gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__BOXED callback;
  gpointer arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (gpointer) va_arg (args_copy, gpointer);
  if ((param_types[0] & G_SIGNAL_TYPE_STATIC_SCOPE) == 0 && arg0 != NULL)
    arg0 = g_boxed_copy (param_types[0] & ~G_SIGNAL_TYPE_STATIC_SCOPE, arg0);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__BOXED) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
  if ((param_types[0] & G_SIGNAL_TYPE_STATIC_SCOPE) == 0 && arg0 != NULL)
    g_boxed_free (param_types[0] & ~G_SIGNAL_TYPE_STATIC_SCOPE, arg0);
}


/* VOID:POINTER (./gmarshal.list:21) */
void
g_cclosure_marshal_VOID__POINTER (GClosure     *closure,
                                  GValue       *return_value G_GNUC_UNUSED,
                                  guint         n_param_values,
                                  const GValue *param_values,
                                  gpointer      invocation_hint G_GNUC_UNUSED,
                                  gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__POINTER) (gpointer     data1,
                                              gpointer     arg_1,
                                              gpointer     data2);
  GMarshalFunc_VOID__POINTER callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__POINTER) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_pointer (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__POINTERv (GClosure     *closure,
                                   GValue       *return_value,
                                   gpointer      instance,
                                   va_list       args,
                                   gpointer      marshal_data,
                                   int           n_params,
                                   GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__POINTER) (gpointer     instance,
                                              gpointer     arg_0,
                                              gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__POINTER callback;
  gpointer arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (gpointer) va_arg (args_copy, gpointer);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__POINTER) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
}


/* VOID:OBJECT (./gmarshal.list:22) */
void
g_cclosure_marshal_VOID__OBJECT (GClosure     *closure,
                                 GValue       *return_value G_GNUC_UNUSED,
                                 guint         n_param_values,
                                 const GValue *param_values,
                                 gpointer      invocation_hint G_GNUC_UNUSED,
                                 gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__OBJECT) (gpointer     data1,
                                             gpointer     arg_1,
                                             gpointer     data2);
  GMarshalFunc_VOID__OBJECT callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__OBJECT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_object (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__OBJECTv (GClosure     *closure,
                                  GValue       *return_value,
                                  gpointer      instance,
                                  va_list       args,
                                  gpointer      marshal_data,
                                  int           n_params,
                                  GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__OBJECT) (gpointer     instance,
                                             gpointer     arg_0,
                                             gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__OBJECT callback;
  gpointer arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (gpointer) va_arg (args_copy, gpointer);
  if (arg0 != NULL)
    arg0 = g_object_ref (arg0);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__OBJECT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
  if (arg0 != NULL)
    g_object_unref (arg0);
}


/* VOID:VARIANT (./gmarshal.list:23) */
void
g_cclosure_marshal_VOID__VARIANT (GClosure     *closure,
                                  GValue       *return_value G_GNUC_UNUSED,
                                  guint         n_param_values,
                                  const GValue *param_values,
                                  gpointer      invocation_hint G_GNUC_UNUSED,
                                  gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__VARIANT) (gpointer     data1,
                                              gpointer     arg_1,
                                              gpointer     data2);
  GMarshalFunc_VOID__VARIANT callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__VARIANT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_variant (param_values + 1),
            data2);
}
void
g_cclosure_marshal_VOID__VARIANTv (GClosure     *closure,
                                   GValue       *return_value,
                                   gpointer      instance,
                                   va_list       args,
                                   gpointer      marshal_data,
                                   int           n_params,
                                   GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__VARIANT) (gpointer     instance,
                                              gpointer     arg_0,
                                              gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__VARIANT callback;
  gpointer arg0;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (gpointer) va_arg (args_copy, gpointer);
  if ((param_types[0] & G_SIGNAL_TYPE_STATIC_SCOPE) == 0 && arg0 != NULL)
    arg0 = g_variant_ref_sink (arg0);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__VARIANT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            data2);
  if ((param_types[0] & G_SIGNAL_TYPE_STATIC_SCOPE) == 0 && arg0 != NULL)
    g_variant_unref (arg0);
}


/* VOID:UINT,POINTER (./gmarshal.list:26) */
void
g_cclosure_marshal_VOID__UINT_POINTER (GClosure     *closure,
                                       GValue       *return_value G_GNUC_UNUSED,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint G_GNUC_UNUSED,
                                       gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__UINT_POINTER) (gpointer     data1,
                                                   guint        arg_1,
                                                   gpointer     arg_2,
                                                   gpointer     data2);
  GMarshalFunc_VOID__UINT_POINTER callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__UINT_POINTER) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_uint (param_values + 1),
            g_marshal_value_peek_pointer (param_values + 2),
            data2);
}
void
g_cclosure_marshal_VOID__UINT_POINTERv (GClosure     *closure,
                                        GValue       *return_value,
                                        gpointer      instance,
                                        va_list       args,
                                        gpointer      marshal_data,
                                        int           n_params,
                                        GType        *param_types)
{
  typedef void (*GMarshalFunc_VOID__UINT_POINTER) (gpointer     instance,
                                                   guint        arg_0,
                                                   gpointer     arg_1,
                                                   gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_VOID__UINT_POINTER callback;
  guint arg0;
  gpointer arg1;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  arg0 = (guint) va_arg (args_copy, guint);
  arg1 = (gpointer) va_arg (args_copy, gpointer);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__UINT_POINTER) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            arg0,
            arg1,
            data2);
}


/* BOOL:FLAGS (./gmarshal.list:27) */
void
g_cclosure_marshal_BOOLEAN__FLAGS (GClosure     *closure,
                                   GValue       *return_value G_GNUC_UNUSED,
                                   guint         n_param_values,
                                   const GValue *param_values,
                                   gpointer      invocation_hint G_GNUC_UNUSED,
                                   gpointer      marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__FLAGS) (gpointer     data1,
                                                   guint        arg_1,
                                                   gpointer     data2);
  GMarshalFunc_BOOLEAN__FLAGS callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  gboolean v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__FLAGS) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       g_marshal_value_peek_flags (param_values + 1),
                       data2);

  g_value_set_boolean (return_value, v_return);
}
void
g_cclosure_marshal_BOOLEAN__FLAGSv (GClosure     *closure,
                                    GValue       *return_value,
                                    gpointer      instance,
                                    va_list       args,
                                    gpointer      marshal_data,
                                    int           n_params,
                                    GType        *param_types)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__FLAGS) (gpointer     instance,
                                                   guint        arg_0,
                                                   gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_BOOLEAN__FLAGS callback;
  guint arg0;
  va_list args_copy;
  gboolean v_return;

  g_return_if_fail (return_value != NULL);

  G_VA_COPY (args_copy, args);
  arg0 = (guint) va_arg (args_copy, guint);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__FLAGS) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       arg0,
                       data2);

  g_value_set_boolean (return_value, v_return);
}


/* STRING:OBJECT,POINTER (./gmarshal.list:28) */
void
g_cclosure_marshal_STRING__OBJECT_POINTER (GClosure     *closure,
                                           GValue       *return_value G_GNUC_UNUSED,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint G_GNUC_UNUSED,
                                           gpointer      marshal_data)
{
  typedef gchar* (*GMarshalFunc_STRING__OBJECT_POINTER) (gpointer     data1,
                                                         gpointer     arg_1,
                                                         gpointer     arg_2,
                                                         gpointer     data2);
  GMarshalFunc_STRING__OBJECT_POINTER callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  gchar* v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_STRING__OBJECT_POINTER) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       g_marshal_value_peek_object (param_values + 1),
                       g_marshal_value_peek_pointer (param_values + 2),
                       data2);

  g_value_take_string (return_value, v_return);
}
void
g_cclosure_marshal_STRING__OBJECT_POINTERv (GClosure     *closure,
                                            GValue       *return_value,
                                            gpointer      instance,
                                            va_list       args,
                                            gpointer      marshal_data,
                                            int           n_params,
                                            GType        *param_types)
{
  typedef gchar* (*GMarshalFunc_STRING__OBJECT_POINTER) (gpointer     instance,
                                                         gpointer     arg_0,
                                                         gpointer     arg_1,
                                                         gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_STRING__OBJECT_POINTER callback;
  gpointer arg0;
  gpointer arg1;
  va_list args_copy;
  gchar* v_return;

  g_return_if_fail (return_value != NULL);

  G_VA_COPY (args_copy, args);
  arg0 = (gpointer) va_arg (args_copy, gpointer);
  if (arg0 != NULL)
    arg0 = g_object_ref (arg0);
  arg1 = (gpointer) va_arg (args_copy, gpointer);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_STRING__OBJECT_POINTER) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       arg0,
                       arg1,
                       data2);
  if (arg0 != NULL)
    g_object_unref (arg0);

  g_value_take_string (return_value, v_return);
}


/* BOOL:BOXED,BOXED (./gmarshal.list:29) */
void
g_cclosure_marshal_BOOLEAN__BOXED_BOXED (GClosure     *closure,
                                         GValue       *return_value G_GNUC_UNUSED,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint G_GNUC_UNUSED,
                                         gpointer      marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__BOXED_BOXED) (gpointer     data1,
                                                         gpointer     arg_1,
                                                         gpointer     arg_2,
                                                         gpointer     data2);
  GMarshalFunc_BOOLEAN__BOXED_BOXED callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  gboolean v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__BOXED_BOXED) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       g_marshal_value_peek_boxed (param_values + 1),
                       g_marshal_value_peek_boxed (param_values + 2),
                       data2);

  g_value_set_boolean (return_value, v_return);
}
void
g_cclosure_marshal_BOOLEAN__BOXED_BOXEDv (GClosure     *closure,
                                          GValue       *return_value,
                                          gpointer      instance,
                                          va_list       args,
                                          gpointer      marshal_data,
                                          int           n_params,
                                          GType        *param_types)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__BOXED_BOXED) (gpointer     instance,
                                                         gpointer     arg_0,
                                                         gpointer     arg_1,
                                                         gpointer     data);
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;
  GMarshalFunc_BOOLEAN__BOXED_BOXED callback;
  gpointer arg0;
  gpointer arg1;
  va_list args_copy;
  gboolean v_return;

  g_return_if_fail (return_value != NULL);

  G_VA_COPY (args_copy, args);
  arg0 = (gpointer) va_arg (args_copy, gpointer);
  if ((param_types[0] & G_SIGNAL_TYPE_STATIC_SCOPE) == 0 && arg0 != NULL)
    arg0 = g_boxed_copy (param_types[0] & ~G_SIGNAL_TYPE_STATIC_SCOPE, arg0);
  arg1 = (gpointer) va_arg (args_copy, gpointer);
  if ((param_types[1] & G_SIGNAL_TYPE_STATIC_SCOPE) == 0 && arg1 != NULL)
    arg1 = g_boxed_copy (param_types[1] & ~G_SIGNAL_TYPE_STATIC_SCOPE, arg1);
  va_end (args_copy);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = instance;
    }
  else
    {
      data1 = instance;
      data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__BOXED_BOXED) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       arg0,
                       arg1,
                       data2);
  if ((param_types[0] & G_SIGNAL_TYPE_STATIC_SCOPE) == 0 && arg0 != NULL)
    g_boxed_free (param_types[0] & ~G_SIGNAL_TYPE_STATIC_SCOPE, arg0);
  if ((param_types[1] & G_SIGNAL_TYPE_STATIC_SCOPE) == 0 && arg1 != NULL)
    g_boxed_free (param_types[1] & ~G_SIGNAL_TYPE_STATIC_SCOPE, arg1);

  g_value_set_boolean (return_value, v_return);
}


