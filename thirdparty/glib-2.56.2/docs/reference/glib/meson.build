if get_option('gtk_doc')
  subdir('xml')

  ignore_headers = [
    'gallocator.h',
    'gdatasetprivate.h',
    'glibintl.h',
    'gbsearcharray.h',
    'glib-private.h',
    'gmoduleconf.h',
    'gthreadprivate.h',
    'gunibreak.h',
    'gunicomp.h',
    'gunidecomp.h',
    'gunichartables.h',
    'glib_probes.h',
    'glib_trace.h',
    'libcharset.h',
    'gdebug.h',
    'gprintfint.h',
    'gmirroringtable.h',
    'gscripttable.h',
    'glib-mirroring-tab',
    'gnulib',
    'pcre',
    'update-pcre',
    'gbytesprivate.h',
    'gvariant-internal.h',
    'gvariant-serialiser.h',
    'gvariant-core.h',
    'gvarianttypeinfo.h',
    'gwakeup.h',
    'gtranslit-data.h',
    'glib-init.h',
    'gconstructor.h',
    'valgrind.h',
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

  gnome.gtkdoc('glib',
               main_xml : 'glib-docs.xml',
               namespace : 'g',
               mode : 'none',
               src_dir : [ 'glib', 'gmodule' ],
               dependencies : libglib_dep,
               scan_args : [
                 '--rebuild-types',
                 '--deprecated-guards=G_DISABLE_DEPRECATED',
                 '--ignore-decorators=' + '|'.join(ignore_decorators),
                 '--ignore-headers=' + ' '.join(ignore_headers),
               ],
               content_files : [
                 'cross.xml',
                 'running.xml',
                 'building.xml',
                 'changes.xml',
                 'compiling.xml',
                 'programming.xml',
                 'resources.xml',
                 'regex-syntax.xml',
                 'glib-gettextize.xml',
                 'gtester.xml',
                 'gtester-report.xml',
                 'gvariant-varargs.xml',
                 'gvariant-text.xml',
               ],
               expand_content_files : [
                 'compiling.xml',
               ],
               html_assets : [
                 'file-name-encodings.png',
                 'mainloop-states.gif',
                 'Sorted_binary_tree_breadth-first_traversal.svg',
                 'Sorted_binary_tree_inorder.svg',
                 'Sorted_binary_tree_postorder.svg',
                 'Sorted_binary_tree_preorder.svg',
               ],
               fixxref_args: [
                 '--html-dir=' + docpath,
                 # TODO: gobject/gio extra dirs
               ],
               install: true)
endif

if get_option('man')
  manpages = ['glib-gettextize', 'gtester', 'gtester-report']
  foreach page : manpages
    custom_target(page + '-man',
      input: page + '.xml',
      output: page + '.1',
      command: xsltproc_command,
      install: true,
      install_dir: man1_dir)
  endforeach
endif
