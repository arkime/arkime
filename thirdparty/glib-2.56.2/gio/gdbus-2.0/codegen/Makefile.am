include $(top_srcdir)/glib.mk

bin_SCRIPTS =

codegendir = $(datadir)/glib-2.0/codegen
codegen_PYTHON =						\
	__init__.py						\
	codegen.py						\
	codegen_main.py						\
	codegen_docbook.py					\
	config.py						\
	dbustypes.py						\
	parser.py						\
	utils.py						\
	$(NULL)

CLEANFILES += config.pyc

bin_SCRIPTS += gdbus-codegen
CLEANFILES += gdbus-codegen
EXTRA_DIST += gdbus-codegen.in

gdbus-codegen: gdbus-codegen.in Makefile $(codegen_PYTHON)
	$(AM_V_GEN) sed -e 's,@DATADIR\@,$(datadir),' -e 's,@PYTHON\@,$(PYTHON),' $< > $@.tmp && mv $@.tmp $@
	@chmod a+x $@

clean-local:
	rm -f *~
