if get_option('gtk_doc')
  subdir('xml')

  ignore_headers = [
    'tests',
    'gatomicarray.h',
    'gobject_trace.h',
    'gtype-private.h'
  ]

  ignore_decorators = [
    'GLIB_VAR',
    'G_GNUC_INTERNAL',
    'G_GNUC_WARN_UNUSED_RESULT',
    'GLIB_AVAILABLE_IN_ALL',
    'GLIB_AVAILABLE_IN_2_26',
    'GLIB_AVAILABLE_IN_2_28',
    'GLIB_AVAILABLE_IN_2_30',
    'GLIB_AVAILABLE_IN_2_32',
    'GLIB_AVAILABLE_IN_2_34',
    'GLIB_AVAILABLE_IN_2_36',
    'GLIB_AVAILABLE_IN_2_38',
    'GLIB_AVAILABLE_IN_2_40',
    'GLIB_AVAILABLE_IN_2_42',
    'GLIB_AVAILABLE_IN_2_44',
    'GLIB_AVAILABLE_IN_2_46',
    'GLIB_AVAILABLE_IN_2_48',
    'GLIB_AVAILABLE_IN_2_50',
    'GLIB_AVAILABLE_IN_2_52',
    'GLIB_AVAILABLE_IN_2_54',
    'GLIB_AVAILABLE_IN_2_56',
    'GLIB_DEPRECATED_IN_2_26',
    'GLIB_DEPRECATED_IN_2_26_FOR',
    'GLIB_DEPRECATED_IN_2_28',
    'GLIB_DEPRECATED_IN_2_28_FOR',
    'GLIB_DEPRECATED_IN_2_30',
    'GLIB_DEPRECATED_IN_2_30_FOR',
    'GLIB_DEPRECATED_IN_2_32',
    'GLIB_DEPRECATED_IN_2_32_FOR',
    'GLIB_DEPRECATED_IN_2_34',
    'GLIB_DEPRECATED_IN_2_34_FOR',
    'GLIB_DEPRECATED_IN_2_36',
    'GLIB_DEPRECATED_IN_2_36_FOR',
    'GLIB_DEPRECATED_IN_2_38',
    'GLIB_DEPRECATED_IN_2_38_FOR',
    'GLIB_DEPRECATED_IN_2_40',
    'GLIB_DEPRECATED_IN_2_40_FOR',
    'GLIB_DEPRECATED_IN_2_42',
    'GLIB_DEPRECATED_IN_2_42_FOR',
    'GLIB_DEPRECATED_IN_2_44',
    'GLIB_DEPRECATED_IN_2_44_FOR',
    'GLIB_DEPRECATED_IN_2_46',
    'GLIB_DEPRECATED_IN_2_46_FOR',
    'GLIB_DEPRECATED_IN_2_48',
    'GLIB_DEPRECATED_IN_2_48_FOR',
    'GLIB_DEPRECATED_IN_2_50',
    'GLIB_DEPRECATED_IN_2_50_FOR',
    'GLIB_DEPRECATED_IN_2_52',
    'GLIB_DEPRECATED_IN_2_52_FOR',
    'GLIB_DEPRECATED_IN_2_54',
    'GLIB_DEPRECATED_IN_2_54_FOR',
    'GLIB_DEPRECATED_IN_2_56',
    'GLIB_DEPRECATED_IN_2_56_FOR',
  ]

  docpath = join_paths(glib_datadir, 'gtk-doc', 'html')
  version_conf = configuration_data()
  version_conf.set('GLIB_VERSION', meson.project_version())
  configure_file(
    input: 'version.xml.in',
    output: 'version.xml',
    configuration: version_conf
  )

  gtkdocincl = include_directories('.')

  gnome.gtkdoc('gobject',
    main_xml : 'gobject-docs.xml',
    namespace : 'g',
    gobject_typesfile : join_paths(meson.current_source_dir(), 'gobject.types'),
    mode : 'none',
    dependencies : [libgobject_dep, libglib_dep],
    include_directories : [gtkdocincl],
    src_dir : 'gobject',
    scan_args : [
      '--deprecated-guards=G_DISABLE_DEPRECATED',
      '--ignore-decorators=' + '|'.join(ignore_decorators),
      '--ignore-headers=' + ' '.join(ignore_headers),
    ],
    content_files : [
      'glib-mkenums.xml',
      'glib-genmarshal.xml',
      'gobject-query.xml',
      'tut_gobject.xml',
      'tut_gsignal.xml',
      'tut_gtype.xml',
      'tut_howto.xml',
      'tut_intro.xml',
      'tut_tools.xml'
    ],
    html_assets : [
      'images/glue.png'
    ],
    fixxref_args: [
      '--html-dir=' + docpath,
      '--extra-dir=' + join_paths(meson.current_build_dir(), '../glib/html'),
    ],
    install: true
  )
endif

if get_option('man')
  manpages = ['glib-mkenums', 'glib-genmarshal', 'gobject-query']
  foreach page : manpages
    custom_target(page + '-man',
      input: page + '.xml',
      output: page + '.1',
      command: xsltproc_command,
      install: true,
      install_dir: man1_dir)
  endforeach
endif
