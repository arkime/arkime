common_gio_tests_deps = [
  libglib_dep,
  libgmodule_dep,
  libgobject_dep,
  libgio_dep,
  libintl
]

subdir('gdbus-object-manager-example')

gengiotypefuncs_prog = find_program('gengiotypefuncs.py')
giotypefuncs_inc = custom_target(
  'giotypefuncs.inc',
  output : 'giotypefuncs.inc',
  input : gio_headers + [gioenumtypes_h] + gobject_install_headers,
  command: [gengiotypefuncs_prog, '@OUTPUT@', '@INPUT@'])

#  Test programs buildable on all platforms
gio_tests = [
  'appmonitor',
  'async-close-output-stream',
  'async-splice-output-stream',
  'buffered-input-stream',
  'buffered-output-stream',
  'cancellable',
  'contexts',
  'contenttype',
  'converter-stream',
  'credentials',
  'data-input-stream',
  'data-output-stream',
  'defaultvalue',
  'fileattributematcher',
  'filter-streams',
  'giomodule',
  'gsubprocess',
  'g-file',
  'g-file-info',
  'g-icon',
  'gdbus-addresses',
  'gdbus-message',
  'inet-address',
  'io-stream',
  'memory-input-stream',
  'memory-output-stream',
  'monitor',
  'network-address',
  'network-monitor',
  'network-monitor-race',
  'permission',
  'pollable',
  'proxy-test',
  'readwrite',
  'simple-async-result',
  'simple-proxy',
  'sleepy-stream',
  'socket',
  'socket-listener',
  'socket-service',
  'srvtarget',
  'task',
  'vfs',
  'volumemonitor',
  'glistmodel',
  'testfilemonitor',
  'thumbnail-verification',
]

test_extra_programs = [
  ['gdbus-connection-flush-helper'],
  ['gdbus-testserver'],
]

test_env = [
  'G_TEST_SRCDIR=' + meson.current_source_dir(),
  'G_TEST_BUILDDIR=' + meson.current_build_dir(),
  'GIO_MODULE_DIR=',
]

test_c_args = [
  '-DG_LOG_DOMAIN="GLib-GIO"',
  '-DTEST_SERVICES="@0@/gio/tests/services"'.format(meson.build_root()),
  '-DGLIB_MKENUMS="@0@"'.format(glib_mkenums),
  '-DGLIB_COMPILE_SCHEMAS="@0@"'.format(glib_compile_schemas.full_path()),
]

# Check for libdbus1 - Optional - is only used in the GDBus test cases
# 1.2.14 required for dbus_message_set_serial
dbus1_dep = dependency('dbus-1', required : false, version : '>= 1.2.14')
if not dbus1_dep.found()
  if cc.get_id() == 'msvc'
    # MSVC: Search for the DBus library by the configuration, which corresponds
    # to the output of CMake builds of DBus.  Note that debugoptimized
    # is really a Release build with .PDB files.
    if buildtype == 'debug'
      dbus1_dep = cc.find_library('dbus-1d', required : false)
    else
      dbus1_dep = cc.find_library('dbus-1', required : false)
    endif
  endif
endif
if dbus1_dep.found()
  glib_conf.set('HAVE_DBUS1', 1)

  exe = executable('gdbus-serialization',
      'gdbus-serialization.c', 'gdbus-tests.c',
      install : false,
      c_args : test_c_args,
      dependencies : common_gio_tests_deps + [dbus1_dep])
  test('gdbus-serialization', exe, env : test_env)
endif

#  Test programs buildable on UNIX only
if host_machine.system() != 'windows'
  gio_tests += [
    'file',
    'gdbus-peer',
    'gdbus-peer-object-manager',
    'live-g-file',
    'socket-address',
    'stream-rw_all',
    'unix-fd',
    'unix-mounts',
    'unix-streams',
    'g-file-info-filesystem-readonly',
    'gschema-compile',
  ]

  # Uninstalled because of the check-for-executable logic in DesktopAppInfo
  # unable to find the installed executable
  if not glib_have_cocoa
    gio_tests += [
      'appinfo',
      'desktop-app-info',
    ]
  endif

  test_extra_programs += [
    ['basic-application'],
    ['dbus-launch'],
    ['appinfo-test'],
  ]

  if not glib_have_cocoa
    test_extra_programs += [['apps']]
    gio_tests += ['mimeapps']
  endif

  #  Test programs that need to bring up a session bus (requires dbus-daemon)
  have_dbus_daemon = find_program('dbus-daemon', required : false).found()
  if have_dbus_daemon
    # Generate gdbus-test-codegen-generated.{c,h}
    gdbus_test_codegen_generated = custom_target('gdbus-test-codegen-generated',
        input :   ['test-codegen.xml'],
        output :  ['gdbus-test-codegen-generated.h',
                   'gdbus-test-codegen-generated.c'],
        depend_files : gdbus_codegen_built_files,
        command : [python, gdbus_codegen,
                   '--interface-prefix', 'org.project.',
                   '--output-directory', '@OUTDIR@',
                   '--generate-c-code', 'gdbus-test-codegen-generated',
                   '--c-generate-object-manager',
                   '--c-namespace', 'Foo_iGen',
                   '--generate-docbook', 'gdbus-test-codegen-generated-doc',
                   '--annotate', 'org.project.Bar', 'Key1', 'Value1',
                   '--annotate', 'org.project.Bar', 'org.gtk.GDBus.Internal', 'Value2',
                   '--annotate', 'org.project.Bar.HelloWorld()', 'Key3', 'Value3',
                   '--annotate', 'org.project.Bar::TestSignal', 'Key4', 'Value4',
                   '--annotate', 'org.project.Bar:ay', 'Key5', 'Value5',
                   '--annotate', 'org.project.Bar.TestPrimitiveTypes()[val_int32]', 'Key6', 'Value6',
                   '--annotate', 'org.project.Bar.TestPrimitiveTypes()[ret_uint32]', 'Key7', 'Value7',
                   '--annotate', 'org.project.Bar::TestSignal[array_of_strings]', 'Key8', 'Value8',
                   '@INPUT@'])

    gio_dbus_tests = [
      ['actions', [], []],
      ['gdbus-auth', [], []],
      ['gdbus-bz627724', [], []],
      ['gdbus-close-pending', [], []],
      ['gdbus-connection', [], []],
      ['gdbus-connection-loss', [], []],
      ['gdbus-connection-slow', [], []],
      ['gdbus-error', [], []],
      ['gdbus-exit-on-close', [], []],
      ['gdbus-export', [], []],
      ['gdbus-introspection', [], []],
      ['gdbus-names', [], []],
      ['gdbus-proxy', [], []],
      ['gdbus-proxy-threads', [], [dbus1_dep]],
      ['gdbus-proxy-well-known-name', [], []],
      ['gdbus-test-codegen', [gdbus_test_codegen_generated], []],
      ['gdbus-threading', [], []],
      ['gmenumodel', [], []],
      ['gnotification', ['gnotification-server.c'], []],
    ]

    if not glib_have_cocoa
      gio_dbus_tests = [['dbus-appinfo', [], []]]
    endif

    # separate loop because extra source files for each test
    foreach dbus_test : gio_dbus_tests
      test_name = dbus_test[0]
      extra_src = dbus_test[1]
      extra_deps = dbus_test[2]
      exe = executable(test_name, '@0@.c'.format(test_name),
          'gdbus-sessionbus.c', 'gdbus-tests.c', extra_src,
          install : false,
          c_args : test_c_args,
          dependencies : common_gio_tests_deps + extra_deps)
      test(test_name, exe, env : test_env)
    endforeach

    exe = executable('gdbus-test-codegen-old', 'gdbus-test-codegen.c',
          'gdbus-sessionbus.c', 'gdbus-tests.c', gdbus_test_codegen_generated,
          install : false,
          c_args : test_c_args + ['-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_36', '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_36'],
          dependencies : common_gio_tests_deps)
    test('gdbus-test-codegen-old', exe, env : test_env)

    # There is already a gapplication exe target in gio so need to use a
    # different name for the unit test executable, since we can't have two
    # targets of the same name even if in different directories
    # (FIXME: just rename source file to gapplication-test.c)
    if not glib_have_cocoa
      exe = executable('gapplication-test', 'gapplication.c',
                       'gdbus-sessionbus.c', 'gdbus-tests.c',
                       install : false,
                       c_args : test_c_args,
                       dependencies : common_gio_tests_deps)
    endif
    test('gapplication', exe, env : test_env)

    gio_tests += ['gdbus-unix-addresses']
  endif # have_dbus_daemon

  # This test is currently unreliable
  executable('gdbus-overflow', 'gdbus-overflow.c',
      install : false,
      c_args : test_c_args,
      dependencies : common_gio_tests_deps)

  exe = executable('gdbus-connection-flush', 'gdbus-connection-flush.c',
      'test-io-stream.c', 'test-pipe-unix.c',
      install : false,
      c_args : test_c_args,
      dependencies : common_gio_tests_deps)
  test('gdbus-connection-flush', exe, env : test_env)

  exe = executable('gdbus-non-socket', 'gdbus-non-socket.c',
      'gdbus-tests.c', 'test-io-stream.c', 'test-pipe-unix.c',
      install : false,
      c_args : test_c_args,
      dependencies : common_gio_tests_deps)
  test('gdbus-non-socket', exe, env : test_env)

  # Generate test.mo from de.po using msgfmt
  msgfmt = find_program('msgfmt', required : false)
  if msgfmt.found()
    subdir('de/LC_MESSAGES')
    # gsettings target exe already exists in gio directory
    exe = executable('gsettings-test', 'gsettings.c', test_mo,
        install : false,
        c_args : test_c_args + [
                   '-DSRCDIR="@0@"'.format(meson.current_source_dir()),
                   '-DTEST_LOCALE_PATH="@0@"'.format(test_mo_dir),
                 ],
        dependencies : common_gio_tests_deps)
    test('gsettings', exe, env : test_env)
  endif
endif # unix

#  Test programs buildable on Windows only
if host_machine.system() == 'windows'
  gio_tests += ['win32-streams']
endif

if cc.get_id() != 'msvc'
  gio_tests += [ 'autoptr' ]
endif

foreach test_name : gio_tests
  extra_deps = []
  srcs = ['@0@.c'.format(test_name)]
  # conflicts with glib/tests/autoptr, can't have two targets with same name
  if test_name == 'autoptr'
    test_name = 'autoptr-gio'
  elif test_name == 'defaultvalue'
    srcs += [giotypefuncs_inc]
  elif test_name == 'gdbus-peer'
    # This is peer to peer so it doesn't need a session bus, so we can run
    # it automatically as a test by default
    extra_deps = [libgdbus_example_objectmanager_dep]
  endif
  exe = executable(test_name, srcs,
      install : false,
      c_args : test_c_args,
      dependencies : common_gio_tests_deps + extra_deps)
  if test_name == 'testfilemonitor'
    test(test_name, exe, env : test_env, timeout : 45)
  else
    test(test_name, exe, env : test_env)
  endif
endforeach

uninstalled_test_extra_programs = [
  ['gio-du'],
  ['echo-server'],
  ['filter-cat'],
  ['gapplication-example-actions'],
  ['gapplication-example-cmdline'],
  ['gapplication-example-cmdline2'],
  ['gapplication-example-cmdline3'],
  ['gapplication-example-cmdline4'],
  ['gapplication-example-dbushooks'],
  ['gapplication-example-open'],
  ['gdbus-daemon', gdbus_daemon_sources],
  ['gdbus-example-export'],
  ['gdbus-example-own-name'],
  ['gdbus-example-peer'],
  ['gdbus-example-proxy-subclass'],
  ['gdbus-example-server'],
  ['gdbus-example-subtree'],
  ['gdbus-example-watch-name'],
  ['gdbus-example-watch-proxy'],
  ['gsubprocess-testprog'],
  ['httpd'],
  ['proxy'],
  ['resolver'],
  ['send-data'],
  ['socket-server'],
  ['socket-client', ['gtlsconsoleinteraction.c']],
  ['tls-certificate', ['gtesttlsbackend.c']],
  ['tls-interaction', ['gtesttlsbackend.c']],
  # These three are manual-run tests because they need a session bus but don't bring one up themselves
  # FIXME: these build but don't seem to work!
  ['gdbus-example-objectmanager-client', [], [libgdbus_example_objectmanager_dep]],
  ['gdbus-example-objectmanager-server', [], [libgdbus_example_objectmanager_dep]],
  ['gdbus-test-fixture', [], [libgdbus_example_objectmanager_dep]],
]

if host_machine.system() != 'windows'
  uninstalled_test_extra_programs += [['gdbus-example-unix-fd-client']]
endif

foreach extra_program : uninstalled_test_extra_programs + test_extra_programs
  srcs = ['@0@.c'.format(extra_program[0])]
  if extra_program.length() > 1
    srcs += extra_program[1]
  endif
  extra_deps = []
  if extra_program.length() > 2
    extra_deps = extra_program[2]
  endif
  executable(extra_program[0], srcs,
      install : false,
      c_args : test_c_args,
      dependencies : common_gio_tests_deps + extra_deps)
endforeach

if not meson.is_cross_build() or meson.has_exe_wrapper()

  plugin_resources_c = custom_target('plugin-resources.c',
    input : 'test4.gresource.xml',
    output : 'plugin-resources.c',
    command : [glib_compile_resources,
               '--target=@OUTPUT@',
               '--sourcedir=' + meson.current_source_dir(),
               '--generate-source',
               '--c-name', '_g_plugin',
               '@INPUT@'])

  if host_system == 'windows'
    resource_plugin_platform_link_args = ['-no-undefined']
  else
    resource_plugin_platform_link_args = []
  endif
  shared_module ('resourceplugin', 'resourceplugin.c', plugin_resources_c,
    link_args : ['-export-dynamic'] + resource_plugin_platform_link_args,
    dependencies : common_gio_tests_deps)

  test_gresource = custom_target('test.gresource',
    input : 'test.gresource.xml',
    output : 'test.gresource',
    command : [glib_compile_resources,
               '--target=@OUTPUT@',
               '--sourcedir=' + meson.current_source_dir(),
               '--sourcedir=' + meson.current_build_dir(),
               '@INPUT@'])

  test_resources2_c = custom_target('test_resources2.c',
    input : 'test3.gresource.xml',
    output : 'test_resources2.c',
    command : [glib_compile_resources,
               '--target=@OUTPUT@',
               '--sourcedir=' + meson.current_source_dir(),
               '--generate',
               '--c-name', '_g_test2',
               '--manual-register',
               '@INPUT@'])

  test_resources2_h = custom_target('test_resources2.h',
    input : 'test3.gresource.xml',
    output : 'test_resources2.h',
    command : [glib_compile_resources,
               '--target=@OUTPUT@',
               '--sourcedir=' + meson.current_source_dir(),
               '--generate',
               '--c-name', '_g_test2',
               '--manual-register',
               '@INPUT@'])

  test_resources_c = custom_target('test_resources.c',
    input : 'test2.gresource.xml',
    output : 'test_resources.c',
    command : [glib_compile_resources,
               '--target=@OUTPUT@',
               '--sourcedir=' + meson.current_source_dir(),
               '--generate-source',
               '--c-name', '_g_test1',
               '@INPUT@'])

  # referenced by test.gresource.xml
  test_generated_txt = configure_file(input : 'test1.txt',
    output : 'test-generated.txt',
    configuration : configuration_data(),
    install : false)

  exe = executable('resources', 'resources.c', test_gresource,
      test_resources_c, test_resources2_c, test_resources2_h,
      install : false,
      c_args : test_c_args,
      dependencies : common_gio_tests_deps)
  test('resources', exe, env : test_env)
endif

# FIXME: subdir('services')
subdir('modules')
