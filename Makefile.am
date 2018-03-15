SUBDIRS = . capture db viewer parliament release
install-exec-local:
	npm install
	/bin/cp -f package.json @prefix@
	(cd @prefix@ ; npm install --production)

config:
	@prefix@/bin/Configure
