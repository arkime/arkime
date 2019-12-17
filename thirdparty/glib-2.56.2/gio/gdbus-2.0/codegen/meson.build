gdbus_codegen_files = [
  '__init__.py',
  'codegen.py',
  'codegen_main.py',
  'codegen_docbook.py',
  'dbustypes.py',
  'parser.py',
  'utils.py',
]

gdbus_codegen_conf = configuration_data()
gdbus_codegen_conf.set('VERSION', glib_version)
gdbus_codegen_conf.set('PYTHON', python.path())
gdbus_codegen_conf.set('DATADIR', glib_datadir)

# Install gdbus-codegen executable
gdbus_codegen = configure_file(input : 'gdbus-codegen.in',
  output : 'gdbus-codegen',
  install : true,
  install_dir : get_option('bindir'),
  configuration : gdbus_codegen_conf
)

codegen_dir = join_paths(get_option('datadir'), 'glib-2.0/codegen')

gdbus_codegen_built_files = []
gdbus_codegen_built_files += configure_file(input : 'config.py.in',
                                            output : 'config.py',
                                            install : true,
                                            install_dir : codegen_dir,
                                            configuration : gdbus_codegen_conf)

blank_conf = configuration_data()
foreach f : gdbus_codegen_files
  # Copy these into the builddir so that gdbus-codegen can be used uninstalled
  # and then install it too so that it can be used after installation
  gdbus_codegen_built_files += configure_file(input : f, output : f,
                                              install : true,
                                              install_dir : codegen_dir,
                                              configuration : blank_conf)
endforeach
