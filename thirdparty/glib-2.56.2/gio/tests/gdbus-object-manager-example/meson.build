# FIXME: set UNINSTALLED_GLIB_{SRC|BUILD}DIR=top_{src|build}dir ?
gdbus_example_objectmanager_generated = custom_target('gdbus-example-objectmanager-generated',
  input  : ['gdbus-example-objectmanager.xml'],
  output : ['gdbus-example-objectmanager-generated.h',
            'gdbus-example-objectmanager-generated.c',
            'gdbus-example-objectmanager-generated-org.gtk.GDBus.Example.ObjectManager.Animal.xml',
            'gdbus-example-objectmanager-generated-org.gtk.GDBus.Example.ObjectManager.Cat.xml'],
  command : [python, gdbus_codegen,
             '--interface-prefix', 'org.gtk.GDBus.Example.ObjectManager.',
             '--c-namespace', 'Example',
             '--c-generate-object-manager',
             '--output-directory', '@OUTDIR@',
             '--generate-c-code', 'gdbus-example-objectmanager-generated',
             '--generate-docbook', 'gdbus-example-objectmanager-generated',
             '@INPUT@'])

libgdbus_example_objectmanager = library('libgdbus-example-objectmanager',
    gdbus_example_objectmanager_generated,
    c_args : gio_c_args,
    dependencies : [libglib_dep, libgmodule_dep, libgobject_dep, libgio_dep],
    install : false)

libgdbus_example_objectmanager_dep = declare_dependency(sources : gdbus_example_objectmanager_generated,
  link_with : libgdbus_example_objectmanager)
