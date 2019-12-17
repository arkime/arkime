gobject_tests = [
  'qdata',
  'boxed',
  'enums',
  'param',
  'threadtests',
  'dynamictests',
  'binding',
  'properties',
  'reference',
  'value',
  'type',
  'private',
  'closure',
  'object',
  'signal-handler',
  'ifaceproperties',
]

# FIXME: use new environment() object
# FIXME: put common bits of test environment() in one location
# Not entirely random of course, but at least it changes over time
random_number = minor_version + meson.version().split('.').get(1).to_int()

test_env = [
  'G_TEST_SRCDIR=' + meson.current_source_dir(),
  'G_TEST_BUILDDIR=' + meson.current_build_dir(),
  'G_DEBUG=gc-friendly',
  'MALLOC_CHECK_=2',
  'MALLOC_PERTURB_=@0@'.format(random_number % 256),
]

foreach test_name : gobject_tests
  deps = [libm, thread_dep, libglib_dep, libgobject_dep]
  test_src = '@0@.c'.format(test_name)
  # private is an existing or reserved target it seems
  if test_name == 'private'
    test_name = 'gobject-private'
  endif
  exe = executable(test_name, test_src,
      c_args : ['-DG_LOG_DOMAIN="GLib-GObject"'],
      dependencies : deps,
  )
  test(test_name, exe, env : test_env)
endforeach

marshalers_h = custom_target('marshalers_h',
  output : 'marshalers.h',
  input : 'marshalers.list',
  command : [
    python, glib_genmarshal,
    '--prefix=test',
    '--valist-marshallers',
    '--output=@OUTPUT@',
    '--quiet',
    '--header',
    '@INPUT@',
  ],
)
marshalers_c = custom_target('marshalers_c',
  output : 'marshalers.c',
  input : 'marshalers.list',
  command : [
    python, glib_genmarshal,
    '--prefix=test',
    '--valist-marshallers',
    '--include-header=marshalers.h',
    '--output=@OUTPUT@',
    '--quiet',
    '--body',
    '@INPUT@',
  ],
)

exe = executable('signals',
    'signals.c', marshalers_h, marshalers_c,
    c_args : ['-DG_LOG_DOMAIN="GLib-GObject"'],
    dependencies : deps,
)
test('signals', exe, env : test_env)
