The snf reader plugin now works directly with Myricom SNF

Optional settings
* ```snfNumRings``` number of rings and threads to use per interface
* ```snfDataRingSize``` SNF_DATARING_SIZE


To use:
* install the snf package on the build hosts and all hosts that will run moloch-capture
* build the plugin by using ```make``` in the ```capture/plugins/snf``` directory
* load the snf plugin by changing configuration file so it has reader-snf.so as a rootPlugins ```rootPlugins=reader-snf.so```
* tell moloch-capture to use snf as the reader method with ```pcapReadMethod=snf`` in your configuration file
* change ```interface``` to either the OS interface name or snf# where # is the portnum

