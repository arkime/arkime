glib_tests = [
  'array-test',
  'asyncqueue',
  'base64',
  'bitlock',
  'bookmarkfile',
  'bytes',
  'cache',
  'checksum',
  'collate',
  'cond',
  'convert',
  'dataset',
  'date',
  'dir',
  'environment',
  'error',
  'fileutils',
  'gdatetime',
  'gvariant',
  'hash',
  'hmac',
  'hook',
  'hostutils',
  'keyfile',
  'list',
  'logging',
  'mainloop',
  'mappedfile',
  'markup',
  'markup-parse',
  'markup-collect',
  'markup-escape',
  'markup-subparser',
  'mem-overflow',
  'mutex',
  'node',
  'once',
  'option-context',
  'option-argv0',
  'overflow',
  # overflow-fallback handled separately below
  'pattern',
  'private',
  'protocol',
  'queue',
  'rand',
  'rec-mutex',
  'regex',
  'rwlock',
  'scannerapi',
  'search-utils',
  'sequence',
  'shell',
  'slice',
  'slist',
  'sort',
  'spawn-multithreaded',
  'spawn-singlethread',
  'strfuncs',
  'string',
  'testing',
  'test-printf',
  'thread',
  'timeout',
  'timer',
  'tree',
  'utf8-performance',
  'utf8-pointer',
  'utf8-validate',
  'utf8-misc',
  'utils',
  'unicode',
  'uri',
  '1bit-mutex',
  '642026',
]

if host_machine.system() != 'windows'
  glib_tests += [ 'unix', 'include' ]
endif

if cc.get_id() != 'msvc'
  glib_tests += [ 'autoptr' ]
endif

# FIXME: use new environment() object
# Not entirely random of course, but at least it changes over time
random_number = minor_version + meson.version().split('.').get(1).to_int()

test_env = [
  'G_TEST_SRCDIR=' + meson.current_source_dir(),
  'G_TEST_BUILDDIR=' + meson.current_build_dir(),
  'G_DEBUG=gc-friendly',
  'MALLOC_CHECK_=2',
  'MALLOC_PERTURB_=@0@'.format(random_number % 256),
]

test_cargs = ['-DG_LOG_DOMAIN="GLib"']

# Tests that consistently time out on the CI infrastructure
slow_tests = [
  'gvariant',
  'sequence',
  '642026',
]

foreach test_name : glib_tests
  deps = [libm, thread_dep, libglib_dep]
  if test_name == 'regex'
    deps += [pcre]
  endif
  exe = executable(test_name, '@0@.c'.format(test_name),
    c_args : ['-DPCRE_STATIC'] + test_cargs,
    dependencies : deps,
    install : false,
  )
  # These tests may take more than 30 seconds to run on the CI infrastructure
  if slow_tests.contains(test_name)
    test(test_name, exe, env : test_env, timeout : 90)
  else
    test(test_name, exe, env : test_env)
  endif
endforeach

c_args_atomic = []
if cc.get_id() == 'gcc'
  c_args_atomic += ['-Wstrict-aliasing=2']
endif

deps = [libm, thread_dep, libglib_dep]

exe = executable('atomic', 'atomic.c',
  c_args : test_cargs + c_args_atomic,
  dependencies : deps,
)
test('atomic', exe, env : test_env)

# FIXME: consolidate all of these into the array
exe = executable('overflow-fallback', 'overflow.c',
  c_args : test_cargs + ['-D_GLIB_TEST_OVERFLOW_FALLBACK'],
  dependencies : deps,
  install : false,
)
test('overflow-fallback', exe, env : test_env)

exe = executable('642026-ec', '642026.c',
  c_args : test_cargs + ['-DG_ERRORCHECK_MUTEXES'],
  dependencies : deps,
  install : false,
)
test('642026-ec', exe, env : test_env, timeout : 45)

exe = executable('1bit-emufutex', '1bit-mutex.c',
  c_args : test_cargs + ['-DTEST_EMULATED_FUTEX'],
  dependencies : deps,
  install : false,
)
test('1bit-emufutex', exe, env : test_env)

exe = executable('gwakeup', 'gwakeuptest.c', '../gwakeup.c',
  c_args : test_cargs,
  dependencies : deps,
  install : false)
test('gwakeup', exe, env : test_env)

if glib_conf.has('HAVE_EVENTFD')
  exe = executable('gwakeup-fallback', 'gwakeuptest.c', '../gwakeup.c',
    c_args : test_cargs + ['-DTEST_EVENTFD_FALLBACK'],
    dependencies : deps,
    install : false,
  )
  test('gwakeup-fallback', exe, env : test_env)
endif

# test-spawn-echo helper binary required by the spawn tests above
executable('test-spawn-echo', 'test-spawn-echo.c',
  c_args : test_cargs,
  dependencies : deps,
  install : false,
)

# some testing of gtester functionality
if not meson.is_cross_build() and host_system != 'windows'
  xmllint = find_program('xmllint', required: false)
  if xmllint.found()
    tmpsample_xml = custom_target('tmpsample.xml',
      output : 'tmpsample.xml',
      build_by_default : true, # hack around meson bug, see PR #1335
      command : [ gtester, '-k', '--quiet', '-o', '@OUTPUT@',
                  '--test-arg=--gtester-selftest', gtester])

    test('gtester-xmllint-check', xmllint,
      args : ['--noout', tmpsample_xml],
      env : test_env)
  endif
endif
