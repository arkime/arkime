GENERATED_ITEMS = \
	glib.vcxproj	\
	glib.vcxproj.filters	\
	gobject.vcxproj	\
	gobject.vcxproj.filters	\
	gio.vcxproj	\
	gio.vcxproj.filters	\
	glib-compile-schemas.vcxproj	\
	glib-compile-schemas.vcxproj.filters	\
	glib-compile-resources.vcxproj	\
	glib-compile-resources.vcxproj.filters	\
	gio-tool.vcxproj	\
	gio-tool.vcxproj.filters	\
	glib-version-paths.props	\
	glib-install.props

MSVC10_HEADERS_LIST = glib.vs10.headers gobject.vs10.headers gio.vs10.headers

EXTRA_DIST = \
	README.txt			\
	glib.sln			\
	glib.vcxprojin			\
	glib.vcxproj.filtersin		\
	gspawn-win32-helper-console.vcxproj	\
	gspawn-win32-helper-console.vcxproj.filters	\
	gspawn-win32-helper.vcxproj		\
	gspawn-win32-helper.vcxproj.filters	\
	gmodule.vcxproj			\
	gmodule.vcxproj.filters		\
	gobject.vcxprojin		\
	gobject.vcxproj.filtersin	\
	gthread.vcxproj			\
	gthread.vcxproj.filters		\
	gio.vcxprojin			\
	gio.vcxproj.filtersin		\
	glib-compile-schemas.vcxprojin		\
	glib-compile-schemas.vcxproj.filtersin		\
	gsettings.vcxproj		\
	gsettings.vcxproj.filters		\
	glib-compile-resources.vcxprojin		\
	glib-compile-resources.vcxproj.filtersin	\
	gio-tool.vcxprojin	\
	gio-tool.vcxproj.filtersin	\
	gresource.vcxproj		\
	gresource.vcxproj.filters		\
	gio-querymodules.vcxproj		\
	gio-querymodules.vcxproj.filters		\
	gdbus.vcxproj			\
	gdbus.vcxproj.filters		\
	glib-install.vcxproj		\
	glib-install.vcxproj.filters	\
	glib-build-defines.props	\
	glib-install.propsin		\
	glib-version-paths.props.in	\
	glib-gen-srcs.props		\
	$(GENERATED_ITEMS)

glib-install.props: $(top_srcdir)/win32/vs10/glib-install.propsin $(MSVC10_HEADERS_LIST)
	-$(RM) $(top_builddir)/win32/vs11/glib-install.props
	-$(RM) $(top_builddir)/win32/vs12/glib-install.props
	-$(RM) $(top_builddir)/win32/vs14/glib-install.props
	-$(RM) $(top_builddir)/win32/vs15/glib-install.props
	$(CPP) -P - <$(top_srcdir)/win32/vs10/glib-install.propsin >$@
	rm $(MSVC10_HEADERS_LIST)

DISTCLEANFILES = $(GENERATED_ITEMS)
