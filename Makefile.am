SUBDIRS = . capture db viewer parliament release
install-exec-local:
	npm install
	@INSTALL@ package.json @prefix@
	(cd @prefix@ ; npm install --production)

config:
	@prefix@/bin/Configure
