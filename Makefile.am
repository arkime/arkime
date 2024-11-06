SUBDIRS = . capture db viewer parliament release tests

if BUILD_CONT3XT
SUBDIRS += cont3xt
endif

if BUILD_WISE
SUBDIRS += wiseService
endif

install-exec-local:
	npm ci
	cp -pr common @prefix@
	cp -pr assets @prefix@
	@INSTALL@ -D package.json @prefix@/package.json
	@INSTALL@ -D package-lock.json @prefix@/package-lock.json
	(cd @prefix@ ; npm ci --production)
	rm -f @prefix@/package-lock.json

check-local:
	npm ci

config:
	@prefix@/bin/Configure
