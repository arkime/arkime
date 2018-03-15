SUBDIRS = . capture db viewer parliament release
install-exec-local:
	npm install
	(cd @prefix@ ; npm install --production)

config:
	@prefix@/bin/Configure
