gobject_tests = [
  ['gvalue-test'],
  ['paramspec-test'],
  ['deftype'],
  ['defaultiface', ['defaultiface.c', 'testmodule.c']],
  ['dynamictype', ['dynamictype.c', 'testmodule.c']],
  ['override'],
  ['signals'],
  ['singleton'],
  ['references'],
  ['testgobject'],
]

# We cannot use gnome.genmarshal() here
testmarshal_h = custom_target('testmarshal_h',
  output : 'testmarshal.h',
  input : 'testmarshal.list',
  command : [
    python, glib_genmarshal,
    '--prefix=test_marshal',
    '--output=@OUTPUT@',
    '--quiet',
    '--header',
    '@INPUT@',
  ],
)

testmarshal_c = custom_target('testmarshal_c',
  output : 'testmarshal.c',
  input : 'testmarshal.list',
  command : [
    python, glib_genmarshal,
    '--prefix=test_marshal',
    '--include-header=testmarshal.h',
    '--output=@OUTPUT@',
    '--quiet',
    '--body',
    '@INPUT@',
  ],
)

gobject_tests += [
  ['accumulator', ['accumulator.c', testmarshal_c, testmarshal_h]],
]

foreach t : gobject_tests
  test_name = t.get(0)
  test_src = t.get(1, test_name + '.c')
  test_extra_cargs = t.get(2, [])
  test_timeout = t.get(3, 30)

  # FIXME? $(GLIB_DEBUG_FLAGS)
  exe = executable(test_name + '-gobject', test_src,
    c_args : test_cargs + test_extra_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS'],
    dependencies : [libm, thread_dep, libglib_dep, libgobject_dep],
    install : false,
  )
  # FIXME? TESTS_ENVIRONMENT = LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset
  test(test_name, exe, env : test_env, timeout : test_timeout)
endforeach

# Don't install these ones, and keep them out of 'make check' because they take too long...
executable('performance', 'performance.c',
    c_args : test_cargs + test_extra_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS'],
    dependencies : [libm, thread_dep, libglib_dep, libgobject_dep],
    install : false)

executable('performance-threaded', 'performance-threaded.c',
    c_args : test_cargs + test_extra_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS'],
    dependencies : [libm, thread_dep, libglib_dep, libgobject_dep],
    install : false)

if host_system != 'windows' and host_system != 'minix'
  executable('timeloop-closure', 'timeloop-closure.c',
      c_args : test_cargs + test_extra_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS'],
      dependencies : [libm, thread_dep, libglib_dep, libgobject_dep],
      install : false)
endif
