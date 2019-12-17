refcount_tests = [
 ['closures', 'closures.c', []],
 ['objects', 'objects.c', []],
 ['objects2', 'objects2.c', []],
 ['properties', 'properties.c', []],
 ['properties2', 'properties2.c', []],
 ['properties3', 'properties3.c', [], 90], # extra long timeout
 ['properties4', 'properties4.c', []],
 ['signal1', 'signals.c', ['-DTESTNUM=1']],
 ['signal2', 'signals.c', ['-DTESTNUM=2']],
 ['signal3', 'signals.c', ['-DTESTNUM=3']],
 ['signal4', 'signals.c', ['-DTESTNUM=4']],
]

foreach t : refcount_tests
  test_name = t.get(0)
  test_src = t.get(1)
  test_extra_cargs = t.get(2)
  test_timeout = t.get(3, 30)

  # FIXME? $(GLIB_DEBUG_FLAGS)
  exe = executable(test_name + '-test', test_src,
    c_args : test_cargs + test_extra_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS'],
    dependencies : [libm, thread_dep, libglib_dep, libgobject_dep],
    install : false,
  )
  # FIXME? TESTS_ENVIRONMENT = LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset
  test(test_name, exe, env : test_env, timeout : test_timeout)
endforeach
