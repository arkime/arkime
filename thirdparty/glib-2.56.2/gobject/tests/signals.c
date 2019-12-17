#include <glib-object.h>
#include "marshalers.h"

#define g_assert_cmpflags(type,n1, cmp, n2) G_STMT_START { \
                                               type __n1 = (n1), __n2 = (n2); \
                                               if (__n1 cmp __n2) ; else \
                                                 g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                             #n1 " " #cmp " " #n2, __n1, #cmp, __n2, 'i'); \
                                            } G_STMT_END
#define g_assert_cmpenum(type,n1, cmp, n2) G_STMT_START { \
                                               type __n1 = (n1), __n2 = (n2); \
                                               if (__n1 cmp __n2) ; else \
                                                 g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                             #n1 " " #cmp " " #n2, __n1, #cmp, __n2, 'i'); \
                                            } G_STMT_END

typedef enum {
  TEST_ENUM_NEGATIVE = -30,
  TEST_ENUM_NONE = 0,
  TEST_ENUM_FOO = 1,
  TEST_ENUM_BAR = 2
} TestEnum;

typedef enum {
  TEST_UNSIGNED_ENUM_FOO = 1,
  TEST_UNSIGNED_ENUM_BAR = 42
  /* Don't test 0x80000000 for now- nothing appears to do this in
   * practice, and it triggers GValue/GEnum bugs on ppc64.
   */
} TestUnsignedEnum;

static void
custom_marshal_VOID__INVOCATIONHINT (GClosure     *closure,
                                     GValue       *return_value G_GNUC_UNUSED,
                                     guint         n_param_values,
                                     const GValue *param_values,
                                     gpointer      invocation_hint,
                                     gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__INVOCATIONHINT) (gpointer     data1,
                                                     gpointer     invocation_hint,
                                                     gpointer     data2);
  GMarshalFunc_VOID__INVOCATIONHINT callback;
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
  callback = (GMarshalFunc_VOID__INVOCATIONHINT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            invocation_hint,
            data2);
}

static GType
test_enum_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { TEST_ENUM_NEGATIVE, "TEST_ENUM_NEGATIVE", "negative" },
        { TEST_ENUM_NONE, "TEST_ENUM_NONE", "none" },
        { TEST_ENUM_FOO, "TEST_ENUM_FOO", "foo" },
        { TEST_ENUM_BAR, "TEST_ENUM_BAR", "bar" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("TestEnum"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

static GType
test_unsigned_enum_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { TEST_UNSIGNED_ENUM_FOO, "TEST_UNSIGNED_ENUM_FOO", "foo" },
        { TEST_UNSIGNED_ENUM_BAR, "TEST_UNSIGNED_ENUM_BAR", "bar" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("TestUnsignedEnum"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

typedef enum {
  MY_ENUM_VALUE = 1,
} MyEnum;

static const GEnumValue my_enum_values[] =
{
  { MY_ENUM_VALUE, "the first value", "one" },
  { 0, NULL, NULL }
};

typedef enum {
  MY_FLAGS_FIRST_BIT = (1 << 0),
  MY_FLAGS_THIRD_BIT = (1 << 2),
  MY_FLAGS_LAST_BIT = (1 << 31)
} MyFlags;

static const GFlagsValue my_flag_values[] =
{
  { MY_FLAGS_FIRST_BIT, "the first bit", "first-bit" },
  { MY_FLAGS_THIRD_BIT, "the third bit", "third-bit" },
  { MY_FLAGS_LAST_BIT, "the last bit", "last-bit" },
  { 0, NULL, NULL }
};

static GType enum_type;
static GType flags_type;

static guint simple_id;
static guint simple2_id;

typedef struct _Test Test;
typedef struct _TestClass TestClass;

struct _Test
{
  GObject parent_instance;
};

static void all_types_handler (Test *test, int i, gboolean b, char c, guchar uc, guint ui, glong l, gulong ul, MyEnum e, MyFlags f, float fl, double db, char *str, GParamSpec *param, GBytes *bytes, gpointer ptr, Test *obj, GVariant *var, gint64 i64, guint64 ui64);

struct _TestClass
{
  GObjectClass parent_class;

  void (* variant_changed) (Test *, GVariant *);
  void (* all_types) (Test *test, int i, gboolean b, char c, guchar uc, guint ui, glong l, gulong ul, MyEnum e, MyFlags f, float fl, double db, char *str, GParamSpec *param, GBytes *bytes, gpointer ptr, Test *obj, GVariant *var, gint64 i64, guint64 ui64);
  void (* all_types_null) (Test *test, int i, gboolean b, char c, guchar uc, guint ui, glong l, gulong ul, MyEnum e, MyFlags f, float fl, double db, char *str, GParamSpec *param, GBytes *bytes, gpointer ptr, Test *obj, GVariant *var, gint64 i64, guint64 ui64);
};

static GType test_get_type (void);
G_DEFINE_TYPE (Test, test, G_TYPE_OBJECT)

static void
test_init (Test *test)
{
}

static void
test_class_init (TestClass *klass)
{
  guint s;

  enum_type = g_enum_register_static ("MyEnum", my_enum_values);
  flags_type = g_flags_register_static ("MyFlag", my_flag_values);

  klass->all_types = all_types_handler;

  simple_id = g_signal_new ("simple",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                0);
  simple2_id = g_signal_new ("simple-2",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                0);
  g_signal_new ("generic-marshaller-1",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                7,
                G_TYPE_CHAR, G_TYPE_UCHAR, G_TYPE_INT, G_TYPE_LONG, G_TYPE_POINTER, G_TYPE_DOUBLE, G_TYPE_FLOAT);
  g_signal_new ("generic-marshaller-2",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                5,
                G_TYPE_INT, test_enum_get_type(), G_TYPE_INT, test_unsigned_enum_get_type (), G_TYPE_INT);
  g_signal_new ("generic-marshaller-enum-return-signed",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                test_enum_get_type(),
                0);
  g_signal_new ("generic-marshaller-enum-return-unsigned",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                test_unsigned_enum_get_type(),
                0);
  g_signal_new ("generic-marshaller-int-return",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_INT,
                0);
  s = g_signal_new ("va-marshaller-int-return",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                test_INT__VOID,
                G_TYPE_INT,
                0);
  g_signal_set_va_marshaller (s, G_TYPE_FROM_CLASS (klass),
			      test_INT__VOIDv);
  g_signal_new ("generic-marshaller-uint-return",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_UINT,
                0);
  s = g_signal_new ("va-marshaller-uint-return",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                test_INT__VOID,
                G_TYPE_UINT,
                0);
  g_signal_set_va_marshaller (s, G_TYPE_FROM_CLASS (klass),
			      test_UINT__VOIDv);
  g_signal_new ("custom-marshaller",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                custom_marshal_VOID__INVOCATIONHINT,
                G_TYPE_NONE,
                1,
                G_TYPE_POINTER);
  g_signal_new ("variant-changed-no-slot",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VARIANT,
                G_TYPE_NONE,
                1,
                G_TYPE_VARIANT);
  g_signal_new ("variant-changed",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                G_STRUCT_OFFSET (TestClass, variant_changed),
                NULL, NULL,
                g_cclosure_marshal_VOID__VARIANT,
                G_TYPE_NONE,
                1,
                G_TYPE_VARIANT);
  g_signal_new ("all-types",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (TestClass, all_types),
                NULL, NULL,
                test_VOID__INT_BOOLEAN_CHAR_UCHAR_UINT_LONG_ULONG_ENUM_FLAGS_FLOAT_DOUBLE_STRING_PARAM_BOXED_POINTER_OBJECT_VARIANT_INT64_UINT64,
                G_TYPE_NONE,
                19,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_UINT,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		enum_type,
		flags_type,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_STRING,
		G_TYPE_PARAM_LONG,
		G_TYPE_BYTES,
		G_TYPE_POINTER,
		test_get_type (),
                G_TYPE_VARIANT,
		G_TYPE_INT64,
		G_TYPE_UINT64);
  s = g_signal_new ("all-types-va",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (TestClass, all_types),
                NULL, NULL,
                test_VOID__INT_BOOLEAN_CHAR_UCHAR_UINT_LONG_ULONG_ENUM_FLAGS_FLOAT_DOUBLE_STRING_PARAM_BOXED_POINTER_OBJECT_VARIANT_INT64_UINT64,
                G_TYPE_NONE,
                19,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_UINT,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		enum_type,
		flags_type,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_STRING,
		G_TYPE_PARAM_LONG,
		G_TYPE_BYTES,
		G_TYPE_POINTER,
		test_get_type (),
                G_TYPE_VARIANT,
		G_TYPE_INT64,
		G_TYPE_UINT64);
  g_signal_set_va_marshaller (s, G_TYPE_FROM_CLASS (klass),
			      test_VOID__INT_BOOLEAN_CHAR_UCHAR_UINT_LONG_ULONG_ENUM_FLAGS_FLOAT_DOUBLE_STRING_PARAM_BOXED_POINTER_OBJECT_VARIANT_INT64_UINT64v);

  g_signal_new ("all-types-generic",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (TestClass, all_types),
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                19,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_UINT,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		enum_type,
		flags_type,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_STRING,
		G_TYPE_PARAM_LONG,
		G_TYPE_BYTES,
		G_TYPE_POINTER,
		test_get_type (),
                G_TYPE_VARIANT,
		G_TYPE_INT64,
		G_TYPE_UINT64);
  g_signal_new ("all-types-null",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (TestClass, all_types_null),
                NULL, NULL,
                test_VOID__INT_BOOLEAN_CHAR_UCHAR_UINT_LONG_ULONG_ENUM_FLAGS_FLOAT_DOUBLE_STRING_PARAM_BOXED_POINTER_OBJECT_VARIANT_INT64_UINT64,
                G_TYPE_NONE,
                19,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_UINT,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		enum_type,
		flags_type,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_STRING,
		G_TYPE_PARAM_LONG,
		G_TYPE_BYTES,
		G_TYPE_POINTER,
		test_get_type (),
                G_TYPE_VARIANT,
		G_TYPE_INT64,
		G_TYPE_UINT64);
  g_signal_new ("all-types-empty",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                test_VOID__INT_BOOLEAN_CHAR_UCHAR_UINT_LONG_ULONG_ENUM_FLAGS_FLOAT_DOUBLE_STRING_PARAM_BOXED_POINTER_OBJECT_VARIANT_INT64_UINT64,
                G_TYPE_NONE,
                19,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_UINT,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		enum_type,
		flags_type,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_STRING,
		G_TYPE_PARAM_LONG,
		G_TYPE_BYTES,
		G_TYPE_POINTER,
		test_get_type (),
                G_TYPE_VARIANT,
		G_TYPE_INT64,
		G_TYPE_UINT64);
}

typedef struct _Test Test2;
typedef struct _TestClass Test2Class;

static GType test2_get_type (void);
G_DEFINE_TYPE (Test2, test2, G_TYPE_OBJECT)

static void
test2_init (Test2 *test)
{
}

static void
test2_class_init (Test2Class *klass)
{
}

static void
test_variant_signal (void)
{
  Test *test;
  GVariant *v;

  /* Tests that the signal emission consumes the variant,
   * even if there are no handlers connected.
   */

  test = g_object_new (test_get_type (), NULL);

  v = g_variant_new_boolean (TRUE);
  g_variant_ref (v);
  g_assert (g_variant_is_floating (v));
  g_signal_emit_by_name (test, "variant-changed-no-slot", v);
  g_assert (!g_variant_is_floating (v));
  g_variant_unref (v);

  v = g_variant_new_boolean (TRUE);
  g_variant_ref (v);
  g_assert (g_variant_is_floating (v));
  g_signal_emit_by_name (test, "variant-changed", v);
  g_assert (!g_variant_is_floating (v));
  g_variant_unref (v);

  g_object_unref (test);
}

static void
on_generic_marshaller_1 (Test *obj,
			 gint8 v_schar,
			 guint8 v_uchar,
			 gint v_int,
			 glong v_long,
			 gpointer v_pointer,
			 gdouble v_double,
			 gfloat v_float,
			 gpointer user_data)
{
  g_assert_cmpint (v_schar, ==, 42);
  g_assert_cmpint (v_uchar, ==, 43);
  g_assert_cmpint (v_int, ==, 4096);
  g_assert_cmpint (v_long, ==, 8192);
  g_assert (v_pointer == NULL);
  g_assert_cmpfloat (v_double, >, 0.0);
  g_assert_cmpfloat (v_double, <, 1.0);
  g_assert_cmpfloat (v_float, >, 5.0);
  g_assert_cmpfloat (v_float, <, 6.0);
}
			 
static void
test_generic_marshaller_signal_1 (void)
{
  Test *test;
  test = g_object_new (test_get_type (), NULL);

  g_signal_connect (test, "generic-marshaller-1", G_CALLBACK (on_generic_marshaller_1), NULL);

  g_signal_emit_by_name (test, "generic-marshaller-1", 42, 43, 4096, 8192, NULL, 0.5, 5.5);

  g_object_unref (test);
}

static void
on_generic_marshaller_2 (Test *obj,
			 gint        v_int1,
			 TestEnum    v_enum,
			 gint        v_int2,
			 TestUnsignedEnum v_uenum,
			 gint        v_int3)
{
  g_assert_cmpint (v_int1, ==, 42);
  g_assert_cmpint (v_enum, ==, TEST_ENUM_BAR);
  g_assert_cmpint (v_int2, ==, 43);
  g_assert_cmpint (v_uenum, ==, TEST_UNSIGNED_ENUM_BAR);
  g_assert_cmpint (v_int3, ==, 44);
}

static void
test_generic_marshaller_signal_2 (void)
{
  Test *test;
  test = g_object_new (test_get_type (), NULL);

  g_signal_connect (test, "generic-marshaller-2", G_CALLBACK (on_generic_marshaller_2), NULL);

  g_signal_emit_by_name (test, "generic-marshaller-2", 42, TEST_ENUM_BAR, 43, TEST_UNSIGNED_ENUM_BAR, 44);

  g_object_unref (test);
}

static TestEnum
on_generic_marshaller_enum_return_signed_1 (Test *obj)
{
  return TEST_ENUM_NEGATIVE;
}

static TestEnum
on_generic_marshaller_enum_return_signed_2 (Test *obj)
{
  return TEST_ENUM_BAR;
}

static void
test_generic_marshaller_signal_enum_return_signed (void)
{
  Test *test;
  guint id;
  TestEnum retval = 0;

  test = g_object_new (test_get_type (), NULL);

  /* Test return value NEGATIVE */
  id = g_signal_connect (test,
                         "generic-marshaller-enum-return-signed",
                         G_CALLBACK (on_generic_marshaller_enum_return_signed_1),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-enum-return-signed", &retval);
  g_assert_cmpint (retval, ==, TEST_ENUM_NEGATIVE);
  g_signal_handler_disconnect (test, id);

  /* Test return value BAR */
  retval = 0;
  id = g_signal_connect (test,
                         "generic-marshaller-enum-return-signed",
                         G_CALLBACK (on_generic_marshaller_enum_return_signed_2),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-enum-return-signed", &retval);
  g_assert_cmpint (retval, ==, TEST_ENUM_BAR);
  g_signal_handler_disconnect (test, id);

  g_object_unref (test);
}

static TestUnsignedEnum
on_generic_marshaller_enum_return_unsigned_1 (Test *obj)
{
  return TEST_UNSIGNED_ENUM_FOO;
}

static TestUnsignedEnum
on_generic_marshaller_enum_return_unsigned_2 (Test *obj)
{
  return TEST_UNSIGNED_ENUM_BAR;
}

static void
test_generic_marshaller_signal_enum_return_unsigned (void)
{
  Test *test;
  guint id;
  TestUnsignedEnum retval = 0;

  test = g_object_new (test_get_type (), NULL);

  /* Test return value FOO */
  id = g_signal_connect (test,
                         "generic-marshaller-enum-return-unsigned",
                         G_CALLBACK (on_generic_marshaller_enum_return_unsigned_1),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-enum-return-unsigned", &retval);
  g_assert_cmpint (retval, ==, TEST_UNSIGNED_ENUM_FOO);
  g_signal_handler_disconnect (test, id);

  /* Test return value BAR */
  retval = 0;
  id = g_signal_connect (test,
                         "generic-marshaller-enum-return-unsigned",
                         G_CALLBACK (on_generic_marshaller_enum_return_unsigned_2),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-enum-return-unsigned", &retval);
  g_assert_cmpint (retval, ==, TEST_UNSIGNED_ENUM_BAR);
  g_signal_handler_disconnect (test, id);

  g_object_unref (test);
}

/**********************/

static gint
on_generic_marshaller_int_return_signed_1 (Test *obj)
{
  return -30;
}

static gint
on_generic_marshaller_int_return_signed_2 (Test *obj)
{
  return 2;
}

static void
test_generic_marshaller_signal_int_return (void)
{
  Test *test;
  guint id;
  gint retval = 0;

  test = g_object_new (test_get_type (), NULL);

  /* Test return value -30 */
  id = g_signal_connect (test,
                         "generic-marshaller-int-return",
                         G_CALLBACK (on_generic_marshaller_int_return_signed_1),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-int-return", &retval);
  g_assert_cmpint (retval, ==, -30);
  g_signal_handler_disconnect (test, id);

  /* Test return value positive */
  retval = 0;
  id = g_signal_connect (test,
                         "generic-marshaller-int-return",
                         G_CALLBACK (on_generic_marshaller_int_return_signed_2),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-int-return", &retval);
  g_assert_cmpint (retval, ==, 2);
  g_signal_handler_disconnect (test, id);

  /* Same test for va marshaller */

  /* Test return value -30 */
  id = g_signal_connect (test,
                         "va-marshaller-int-return",
                         G_CALLBACK (on_generic_marshaller_int_return_signed_1),
                         NULL);
  g_signal_emit_by_name (test, "va-marshaller-int-return", &retval);
  g_assert_cmpint (retval, ==, -30);
  g_signal_handler_disconnect (test, id);

  /* Test return value positive */
  retval = 0;
  id = g_signal_connect (test,
                         "va-marshaller-int-return",
                         G_CALLBACK (on_generic_marshaller_int_return_signed_2),
                         NULL);
  g_signal_emit_by_name (test, "va-marshaller-int-return", &retval);
  g_assert_cmpint (retval, ==, 2);
  g_signal_handler_disconnect (test, id);

  g_object_unref (test);
}

static guint
on_generic_marshaller_uint_return_1 (Test *obj)
{
  return 1;
}

static guint
on_generic_marshaller_uint_return_2 (Test *obj)
{
  return G_MAXUINT;
}

static void
test_generic_marshaller_signal_uint_return (void)
{
  Test *test;
  guint id;
  guint retval = 0;

  test = g_object_new (test_get_type (), NULL);

  id = g_signal_connect (test,
                         "generic-marshaller-uint-return",
                         G_CALLBACK (on_generic_marshaller_uint_return_1),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-uint-return", &retval);
  g_assert_cmpint (retval, ==, 1);
  g_signal_handler_disconnect (test, id);

  retval = 0;
  id = g_signal_connect (test,
                         "generic-marshaller-uint-return",
                         G_CALLBACK (on_generic_marshaller_uint_return_2),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-uint-return", &retval);
  g_assert_cmpint (retval, ==, G_MAXUINT);
  g_signal_handler_disconnect (test, id);

  /* Same test for va marshaller */

  id = g_signal_connect (test,
                         "va-marshaller-uint-return",
                         G_CALLBACK (on_generic_marshaller_uint_return_1),
                         NULL);
  g_signal_emit_by_name (test, "va-marshaller-uint-return", &retval);
  g_assert_cmpint (retval, ==, 1);
  g_signal_handler_disconnect (test, id);

  retval = 0;
  id = g_signal_connect (test,
                         "va-marshaller-uint-return",
                         G_CALLBACK (on_generic_marshaller_uint_return_2),
                         NULL);
  g_signal_emit_by_name (test, "va-marshaller-uint-return", &retval);
  g_assert_cmpint (retval, ==, G_MAXUINT);
  g_signal_handler_disconnect (test, id);

  g_object_unref (test);
}

static const GSignalInvocationHint dont_use_this = { 0, };

static void
custom_marshaller_callback (Test                  *test,
                            GSignalInvocationHint *hint,
                            gpointer               unused)
{
  GSignalInvocationHint *ihint;

  g_assert (hint != &dont_use_this);

  ihint = g_signal_get_invocation_hint (test);

  g_assert_cmpuint (hint->signal_id, ==, ihint->signal_id);
  g_assert_cmpuint (hint->detail , ==, ihint->detail);
  g_assert_cmpflags (GSignalFlags, hint->run_type, ==, ihint->run_type); 
}

static void
test_custom_marshaller (void)
{
  Test *test;

  test = g_object_new (test_get_type (), NULL);

  g_signal_connect (test,
                    "custom-marshaller",
                    G_CALLBACK (custom_marshaller_callback),
                    NULL);

  g_signal_emit_by_name (test, "custom-marshaller", &dont_use_this);

  g_object_unref (test);
}

static int all_type_handlers_count = 0;

static void
all_types_handler (Test *test, int i, gboolean b, char c, guchar uc, guint ui, glong l, gulong ul, MyEnum e, MyFlags f, float fl, double db, char *str, GParamSpec *param, GBytes *bytes, gpointer ptr, Test *obj, GVariant *var, gint64 i64, guint64 ui64)
{
  all_type_handlers_count++;

  g_assert_cmpint (i, ==, 42);
  g_assert_cmpint (b, ==, TRUE);
  g_assert_cmpint (c, ==, 17);
  g_assert_cmpuint (uc, ==, 140);
  g_assert_cmpuint (ui, ==, G_MAXUINT - 42);
  g_assert_cmpint (l, ==, -1117);
  g_assert_cmpuint (ul, ==, G_MAXULONG - 999);
  g_assert_cmpenum (MyEnum, e, ==, MY_ENUM_VALUE);
  g_assert_cmpflags (MyFlags, f, ==, MY_FLAGS_FIRST_BIT | MY_FLAGS_THIRD_BIT | MY_FLAGS_LAST_BIT);
  g_assert_cmpfloat (fl, ==, 0.25);
  g_assert_cmpfloat (db, ==, 1.5);
  g_assert_cmpstr (str, ==, "Test");
  g_assert_cmpstr (g_param_spec_get_nick (param), ==, "nick");
  g_assert_cmpstr (g_bytes_get_data (bytes, NULL), ==, "Blah");
  g_assert (ptr == &enum_type);
  g_assert_cmpuint (g_variant_get_uint16 (var), == , 99);
  g_assert_cmpint (i64, ==, G_MAXINT64 - 1234);
  g_assert_cmpuint (ui64, ==, G_MAXUINT64 - 123456);
}

static void
all_types_handler_cb (Test *test, int i, gboolean b, char c, guchar uc, guint ui, glong l, gulong ul, MyEnum e, guint f, float fl, double db, char *str, GParamSpec *param, GBytes *bytes, gpointer ptr, Test *obj, GVariant *var, gint64 i64, guint64 ui64, gpointer user_data)
{
  g_assert (user_data == &flags_type);
  all_types_handler (test, i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, obj, var, i64, ui64);
}

static void
test_all_types (void)
{
  Test *test;

  int i = 42;
  gboolean b = TRUE;
  char c = 17;
  guchar uc = 140;
  guint ui = G_MAXUINT - 42;
  glong l =  -1117;
  gulong ul = G_MAXULONG - 999;
  MyEnum e = MY_ENUM_VALUE;
  MyFlags f = MY_FLAGS_FIRST_BIT | MY_FLAGS_THIRD_BIT | MY_FLAGS_LAST_BIT;
  float fl = 0.25;
  double db = 1.5;
  char *str = "Test";
  GParamSpec *param = g_param_spec_long	 ("param", "nick", "blurb", 0, 10, 4, 0);
  GBytes *bytes = g_bytes_new_static ("Blah", 5);
  gpointer ptr = &enum_type;
  GVariant *var = g_variant_new_uint16 (99);
  gint64 i64;
  guint64 ui64;
  g_variant_ref_sink (var);
  i64 = G_MAXINT64 - 1234;
  ui64 = G_MAXUINT64 - 123456;

  test = g_object_new (test_get_type (), NULL);

  all_type_handlers_count = 0;

  g_signal_emit_by_name (test, "all-types",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-va",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-generic",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-empty",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-null",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);

  g_assert_cmpint (all_type_handlers_count, ==, 3);

  all_type_handlers_count = 0;

  g_signal_connect (test, "all-types", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-va", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-generic", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-empty", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-null", G_CALLBACK (all_types_handler_cb), &flags_type);

  g_signal_emit_by_name (test, "all-types",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-va",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-generic",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-empty",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-null",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);

  g_assert_cmpint (all_type_handlers_count, ==, 3 + 5);

  all_type_handlers_count = 0;

  g_signal_connect (test, "all-types", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-va", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-generic", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-empty", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-null", G_CALLBACK (all_types_handler_cb), &flags_type);

  g_signal_emit_by_name (test, "all-types",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-va",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-generic",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-empty",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-null",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);

  g_assert_cmpint (all_type_handlers_count, ==, 3 + 5 + 5);

  g_object_unref (test);
  g_param_spec_unref (param);
  g_bytes_unref (bytes);
  g_variant_unref (var);
}

static void
test_connect (void)
{
  GObject *test;
  gint retval;

  test = g_object_new (test_get_type (), NULL);

  g_object_connect (test,
                    "signal::generic-marshaller-int-return",
                    G_CALLBACK (on_generic_marshaller_int_return_signed_1),
                    NULL,
                    "object-signal::va-marshaller-int-return",
                    G_CALLBACK (on_generic_marshaller_int_return_signed_2),
                    NULL,
                    NULL);
  g_signal_emit_by_name (test, "generic-marshaller-int-return", &retval);
  g_assert_cmpint (retval, ==, -30);
  g_signal_emit_by_name (test, "va-marshaller-int-return", &retval);
  g_assert_cmpint (retval, ==, 2);

  g_object_disconnect (test,
                       "any-signal",
                       G_CALLBACK (on_generic_marshaller_int_return_signed_1),
                       NULL,
                       "any-signal::va-marshaller-int-return",
                       G_CALLBACK (on_generic_marshaller_int_return_signed_2),
                       NULL,
                       NULL);

  g_object_unref (test);
}

static void
simple_handler1 (GObject *sender,
                 GObject *target)
{
  g_object_unref (target);
}

static void
simple_handler2 (GObject *sender,
                 GObject *target)
{
  g_object_unref (target);
}

static void
test_destroy_target_object (void)
{
  Test *sender, *target1, *target2;

  sender = g_object_new (test_get_type (), NULL);
  target1 = g_object_new (test_get_type (), NULL);
  target2 = g_object_new (test_get_type (), NULL);
  g_signal_connect_object (sender, "simple", G_CALLBACK (simple_handler1), target1, 0);
  g_signal_connect_object (sender, "simple", G_CALLBACK (simple_handler2), target2, 0);
  g_signal_emit_by_name (sender, "simple");
  g_object_unref (sender);
}

static gboolean
hook_func (GSignalInvocationHint *ihint,
           guint                  n_params,
           const GValue          *params,
           gpointer               data)
{
  gint *count = data;

  (*count)++;

  return TRUE;
}

static void
test_emission_hook (void)
{
  GObject *test1, *test2;
  gint count = 0;
  gulong hook;

  test1 = g_object_new (test_get_type (), NULL);
  test2 = g_object_new (test_get_type (), NULL);

  hook = g_signal_add_emission_hook (simple_id, 0, hook_func, &count, NULL);
  g_assert_cmpint (count, ==, 0);
  g_signal_emit_by_name (test1, "simple");
  g_assert_cmpint (count, ==, 1);
  g_signal_emit_by_name (test2, "simple");
  g_assert_cmpint (count, ==, 2);
  g_signal_remove_emission_hook (simple_id, hook);
  g_signal_emit_by_name (test1, "simple");
  g_assert_cmpint (count, ==, 2);

  g_object_unref (test1);
  g_object_unref (test2);
}

static void
simple_cb (gpointer instance, gpointer data)
{
  GSignalInvocationHint *ihint;

  ihint = g_signal_get_invocation_hint (instance);

  g_assert_cmpstr (g_signal_name (ihint->signal_id), ==, "simple");

  g_signal_emit_by_name (instance, "simple-2");
}

static void
simple2_cb (gpointer instance, gpointer data)
{
  GSignalInvocationHint *ihint;

  ihint = g_signal_get_invocation_hint (instance);

  g_assert_cmpstr (g_signal_name (ihint->signal_id), ==, "simple-2");
}

static void
test_invocation_hint (void)
{
  GObject *test;

  test = g_object_new (test_get_type (), NULL);

  g_signal_connect (test, "simple", G_CALLBACK (simple_cb), NULL);
  g_signal_connect (test, "simple-2", G_CALLBACK (simple2_cb), NULL);
  g_signal_emit_by_name (test, "simple");

  g_object_unref (test);
}

static gboolean
in_set (const gchar *s,
        const gchar *set[])
{
  gint i;

  for (i = 0; set[i]; i++)
    {
      if (g_strcmp0 (s, set[i]) == 0)
        return TRUE;
    }

  return FALSE;
}

static void
test_introspection (void)
{
  guint *ids;
  guint n_ids;
  const gchar *name;
  gint i;
  const gchar *names[] = {
    "simple",
    "simple-2",
    "generic-marshaller-1",
    "generic-marshaller-2",
    "generic-marshaller-enum-return-signed",
    "generic-marshaller-enum-return-unsigned",
    "generic-marshaller-int-return",
    "va-marshaller-int-return",
    "generic-marshaller-uint-return",
    "va-marshaller-uint-return",
    "variant-changed-no-slot",
    "variant-changed",
    "all-types",
    "all-types-va",
    "all-types-generic",
    "all-types-null",
    "all-types-empty",
    "custom-marshaller",
    NULL
  };
  GSignalQuery query;

  ids = g_signal_list_ids (test_get_type (), &n_ids);
  g_assert_cmpuint (n_ids, ==, g_strv_length ((gchar**)names));

  for (i = 0; i < n_ids; i++)
    {
      name = g_signal_name (ids[i]);
      g_assert (in_set (name, names));
    }

  g_signal_query (simple_id, &query);
  g_assert_cmpuint (query.signal_id, ==, simple_id);
  g_assert_cmpstr (query.signal_name, ==, "simple");
  g_assert (query.itype == test_get_type ());
  g_assert (query.signal_flags == G_SIGNAL_RUN_LAST);
  g_assert (query.return_type == G_TYPE_NONE);
  g_assert_cmpuint (query.n_params, ==, 0);

  g_free (ids);
}

static void
test_handler (gpointer instance, gpointer data)
{
  gint *count = data;

  (*count)++;
}

static void
test_block_handler (void)
{
  GObject *test1, *test2;
  gint count1 = 0;
  gint count2 = 0;
  gulong handler1, handler;

  test1 = g_object_new (test_get_type (), NULL);
  test2 = g_object_new (test_get_type (), NULL);

  handler1 = g_signal_connect (test1, "simple", G_CALLBACK (test_handler), &count1);
  g_signal_connect (test2, "simple", G_CALLBACK (test_handler), &count2);

  handler = g_signal_handler_find (test1, G_SIGNAL_MATCH_ID, simple_id, 0, NULL, NULL, NULL);

  g_assert (handler == handler1);

  g_assert_cmpint (count1, ==, 0);
  g_assert_cmpint (count2, ==, 0);

  g_signal_emit_by_name (test1, "simple");
  g_signal_emit_by_name (test2, "simple");

  g_assert_cmpint (count1, ==, 1);
  g_assert_cmpint (count2, ==, 1);

  g_signal_handler_block (test1, handler1);

  g_signal_emit_by_name (test1, "simple");
  g_signal_emit_by_name (test2, "simple");

  g_assert_cmpint (count1, ==, 1);
  g_assert_cmpint (count2, ==, 2);

  g_signal_handler_unblock (test1, handler1);

  g_signal_emit_by_name (test1, "simple");
  g_signal_emit_by_name (test2, "simple");

  g_assert_cmpint (count1, ==, 2);
  g_assert_cmpint (count2, ==, 3);

  g_assert_cmpuint (g_signal_handlers_block_matched (test1, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, test_block_handler, NULL), ==, 0);
  g_assert_cmpuint (g_signal_handlers_block_matched (test2, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, test_handler, NULL), ==, 1);

  g_signal_emit_by_name (test1, "simple");
  g_signal_emit_by_name (test2, "simple");

  g_assert_cmpint (count1, ==, 3);
  g_assert_cmpint (count2, ==, 3);

  g_signal_handlers_unblock_matched (test2, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, test_handler, NULL);

  g_object_unref (test1);
  g_object_unref (test2);
}

static void
stop_emission (gpointer instance, gpointer data)
{
  g_signal_stop_emission (instance, simple_id, 0);
}

static void
stop_emission_by_name (gpointer instance, gpointer data)
{
  g_signal_stop_emission_by_name (instance, "simple");
}

static void
dont_reach (gpointer instance, gpointer data)
{
  g_assert_not_reached ();
}

static void
test_stop_emission (void)
{
  GObject *test1;
  gulong handler;

  test1 = g_object_new (test_get_type (), NULL);
  handler = g_signal_connect (test1, "simple", G_CALLBACK (stop_emission), NULL);
  g_signal_connect_after (test1, "simple", G_CALLBACK (dont_reach), NULL);

  g_signal_emit_by_name (test1, "simple");

  g_signal_handler_disconnect (test1, handler);
  g_signal_connect (test1, "simple", G_CALLBACK (stop_emission_by_name), NULL);

  g_signal_emit_by_name (test1, "simple");

  g_object_unref (test1);
}

static void
test_signal_disconnect_wrong_object (void)
{
  Test *object, *object2;
  Test2 *object3;
  guint signal_id;

  object = g_object_new (test_get_type (), NULL);
  object2 = g_object_new (test_get_type (), NULL);
  object3 = g_object_new (test2_get_type (), NULL);

  signal_id = g_signal_connect (object,
                                "simple",
                                G_CALLBACK (simple_handler1),
                                NULL);

  /* disconnect from the wrong object (same type), should warn */
  g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_WARNING,
                         "*: instance '*' has no handler with id '*'");
  g_signal_handler_disconnect (object2, signal_id);
  g_test_assert_expected_messages ();

  /* and from an object of the wrong type */
  g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_WARNING,
                         "*: instance '*' has no handler with id '*'");
  g_signal_handler_disconnect (object3, signal_id);
  g_test_assert_expected_messages ();

  /* it's still connected */
  g_assert (g_signal_handler_is_connected (object, signal_id));

  g_object_unref (object);
  g_object_unref (object2);
  g_object_unref (object3);
}

/* --- */

int
main (int argc,
     char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gobject/signals/all-types", test_all_types);
  g_test_add_func ("/gobject/signals/variant", test_variant_signal);
  g_test_add_func ("/gobject/signals/destroy-target-object", test_destroy_target_object);
  g_test_add_func ("/gobject/signals/generic-marshaller-1", test_generic_marshaller_signal_1);
  g_test_add_func ("/gobject/signals/generic-marshaller-2", test_generic_marshaller_signal_2);
  g_test_add_func ("/gobject/signals/generic-marshaller-enum-return-signed", test_generic_marshaller_signal_enum_return_signed);
  g_test_add_func ("/gobject/signals/generic-marshaller-enum-return-unsigned", test_generic_marshaller_signal_enum_return_unsigned);
  g_test_add_func ("/gobject/signals/generic-marshaller-int-return", test_generic_marshaller_signal_int_return);
  g_test_add_func ("/gobject/signals/generic-marshaller-uint-return", test_generic_marshaller_signal_uint_return);
  g_test_add_func ("/gobject/signals/custom-marshaller", test_custom_marshaller);
  g_test_add_func ("/gobject/signals/connect", test_connect);
  g_test_add_func ("/gobject/signals/emission-hook", test_emission_hook);
  g_test_add_func ("/gobject/signals/introspection", test_introspection);
  g_test_add_func ("/gobject/signals/block-handler", test_block_handler);
  g_test_add_func ("/gobject/signals/stop-emission", test_stop_emission);
  g_test_add_func ("/gobject/signals/invocation-hint", test_invocation_hint);
  g_test_add_func ("/gobject/signals/test-disconnection-wrong-object", test_signal_disconnect_wrong_object);

  return g_test_run ();
}
