find . -name 'node_modules' -type d -prune -exec rm -rf '{}' +;
find . -name 'package-lock.json' -prune -exec rm '{}' +;

