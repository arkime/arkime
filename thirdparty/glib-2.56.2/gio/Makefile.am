include $(top_srcdir)/glib.mk

SUBDIRS = gdbus-2.0/codegen

if OS_UNIX
SUBDIRS += xdgmime
endif

if OS_WIN32_AND_DLL_COMPILATION
if MS_LIB_AVAILABLE
noinst_DATA += gio-2.0.lib

install_ms_lib_cmd = $(INSTALL) gio-2.0.lib $(DESTDIR)$(libdir)
uninstall_ms_lib_cmd = -rm $(DESTDIR)$(libdir)/gio-2.0.lib
endif
endif

install-ms-lib:
	$(install_ms_lib_cmd)

uninstall-ms-lib:
	$(uninstall_ms_lib_cmd)

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"GLib-GIO\"			\
	$(gmodule_INCLUDES)				\
	$(GLIB_DEBUG_FLAGS)				\
	-DGIO_COMPILATION				\
	-DGIO_MODULE_DIR=\"$(GIO_MODULE_DIR)\"	

AM_CFLAGS = $(GLIB_WARN_CFLAGS)

lib_LTLIBRARIES = libgio-2.0.la

gdbus_headers = 			\
	gdbusauthobserver.h		\
	gcredentials.h			\
	gdbusutils.h			\
	gdbuserror.h			\
	gdbusaddress.h			\
	gdbusconnection.h		\
	gdbusmessage.h			\
	gdbusnameowning.h		\
	gdbusnamewatching.h		\
	gdbusproxy.h			\
	gdbusintrospection.h		\
	gdbusmethodinvocation.h		\
	gdbusserver.h			\
	gdbusinterface.h		\
	gdbusinterfaceskeleton.h	\
	gdbusobject.h			\
	gdbusobjectskeleton.h		\
	gdbusobjectproxy.h		\
	gdbusobjectmanager.h		\
	gdbusobjectmanagerclient.h	\
	gdbusobjectmanagerserver.h	\
	gtestdbus.h			\
	$(NULL)

gdbus_sources = 							\
	gdbusutils.h			gdbusutils.c			\
	gdbusaddress.h			gdbusaddress.c			\
	gdbusauthobserver.h		gdbusauthobserver.c		\
	gdbusauth.h			gdbusauth.c			\
	gdbusauthmechanism.h		gdbusauthmechanism.c		\
	gdbusauthmechanismanon.h	gdbusauthmechanismanon.c	\
	gdbusauthmechanismexternal.h	gdbusauthmechanismexternal.c	\
	gdbusauthmechanismsha1.h	gdbusauthmechanismsha1.c	\
	gdbuserror.h			gdbuserror.c			\
	gdbusconnection.h		gdbusconnection.c		\
	gdbusmessage.h			gdbusmessage.c			\
	gdbusnameowning.h		gdbusnameowning.c		\
	gdbusnamewatching.h		gdbusnamewatching.c		\
	gdbusproxy.h			gdbusproxy.c			\
	gdbusprivate.h			gdbusprivate.c			\
	gdbusintrospection.h		gdbusintrospection.c		\
	gdbusmethodinvocation.h		gdbusmethodinvocation.c		\
	gdbusserver.h			gdbusserver.c			\
	gdbusinterface.h		gdbusinterface.c		\
	gdbusinterfaceskeleton.h	gdbusinterfaceskeleton.c	\
	gdbusobject.h			gdbusobject.c			\
	gdbusobjectskeleton.h		gdbusobjectskeleton.c		\
	gdbusobjectproxy.h		gdbusobjectproxy.c		\
	gdbusobjectmanager.h		gdbusobjectmanager.c		\
	gdbusobjectmanagerclient.h	gdbusobjectmanagerclient.c	\
	gdbusobjectmanagerserver.h	gdbusobjectmanagerserver.c	\
	gtestdbus.h			gtestdbus.c			\
	$(NULL)

# These are not built into the library yet
EXTRA_DIST += gdbusdaemon.c gdbusdaemon.h dbus-daemon.xml

GDBUS_PYTHON_DEPS = 					\
	$(srcdir)/gdbus-2.0/codegen/gdbus-codegen.in	\
	$(srcdir)/gdbus-2.0/codegen/codegen_main.py 	\
	$(srcdir)/gdbus-2.0/codegen/parser.py 		\
	$(srcdir)/gdbus-2.0/codegen/codegen_docbook.py 	\
	$(srcdir)/gdbus-2.0/codegen/codegen.py 		\
	$(srcdir)/gdbus-2.0/codegen/__init__.py 	\
	$(srcdir)/gdbus-2.0/codegen/dbustypes.py 	\
	$(builddir)/gdbus-2.0/codegen/config.py 	\
	$(srcdir)/gdbus-2.0/codegen/utils.py

gdbus-daemon-generated.h gdbus-daemon-generated.c: $(srcdir)/dbus-daemon.xml $(GDBUS_PYTHON_DEPS)
	$(AM_V_GEN) UNINSTALLED_GLIB_SRCDIR=$(top_srcdir) \
		UNINSTALLED_GLIB_BUILDDIR=$(top_builddir) \
		$(PYTHON) $(srcdir)/gdbus-2.0/codegen/gdbus-codegen.in \
		--interface-prefix org. \
		--generate-c-code gdbus-daemon-generated \
		--c-namespace _G \
		$(srcdir)/dbus-daemon.xml \
		$(NULL)

settings_headers = \
	gsettingsbackend.h		\
	gsettingsschema.h		\
	gsettings.h

settings_base_sources = \
	gvdb/gvdb-format.h		\
	gvdb/gvdb-reader.h		\
	gvdb/gvdb-reader.c		\
	gdelayedsettingsbackend.h	\
	gdelayedsettingsbackend.c	\
	gkeyfilesettingsbackend.c	\
	gmemorysettingsbackend.c	\
	gnullsettingsbackend.c		\
	gsettingsbackendinternal.h	\
	gsettingsbackend.c		\
	gsettingsschema.h		\
	gsettingsschema-internal.h	\
	gsettingsschema.c		\
	gsettings-mapping.h		\
	gsettings-mapping.c		\
	gsettings.c

settings_sources = $(settings_base_sources)

if OS_WIN32
win32_settings_sources = \
	gregistrysettingsbackend.h	\
	gregistrysettingsbackend.c

settings_sources += $(win32_settings_sources)
endif

application_headers = \
	gapplication.h			\
	gapplicationcommandline.h	\
					\
	gactiongroup.h			\
	gactionmap.h			\
	gsimpleactiongroup.h		\
	gremoteactiongroup.h		\
	gactiongroupexporter.h		\
	gdbusactiongroup.h		\
	gaction.h			\
	gpropertyaction.h		\
	gsimpleaction.h			\
					\
	gmenumodel.h			\
	gmenu.h				\
	gmenuexporter.h			\
	gdbusmenumodel.h		\
	gnotification.h			\
	$(NULL)

application_sources = \
	gapplication.c				\
	gapplicationcommandline.c		\
	gapplicationimpl-dbus.c			\
	gapplicationimpl.h			\
						\
	gactiongroup.c				\
	gactionmap.c				\
	gsimpleactiongroup.c			\
	gremoteactiongroup.c			\
	gactiongroupexporter.c			\
	gdbusactiongroup-private.h		\
	gdbusactiongroup.c			\
	gaction.c				\
	gpropertyaction.c			\
	gsimpleaction.c				\
						\
	gmenumodel.c				\
	gmenu.c					\
	gmenuexporter.c				\
	gdbusmenumodel.c			\
	gnotification-private.h			\
	gnotificationbackend.h			\
	gnotification.c				\
	gnotificationbackend.c			\
	$(NULL)

local_sources = \
	ghttpproxy.c			\
	ghttpproxy.h			\
	glocalfile.c 			\
	glocalfile.h 			\
	glocalfileprivate.h		\
	glocalfileenumerator.c 		\
	glocalfileenumerator.h 		\
	glocalfileinfo.c 		\
	glocalfileinfo.h 		\
	glocalfileinputstream.c 	\
	glocalfileinputstream.h 	\
	glocalfilemonitor.c 		\
	glocalfilemonitor.h 		\
	glocalfileoutputstream.c 	\
	glocalfileoutputstream.h 	\
	glocalfileiostream.c		\
	glocalfileiostream.h		\
	glocalvfs.c 			\
	glocalvfs.h 			\
	gsocks4proxy.c			\
	gsocks4proxy.h			\
	gsocks4aproxy.c			\
	gsocks4aproxy.h			\
	gsocks5proxy.c			\
	gsocks5proxy.h			\
	thumbnail-verify.h		\
	thumbnail-verify.c		\
	$(NULL)

platform_libadd =
platform_deps =
appinfo_sources =
appinfo_headers =
contenttype_sources =

if HAVE_INOTIFY
SUBDIRS += inotify
platform_libadd += inotify/libinotify.la
platform_deps += inotify/libinotify.la
endif

if HAVE_KQUEUE
SUBDIRS += kqueue
platform_libadd += kqueue/libkqueue.la
platform_deps += kqueue/libkqueue.la
endif

if OS_WIN32
SUBDIRS += win32
platform_libadd += win32/libgiowin32.la
platform_deps += win32/libgiowin32.la
endif

SUBDIRS += . tests

if HAVE_FAM
SUBDIRS += fam
endif

if OS_UNIX
platform_libadd += xdgmime/libxdgmime.la
platform_deps += xdgmime/libxdgmime.la
if !OS_COCOA
appinfo_headers += gdesktopappinfo.h
endif


unix_sources = \
	gfiledescriptorbased.c  \
	gunixconnection.c	\
	gunixcredentialsmessage.c	\
	gunixfdlist.c		\
	gunixfdmessage.c	\
	gunixmount.c		\
	gunixmount.h		\
	gunixmounts.c 		\
	gunixsocketaddress.c	\
	gunixvolume.c 		\
	gunixvolume.h 		\
	gunixvolumemonitor.c 	\
	gunixvolumemonitor.h 	\
	gunixinputstream.c 	\
	gunixoutputstream.c 	\
	gcontenttypeprivate.h 	\
	gfdonotificationbackend.c \
	ggtknotificationbackend.c \
	gportalnotificationbackend.c \
	gdocumentportal.c	\
	gdocumentportal.h	\
	gopenuriportal.c	\
	gopenuriportal.h	\
	gportalsupport.c	\
	gportalsupport.h	\
	$(portal_sources)	\
	$(NULL)

giounixincludedir=$(includedir)/gio-unix-2.0/gio
giounixinclude_HEADERS = \
	gfiledescriptorbased.h  \
	gunixconnection.h	\
	gunixcredentialsmessage.h	\
	gunixmounts.h 		\
	gunixfdlist.h		\
	gunixfdmessage.h	\
	gunixinputstream.h 	\
	gunixoutputstream.h 	\
	gunixsocketaddress.h	\
	$(appinfo_headers) \
	$(NULL)

if HAVE_NETLINK
unix_sources +=			 \
	gnetworkmonitornetlink.c \
	gnetworkmonitornetlink.h \
	gnetworkmonitornm.c	 \
	gnetworkmonitornm.h	 \
	$(NULL)
endif
endif

gdbus_daemon_sources = \
	gdbusdaemon.c \
	gdbusdaemon.h \
	gdbus-daemon-generated.c \
	gdbus-daemon-generated.h \
	$(NULL)

win32_actual_sources = \
	$(gdbus_daemon_sources) \
	gwin32registrykey.c \
	gwin32registrykey.h \
	gwin32mount.c \
	gwin32mount.h \
	gwin32volumemonitor.c \
	gwin32volumemonitor.h \
	gwin32inputstream.c \
	gwin32outputstream.c \
	gwin32outputstream.h \
	gwin32networking.h \
	gwin32networkmonitor.c \
	gwin32networkmonitor.h \
	$(NULL)

win32_more_sources_for_vcproj = \
	gwin32appinfo.c \
	gcontenttype-win32.c \
	gregistrysettingsbackend.c \
	win32/gwinhttpfile.c \
	win32/gwinhttpfileinputstream.c \
	win32/gwinhttpfileoutputstream.c \
	win32/gwinhttpvfs.c \
	win32/gwin32fsmonitorutils.c \
	win32/gwin32filemonitor.c

if OS_WIN32
appinfo_sources += gwin32appinfo.c gwin32appinfo.h
contenttype_sources += gcontenttype-win32.c
platform_libadd += -lshlwapi -lws2_32 -ldnsapi -liphlpapi
win32_sources = $(win32_actual_sources)

giowin32includedir=$(includedir)/gio-win32-2.0/gio
giowin32include_HEADERS = \
	gwin32inputstream.h \
	gwin32outputstream.h \
	$(NULL)

endif

xdp_dbus_built_sources = xdp-dbus.c xdp-dbus.h
BUILT_SOURCES += $(xdp_dbus_built_sources)
CLEANFILES += $(xdp_dbus_built_sources)

portal_interfaces = \
	org.freedesktop.portal.Documents.xml \
	org.freedesktop.portal.OpenURI.xml \
	org.freedesktop.portal.NetworkMonitor.xml \
	org.freedesktop.portal.ProxyResolver.xml \
	$(NULL)

EXTRA_DIST += $(portal_interfaces)

$(xdp_dbus_built_sources) : $(portal_interfaces)
	$(AM_V_GEN) UNINSTALLED_GLIB_SRCDIR=$(top_srcdir) 		\
		UNINSTALLED_GLIB_BUILDDIR=$(top_builddir) 		\
		$(PYTHON) $(srcdir)/gdbus-2.0/codegen/gdbus-codegen.in 	\
		--interface-prefix org.freedesktop.portal.      	\
		--c-namespace GXdp					\
		--generate-c-code $(builddir)/xdp-dbus			\
		--annotate "org.freedesktop.portal.Documents.Add()" "org.gtk.GDBus.C.UnixFD" "true" \
		--annotate "org.freedesktop.portal.Documents.AddNamed()" "org.gtk.GDBus.C.UnixFD" "true" \
		--annotate "org.freedesktop.portal.Documents.AddFull()" "org.gtk.GDBus.C.UnixFD" "true" \
		--annotate "org.freedesktop.portal.OpenURI.OpenFile()" "org.gtk.GDBus.C.UnixFD" "true" \
		$^

portal_sources = \
	gnetworkmonitorportal.c 	\
	gnetworkmonitorportal.h		\
	gproxyresolverportal.c		\
	gproxyresolverportal.h		\
	$(xdp_dbus_built_sources)	\
	$(NULL)

if OS_UNIX
if !OS_COCOA
contenttype_sources += gcontenttype.c
appinfo_sources += gdesktopappinfo.c
endif
endif

gio_base_sources =		\
	gappinfo.c 		\
	gappinfoprivate.h	\
	gasynchelper.c 		\
	gasynchelper.h 		\
	gasyncinitable.c	\
	gasyncresult.c 		\
	gbufferedinputstream.c 	\
	gbufferedoutputstream.c \
	gbytesicon.c		\
	gcancellable.c 		\
	gcharsetconverter.c	\
	gcontextspecificgroup.c	\
	gcontextspecificgroup.h	\
	gconverter.c		\
	gconverterinputstream.c	\
	gconverteroutputstream.c	\
	gcredentials.c		\
	gcredentialsprivate.h	\
	gdatagrambased.c	\
	gdatainputstream.c 	\
	gdataoutputstream.c 	\
	gdrive.c 		\
	gdummyfile.h 		\
	gdummyfile.c 		\
	gdummyproxyresolver.c	\
	gdummyproxyresolver.h	\
	gdummytlsbackend.c	\
	gdummytlsbackend.h	\
	gemblem.h 		\
	gemblem.c 		\
	gemblemedicon.h		\
	gemblemedicon.c		\
	gfile.c 		\
	gfileattribute.c 	\
	gfileattribute-priv.h 	\
	gfileenumerator.c 	\
	gfileicon.c 		\
	gfileinfo.c 		\
	gfileinfo-priv.h 	\
	gfileinputstream.c 	\
	gfilemonitor.c 		\
	gfilenamecompleter.c 	\
	gfileoutputstream.c 	\
	gfileiostream.c		\
	gfilterinputstream.c 	\
	gfilteroutputstream.c 	\
	gicon.c 		\
	ginetaddress.c		\
	ginetaddressmask.c	\
	ginetsocketaddress.c	\
	ginitable.c		\
	ginputstream.c 		\
	gio_probes.d		\
	gio_trace.h		\
	gioenums.h		\
	gioerror.c 		\
	giomodule.c 		\
	giomodule-priv.c	\
	giomodule-priv.h	\
	gioscheduler.c 		\
	giostream.c		\
	gioprivate.h		\
	giowin32-priv.h 	\
	gloadableicon.c 	\
	gmount.c 		\
	gmemoryinputstream.c 	\
	gmemoryoutputstream.c 	\
	gmountoperation.c 	\
	gnativevolumemonitor.c 	\
	gnativevolumemonitor.h 	\
	gnativesocketaddress.c	\
	gnativesocketaddress.h	\
	gnetworkaddress.c	\
	gnetworking.c		\
	gnetworkingprivate.h	\
	gnetworkmonitor.c	\
	gnetworkmonitorbase.c	\
	gnetworkmonitorbase.h	\
	gnetworkservice.c	\
	goutputstream.c 	\
	gpermission.c 		\
	gpollableinputstream.c 	\
	gpollableoutputstream.c \
	gpollableutils.c	\
	gpollfilemonitor.c 	\
	gpollfilemonitor.h 	\
	gproxy.c		\
	gproxyaddress.c         \
	gproxyaddressenumerator.c \
	gproxyresolver.c	\
	gresolver.c		\
	gresource.c		\
	gresourcefile.c		\
	gresourcefile.h		\
	gseekable.c 		\
	gsimpleasyncresult.c 	\
	gsimpleiostream.c	\
	gsimplepermission.c 	\
	gsocket.c		\
	gsocketaddress.c	\
	gsocketaddressenumerator.c \
	gsocketclient.c		\
	gsocketconnectable.c	\
	gsocketconnection.c	\
	gsocketcontrolmessage.c	\
	gsocketinputstream.c	\
	gsocketinputstream.h	\
	gsocketlistener.c	\
	gsocketoutputstream.c	\
	gsocketoutputstream.h	\
	gsubprocesslauncher.c	\
	gsubprocess.c		\
	gsubprocesslauncher-private.h	\
	gsocketservice.c	\
	gsrvtarget.c		\
	gsimpleproxyresolver.c	\
	gtask.c			\
	gtcpconnection.c	\
	gtcpwrapperconnection.c \
	gthreadedsocketservice.c\
	gthemedicon.c 		\
	gthreadedresolver.c	\
	gthreadedresolver.h	\
	gtlsbackend.c		\
	gtlscertificate.c	\
	gtlsclientconnection.c	\
	gtlsconnection.c	\
	gtlsdatabase.c		\
	gtlsfiledatabase.c	\
	gtlsinteraction.c	\
	gtlspassword.c		\
	gtlsserverconnection.c	\
	gdtlsconnection.c	\
	gdtlsclientconnection.c	\
	gdtlsserverconnection.c	\
	gunionvolumemonitor.c 	\
	gunionvolumemonitor.h 	\
	gvfs.c 			\
	gvolume.c 		\
	gvolumemonitor.c 	\
	gzlibcompressor.c	\
	gzlibdecompressor.c	\
	gmountprivate.h 	\
	gioenumtypes.h		\
	gioenumtypes.c		\
	glistmodel.c 		\
	gliststore.c		\
	$(application_sources) 	\
	$(gdbus_sources) 	\
	$(local_sources) 	\
	$(NULL)

libgio_2_0_la_SOURCES =		\
	$(gio_base_sources)	\
	$(appinfo_sources) 	\
	$(contenttype_sources) \
	$(unix_sources) 	\
	$(win32_sources) 	\
	$(settings_sources) 	\
	$(NULL)

EXTRA_DIST += strinfo.c

libgio_2_0_la_LIBADD = \
	$(top_builddir)/gobject/libgobject-2.0.la 	\
	$(top_builddir)/gmodule/libgmodule-2.0.la 	\
	$(top_builddir)/glib/libglib-2.0.la 		\
	$(platform_libadd) 				\
	$(ZLIB_LIBS)					\
	$(SELINUX_LIBS) 				\
	$(GLIB_LIBS) 					\
	$(XATTR_LIBS) 					\
	$(NETWORK_LIBS)					\
	$(NULL)

libgio_2_0_la_CPPFLAGS = $(ZLIB_CFLAGS) $(AM_CPPFLAGS)

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32_AND_DLL_COMPILATION
gio_win32_res = gio-win32-res.o
gio_win32_res_ldflag = -Wl,$(gio_win32_res)
endif

install-data-local: install-ms-lib
	$(mkinstalldirs) $(DESTDIR)$(GIO_MODULE_DIR)

uninstall-local: uninstall-ms-lib

libgio_2_0_la_CFLAGS = $(AM_CFLAGS) $(GLIB_HIDDEN_VISIBILITY_CFLAGS)
libgio_2_0_la_LDFLAGS = $(GLIB_LINK_FLAGS) \
	$(gio_win32_res_ldflag) \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined)

if OS_COCOA
# Let's avoid polluting the CFLAGS of libgio with -xobjective-c
# We cannot simply rename the ObjC files to .m because it plays
# tricks with Automake. For further reference, see:
# https://bugzilla.gnome.org/show_bug.cgi?id=672777
libgio_objc_2_0_la_CFLAGS = $(libgio_2_0_la_CFLAGS) -xobjective-c
libgio_objc_2_0_la_CPPFLAGS = $(libgio_2_0_la_CPPFLAGS)
libgio_objc_2_0_la_LDFLAGS = $(libgio_2_0_la_LDFLAGS) -Wl,-framework,Foundation -Wl,-framework,AppKit
libgio_objc_2_0_la_SOURCES = \
	gnextstepsettingsbackend.c \
	gosxcontenttype.c \
	gosxappinfo.c \
	gosxappinfo.h
if MAC_OS_X_9
libgio_objc_2_0_la_SOURCES += \
	gcocoanotificationbackend.c
endif

noinst_LTLIBRARIES += libgio-objc-2.0.la
libgio_2_0_la_LIBADD += libgio-objc-2.0.la
endif

if HAVE_LIBMOUNT
libgio_2_0_la_CFLAGS += $(LIBMOUNT_CFLAGS)
libgio_2_0_la_LIBADD += $(LIBMOUNT_LIBS)
endif

EXTRA_libgio_2_0_la_DEPENDENCIES = $(gio_win32_res) $(gio_def) $(platform_deps)

gio-win32-res.o: gio.rc
	$(WINDRES) gio.rc $@

gio_headers =			\
	gappinfo.h 		\
	gasyncinitable.h	\
	gasyncresult.h 		\
	gbufferedinputstream.h 	\
	gbufferedoutputstream.h \
	gbytesicon.h		\
	gcancellable.h 		\
	gcontenttype.h 		\
	gcharsetconverter.h	\
	gconverter.h		\
	gconverterinputstream.h	\
	gconverteroutputstream.h	\
	gdatagrambased.h	\
	gdatainputstream.h 	\
	gdataoutputstream.h 	\
	gdrive.h 		\
	gemblem.h 		\
	gemblemedicon.h		\
	gfile.h 		\
	gfileattribute.h 	\
	gfileenumerator.h 	\
	gfileicon.h 		\
	gfileinfo.h 		\
	gfileinputstream.h 	\
	gfilemonitor.h 		\
	gfilenamecompleter.h 	\
	gfileoutputstream.h 	\
	gfileiostream.h		\
	gfilterinputstream.h 	\
	gfilteroutputstream.h 	\
	gicon.h 		\
	ginetaddress.h		\
	ginetaddressmask.h	\
	ginetsocketaddress.h	\
	ginputstream.h 		\
	ginitable.h		\
	gio.h			\
	gio-autocleanups.h	\
	giotypes.h		\
	gioenums.h		\
	gioerror.h 		\
	giomodule.h 		\
	gioscheduler.h 		\
	giostream.h		\
	gloadableicon.h 	\
	gmount.h 		\
	gmemoryinputstream.h 	\
	gmemoryoutputstream.h 	\
	gmountoperation.h 	\
	gnativevolumemonitor.h 	\
	gnetworkaddress.h	\
	gnetworkmonitor.h	\
	gnetworkservice.h	\
	goutputstream.h 	\
	gpermission.h 		\
	gpollableinputstream.h 	\
	gpollableoutputstream.h \
	gpollableutils.h	\
	gproxyaddress.h         \
	gproxy.h		\
	gproxyaddressenumerator.h \
	gproxyresolver.h	\
	gresolver.h		\
	gresource.h		\
	gseekable.h 		\
	gsimpleasyncresult.h 	\
	gsimpleiostream.h	\
	gsimplepermission.h 	\
	gsocket.h		\
	gsocketaddress.h	\
	gsocketaddressenumerator.h \
	gsocketclient.h		\
	gsocketconnectable.h	\
	gsocketconnection.h	\
	gsocketcontrolmessage.h	\
	gsocketlistener.h	\
	gsocketservice.h	\
	gsrvtarget.h		\
	gsimpleproxyresolver.h	\
	gtask.h			\
	gsubprocess.h		\
	gsubprocesslauncher.h	\
	gtcpconnection.h	\
	gtcpwrapperconnection.h \
	gthreadedsocketservice.h\
	gthemedicon.h 		\
	gtlsbackend.h		\
	gtlscertificate.h	\
	gtlsclientconnection.h	\
	gtlsconnection.h	\
	gtlsdatabase.h		\
	gtlsfiledatabase.h	\
	gtlsinteraction.h	\
	gtlspassword.h		\
	gtlsserverconnection.h	\
	gdtlsconnection.h	\
	gdtlsclientconnection.h	\
	gdtlsserverconnection.h	\
	gvfs.h 			\
	gvolume.h 		\
	gvolumemonitor.h 	\
	gzlibcompressor.h	\
	gzlibdecompressor.h	\
	glistmodel.h		\
	gliststore.h		\
	$(application_headers)	\
	$(settings_headers)	\
	$(gdbus_headers)	\
	$(NULL)

gioincludedir=$(includedir)/glib-2.0/gio/
gioinclude_HEADERS = 		\
	$(gio_headers)		\
	gioenumtypes.h

nodist_gioinclude_HEADERS =	\
	gnetworking.h		\
	$(NULL)

# these sources (also mentioned above) are generated.
BUILT_SOURCES += 		\
	gconstructor_as_data.h	\
	gioenumtypes.h		\
	gioenumtypes.c		\
	gdbus-daemon-generated.c \
	gdbus-daemon-generated.h \
	gnetworking.h		\
	$(NULL)

EXTRA_DIST += 			\
	data-to-c.py		\
	gioenumtypes.h.template	\
	gioenumtypes.c.template	\
	gio.rc.in		\
	gschema.dtd		\
	gconstructor_as_data.h	\
	gnetworking.h.win32	\
	$(NULL)

BUILT_EXTRA_DIST += 		\
	gio.rc

# This is read by gobject-introspection/misc/ and gtk-doc
gio-public-headers.txt: Makefile
	$(AM_V_GEN) echo $(gioinclude_HEADERS) $(giowin32include_HEADERS) $(giounixinclude_HEADERS) > $@.tmp && mv $@.tmp $@

CLEANFILES += gdbus-daemon-generated.c gdbus-daemon-generated.h gio-public-headers.txt gconstructor_as_data.h


DISTCLEANFILES += 		\
	gioenumtypes.h		\
	gioenumtypes.c

all-local: gio-public-headers.txt

gioenumtypes.h: $(gio_headers) gioenumtypes.h.template
	$(AM_V_GEN) $(top_builddir)/gobject/glib-mkenums --template $(filter %.template,$^) $(filter-out %.template,$^) > \
	    gioenumtypes.h.tmp && mv gioenumtypes.h.tmp gioenumtypes.h

gioenumtypes.c: $(gio_headers) gioenumtypes.c.template
	$(AM_V_GEN) $(top_builddir)/gobject/glib-mkenums --template $(filter %.template,$^) $(filter-out %.template,$^) > \
	    gioenumtypes.c.tmp && mv gioenumtypes.c.tmp gioenumtypes.c

gio.def: libgio-2.0.la
	$(AM_V_GEN) dumpbin.exe -exports .libs/libgio-2.0-0.dll | awk 'BEGIN { print "EXPORTS" } / +[[:digit:]]+ +[[:xdigit:]]+ +[[:xdigit:]]+/{ print $$4 }' > gio.def.tmp && mv gio.def.tmp gio.def

gio-2.0.lib: libgio-2.0.la gio.def
	$(AM_V_GEN) lib.exe -machine:@LIB_EXE_MACHINE_FLAG@ -name:libgio-2.0-$(LT_CURRENT_MINUS_AGE).dll -def:$(builddir)/gio.def -out:$@

bin_PROGRAMS = gio-querymodules glib-compile-schemas glib-compile-resources gsettings

glib_compile_resources_LDADD = libgio-2.0.la 		\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la 	\
	$(top_builddir)/glib/libglib-2.0.la 		\
	$(NULL)

glib_compile_resources_SOURCES = \
	gvdb/gvdb-format.h		\
	gvdb/gvdb-builder.h		\
	gvdb/gvdb-builder.c		\
	glib-compile-resources.c

gio_querymodules_SOURCES = gio-querymodules.c giomodule-priv.c
gio_querymodules_LDADD	 = libgio-2.0.la 		\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la	\
	$(top_builddir)/glib/libglib-2.0.la		\
	$(NULL)

gconstructor_as_data.h: $(top_srcdir)/glib/gconstructor.h data-to-c.py
	$(AM_V_GEN) $(srcdir)/data-to-c.py $(top_srcdir)/glib/gconstructor.h gconstructor_code $@

glib_compile_schemas_LDADD = $(top_builddir)/glib/libglib-2.0.la
glib_compile_schemas_SOURCES = \
	gconstructor_as_data.h \
	gvdb/gvdb-format.h		\
	gvdb/gvdb-builder.h		\
	gvdb/gvdb-builder.c		\
	glib-compile-schemas.c

gsettings_LDADD = libgio-2.0.la 			\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la 	\
	$(top_builddir)/glib/libglib-2.0.la		\
	$(NULL)
gsettings_SOURCES = gsettings-tool.c

schemadir = $(datadir)/glib-2.0/schemas
dist_schema_DATA = gschema.dtd

itsdir = $(datadir)/gettext/its
dist_its_DATA = gschema.loc gschema.its

# ------------------------------------------------------------------------
# SystemTap and dtrace

if ENABLE_DTRACE
gio_probes.h: gio_probes.d
	$(AM_V_GEN) $(DTRACE) -C -h -s $< -o $@.tmp
	@$(SED) \
		-e "s,define STAP_HAS_SEMAPHORES 1,undef STAP_HAS_SEMAPHORES," \
		-e "s,define _SDT_HAS_SEMAPHORES 1,undef _SDT_HAS_SEMAPHORES," \
		< $@.tmp > $@ && rm -f $@.tmp

gio_probes.lo: gio_probes.d
	$(AM_V_GEN) $(LIBTOOL) --mode=compile $(AM_V_lt) --tag=CC $(DTRACE) -G -s $< -o $@

BUILT_SOURCES += gio_probes.h gio_probes.lo
CLEANFILES += gio_probes.h gio_probes.h.tmp
libgio_2_0_la_LIBADD += gio_probes.lo
endif

tapsetdir   = @ABS_TAPSET_DIR@
EXTRA_DIST += gio.stp.in

if ENABLE_SYSTEMTAP
tapset_DATA = libgio-2.0.so.0.@LT_CURRENT@.@LT_REVISION@.stp
CLEANFILES += $(tapset_DATA)

$(tapset_DATA): gio.stp.in Makefile
	$(AM_V_GEN)$(SED) \
		-e 's|[@]ABS_GLIB_RUNTIME_LIBDIR[@]|$(ABS_GLIB_RUNTIME_LIBDIR)|g' \
		-e 's|[@]LT_CURRENT[@]|$(LT_CURRENT)|g' \
		-e 's|[@]LT_REVISION[@]|$(LT_REVISION)|g' \
		$< > $@
endif

# ------------------------------------------------------------------------
# gdbus(1) tool

bin_PROGRAMS += gdbus
gdbus_SOURCES = gdbus-tool.c
gdbus_LDADD = libgio-2.0.la 				\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la 	\
	$(top_builddir)/glib/libglib-2.0.la		\
	$(NULL)

if OS_UNIX
if !OS_COCOA
# ------------------------------------------------------------------------
# gapplication(1) tool
bin_PROGRAMS += gapplication
gapplication_SOURCES = gapplication-tool.c
gapplication_LDADD = libgio-2.0.la 			\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la 	\
	$(top_builddir)/glib/libglib-2.0.la		\
	$(NULL)
endif
endif

completiondir = $(datadir)/bash-completion/completions
completion_DATA = \
	completion/gapplication				\
	completion/gdbus				\
	completion/gsettings				\
	completion/gresource
EXTRA_DIST += $(completion_DATA)

# ------------------------------------------------------------------------
# gresource tool

bin_PROGRAMS += gresource
gresource_SOURCES = gresource-tool.c
gresource_CPPFLAGS = $(LIBELF_CFLAGS) $(AM_CPPFLAGS)
gresource_LDADD = libgio-2.0.la				\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la 	\
	$(top_builddir)/glib/libglib-2.0.la		\
	$(LIBELF_LIBS)

# ------------------------------------------------------------------------
# gio tool

bin_PROGRAMS += gio
gio_SOURCES = \
	gio-tool.c					\
	gio-tool.h					\
	gio-tool-cat.c					\
	gio-tool-copy.c					\
	gio-tool-info.c					\
	gio-tool-list.c					\
	gio-tool-mime.c					\
	gio-tool-mkdir.c				\
	gio-tool-monitor.c				\
	gio-tool-mount.c				\
	gio-tool-move.c					\
	gio-tool-open.c					\
	gio-tool-rename.c				\
	gio-tool-remove.c				\
	gio-tool-save.c					\
	gio-tool-set.c					\
	gio-tool-trash.c				\
	gio-tool-tree.c					\
	$(NULL)
gio_LDADD = libgio-2.0.la 				\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/glib/libglib-2.0.la		\
	$(NULL)

# ------------------------------------------------------------------------
# ------ MSVC Project File Generation ------
# ------------------------------------------------------------------------
#
MSVCPROJS = gio glib-compile-resources glib-compile-schemas gio-tool

gio_FILES =	\
	$(gio_base_sources)			\
	$(win32_actual_sources)			\
	$(win32_more_sources_for_vcproj)	\
	$(settings_base_sources)		\
	$(win32_settings_sources)

gio_EXCLUDES = dummy

gio_HEADERS_DIR = $(includedir)/glib-2.0/gio
gio_HEADERS_INST = $(gioinclude_HEADERS) $(nodist_gioinclude_HEADERS)
gio_HEADERS_EXCLUDES = dummy

glib_compile_resources_FILES = $(glib_compile_resources_SOURCES)
glib_compile_resources_EXCLUDES = dummy

glib_compile_schemas_FILES = $(glib_compile_schemas_SOURCES)
glib_compile_schemas_EXCLUDES = dummy

gio_tool_FILES = $(gio_SOURCES)
gio_tool_EXCLUDES = dummy

include $(top_srcdir)/win32/Makefile.msvcproj

dist-hook: \
	$(BUILT_EXTRA_DIST)						\
	$(top_builddir)/win32/vs9/gio.vcproj			\
	$(top_builddir)/win32/vs9/gio.headers			\
	$(top_builddir)/win32/vs9/glib-compile-schemas.vcproj	\
	$(top_builddir)/win32/vs9/glib-compile-resources.vcproj	\
	$(top_builddir)/win32/vs9/gio-tool.vcproj
	files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  cp $$d/$$f $(distdir) || exit 1; done

if HAVE_GLIB_RUNTIME_LIBDIR
ABS_GLIB_RUNTIME_LIBDIR = $(realpath $(libdir)/$(GLIB_RUNTIME_LIBDIR))
else
ABS_GLIB_RUNTIME_LIBDIR = $(libdir)
endif

if HAVE_GLIB_RUNTIME_LIBDIR
install-data-hook:
	mkdir -p $(DESTDIR)$(libdir)/$(GLIB_RUNTIME_LIBDIR)
	mv $(DESTDIR)$(libdir)/libgio-2.0.so.0 $(DESTDIR)$(libdir)/$(GLIB_RUNTIME_LIBDIR)
	mv $(DESTDIR)$(libdir)/libgio-2.0.so.0.$(LT_CURRENT).$(LT_REVISION) $(DESTDIR)$(libdir)/$(GLIB_RUNTIME_LIBDIR)
	rm -f $(DESTDIR)$(libdir)/libgio-2.0.so
	ln -s $(GLIB_RUNTIME_LIBDIR)/libgio-2.0.so.0.$(LT_CURRENT).$(LT_REVISION) $(DESTDIR)$(libdir)/libgio-2.0.so
endif
