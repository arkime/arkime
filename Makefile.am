SUBDIRS = . capture db viewer parliament release
install-exec-local:
	npm install
	@INSTALL@ -D package.json @prefix@/package.json
	@INSTALL@ -D package-lock.json @prefix@/package-lock.json
	(cd @prefix@ ; npm install --production)
	rm -f @prefix@/package-lock.json

config:
	@prefix@/bin/Configure
