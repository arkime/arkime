SUBDIRS = . capture db viewer

if BUILD_CONT3XT
SUBDIRS += cont3xt
endif

if BUILD_WISE
SUBDIRS += wiseService
endif

if BUILD_PARLIAMENT
SUBDIRS += parliament
endif

if BUILD_RELEASE
SUBDIRS += release
endif

SUBDIRS += tests

install-exec-local:
	$(MKDIR_P) $(DESTDIR)@prefix@
	npm ci
	cp -pr common $(DESTDIR)@prefix@
	cp -pr assets $(DESTDIR)@prefix@ 
	@INSTALL@ package.json $(DESTDIR)@prefix@/package.json
	@INSTALL@ package-lock.json $(DESTDIR)@prefix@/package-lock.json
	(cd $(DESTDIR)@prefix@ ; npm ci --production)
	rm -f $(DESTDIR)@prefix@/package-lock.json

check-local:
	npm ci

config:
	$(DESTDIR)@prefix@/bin/Configure
