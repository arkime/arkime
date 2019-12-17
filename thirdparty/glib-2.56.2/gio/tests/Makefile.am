include $(top_srcdir)/glib-tap.mk

dist_uninstalled_test_data =
test_ltlibraries =

SUBDIRS = gdbus-object-manager-example services modules

LDADD = \
	$(top_builddir)/gio/libgio-2.0.la		\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la	\
	$(top_builddir)/glib/libglib-2.0.la		\
	$(NULL)

AM_CPPFLAGS = $(gio_INCLUDES) $(GLIB_DEBUG_FLAGS) -I$(top_builddir)/gio -I$(top_srcdir)/gio
DEFS = -DG_LOG_DOMAIN=\"GLib-GIO\" -DTEST_SERVICES=\""$(abs_top_builddir)/gio/tests/services"\"
AM_CFLAGS = $(GLIB_WARN_CFLAGS)
AM_TESTS_ENVIRONMENT += GIO_MODULE_DIR=

# -----------------------------------------------------------------------------
#  Test programs buildable on all platforms

test_programs = \
	appmonitor				\
	async-close-output-stream		\
	async-splice-output-stream		\
	buffered-input-stream			\
	buffered-output-stream			\
	cancellable				\
	contexts				\
	contenttype				\
	converter-stream			\
	credentials				\
	data-input-stream			\
	data-output-stream			\
	fileattributematcher			\
	filter-streams				\
	giomodule				\
	gsubprocess				\
	g-file					\
	g-file-info				\
	g-icon					\
	gdbus-addresses				\
	gdbus-message				\
	inet-address				\
	io-stream				\
	memory-input-stream			\
	memory-output-stream			\
	monitor					\
	network-address				\
	network-monitor				\
	network-monitor-race			\
	permission				\
	pollable				\
	proxy-test				\
	readwrite				\
	simple-async-result			\
	simple-proxy				\
	sleepy-stream				\
	socket					\
	socket-listener				\
	socket-service				\
	srvtarget				\
	task					\
	tls-interaction				\
	vfs					\
	volumemonitor				\
	glistmodel				\
	testfilemonitor				\
	$(NULL)

uninstalled_test_programs = \
	$(NULL)

dist_test_data = \
	contexts.c				\
	g-icon.c				\
	$(NULL)

test_data = \
	$(NULL)

uninstalled_test_extra_programs = \
	gio-du					\
	echo-server				\
	filter-cat				\
	gapplication-example-actions		\
	gapplication-example-cmdline		\
	gapplication-example-cmdline2		\
	gapplication-example-cmdline3		\
	gapplication-example-cmdline4		\
	gapplication-example-dbushooks		\
	gapplication-example-open		\
	gdbus-example-export			\
	gdbus-example-own-name			\
	gdbus-example-peer			\
	gdbus-example-proxy-subclass		\
	gdbus-example-server			\
	gdbus-example-subtree			\
	gdbus-example-watch-name		\
	gdbus-example-watch-proxy		\
	gsubprocess-testprog			\
	httpd					\
	proxy					\
	resolver				\
	send-data				\
	socket-server				\
	$(NULL)

test_extra_programs = \
	gdbus-connection-flush-helper		\
	gdbus-testserver			\
	$(NULL)

dist_uninstalled_test_data += $(addprefix schema-tests/,$(schema_tests))
schema_tests = \
	array-default-not-in-choices.gschema.xml	\
	bad-choice.gschema.xml				\
	bad-key.gschema.xml				\
	bad-key2.gschema.xml				\
	bad-key3.gschema.xml				\
	bad-key4.gschema.xml				\
	bad-type.gschema.xml				\
	bare-alias.gschema.xml				\
	choice-alias.gschema.xml			\
	choice-bad.gschema.xml				\
	choice-badtype.gschema.xml			\
	choice-invalid-alias.gschema.xml		\
	choice-missing-value.gschema.xml		\
	choice-shadowed-alias.gschema.xml		\
	choice-upside-down.gschema.xml			\
	choice.gschema.xml				\
	choices-wrong-type.gschema.xml			\
	default-in-aliases.gschema.xml			\
	default-not-in-choices.gschema.xml		\
	default-out-of-range.gschema.xml		\
	description-xmllang.gschema.xml			\
	empty-key.gschema.xml				\
	enum-with-aliases.gschema.xml			\
	enum-with-bad-default.gschema.xml		\
	enum-with-chained-alias.gschema.xml		\
	enum-with-choice.gschema.xml			\
	enum-with-invalid-alias.gschema.xml		\
	enum-with-invalid-value.gschema.xml		\
	enum-with-repeated-alias.gschema.xml		\
	enum-with-repeated-nick.gschema.xml		\
	enum-with-repeated-value.gschema.xml		\
	enum-with-shadow-alias.gschema.xml		\
	enum.gschema.xml				\
	extend-and-shadow-indirect.gschema.xml		\
	extend-and-shadow.gschema.xml			\
	extend-missing.gschema.xml			\
	extend-nonlist.gschema.xml			\
	extend-self.gschema.xml				\
	extend-wrong-list-indirect.gschema.xml		\
	extend-wrong-list.gschema.xml			\
	extending.gschema.xml				\
	flags-aliased-default.gschema.xml		\
	flags-bad-default.gschema.xml			\
	flags-more-than-one-bit.gschema.xml		\
	flags-with-enum-attr.gschema.xml		\
	flags-with-enum-tag.gschema.xml			\
	from-docs.gschema.xml				\
	incomplete-list.gschema.xml			\
	inherit-gettext-domain.gschema.xml		\
	invalid-path.gschema.xml			\
	key-in-list-indirect.gschema.xml		\
	key-in-list.gschema.xml				\
	list-of-missing.gschema.xml			\
	missing-quotes.gschema.xml			\
	no-default.gschema.xml				\
	overflow.gschema.xml				\
	override-missing.gschema.xml			\
	override-range-error.gschema.xml		\
	override-then-key.gschema.xml			\
	override-twice.gschema.xml			\
	override-type-error.gschema.xml			\
	override.gschema.xml				\
	range-badtype.gschema.xml			\
	range-default-high.gschema.xml			\
	range-default-low.gschema.xml			\
	range-high-default.gschema.xml			\
	range-low-default.gschema.xml			\
	range-missing-max.gschema.xml			\
	range-missing-min.gschema.xml			\
	range-parse-error.gschema.xml			\
	range-wrong-type.gschema.xml			\
	range.gschema.xml				\
	summary-xmllang.gschema.xml			\
	summary-xmllang-and-attrs.gschema.xml		\
	wrong-category.gschema.xml			\
	$(NULL)

test_programs += thumbnail-verification
dist_test_data += $(thumbnail_data_files)
thumbnail_data_files = $(addprefix thumbnails/,$(thumbnail_tests))
thumbnail_tests = \
	bad-header.png \
	empty-key.png \
	header-and-chunk-size.png \
	header-only.png \
	huge-chunk-size.png \
	mtime-zero.png \
	no-text-data.png \
	overlong-value.png \
	uri-mismatch.png \
	valid.png \
	valid-no-size.png \
	$(NULL)

test_programs += tls-certificate
tls_certificate_SOURCES = \
	tls-certificate.c			\
	gtesttlsbackend.c			\
	gtesttlsbackend.h
dist_test_data += $(cert_data_files)
cert_data_files = $(addprefix cert-tests/,$(cert_tests))
cert_tests = \
	cert1.pem	\
	cert2.pem	\
	cert3.pem	\
	cert-key.pem	\
	cert-list.pem	\
	key8.pem	\
	key-cert.pem	\
	key.pem		\
	nothing.pem	\
	$(NULL)

uninstalled_test_extra_programs += socket-client
socket_client_SOURCES = \
	socket-client.c				\
	gtlsconsoleinteraction.c		\
	gtlsconsoleinteraction.h
EXTRA_DIST += socket-common.c

uninstalled_test_extra_programs += gdbus-daemon
gdbus_daemon_SOURCES = gdbus-daemon.c
nodist_gdbus_daemon_SOURCES = \
	gdbus-daemon-generated.c \
	gdbus-daemon-impl.c
CLEANFILES += gdbus-daemon-impl.c gdbus-daemon-generated.c gdbus-daemon-generated.h

# With subdir-objects we need to create a link to the original
# file in the right directory, otherwise libtool will complain
# that it cannot find the wrapper file
gdbus-daemon-impl.c: $(top_srcdir)/gio/gdbusdaemon.c
	$(AM_V_GEN) $(LN_S) $^ $@

# These files are only generated on Windows builds inside GIO,
# but we want them on non-Windows builds for the tests
gdbus-daemon-generated.h gdbus-daemon-generated.c: $(top_srcdir)/gio/dbus-daemon.xml $(GDBUS_PYTHON_DEPS)
	$(AM_V_GEN) UNINSTALLED_GLIB_SRCDIR=$(top_srcdir) \
		UNINSTALLED_GLIB_BUILDDIR=$(top_builddir) \
		$(PYTHON) $(top_srcdir)/gio/gdbus-2.0/codegen/gdbus-codegen.in \
		--interface-prefix org. \
		--generate-c-code gdbus-daemon-generated \
		--c-namespace _G \
		$(top_srcdir)/gio/dbus-daemon.xml \
		$(NULL)

# -----------------------------------------------------------------------------
#  Test programs buildable on UNIX only

if OS_UNIX
test_programs += \
	file					\
	gdbus-peer-object-manager		\
	live-g-file				\
	socket-address				\
	stream-rw_all				\
	unix-fd					\
	unix-mounts				\
	unix-streams				\
	g-file-info-filesystem-readonly		\
	$(NULL)

test_extra_programs += \
	basic-application			\
	dbus-launch				\
	$(NULL)

if !OS_COCOA
# Uninstalled because of the check-for-executable logic in DesktopAppInfo unable to find the installed executable
uninstalled_test_programs += \
	appinfo					\
	desktop-app-info			\
	$(NULL)
endif

home_desktop_files = \
	epiphany-weather-for-toronto-island-9c6a4e022b17686306243dada811d550d25eb1fb.desktop	\
	eog.desktop

usr_desktop_files = \
	baobab.desktop				\
	cheese.desktop				\
	dconf-editor.desktop			\
	eog.desktop				\
	evince-previewer.desktop		\
	evince.desktop				\
	file-roller.desktop			\
	gcr-prompter.desktop			\
	gcr-viewer.desktop			\
	gedit.desktop				\
	glade.desktop				\
	gnome-clocks.desktop			\
	gnome-contacts.desktop			\
	gnome-font-viewer.desktop		\
	gnome-music.desktop			\
	gnome-terminal.desktop			\
	gucharmap.desktop			\
	kde4/dolphin.desktop			\
	kde4/kate.desktop			\
	kde4/konqbrowser.desktop		\
	kde4/okular.desktop			\
	mimeinfo.cache				\
	nautilus-autorun-software.desktop	\
	nautilus-classic.desktop		\
	nautilus-connect-server.desktop		\
	nautilus.desktop			\
	totem.desktop				\
	yelp.desktop

dist_test_data += \
	$(addprefix desktop-files/usr/applications/,$(usr_desktop_files))	\
	$(addprefix desktop-files/home/applications/,$(home_desktop_files))

dist_test_data += \
	appinfo-test-actions.desktop		\
	appinfo-test-gnome.desktop		\
	appinfo-test-notgnome.desktop		\
	appinfo-test.desktop			\
	appinfo-test2.desktop			\
	file.c					\
	org.gtk.test.dbusappinfo.desktop	\
	x-content/image-dcf/DCIM/Camera/20130831_203925.jpg \
	x-content/image-dcf/DCIM/Camera/20130831_203928.jpg \
	x-content/unix-software/autorun.sh 	\
	x-content/win32-software/autorun.exe	\
	$(NULL)

test_extra_programs += \
	appinfo-test				\
	$(NULL)

uninstalled_test_extra_programs += \
	gdbus-example-unix-fd-client		\
	$(NULL)

if !OS_COCOA
test_extra_programs += apps
test_programs += mimeapps
clean-local: clean-mimeapps
clean-mimeapps:
	rm -rf xdgdatadir xdgdatahome xdgconfighome
endif

uninstalled_test_programs += gsettings gschema-compile
gsettings_DEPENDENCIES = test.mo
CLEANFILES += test.mo de/LC_MESSAGES/test.mo keyfile/gsettings.store
gsettings_CFLAGS = $(AM_CFLAGS) -DSRCDIR=\"$(abs_srcdir)\"
test.mo: de.po
	$(AM_V_GEN) $(MSGFMT) -o test.mo $(srcdir)/de.po; \
	$(MKDIR_P) de/LC_MESSAGES; \
	cp -f test.mo de/LC_MESSAGES
EXTRA_DIST += de.po
dist_uninstalled_test_data += \
	org.gtk.test.gschema.xml.orig		\
	org.gtk.schemasourcecheck.gschema.xml	\
	testenum.h				\
	enums.xml.template
# Generated while running the testcase itself...
CLEANFILES += \
	org.gtk.test.gschema.xml	\
	org.gtk.test.enums.xml		\
	gsettings.store			\
	gschemas.compiled		\
	schema-source/gschemas.compiled

test_programs += gdbus-connection-flush
gdbus_connection_flush_SOURCES = \
	gdbus-connection-flush.c		\
	test-io-stream.c			\
	test-io-stream.h			\
	test-pipe-unix.c			\
	test-pipe-unix.h

test_programs += gdbus-non-socket
gdbus_non_socket_SOURCES = \
	gdbus-non-socket.c			\
	gdbus-tests.c				\
	gdbus-tests.h				\
	test-io-stream.c			\
	test-io-stream.h			\
	test-pipe-unix.c			\
	test-pipe-unix.h

# These three are manual-run tests because they need a session bus but don't bring one up themselves
uninstalled_test_extra_programs += gdbus-example-objectmanager-client
gdbus_example_objectmanager_client_LDADD = gdbus-object-manager-example/libgdbus-example-objectmanager.la $(LDADD)

uninstalled_test_extra_programs += gdbus-example-objectmanager-server
gdbus_example_objectmanager_server_LDADD = gdbus-object-manager-example/libgdbus-example-objectmanager.la $(LDADD)

test_extra_programs += gsubprocess-testprog

uninstalled_test_extra_programs += gdbus-test-fixture
gdbus_test_fixture_LDADD = gdbus-object-manager-example/libgdbus-example-objectmanager.la $(LDADD)

# This is peer to peer so it doesn't need a session bus (so we can run it normally)
gdbus_peer_LDADD = gdbus-object-manager-example/libgdbus-example-objectmanager.la $(LDADD)

# This test is currently unreliable
test_extra_programs += gdbus-overflow

# -----------------------------------------------------------------------------
#  Test programs that need to bring up a session bus (requires dbus-daemon)

if HAVE_DBUS_DAEMON
gdbus_sessionbus_sources = gdbus-sessionbus.c gdbus-sessionbus.h gdbus-tests.h gdbus-tests.c

test_programs += \
	actions					\
	defaultvalue				\
	gapplication				\
	gdbus-auth				\
	gdbus-bz627724				\
	gdbus-close-pending			\
	gdbus-connection			\
	gdbus-connection-loss			\
	gdbus-connection-slow			\
	gdbus-error				\
	gdbus-exit-on-close			\
	gdbus-export				\
	gdbus-introspection			\
	gdbus-names				\
	gdbus-peer				\
	gdbus-proxy				\
	gdbus-proxy-threads			\
	gdbus-proxy-well-known-name		\
	gdbus-test-codegen			\
	gdbus-test-codegen-old			\
	gdbus-threading				\
	gmenumodel				\
	gnotification				\
	$(NULL)

if OS_UNIX
test_programs += gdbus-unix-addresses

if !OS_COCOA
test_programs += dbus-appinfo
endif

endif

gdbus_proxy_threads_CFLAGS = $(AM_CFLAGS) $(DBUS1_CFLAGS)
actions_SOURCES                          = $(gdbus_sessionbus_sources) actions.c
dbus_appinfo_SOURCES                     = $(gdbus_sessionbus_sources) dbus-appinfo.c
gapplication_SOURCES                     = $(gdbus_sessionbus_sources) gapplication.c
gdbus_auth_SOURCES                       = $(gdbus_sessionbus_sources) gdbus-auth.c
gdbus_bz627724_SOURCES                   = $(gdbus_sessionbus_sources) gdbus-bz627724.c
gdbus_close_pending_SOURCES              = $(gdbus_sessionbus_sources) gdbus-close-pending.c
gdbus_connection_SOURCES                 = $(gdbus_sessionbus_sources) gdbus-connection.c
gdbus_connection_loss_SOURCES            = $(gdbus_sessionbus_sources) gdbus-connection-loss.c
gdbus_connection_slow_SOURCES            = $(gdbus_sessionbus_sources) gdbus-connection-slow.c
gdbus_error_SOURCES                      = $(gdbus_sessionbus_sources) gdbus-error.c
gdbus_exit_on_close_SOURCES              = $(gdbus_sessionbus_sources) gdbus-exit-on-close.c
gdbus_export_SOURCES                     = $(gdbus_sessionbus_sources) gdbus-export.c
gdbus_introspection_SOURCES              = $(gdbus_sessionbus_sources) gdbus-introspection.c
gdbus_names_SOURCES                      = $(gdbus_sessionbus_sources) gdbus-names.c
gdbus_proxy_SOURCES                      = $(gdbus_sessionbus_sources) gdbus-proxy.c
gdbus_proxy_threads_SOURCES              = $(gdbus_sessionbus_sources) gdbus-proxy-threads.c
gdbus_proxy_well_known_name_SOURCES      = $(gdbus_sessionbus_sources) gdbus-proxy-well-known-name.c
gdbus_test_codegen_SOURCES               = $(gdbus_sessionbus_sources) gdbus-test-codegen.c
nodist_gdbus_test_codegen_SOURCES        = gdbus-test-codegen-generated.c gdbus-test-codegen-generated.h
gdbus_test_codegen_old_SOURCES           = $(gdbus_sessionbus_sources) gdbus-test-codegen.c
nodist_gdbus_test_codegen_old_SOURCES    = gdbus-test-codegen-generated.c gdbus-test-codegen-generated.h
gdbus_test_codegen_old_CPPFLAGS          = $(AM_CPPFLAGS) -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_36 -DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_36
gdbus_threading_SOURCES                  = $(gdbus_sessionbus_sources) gdbus-threading.c
gmenumodel_SOURCES                       = $(gdbus_sessionbus_sources) gmenumodel.c
gnotification_SOURCES                    = $(gdbus_sessionbus_sources) gnotification.c gnotification-server.h gnotification-server.c

BUILT_SOURCES += gdbus-test-codegen-generated.c gdbus-test-codegen-generated.h
gdbus-test-codegen.o: gdbus-test-codegen-generated.h
gdbus-test-codegen-generated.h: test-codegen.xml Makefile $(top_builddir)/gio/gdbus-2.0/codegen/gdbus-codegen
	$(AM_V_GEN) UNINSTALLED_GLIB_SRCDIR=$(top_srcdir) \
		UNINSTALLED_GLIB_BUILDDIR=$(top_builddir) \
		$(PYTHON) $(top_builddir)/gio/gdbus-2.0/codegen/gdbus-codegen \
		--interface-prefix org.project. \
		--generate-c-code gdbus-test-codegen-generated \
		--c-generate-object-manager \
		--c-namespace Foo_iGen \
		--generate-docbook gdbus-test-codegen-generated-doc \
		--annotate "org.project.Bar" Key1 Value1 \
		--annotate "org.project.Bar" org.gtk.GDBus.Internal Value2 \
		--annotate "org.project.Bar.HelloWorld()" Key3 Value3 \
		--annotate "org.project.Bar::TestSignal" Key4 Value4 \
		--annotate "org.project.Bar:ay" Key5 Value5 \
		--annotate "org.project.Bar.TestPrimitiveTypes()[val_int32]" Key6 Value6 \
		--annotate "org.project.Bar.TestPrimitiveTypes()[ret_uint32]" Key7 Value7 \
		--annotate "org.project.Bar::TestSignal[array_of_strings]" Key8 Value8 \
		$(srcdir)/test-codegen.xml \
		$(NULL)
gdbus-test-codegen-generated.c: gdbus-test-codegen-generated.h
	@: # Generated as side-effect of .h

EXTRA_DIST += test-codegen.xml
CLEANFILES += gdbus-test-codegen-generated.[ch] gdbus-test-codegen-generated-doc-*.xml
endif # OS_UNIX
endif # HAVE_DBUS_DAEMON

tls_interaction_SOURCES = tls-interaction.c gtesttlsbackend.c gtesttlsbackend.h

# -----------------------------------------------------------------------------

if OS_WIN32
test_programs += win32-streams
endif

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if HAVE_DBUS1
test_programs += gdbus-serialization
gdbus_serialization_CFLAGS = $(AM_CFLAGS) $(DBUS1_CFLAGS)
gdbus_serialization_LDADD = $(LDADD) $(DBUS1_LIBS)
gdbus_serialization_SOURCES = \
	gdbus-serialization.c			\
	gdbus-tests.h				\
	gdbus-tests.c
endif

if HAVE_GCC
test_programs += \
	autoptr				\
	$(NULL)
endif

# -----------------------------------------------------------------------------
#  The resources test is a bit more complicated, and we cannot build it when
#  cross-compiling GIO because it requires running a binary...

if !CROSS_COMPILING
test_programs += resources
resources_SOURCES = resources.c
nodist_resources_SOURCES = test_resources.c test_resources2.c test_resources2.h
resources_DEPENDENCIES = test.gresource

test_ltlibraries += libresourceplugin.la
libresourceplugin_la_SOURCES = resourceplugin.c plugin_resources.c
libresourceplugin_la_LDFLAGS = -avoid-version -module -export-dynamic $(no_undefined)
libresourceplugin_la_LIBADD = $(LDADD)

test_data += test.gresource

# libtool contains a bug whereby the created .la file doesn't contain the correct dlname='' in the case that
# you're building a library but not installing it.  This is apparently because the only considered use for an
# uninstalled library is as a convenience library for linking (despite the fact that we give -module).  The lack
# of dlname='' in the .la trips up libltdl and GModule as well.  We can trick libtool into believing that we
# will install the module by giving it a bogus -rpath for the uninstalled cases.
#
# See http://lists.gnu.org/archive/html/bug-libtool/2013-05/msg00009.html
if !ENABLE_INSTALLED_TESTS
libresourceplugin_la_LDFLAGS += -rpath /
endif

glib_compile_resources=$(top_builddir)/gio/glib-compile-resources

test-generated.txt: test1.txt
	$(AM_V_GEN) echo "Generated" > $@ && \
	  cat $< >> $@

resources.o: test_resources2.h
test_resources.c: test2.gresource.xml Makefile $(shell $(glib_compile_resources) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/test2.gresource.xml)
	$(AM_V_GEN) $(glib_compile_resources) --target=$@ --sourcedir=$(srcdir) --generate-source --c-name _g_test1 $<

test_resources2.h test_resources2.c: test3.gresource.xml Makefile $(shell $(glib_compile_resources) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/test3.gresource.xml)
	$(AM_V_GEN) $(glib_compile_resources) --target=$@ --sourcedir=$(srcdir) --generate --c-name _g_test2 --manual-register $<

plugin_resources.c: test4.gresource.xml Makefile $(shell $(glib_compile_resources) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/test4.gresource.xml)
	$(AM_V_GEN) $(glib_compile_resources) --target=$@ --sourcedir=$(srcdir) --generate-source --c-name _g_plugin $<

test.gresource: test.gresource.xml Makefile $(shell $(glib_compile_resources) --sourcedir=. --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/test.gresource.xml)
	$(AM_V_GEN) $(glib_compile_resources) --target=$@ --sourcedir=. --sourcedir=$(srcdir) $<

EXTRA_DIST += test.gresource.xml test1.txt test2.gresource.xml test2.txt test3.gresource.xml test3.txt test4.gresource.xml
CLEANFILES += test-generated.txt test_resources.c test_resources2.[ch] plugin_resources.c test.gresource
endif # !CROSS_COMPILING

BUILT_SOURCES += giotypefuncs.inc

giotypefuncs.inc: Makefile
	$(AM_V_GEN) echo '#include <gio/gio.h>' > xgen-giosrc.c && \
	  echo "G_GNUC_BEGIN_IGNORE_DEPRECATIONS" > xgen-gio && \
	  ${CPP} $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) xgen-giosrc.c | \
	  $(GREP) -o '\bg_[A-Za-z0-9_]*_get_type\b' | \
	  $(GREP) -v 'g_io_extension_get_type\|g_variant_get_type' | \
	  LC_ALL=C sort | uniq | \
	  $(SED) -e 's/^/*tp++ = /' -e 's/$$/ ();/' >> xgen-gio && \
	  cp xgen-gio $@ # && rm -f xgen-gio xgen-giosrc.c

CLEANFILES += xgen-giosrc.c xgen-gio giotypefuncs.inc

if ENABLE_INSTALLED_TESTS
if OS_UNIX
install-data-hook:
	$(AM_V_at) chmod a+x $(DESTDIR)$(installed_testdir)/x-content/win32-software/autorun.exe
endif
endif
