SUBDIRS = . capture db viewer parliament release
install-exec-local:
	npm install
	@INSTALL@ -D package.json @prefix@/package.json
	(cd @prefix@ ; npm install --production)

config:
	@prefix@/bin/Configure
