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

# npm ci builds arkime-iptrie, re2 and other native modules in parallel, and each
# node-gyp downloads the node headers into the same ~/.cache/node-gyp/<version>
# if they are missing, so one can compile against half extracted headers.
# Fetching the headers first (a no-op once they are cached) means none of them download.
NODE_GYP_ENSURE = npx --yes node-gyp install --ensure

install-exec-local:
	$(MKDIR_P) $(DESTDIR)@prefix@
	$(NODE_GYP_ENSURE)
	npm ci
	cp -pr common $(DESTDIR)@prefix@
	cp -pr assets $(DESTDIR)@prefix@
	@INSTALL@ package.json $(DESTDIR)@prefix@/package.json
	@INSTALL@ package-lock.json $(DESTDIR)@prefix@/package-lock.json
	(cd $(DESTDIR)@prefix@ ; npm ci --production)
	rm -f $(DESTDIR)@prefix@/package-lock.json

check-local:
	$(NODE_GYP_ENSURE)
	npm ci

config:
	$(DESTDIR)@prefix@/bin/Configure
