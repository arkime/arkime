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
	npm ci
	cp -pr common @prefix@
	cp -pr assets @prefix@
	@INSTALL@ package.json @prefix@/package.json
	@INSTALL@ package-lock.json @prefix@/package-lock.json
	(cd @prefix@ ; npm ci --production)
	rm -f @prefix@/package-lock.json

check-local:
	npm ci

config:
	@prefix@/bin/Configure
