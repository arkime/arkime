GENERATED_ITEMS = \
	glib.vcproj		\
	gobject.vcproj		\
	gio.vcproj		\
	glib-compile-schemas.vcproj	\
	glib-compile-resources.vcproj	\
	gio-tool.vcproj	\
	glib-version-paths.vsprops	\
	glib-install.vsprops

MSVC_HEADERS_LIST = glib.headers gobject.headers gio.headers

EXTRA_DIST = \
	README.txt		\
	glib.sln		\
	glib.vcprojin		\
	gspawn-win32-helper-console.vcproj 	\
	gspawn-win32-helper.vcproj	\
	gmodule.vcproj		\
	gobject.vcprojin	\
	gthread.vcproj		\
	gio.vcprojin		\
	gsettings.vcproj	\
	gresource.vcproj	\
	gio-querymodules.vcproj	\
	gdbus.vcproj		\
	glib-compile-schemas.vcprojin	\
	glib-compile-resources.vcprojin	\
	gio-tool.vcprojin	\
	glib-install.vcproj	\
	glib-build-defines.vsprops	\
	glib-version-paths.vsprops.in	\
	glib-gen-srcs.vsprops	\
	glib-install.vspropsin	\
	$(GENERATED_ITEMS)

glib-install.vsprops: $(top_srcdir)/win32/vs9/glib-install.vspropsin $(MSVC_HEADERS_LIST)
	$(CPP) -P - <$(top_srcdir)/win32/vs9/glib-install.vspropsin >$@
	rm $(MSVC_HEADERS_LIST)

DISTCLEANFILES = $(GENERATED_ITEMS)
