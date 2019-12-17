configure_file(input : 'glibconfig.h.in', output : 'glibconfig.h',
  install : true,
  install_dir : join_paths(get_option('libdir'), 'glib-2.0/include'),
  configuration : glibconfig_conf)

subdir('libcharset')
if not use_system_pcre
  subdir('pcre')
endif
if have_good_vsnprintf and have_good_snprintf
  gnulib_lib = []
else
  subdir('gnulib')
endif

glib_headers = files(
  'glib.h',
  'glib-unix.h',
  'glib-object.h',
)
install_headers(glib_headers, subdir : 'glib-2.0')

glib_deprecated_headers = files(
  'deprecated/gallocator.h',
  'deprecated/gcache.h',
  'deprecated/gcompletion.h',
  'deprecated/gmain.h',
  'deprecated/grel.h',
  'deprecated/gthread.h',
)
install_headers(glib_deprecated_headers, subdir : 'glib-2.0/glib/deprecated')

glib_sub_headers = files(
  'glib-autocleanups.h',
  'galloca.h',
  'garray.h',
  'gasyncqueue.h',
  'gatomic.h',
  'gbacktrace.h',
  'gbase64.h',
  'gbitlock.h',
  'gbookmarkfile.h',
  'gbytes.h',
  'gcharset.h',
  'gchecksum.h',
  'gconvert.h',
  'gdataset.h',
  'gdate.h',
  'gdatetime.h',
  'gdir.h',
  'genviron.h',
  'gerror.h',
  'gfileutils.h',
  'ggettext.h',
  'ghash.h',
  'ghmac.h',
  'ghook.h',
  'ghostutils.h',
  'gi18n.h',
  'gi18n-lib.h',
  'giochannel.h',
  'gkeyfile.h',
  'glist.h',
  'gmacros.h',
  'gmain.h',
  'gmappedfile.h',
  'gmarkup.h',
  'gmem.h',
  'gmessages.h',
  'gnode.h',
  'goption.h',
  'gpattern.h',
  'gpoll.h',
  'gprimes.h',
  'gqsort.h',
  'gquark.h',
  'gqueue.h',
  'grand.h',
  'gregex.h',
  'gscanner.h',
  'gsequence.h',
  'gshell.h',
  'gslice.h',
  'gslist.h',
  'gspawn.h',
  'gstdio.h',
  'gstrfuncs.h',
  'gtestutils.h',
  'gstring.h',
  'gstringchunk.h',
  'gthread.h',
  'gthreadpool.h',
  'gtimer.h',
  'gtimezone.h',
  'gtrashstack.h',
  'gtree.h',
  'gtypes.h',
  'guuid.h',
  'gunicode.h',
  'gurifuncs.h',
  'gutils.h',
  'gvarianttype.h',
  'gvariant.h',
  'gversion.h',
  'gversionmacros.h',
  'gwin32.h',
  'gprintf.h',
)
install_headers(glib_sub_headers, subdir : 'glib-2.0/glib')

deprecated_sources = files(
  'deprecated/gallocator.c',
  'deprecated/gcache.c',
  'deprecated/gcompletion.c',
  'deprecated/grel.c',
  'deprecated/gthread-deprecated.c'
)

glib_sources = files(
  'garray.c',
  'gasyncqueue.c',
  'gatomic.c',
  'gbacktrace.c',
  'gbase64.c',
  'gbitlock.c',
  'gbookmarkfile.c',
  'gbytes.c',
  'gcharset.c',
  'gchecksum.c',
  'gconvert.c',
  'gdataset.c',
  'gdate.c',
  'gdatetime.c',
  'gdir.c',
  'genviron.c',
  'gerror.c',
  'gfileutils.c',
  'ggettext.c',
  'ghash.c',
  'ghmac.c',
  'ghook.c',
  'ghostutils.c',
  'giochannel.c',
  'gkeyfile.c',
  'glib-init.c',
  'glib-private.c',
  'glist.c',
  'gmain.c',
  'gmappedfile.c',
  'gmarkup.c',
  'gmem.c',
  'gmessages.c',
  'gnode.c',
  'goption.c',
  'gpattern.c',
  'gpoll.c',
  'gprimes.c',
  'gqsort.c',
  'gquark.c',
  'gqueue.c',
  'grand.c',
  'gregex.c',
  'gscanner.c',
  'gsequence.c',
  'gshell.c',
  'gslice.c',
  'gslist.c',
  'gstdio.c',
  'gstrfuncs.c',
  'gstring.c',
  'gstringchunk.c',
  'gtestutils.c',
  'gthread.c',
  'gthreadpool.c',
  'gtimer.c',
  'gtimezone.c',
  'gtranslit.c',
  'gtrashstack.c',
  'gtree.c',
  'guniprop.c',
  'gutf8.c',
  'gunibreak.c',
  'gunicollate.c',
  'gunidecomp.c',
  'gurifuncs.c',
  'gutils.c',
  'guuid.c',
  'gvariant.c',
  'gvariant-core.c',
  'gvariant-parser.c',
  'gvariant-serialiser.c',
  'gvarianttypeinfo.c',
  'gvarianttype.c',
  'gversion.c',
  'gwakeup.c',
  'gprintf.c',
)

if host_system == 'windows'
  glib_win_rc = configure_file(
    input: 'glib.rc.in',
    output: 'glib.rc',
    configuration: glibconfig_conf,
  )
  glib_win_res = windows.compile_resources(glib_win_rc)
  glib_sources += [glib_win_res]
  glib_sources += files('gthread-win32.c')
  glib_sources += files('gwin32.c', 'gspawn-win32.c', 'giowin32.c')
  platform_deps = [winsock2, cc.find_library('winmm')]
else
  glib_sources += files('gthread-posix.c')
  glib_sources += files('glib-unix.c', 'gspawn.c', 'giounix.c')
  platform_deps = []
endif

if enable_dtrace
  glib_dtrace_obj = dtrace_obj_gen.process('glib_probes.d')
  glib_dtrace_hdr = dtrace_hdr_gen.process('glib_probes.d')
else
  glib_dtrace_obj = []
  glib_dtrace_hdr = []
endif

pcre_static_args = []

if use_pcre_static_flag
  pcre_static_args = ['-DPCRE_STATIC']
endif

libglib = library('glib-2.0',
  glib_dtrace_obj, glib_dtrace_hdr,
  sources : [deprecated_sources, glib_sources],
  version : library_version,
  soversion : soversion,
  install : true,
  # intl.lib is not compatible with SAFESEH
  link_args : platform_ldflags + noseh_link_args,
  include_directories : configinc,
  link_with : [charset_lib, gnulib_lib],
  dependencies : [pcre, thread_dep, libintl, librt] + libiconv + platform_deps,
  c_args : ['-DG_LOG_DOMAIN="GLib"', '-DGLIB_COMPILATION'] + pcre_static_args + glib_hidden_visibility_args
)

libglib_dep = declare_dependency(
  link_with : libglib,
  # thread_dep doesn't get pulled in from libglib atm,
  # see https://github.com/mesonbuild/meson/issues/1426
  dependencies : [thread_dep],
  # We sadly need to export configinc here because everyone includes <glib/*.h>
  include_directories : [configinc, glibinc])

# On Windows, glib needs a spawn helper for g_spawn* API
if host_system == 'windows'
  if host_machine.cpu_family() == 'x86'
    executable('gspawn-win32-helper', 'gspawn-win32-helper.c',
      install : true,
      gui_app : true,
      include_directories : configinc,
      dependencies : [libintl, libglib_dep])
    executable('gspawn-win32-helper-console', 'gspawn-win32-helper.c',
      install : true,
      c_args : ['-DHELPER_CONSOLE'],
      include_directories : configinc,
      dependencies : [libintl, libglib_dep])
  else
    executable('gspawn-win64-helper', 'gspawn-win32-helper.c',
      install : true,
      gui_app : true,
      include_directories : configinc,
      dependencies : [libintl, libglib_dep])
    executable('gspawn-win64-helper-console', 'gspawn-win32-helper.c',
      install : true,
      c_args : ['-DHELPER_CONSOLE'],
      include_directories : configinc,
      dependencies : [libintl, libglib_dep])
  endif
else
  gtester = executable('gtester', 'gtester.c',
    install : true,
    include_directories : configinc,
    dependencies : [libglib_dep])
endif

install_data('gtester-report', install_dir : get_option('bindir'))

install_data('glib_gdb.py', install_dir : join_paths(glib_pkgdatadir, 'gdb'))

gdb_conf = configuration_data()
gdb_conf.set('datadir', glib_datadir)
# This is also used in gobject/meson.build
if host_system != 'windows'
  # XXX: We add a leading './' because glib_libdir is an absolute path and we
  # need it to be a relative path so that join_paths appends it to the end.
  gdb_install_dir = join_paths(glib_datadir, 'gdb', 'auto-load', './' + glib_libdir)
else
  # FIXME: Cannot install on Windows because the path will contain a drive
  # letter and colons are not allowed in paths.
  gdb_install_dir = false
endif
configure_file(
  input: 'libglib-gdb.py.in',
  output: 'libglib-2.0.so.@0@-gdb.py'.format(library_version),
  configuration: gdb_conf,
  install_dir: gdb_install_dir,
)

if enable_systemtap
  glib_stp = configure_file(input : 'glib.stp.in',
    output : '@0@.stp'.format(libglib.full_path().split('/').get(-1)),
    configuration : stp_cdata,
    install_dir : tapset_install_dir,
    install : true)
endif

# gtester doesn't work on native windows
if cc.get_id() != 'msvc'
  subdir('tests')
endif
