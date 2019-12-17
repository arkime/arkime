gobject_install_headers = files(
  'gobject-autocleanups.h',
  'glib-types.h',
  'gbinding.h',
  'gboxed.h',
  'gclosure.h',
  'genums.h',
  'gmarshal.h',
  'gobject.h',
  'gparam.h',
  'gparamspecs.h',
  'gsignal.h',
  'gsourceclosure.h',
  'gtype.h',
  'gtypemodule.h',
  'gtypeplugin.h',
  'gvalue.h',
  'gvaluearray.h',
  'gvaluecollector.h',
  'gvaluetypes.h',
  'gobjectnotifyqueue.c', # sic
)
install_headers(gobject_install_headers, subdir : 'glib-2.0/gobject')

gobject_sources = files(
  'gatomicarray.c',
  'gbinding.c',
  'gboxed.c',
  'gclosure.c',
  'genums.c',
  'gmarshal.c',
  'gobject.c',
  'gparam.c',
  'gparamspecs.c',
  'gsignal.c',
  'gsourceclosure.c',
  'gtype.c',
  'gtypemodule.c',
  'gtypeplugin.c',
  'gvalue.c',
  'gvaluearray.c',
  'gvaluetransform.c',
  'gvaluetypes.c',
)

if host_system == 'windows'
  gobject_win_rc = configure_file(
    input: 'gobject.rc.in',
    output: 'gobject.rc',
    configuration: glibconfig_conf,
  )
  gobject_win_res = windows.compile_resources(gobject_win_rc)
  gobject_sources += [gobject_win_res]
endif

if enable_dtrace
  gobject_dtrace_obj = dtrace_obj_gen.process('gobject_probes.d')
  gobject_dtrace_hdr = dtrace_hdr_gen.process('gobject_probes.d')
else
  gobject_dtrace_obj = []
  gobject_dtrace_hdr = []
endif

libgobject = library('gobject-2.0',
  gobject_dtrace_obj, gobject_dtrace_hdr,
  sources : gobject_sources,
  version : library_version,
  soversion : soversion,
  install : true,
  include_directories : [configinc],
  dependencies : [libintl, libffi_dep, libglib_dep],
  c_args : ['-DG_LOG_DOMAIN="GLib-GObject"', '-DGOBJECT_COMPILATION'] + glib_hidden_visibility_args)

libgobject_dep = declare_dependency(link_with : libgobject,
  include_directories : gobjectinc)

python_tools = [
  'glib-genmarshal',
  'glib-mkenums',
]

python_tools_conf = configuration_data()
python_tools_conf.set('VERSION', glib_version)
python_tools_conf.set('PYTHON', python.path())

foreach tool: python_tools
  tool_bin = configure_file(
    input : tool + '.in',
    output : tool,
    configuration : python_tools_conf,
    install : true,
    install_dir : glib_bindir,
  )

  # Set variables for later use
  set_variable(tool.underscorify(), tool_bin)
endforeach

executable('gobject-query', 'gobject-query.c',
  install : true,
  dependencies : [libglib_dep, libgobject_dep])

install_data('gobject_gdb.py', install_dir : join_paths(glib_pkgdatadir, 'gdb'))
gdb_conf = configuration_data()
gdb_conf.set('datadir', glib_datadir)
configure_file(
  input: 'libgobject-gdb.py.in',
  output: 'libgobject-2.0.so.@0@-gdb.py'.format(library_version),
  configuration: gdb_conf,
  install_dir: gdb_install_dir,
)

if enable_systemtap
  gobject_stp = configure_file(input : 'gobject.stp.in',
    output : '@0@.stp'.format(libgobject.full_path().split('/').get(-1)),
    configuration : stp_cdata,
    install_dir : tapset_install_dir,
    install : true)
endif

subdir('tests')
