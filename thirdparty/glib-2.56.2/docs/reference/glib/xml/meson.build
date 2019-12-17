ent_conf = configuration_data()
ent_conf.set('PACKAGE', 'glib')
ent_conf.set('PACKAGE_BUGREPORT', 'https://bugzilla.gnome.org/enter_bug.cgi?product=glib')
ent_conf.set('PACKAGE_NAME', 'glib')
ent_conf.set('PACKAGE_STRING', 'glib')
ent_conf.set('PACKAGE_TARNAME', 'glib')
ent_conf.set('PACKAGE_URL', 'FIXME')
ent_conf.set('PACKAGE_VERSION', glib_version)
ent_conf.set('PACKAGE_API_VERSION', glib_api_version)
configure_file(
  input: 'gtkdocentities.ent.in',
  output: 'gtkdocentities.ent',
  configuration: ent_conf
)
