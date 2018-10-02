SUBDIRS = . capture db viewer parliament release tests
install-exec-local:
	npm ci
	@INSTALL@ -D package.json @prefix@/package.json
	@INSTALL@ -D package-lock.json @prefix@/package-lock.json
	(cd @prefix@ ; npm ci --production)
	rm -f @prefix@/package-lock.json

check-local:
	npm ci

config:
	@prefix@/bin/Configure
